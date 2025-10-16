#ifndef IBKRCLIENT_H
#define IBKRCLIENT_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <memory>
#include "EClientSocket.h"
#include "EReader.h"
#include "EReaderOSSignal.h"
#include "client/ibkrwrapper.h"

class IBKRClient : public QObject
{
    Q_OBJECT

public:
    explicit IBKRClient(QObject *parent = nullptr);
    ~IBKRClient();

    bool isConnected() const { return m_isConnected; }
    QString activeAccount() const { return m_activeAccount; }

    void connect(const QString& host, int port, int clientId);
    void disconnect();
    void disconnect(bool stopReconnect);

    // Market Data
    void requestMarketData(int tickerId, const QString& symbol);
    void cancelMarketData(int tickerId);
    void requestTickByTick(int tickerId, const QString& symbol);
    void cancelTickByTick(int tickerId);
    void requestRealTimeBars(int tickerId, const QString& symbol);
    void cancelRealTimeBars(int tickerId);

    // Historical Data
    void requestHistoricalData(int reqId, const QString& symbol, const QString& endDateTime, const QString& duration, const QString& barSize);

    // Orders
    int placeOrder(const QString& symbol, const QString& action, int quantity, double limitPrice,
                   const QString& orderType = "LMT", const QString& tif = "DAY", bool outsideRth = false,
                   const QString& primaryExchange = QString());
    void cancelOrder(int orderId);
    void cancelAllOrders();

    // Account
    void requestAccountUpdates(bool subscribe, const QString& account);
    void requestManagedAccounts();
    void requestOpenOrders();
    void requestCompletedOrders();

    // Contract Search
    void searchSymbol(int reqId, const QString& pattern);

    EClientSocket* socket() { return m_socket.get(); }

signals:
    void connected();
    void disconnected();
    void error(int id, int code, const QString& message);

    void tickPriceUpdated(int tickerId, int field, double price);
    void tickByTickUpdated(int reqId, double price, double bidPrice, double askPrice);
    void marketDataUpdated(int tickerId, double lastPrice, double bidPrice, double askPrice);

    void realTimeBarReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void historicalBarReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void historicalDataFinished(int reqId);

    void orderConfirmed(int orderId, const QString& symbol, const QString& action, int quantity, double price, long long permId);
    void orderStatusUpdated(int orderId, const QString& status, double filled, double remaining, double avgFillPrice);
    void orderFilled(int orderId, const QString& symbol, const QString& side, double fillPrice, int fillQuantity);

    void accountUpdated(const QString& key, const QString& value, const QString& currency, const QString& account);
    void positionUpdated(const QString& account, const QString& symbol, double position, double avgCost, double marketPrice, double unrealizedPNL);

    void accountsReceived(const QString& accounts);
    void symbolFound(int reqId, const QString& symbol, const QString& exchange, int conId);
    void symbolSearchResultsReceived(int reqId, const QList<QPair<QString, QPair<QString, QString>>>& results);
    void activeAccountChanged(const QString& account);

private slots:
    void processMessages();
    void attemptReconnect();

private:
    void setupSignals();

    std::unique_ptr<IBKRWrapper> m_wrapper;
    std::unique_ptr<EClientSocket> m_socket;
    std::unique_ptr<EReader> m_reader;
    std::unique_ptr<EReaderOSSignal> m_signal;
    QTimer *m_messageTimer;
    QTimer *m_reconnectTimer;

    bool m_isConnected;
    QString m_host;
    int m_port;
    int m_clientId;
    int m_nextOrderId;
    int m_reconnectAttempts;
    QString m_activeAccount;
    bool m_disconnectLogged;
};

#endif // IBKRCLIENT_H
