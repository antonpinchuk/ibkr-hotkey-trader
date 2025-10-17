#include "server/remotecontrolserver.h"
#include "client/ibkrclient.h"
#include "models/tickerdatamanager.h"
#include "widgets/tickerlistwidget.h"
#include "utils/logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>

RemoteControlServer::RemoteControlServer(IBKRClient* client, TickerDataManager* tickerDataManager,
                                         TickerListWidget* tickerList, QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_client(client)
    , m_tickerDataManager(tickerDataManager)
    , m_tickerList(tickerList)
    , m_nextSearchReqId(10000) // Start from 10000 to avoid conflicts
{
    connect(m_server, &QTcpServer::newConnection, this, &RemoteControlServer::onNewConnection);
    connect(m_client, &IBKRClient::symbolSearchResultsReceived, this, &RemoteControlServer::onSymbolSearchResults);
}

RemoteControlServer::~RemoteControlServer()
{
    stop();
}

bool RemoteControlServer::start(quint16 port)
{
    if (m_server->isListening()) {
        stop();
    }

    // Listen on all network interfaces (0.0.0.0)
    if (!m_server->listen(QHostAddress::Any, port)) {
        LOG_ERROR(QString("Failed to start Remote Control Server on port %1: %2")
                  .arg(port).arg(m_server->errorString()));
        return false;
    }

    LOG_DEBUG(QString("Remote Control Server started on port %1").arg(port));
    return true;
}

void RemoteControlServer::stop()
{
    if (m_server->isListening()) {
        m_server->close();
        LOG_DEBUG("Remote Control Server stopped");
    }
}

bool RemoteControlServer::isListening() const
{
    return m_server->isListening();
}

void RemoteControlServer::onNewConnection()
{
    QTcpSocket* socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QTcpSocket::readyRead, this, &RemoteControlServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &RemoteControlServer::onDisconnected);
}

void RemoteControlServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QByteArray requestData = socket->readAll();
    HttpRequest request = parseHttpRequest(requestData);

    // Check if TWS is connected
    if (!m_client || !m_client->isConnected()) {
        sendHttpResponse(socket, 502, "Bad Gateway", QJsonObject(), "No connection with TWS");
        socket->disconnectFromHost();
        return;
    }

    if (!request.isValid) {
        sendHttpResponse(socket, 400, "Bad Request", QJsonObject(), request.errorMessage);
        socket->disconnectFromHost();
        return;
    }

    // Route request
    if (request.path == "/ticker") {
        if (request.method == "POST") {
            handlePostTicker(socket, request.body);
        } else if (request.method == "PUT") {
            handlePutTicker(socket, request.body);
        } else if (request.method == "DELETE") {
            handleDeleteTicker(socket, request.body);
        } else {
            sendHttpResponse(socket, 405, "Method Not Allowed");
            socket->disconnectFromHost();
        }
    } else {
        sendHttpResponse(socket, 404, "Not Found");
        socket->disconnectFromHost();
    }
}

void RemoteControlServer::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

RemoteControlServer::HttpRequest RemoteControlServer::parseHttpRequest(const QByteArray& data)
{
    HttpRequest request;
    request.isValid = false;

    QString requestStr = QString::fromUtf8(data);
    QStringList lines = requestStr.split("\r\n");

    if (lines.isEmpty()) {
        request.errorMessage = "Empty request";
        return request;
    }

    // Parse request line
    QStringList requestLine = lines[0].split(" ");
    if (requestLine.size() < 3) {
        request.errorMessage = "Invalid request line";
        return request;
    }

    request.method = requestLine[0];
    request.path = requestLine[1];

    // Find empty line (separates headers from body)
    int emptyLineIndex = -1;
    for (int i = 1; i < lines.size(); i++) {
        if (lines[i].isEmpty()) {
            emptyLineIndex = i;
            break;
        }
    }

    // Parse JSON body if exists
    if (emptyLineIndex >= 0 && emptyLineIndex + 1 < lines.size()) {
        QString bodyStr = lines.mid(emptyLineIndex + 1).join("\r\n");
        QJsonDocument jsonDoc = QJsonDocument::fromJson(bodyStr.toUtf8());

        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            request.errorMessage = "Invalid JSON";
            return request;
        }

        request.body = jsonDoc.object();

        // Validate required fields
        if (!request.body.contains("symbol") || !request.body.contains("exchange")) {
            request.errorMessage = "Missing required fields: symbol, exchange";
            return request;
        }
    }

    request.isValid = true;
    return request;
}

void RemoteControlServer::sendHttpResponse(QTcpSocket* socket, int statusCode,
                                           const QString& statusMessage,
                                           const QJsonObject& body,
                                           const QString& errorMessage)
{
    QString response;
    response += QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusMessage);
    response += "Content-Type: application/json\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n";

    QJsonObject responseBody = body;
    if (!errorMessage.isEmpty()) {
        responseBody["error"] = errorMessage;
    }

    QByteArray bodyData;
    if (!responseBody.isEmpty()) {
        bodyData = QJsonDocument(responseBody).toJson(QJsonDocument::Compact);
        response += QString("Content-Length: %1\r\n").arg(bodyData.size());
    } else {
        response += "Content-Length: 0\r\n";
    }

    response += "\r\n";

    socket->write(response.toUtf8());
    if (!bodyData.isEmpty()) {
        socket->write(bodyData);
    }
    socket->flush();
}

