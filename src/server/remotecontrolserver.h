#ifndef REMOTECONTROLSERVER_H
#define REMOTECONTROLSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>

class IBKRClient;
class TickerDataManager;
class TickerListWidget;

class RemoteControlServer : public QObject
{
    Q_OBJECT

public:
    explicit RemoteControlServer(IBKRClient* client, TickerDataManager* tickerDataManager,
                                 TickerListWidget* tickerList, QObject* parent = nullptr);
    ~RemoteControlServer();

    bool start(quint16 port);
    void stop();
    bool isListening() const;

signals:
    void tickerAddRequested(const QString& symbol, const QString& exchange);
    void tickerSelectRequested(const QString& symbol);
    void tickerDeleteRequested(const QString& symbol);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onSymbolSearchResults(int reqId, const QList<QPair<QString, QPair<QString, QString>>>& results);

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
    void handleGetTickerBySymbol(QTcpSocket* socket, const QString& symbol);

    QTcpServer* m_server;
    IBKRClient* m_client;
    TickerDataManager* m_tickerDataManager;
    TickerListWidget* m_tickerList;

    // For async symbol search
    QMap<int, QTcpSocket*> m_searchReqIdToSocket;
    QMap<int, QPair<QString, QString>> m_searchReqIdToSymbolExchange; // reqId -> (symbol, exchange)
    int m_nextSearchReqId;
};

#endif // REMOTECONTROLSERVER_H
