#include "server/remotecontrolserver.h"
#include "client/ibkrclient.h"
#include "models/tickerdatamanager.h"
#include "models/symbolsearchmanager.h"
#include "widgets/tickerlistwidget.h"
#include "utils/logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>

RemoteControlServer::RemoteControlServer(IBKRClient* client, TickerDataManager* tickerDataManager,
                                         TickerListWidget* tickerList, SymbolSearchManager* searchManager,
                                         QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_client(client)
    , m_tickerDataManager(tickerDataManager)
    , m_tickerList(tickerList)
    , m_searchManager(searchManager)
    , m_nextCallbackId(1)
{
    connect(m_server, &QTcpServer::newConnection, this, &RemoteControlServer::onNewConnection);
    connect(m_searchManager, &SymbolSearchManager::symbolFound, this, &RemoteControlServer::onSymbolFound);
    connect(m_searchManager, &SymbolSearchManager::symbolNotFound, this, &RemoteControlServer::onSymbolNotFound);
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
        if (request.method == "GET") {
            handleGetTicker(socket);
        } else if (request.method == "POST") {
            handlePostTicker(socket, request.body);
        } else if (request.method == "PUT") {
            handlePutTicker(socket, request.body);
        } else if (request.method == "DELETE") {
            handleDeleteTicker(socket, request.body);
        } else {
            sendHttpResponse(socket, 405, "Method Not Allowed");
            socket->disconnectFromHost();
        }
    } else if (request.path.startsWith("/ticker/")) {
        // Extract exchange and symbol from path: /ticker/{exchange}/{symbol}
        QString pathPart = request.path.mid(8); // Skip "/ticker/"
        QStringList parts = pathPart.split('/');

        if (request.method == "GET") {
            QString exchange, symbol;

            if (parts.size() == 2) {
                // Format: /ticker/{exchange}/{symbol}
                exchange = parts[0].toUpper();
                symbol = parts[1].toUpper();
            } else if (parts.size() == 1) {
                // Backward compatibility: /ticker/{symbol} or /ticker/{symbol@exchange}
                QString tickerKey = parts[0];
                if (tickerKey.contains('@')) {
                    QStringList keyParts = tickerKey.split('@');
                    symbol = keyParts[0].toUpper();
                    exchange = keyParts[1].toUpper();
                } else {
                    symbol = tickerKey.toUpper();
                    exchange = ""; // Will find first ticker with this symbol
                }
            } else {
                sendHttpResponse(socket, 400, "Bad Request", QJsonObject(), "Invalid path format");
                socket->disconnectFromHost();
                return;
            }

            handleGetTickerByExchangeAndSymbol(socket, exchange, symbol);
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

    // Parse JSON body if exists (for POST, PUT, DELETE methods)
    if (emptyLineIndex >= 0 && emptyLineIndex + 1 < lines.size()) {
        QString bodyStr = lines.mid(emptyLineIndex + 1).join("\r\n");

        if (!bodyStr.trimmed().isEmpty()) {
            QJsonDocument jsonDoc = QJsonDocument::fromJson(bodyStr.toUtf8());

            if (jsonDoc.isNull() || !jsonDoc.isObject()) {
                request.errorMessage = "Invalid JSON";
                return request;
            }

            request.body = jsonDoc.object();

            // Validate required fields only for POST, PUT, DELETE methods
            if (request.method != "GET") {
                if (!request.body.contains("symbol") || !request.body.contains("exchange")) {
                    request.errorMessage = "Missing required fields: symbol, exchange";
                    return request;
                }
            }
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
    // POST /ticker - Add new ticker to the list
    // This initiates symbol search in TWS and adds ticker if found
    QString symbol = body["symbol"].toString().toUpper();
    QString exchange = body["exchange"].toString().toUpper();

    // Check if this exact ticker (symbol@exchange) already exists
    if (m_tickerList->hasTickerKey(symbol, exchange)) {
        LOG_DEBUG(QString("Remote Control: POST /ticker - symbol=%1, exchange=%2; 409: Ticker already added")
                  .arg(symbol).arg(exchange));
        sendHttpResponse(socket, 409, "Conflict", QJsonObject(), "Ticker already added");
        socket->disconnectFromHost();
        return;
    }

    // Start async symbol search via SymbolSearchManager
    int callbackId = m_nextCallbackId++;
    m_callbackIdToSocket[callbackId] = socket;

    m_searchManager->searchSymbolWithExchange(symbol, exchange, callbackId);

    // Response will be sent in onSymbolFound/onSymbolNotFound
}

void RemoteControlServer::handlePutTicker(QTcpSocket* socket, const QJsonObject& body)
{
    // PUT /ticker - Activate existing ticker (make it current/active)
    // Returns 404 if ticker doesn't exist -> client should then call POST to add it
    QString symbol = body["symbol"].toString().toUpper();
    QString exchange = body["exchange"].toString().toUpper();

    // Check if this exact ticker (symbol@exchange) exists
    if (m_tickerList->hasTickerKey(symbol, exchange)) {
        // Ticker exists - activate it
        emit tickerSelectRequested(symbol, exchange);
        LOG_DEBUG(QString("Remote Control: PUT /ticker - symbol=%1, exchange=%2; 200: OK").arg(symbol).arg(exchange));
        sendHttpResponse(socket, 200, "OK");
        socket->disconnectFromHost();
        return;
    }

    // Ticker not found - return 404 to trigger POST from client
    LOG_DEBUG(QString("Remote Control: PUT /ticker - symbol=%1, exchange=%2; 404: No ticker found")
              .arg(symbol).arg(exchange));
    sendHttpResponse(socket, 404, "Not Found", QJsonObject(), "No ticker found");
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

void RemoteControlServer::onSymbolFound(int callbackId, const QString& symbol, const QString& exchange, int conId)
{
    Q_UNUSED(conId);  // Already stored in TickerDataManager by SymbolSearchManager

    // Get socket for this callback
    if (!m_callbackIdToSocket.contains(callbackId)) {
        return;
    }

    QTcpSocket* socket = m_callbackIdToSocket.value(callbackId);
    m_callbackIdToSocket.remove(callbackId);

    // Check if socket is still valid
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // Add ticker and make it active
    emit tickerAddRequested(symbol, exchange, conId);

    LOG_DEBUG(QString("Remote Control: POST /ticker - symbol=%1, exchange=%2, conId=%3; 201: Ticker added")
              .arg(symbol).arg(exchange).arg(conId));
    sendHttpResponse(socket, 201, "Created");
    socket->disconnectFromHost();
}

void RemoteControlServer::onSymbolNotFound(int callbackId, const QString& symbol, const QString& exchange)
{
    // Get socket for this callback
    if (!m_callbackIdToSocket.contains(callbackId)) {
        return;
    }

    QTcpSocket* socket = m_callbackIdToSocket.value(callbackId);
    m_callbackIdToSocket.remove(callbackId);

    // Check if socket is still valid
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    LOG_DEBUG(QString("Remote Control: POST /ticker - symbol=%1, exchange=%2; 404: No ticker found")
              .arg(symbol).arg(exchange));
    sendHttpResponse(socket, 404, "Not Found", QJsonObject(), "No ticker found");
    socket->disconnectFromHost();
}

void RemoteControlServer::handleGetTicker(QTcpSocket* socket)
{
    QList<QPair<QString, QString>> tickers = m_tickerList->getAllTickersWithExchange();

    // Build JSON array of tickers
    QJsonArray tickersArray;
    for (const auto& ticker : tickers) {
        QString symbol = ticker.first;
        QString exchange = ticker.second;

        QJsonObject tickerObj;
        tickerObj["symbol"] = symbol;
        tickerObj["exchange"] = exchange;
        tickersArray.append(tickerObj);
    }

    LOG_DEBUG(QString("Remote Control: GET /ticker - 200: Returned %1 tickers").arg(tickers.size()));

    // Send response with array in body
    QString response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n";

    QByteArray bodyData = QJsonDocument(tickersArray).toJson(QJsonDocument::Compact);
    response += QString("Content-Length: %1\r\n").arg(bodyData.size());
    response += "\r\n";

    socket->write(response.toUtf8());
    socket->write(bodyData);
    socket->flush();
    socket->disconnectFromHost();
}

void RemoteControlServer::handleGetTickerByExchangeAndSymbol(QTcpSocket* socket, const QString& exchange, const QString& symbol)
{
    QString requestedExchange = exchange;

    if (!requestedExchange.isEmpty()) {
        // Exchange specified - check if this exact ticker exists
        if (!m_tickerList->hasTickerKey(symbol, requestedExchange)) {
            LOG_DEBUG(QString("Remote Control: GET /ticker/%1/%2 - 404: Ticker not found").arg(requestedExchange).arg(symbol));
            sendHttpResponse(socket, 404, "Not Found", QJsonObject(), "Ticker not found");
            socket->disconnectFromHost();
            return;
        }
    } else {
        // No exchange specified - find ticker with this symbol
        QList<QPair<QString, QString>> tickers = m_tickerList->getAllTickersWithExchange();
        bool found = false;

        for (const auto& ticker : tickers) {
            if (ticker.first == symbol) {
                requestedExchange = ticker.second;
                found = true;
                break;
            }
        }

        if (!found) {
            LOG_DEBUG(QString("Remote Control: GET /ticker/%1 - 404: Ticker not found").arg(symbol));
            sendHttpResponse(socket, 404, "Not Found", QJsonObject(), "Ticker not found");
            socket->disconnectFromHost();
            return;
        }
    }

    QJsonObject tickerObj;
    tickerObj["symbol"] = symbol;
    tickerObj["exchange"] = requestedExchange;

    LOG_DEBUG(QString("Remote Control: GET /ticker/%1/%2 - 200: OK").arg(requestedExchange).arg(symbol));
    sendHttpResponse(socket, 200, "OK", tickerObj);
    socket->disconnectFromHost();
}
