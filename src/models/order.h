#ifndef ORDER_H
#define ORDER_H

#include <QString>
#include <QDateTime>

enum class OrderStatus {
    Pending,
    Filled,
    Cancelled
};

enum class OrderAction {
    Buy,
    Sell
};

struct TradeOrder {
    int orderId;
    QString symbol;
    OrderAction action;
    int quantity;
    double price;
    OrderStatus status;
    QDateTime timestamp;
    double fillPrice;
    QDateTime fillTime;
    double commission;
    long long permId; // Permanent order ID from TWS (for sorting)
    qint64 sortOrder; // Sorting field: timestamp for new orders, counter for historical orders

    TradeOrder()
        : orderId(0)
        , action(OrderAction::Buy)
        , quantity(0)
        , price(0.0)
        , status(OrderStatus::Pending)
        , timestamp(QDateTime::currentDateTime())
        , fillPrice(0.0)
        , fillTime()
        , commission(0.0)
        , permId(0)
        , sortOrder(0)
    {}

    bool isBuy() const { return action == OrderAction::Buy; }
    bool isSell() const { return action == OrderAction::Sell; }
    bool isPending() const { return status == OrderStatus::Pending; }
    bool isFilled() const { return status == OrderStatus::Filled; }
    bool isCancelled() const { return status == OrderStatus::Cancelled; }

    double total() const {
        return (isFilled() ? fillPrice : price) * quantity;
    }
};

#endif // ORDER_H
