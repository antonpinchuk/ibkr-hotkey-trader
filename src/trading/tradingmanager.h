#ifndef TRADINGMANAGER_H
#define TRADINGMANAGER_H

#include <QObject>
#include <QMap>
#include "models/order.h"

class IBKRClient;

class TradingManager : public QObject
{
    Q_OBJECT

public:
    explicit TradingManager(IBKRClient *client, QObject *parent = nullptr);

    void setSymbol(const QString& symbol);
    void setSymbolExchange(const QString& symbol, const QString& exchange);
    void resetTickLogging(int reqId); // Reset tick logging for new subscription
    QString currentSymbol() const { return m_currentSymbol; }
    QString currentExchange() const { return m_currentExchange; }

    void openPosition(int percentage);
    void addToPosition(int percentage);
    void closePosition(int percentage);
    void cancelAllOrders();

    double getCurrentPosition() const;
    double getPendingBuyQuantity() const;

signals:
    void orderPlaced(const TradeOrder& order);
    void orderUpdated(const TradeOrder& order);
    void orderCancelled(int orderId);
    void positionUpdated(const QString& symbol, double quantity, double avgCost);
    void warning(const QString& message);
    void error(const QString& message);

private slots:
    void onTickByTickUpdated(int reqId, double price, double bidPrice, double askPrice);
    void onOrderConfirmed(int orderId, const QString& symbol, const QString& action, int quantity, double price, long long permId);
    void onOrderStatusUpdated(int orderId, const QString& status, double filled, double remaining, double avgFillPrice);
    void onError(int id, int code, const QString& message);

private:
    int calculateSharesFromPercentage(int percentage);
    double getBudget() const;
    int getAskOffset() const;
    int getBidOffset() const;
    bool isRegularTradingHours() const;

    int placeOrder(const QString& action, int quantity, double price);
    void updatePendingOrder(int& pendingOrderId, const QString& action, int quantity, double price);

    IBKRClient *m_client;
    QString m_currentSymbol;
    QString m_currentExchange;

    // Current market data
    double m_currentPrice;
    double m_bidPrice;
    double m_askPrice;

    // Position tracking
    QMap<QString, double> m_positions;  // symbol -> quantity

    // Order tracking
    QMap<int, TradeOrder> m_orders;  // orderId -> TradeOrder
    int m_pendingBuyOrderId;
    int m_pendingSellOrderId;

    // Logging tracking
    QMap<int, bool> m_tickByTickLogged; // Track which reqIds have logged at least one tick
};

#endif // TRADINGMANAGER_H