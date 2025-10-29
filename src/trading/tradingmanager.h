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

    // Set target prices (from OrderPanel or auto-calculated from ticks)
    void setTargetBuyPrice(double price);
    void setTargetSellPrice(double price);

    // Get target prices (for button state checks)
    double targetBuyPrice() const { return m_targetBuyPrice; }
    double targetSellPrice() const { return m_targetSellPrice; }

    double getCurrentPosition() const;
    double getPendingBuyQuantity() const;
    double getPendingSellQuantity() const;

    // Helper methods for button state calculation
    double getPositionPercentageOfBudget() const; // Returns % of budget that current position occupies
    double getPendingBuyPercentageOfBudget() const; // Returns % of budget that pending buy orders occupy
    bool canAddPercentage(int percentage) const; // Check if can add X% without exceeding 100% budget
    bool canClosePercentage(int percentage) const; // Check if floor(position * %) >= 1

    // Trading hours check
    bool isRegularTradingHours() const;

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

    int placeOrder(const QString& action, int quantity, double price);
    void updatePendingOrder(int& pendingOrderId, const QString& action, int quantity, double price);

    IBKRClient *m_client;
    QString m_currentSymbol;
    QString m_currentExchange;

    // Current market data (for chart and other purposes)
    double m_currentPrice;
    double m_bidPrice;
    double m_askPrice;

    // Target prices for orders (auto-calculated or manually set from OrderPanel)
    double m_targetBuyPrice;   // Ask + offset, or manual price
    double m_targetSellPrice;  // Bid - offset, or manual price

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