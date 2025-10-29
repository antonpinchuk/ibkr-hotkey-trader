#ifndef REMOTECONTROLSERVER_H
#define REMOTECONTROLSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>

class IBKRClient;
class TickerDataManager;
class TickerListWidget;
class SymbolSearchManager;

class RemoteControlServer : public QObject
{
    Q_OBJECT

public:
    explicit RemoteControlServer(IBKRClient* client, TickerDataManager* tickerDataManager,
                                 TickerListWidget* tickerList, SymbolSearchManager* searchManager,
                                 QObject* parent = nullptr);
    ~RemoteControlServer();

    bool start(quint16 port);
    void stop();
    bool isListening() const;

signals:
    void tickerAddRequested(const QString& symbol, const QString& exchange, int conId = 0);
    void tickerSelectRequested(const QString& symbol, const QString& exchange);
    void tickerDeleteRequested(const QString& symbol);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onSymbolFound(int callbackId, const QString& symbol, const QString& exchange, int conId);
    void onSymbolNotFound(int callbackId, const QString& symbol, const QString& exchange);

private:
    struct HttpRequest {
        QString method;
        QString path;
        QJsonObject body;
        bool isValid;
        QString errorMessage;
    };

    HttpRequest parseHttpRequest(const QByteArray& data);
    void sendHttpResponse(QTcpSocket* socket, int statusCode, const QString& statusMessage,
                         const QJsonObject& body = QJsonObject(), const QString& errorMessage = QString());

    void handlePostTicker(QTcpSocket* socket, const QJsonObject& body);
    void handlePutTicker(QTcpSocket* socket, const QJsonObject& body);
    void handleDeleteTicker(QTcpSocket* socket, const QJsonObject& body);
    void handleGetTicker(QTcpSocket* socket);
    void handleGetTickerByExchangeAndSymbol(QTcpSocket* socket, const QString& exchange, const QString& symbol);

    QTcpServer* m_server;
    IBKRClient* m_client;
    TickerDataManager* m_tickerDataManager;
    TickerListWidget* m_tickerList;
    SymbolSearchManager* m_searchManager;

    // For async symbol search - callbackId -> socket
    QMap<int, QTcpSocket*> m_callbackIdToSocket;
    int m_nextCallbackId;
};

#endif // REMOTECONTROLSERVER_H
