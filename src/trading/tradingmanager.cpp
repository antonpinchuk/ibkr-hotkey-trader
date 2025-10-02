#include "trading/tradingmanager.h"
#include "client/ibkrclient.h"
#include "models/settings.h"
#include <QDebug>

TradingManager::TradingManager(IBKRClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_currentPrice(0.0)
    , m_bidPrice(0.0)
    , m_askPrice(0.0)
    , m_pendingBuyOrderId(-1)
    , m_pendingSellOrderId(-1)
    , m_currentSellBidOffset(0)
{
    m_sellOrderMonitor = new QTimer(this);
    m_sellOrderMonitor->setInterval(100);  // Check every 100ms
    connect(m_sellOrderMonitor, &QTimer::timeout, this, &TradingManager::checkAndUpdateSellOrders);

    // Connect to IBKR client signals
    connect(m_client, &IBKRClient::tickByTickUpdated, this, &TradingManager::onTickByTickUpdated);
    connect(m_client, &IBKRClient::orderStatusUpdated, this, &TradingManager::onOrderStatusUpdated);
    connect(m_client, &IBKRClient::positionUpdated, [this](const QString& account, const QString& symbol, double position, double avgCost) {
        // Only track positions for active account
        if (account == m_client->activeAccount()) {
            m_positions[symbol] = position;
            m_avgCosts[symbol] = avgCost;
            emit positionUpdated(symbol, position, avgCost);
        }
    });
}

void TradingManager::setSymbol(const QString& symbol)
{
    if (m_currentSymbol == symbol) {
        return;
    }

    // Check if there are open positions for current symbol
    if (!m_currentSymbol.isEmpty() && getCurrentPosition() > 0) {
        emit warning("Cannot switch symbols while holding a position. Close all positions first.");
        return;
    }

    m_currentSymbol = symbol;

    // Cancel previous symbol's data stream and orders
    if (!symbol.isEmpty()) {
        m_client->requestTickByTick(1, symbol);
        m_sellOrderMonitor->start();
    }
}

void TradingManager::openPosition(int percentage)
{
    if (m_currentSymbol.isEmpty()) {
        emit warning("No symbol selected");
        return;
    }

    // Check if already has open position
    if (getCurrentPosition() > 0) {
        emit warning("Cannot open new position. Position already exists. Use Add buttons to increase position.");
        return;
    }

    int shares = calculateSharesFromPercentage(percentage);
    if (shares <= 0) {
        emit warning("Calculated share quantity is 0. Check your budget settings.");
        return;
    }

    // Check if total would exceed 100%
    double pendingBuy = getPendingBuyQuantity();
    if (pendingBuy > 0) {
        // Update existing pending order
        updatePendingBuyOrder(shares, m_askPrice + (getAskOffset() / 100.0));
    } else {
        // Place new order
        placeBuyOrder(shares, m_askPrice + (getAskOffset() / 100.0));
    }
}

void TradingManager::addToPosition(int percentage)
{
    if (m_currentSymbol.isEmpty()) {
        emit warning("No symbol selected");
        return;
    }

    // Check if has open position
    if (getCurrentPosition() <= 0) {
        emit warning("No open position. Use Open buttons to create a position first.");
        return;
    }

    int additionalShares = calculateSharesFromPercentage(percentage);
    if (additionalShares <= 0) {
        emit warning("Calculated share quantity is 0. Check your budget settings.");
        return;
    }

    // Check if total would exceed 100% of budget
    double currentPosition = getCurrentPosition();
    double pendingBuy = getPendingBuyQuantity();
    double budget = getBudget();
    double currentValue = currentPosition * m_currentPrice;
    double pendingValue = pendingBuy * m_currentPrice;
    double additionalValue = additionalShares * (m_askPrice + (getAskOffset() / 100.0));

    if (currentValue + pendingValue + additionalValue > budget) {
        emit warning("Cannot exceed 100% of budget");
        return;
    }

    // Update or place buy order
    if (m_pendingBuyOrderId >= 0) {
        // Update existing pending order - add to quantity
        Order& order = m_orders[m_pendingBuyOrderId];
        int newQuantity = order.quantity + additionalShares;
        updatePendingBuyOrder(newQuantity, m_askPrice + (getAskOffset() / 100.0));
    } else {
        // Place new order
        placeBuyOrder(additionalShares, m_askPrice + (getAskOffset() / 100.0));
    }
}

void TradingManager::closePosition(int percentage)
{
    if (m_currentSymbol.isEmpty()) {
        emit warning("No symbol selected");
        return;
    }

    double currentPosition = getCurrentPosition();
    if (currentPosition <= 0) {
        emit warning("No position to close");
        return;
    }

    // Calculate shares to sell (percentage of current position, not budget)
    int sharesToSell = static_cast<int>(currentPosition * percentage / 100.0);
    if (sharesToSell <= 0) {
        return;
    }

    // Cancel and recreate sell order with new quantity and reset Y
    if (m_pendingSellOrderId >= 0) {
        m_client->cancelOrder(m_pendingSellOrderId);
        m_pendingSellOrderId = -1;
    }

    m_currentSellBidOffset = getBidOffset();  // Reset Y to default
    placeSellOrder(sharesToSell, m_bidPrice - (m_currentSellBidOffset / 100.0));
}

void TradingManager::cancelAllOrders()
{
    if (m_pendingBuyOrderId >= 0) {
        m_client->cancelOrder(m_pendingBuyOrderId);
        m_pendingBuyOrderId = -1;
    }

    if (m_pendingSellOrderId >= 0) {
        m_client->cancelOrder(m_pendingSellOrderId);
        m_pendingSellOrderId = -1;
    }

    // Cancel all other orders for this symbol
    for (auto it = m_orders.begin(); it != m_orders.end(); ++it) {
        if (it->symbol == m_currentSymbol && it->isPending()) {
            m_client->cancelOrder(it.key());
        }
    }
}

void TradingManager::closeAllPositions()
{
    for (auto it = m_positions.begin(); it != m_positions.end(); ++it) {
        if (it.value() > 0) {
            // TODO: Place sell order for entire position
        }
    }
}

double TradingManager::getCurrentPosition() const
{
    return m_positions.value(m_currentSymbol, 0.0);
}

double TradingManager::getPendingBuyQuantity() const
{
    if (m_pendingBuyOrderId >= 0 && m_orders.contains(m_pendingBuyOrderId)) {
        const Order& order = m_orders[m_pendingBuyOrderId];
        if (order.isPending()) {
            return order.quantity;
        }
    }
    return 0.0;
}

double TradingManager::getPendingSellQuantity() const
{
    if (m_pendingSellOrderId >= 0 && m_orders.contains(m_pendingSellOrderId)) {
        const Order& order = m_orders[m_pendingSellOrderId];
        if (order.isPending()) {
            return order.quantity;
        }
    }
    return 0.0;
}

void TradingManager::onTickByTickUpdated(int reqId, double price, double bidPrice, double askPrice)
{
    m_currentPrice = price;
    m_bidPrice = bidPrice;
    m_askPrice = askPrice;

    // Check if pending buy order needs updating
    if (m_pendingBuyOrderId >= 0 && m_orders.contains(m_pendingBuyOrderId)) {
        const Order& order = m_orders[m_pendingBuyOrderId];
        double targetPrice = m_askPrice + (getAskOffset() / 100.0);

        // If current ask moved beyond our order price, update it
        if (m_askPrice > order.price) {
            updatePendingBuyOrder(order.quantity, targetPrice);
        }
    }
}

void TradingManager::onOrderStatusUpdated(int orderId, const QString& status, double filled, double remaining, double avgFillPrice)
{
    if (m_orders.contains(orderId)) {
        Order& order = m_orders[orderId];

        // Convert string status to enum
        if (status == "Filled") {
            order.status = OrderStatus::Filled;
        } else if (status == "Cancelled") {
            order.status = OrderStatus::Cancelled;
        } else {
            order.status = OrderStatus::Pending;
        }

        order.fillPrice = avgFillPrice;

        if (order.isFilled()) {
            // Update position
            if (order.isBuy()) {
                m_positions[order.symbol] = getCurrentPosition() + filled;
                if (orderId == m_pendingBuyOrderId) {
                    m_pendingBuyOrderId = -1;
                }
            } else if (order.isSell()) {
                m_positions[order.symbol] = getCurrentPosition() - filled;
                if (orderId == m_pendingSellOrderId) {
                    m_pendingSellOrderId = -1;
                    m_currentSellBidOffset = getBidOffset();  // Reset for next sell
                }
            }
        } else if (order.isCancelled()) {
            if (orderId == m_pendingBuyOrderId) {
                m_pendingBuyOrderId = -1;
            }
            if (orderId == m_pendingSellOrderId) {
                m_pendingSellOrderId = -1;
                m_currentSellBidOffset = getBidOffset();  // Reset
            }
            emit orderCancelled(orderId);
        }

        emit orderUpdated(order);
    }
}

void TradingManager::checkAndUpdateSellOrders()
{
    if (m_pendingSellOrderId < 0 || !m_orders.contains(m_pendingSellOrderId)) {
        return;
    }

    const Order& order = m_orders[m_pendingSellOrderId];
    if (!order.isPending()) {
        return;
    }

    // Check if bid price moved beyond our sell order price
    double targetPrice = m_bidPrice - (m_currentSellBidOffset / 100.0);

    if (m_bidPrice < order.price) {
        // Bid moved down, update order with doubled offset
        m_currentSellBidOffset *= 2;
        updatePendingSellOrder(order.quantity, m_bidPrice - (m_currentSellBidOffset / 100.0));
    }
}

int TradingManager::calculateSharesFromPercentage(int percentage)
{
    double budget = getBudget();
    double amount = budget * percentage / 100.0;
    int shares = static_cast<int>(amount / m_currentPrice);
    return shares;
}

double TradingManager::getBudget() const
{
    return Settings::instance().budget();
}

int TradingManager::getAskOffset() const
{
    return Settings::instance().askOffset();
}

int TradingManager::getBidOffset() const
{
    return Settings::instance().bidOffset();
}

void TradingManager::placeBuyOrder(int quantity, double price)
{
    int orderId = m_client->placeOrder(m_currentSymbol, "BUY", quantity, price);

    Order order;
    order.orderId = orderId;
    order.symbol = m_currentSymbol;
    order.action = OrderAction::Buy;
    order.quantity = quantity;
    order.price = price;
    order.status = OrderStatus::Pending;
    order.fillPrice = 0.0;
    order.timestamp = QDateTime::currentDateTime();

    m_orders[orderId] = order;
    m_pendingBuyOrderId = orderId;

    emit orderPlaced(order);
}

void TradingManager::placeSellOrder(int quantity, double price)
{
    int orderId = m_client->placeOrder(m_currentSymbol, "SELL", quantity, price);

    Order order;
    order.orderId = orderId;
    order.symbol = m_currentSymbol;
    order.action = OrderAction::Sell;
    order.quantity = quantity;
    order.price = price;
    order.status = OrderStatus::Pending;
    order.fillPrice = 0.0;
    order.timestamp = QDateTime::currentDateTime();

    m_orders[orderId] = order;
    m_pendingSellOrderId = orderId;

    emit orderPlaced(order);
}

void TradingManager::updatePendingBuyOrder(int quantity, double price)
{
    if (m_pendingBuyOrderId < 0) {
        return;
    }

    // Cancel old order
    m_client->cancelOrder(m_pendingBuyOrderId);

    // Place new order
    int orderId = m_client->placeOrder(m_currentSymbol, "BUY", quantity, price);

    Order order;
    order.orderId = orderId;
    order.symbol = m_currentSymbol;
    order.action = OrderAction::Buy;
    order.quantity = quantity;
    order.price = price;
    order.status = OrderStatus::Pending;
    order.fillPrice = 0.0;
    order.timestamp = QDateTime::currentDateTime();

    m_orders[orderId] = order;
    m_pendingBuyOrderId = orderId;

    emit orderPlaced(order);
}

void TradingManager::updatePendingSellOrder(int quantity, double price)
{
    if (m_pendingSellOrderId < 0) {
        return;
    }

    // Cancel old order
    m_client->cancelOrder(m_pendingSellOrderId);

    // Place new order
    int orderId = m_client->placeOrder(m_currentSymbol, "SELL", quantity, price);

    Order order;
    order.orderId = orderId;
    order.symbol = m_currentSymbol;
    order.action = OrderAction::Sell;
    order.quantity = quantity;
    order.price = price;
    order.status = OrderStatus::Pending;
    order.fillPrice = 0.0;
    order.timestamp = QDateTime::currentDateTime();

    m_orders[orderId] = order;
    m_pendingSellOrderId = orderId;

    emit orderPlaced(order);
}