void RemoteControlServer::handlePostTicker(QTcpSocket* socket, const QJsonObject& body)
{
    QString symbol = body["symbol"].toString().toUpper();
    QString exchange = body["exchange"].toString().toUpper();

    // Check if ticker already exists in our list
    QStringList existingTickers = m_tickerList->getAllSymbols();
    if (existingTickers.contains(symbol)) {
        LOG_DEBUG(QString("Remote Control: POST /ticker - symbol=%1, exchange=%2; 409: Ticker already added")
                  .arg(symbol).arg(exchange));
        sendHttpResponse(socket, 409, "Conflict", QJsonObject(), "Ticker already added");
        socket->disconnectFromHost();
        return;
    }

    // Start async symbol search in TWS
    int reqId = m_nextSearchReqId++;
    m_searchReqIdToSocket[reqId] = socket;
    m_searchReqIdToSymbolExchange[reqId] = qMakePair(symbol, exchange);

    m_client->searchSymbol(reqId, symbol);

    // Response will be sent in onSymbolSearchResults
}

void RemoteControlServer::handlePutTicker(QTcpSocket* socket, const QJsonObject& body)
{
    QString symbol = body["symbol"].toString().toUpper();
    QString exchange = body["exchange"].toString().toUpper();

    // Check if ticker exists in our list
    QStringList existingTickers = m_tickerList->getAllSymbols();
    if (!existingTickers.contains(symbol)) {
        LOG_DEBUG(QString("Remote Control: PUT /ticker - symbol=%1, exchange=%2; 404: No ticker found")
                  .arg(symbol).arg(exchange));
        sendHttpResponse(socket, 404, "Not Found", QJsonObject(), "No ticker found");
        socket->disconnectFromHost();
        return;
    }

    // Activate ticker (make it active)
    emit tickerSelectRequested(symbol);

    LOG_DEBUG(QString("Remote Control: PUT /ticker - symbol=%1, exchange=%2; 200: OK").arg(symbol).arg(exchange));
    sendHttpResponse(socket, 200, "OK");
    socket->disconnectFromHost();
}

void RemoteControlServer::handleDeleteTicker(QTcpSocket* socket, const QJsonObject& body)
{
    QString symbol = body["symbol"].toString().toUpper();
    QString exchange = body["exchange"].toString().toUpper();

    // Check if ticker exists in our list
    QStringList existingTickers = m_tickerList->getAllSymbols();
    if (!existingTickers.contains(symbol)) {
        LOG_DEBUG(QString("Remote Control: DELETE /ticker - symbol=%1, exchange=%2; 404: No ticker found")
                  .arg(symbol).arg(exchange));
        sendHttpResponse(socket, 404, "Not Found", QJsonObject(), "No ticker found");
        socket->disconnectFromHost();
        return;
    }

    // Delete ticker
    emit tickerDeleteRequested(symbol);

    LOG_DEBUG(QString("Remote Control: DELETE /ticker - symbol=%1, exchange=%2; 204: Deleted").arg(symbol).arg(exchange));
    sendHttpResponse(socket, 204, "No Content");
    socket->disconnectFromHost();
}

void RemoteControlServer::onSymbolSearchResults(int reqId, const QList<QPair<QString, QPair<QString, QString>>>& results)
{
    // Check if this is one of our requests
    if (!m_searchReqIdToSocket.contains(reqId)) {
        return;
    }

    QTcpSocket* socket = m_searchReqIdToSocket.value(reqId);
    QPair<QString, QString> symbolExchange = m_searchReqIdToSymbolExchange.value(reqId);
    QString requestedSymbol = symbolExchange.first;
    QString requestedExchange = symbolExchange.second;

    // Clean up
    m_searchReqIdToSocket.remove(reqId);
    m_searchReqIdToSymbolExchange.remove(reqId);

    // Check if socket is still valid
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // Find matching symbol and exchange
    bool found = false;
    QString matchedSymbol;
    QString matchedExchange;

    for (const auto& result : results) {
        QString symbol = result.first;
        QString exchange = result.second.second;

        if (symbol.toUpper() == requestedSymbol && exchange.toUpper() == requestedExchange) {
            found = true;
            matchedSymbol = symbol;
            matchedExchange = exchange;
            break;
        }
    }

    if (!found) {
        LOG_DEBUG(QString("Remote Control: POST /ticker - symbol=%1, exchange=%2; 404: No ticker found")
                  .arg(requestedSymbol).arg(requestedExchange));
        sendHttpResponse(socket, 404, "Not Found", QJsonObject(), "No ticker found");
        socket->disconnectFromHost();
        return;
    }

    // Add ticker and make it active
    emit tickerAddRequested(matchedSymbol, matchedExchange);

    LOG_DEBUG(QString("Remote Control: POST /ticker - symbol=%1, exchange=%2; 201: Ticker added")
              .arg(matchedSymbol).arg(matchedExchange));
    sendHttpResponse(socket, 201, "Created");
    socket->disconnectFromHost();
}
