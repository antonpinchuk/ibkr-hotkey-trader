#ifndef TRADINGMANAGER_H
#define TRADINGMANAGER_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include "order.h"

class IBKRClient;

class TradingManager : public QObject
{
    Q_OBJECT

public:
    explicit TradingManager(IBKRClient *client, QObject *parent = nullptr);

    void setSymbol(const QString& symbol);
    QString currentSymbol() const { return m_currentSymbol; }

    void openPosition(int percentage);
    void addToPosition(int percentage);
    void closePosition(int percentage);
    void cancelAllOrders();
    void closeAllPositions();

    double getCurrentPosition() const;
    double getPendingBuyQuantity() const;
    double getPendingSellQuantity() const;

signals:
    void orderPlaced(const Order& order);
    void orderUpdated(const Order& order);
    void orderCancelled(int orderId);
    void positionUpdated(const QString& symbol, double quantity, double avgCost);
    void warning(const QString& message);
    void error(const QString& message);

private slots:
    void onTickByTickUpdated(int reqId, double price, double bidPrice, double askPrice);
    void onOrderStatusUpdated(int orderId, const QString& status, double filled, double remaining, double avgFillPrice);
    void checkAndUpdateSellOrders();

private:
    int calculateSharesFromPercentage(int percentage);
    double getBudget() const;
    int getAskOffset() const;
    int getBidOffset() const;

    void placeBuyOrder(int quantity, double price);
    void placeSellOrder(int quantity, double price);
    void updatePendingBuyOrder(int quantity, double price);
    void updatePendingSellOrder(int quantity, double price);

    IBKRClient *m_client;
    QString m_currentSymbol;

    // Current market data
    double m_currentPrice;
    double m_bidPrice;
    double m_askPrice;

    // Position tracking
    QMap<QString, double> m_positions;  // symbol -> quantity
    QMap<QString, double> m_avgCosts;   // symbol -> avg cost

    // Order tracking
    QMap<int, Order> m_orders;  // orderId -> Order
    int m_pendingBuyOrderId;
    int m_pendingSellOrderId;
    int m_currentSellBidOffset;  // Track Y doubling for sell orders

    // Timer for sell order price monitoring
    QTimer *m_sellOrderMonitor;
};

#endif // TRADINGMANAGER_H