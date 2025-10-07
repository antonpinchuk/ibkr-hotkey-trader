#include "trading/tradingmanager.h"
#include "client/ibkrclient.h"
#include "models/settings.h"
#include "utils/logger.h"
#include <QDebug>
#include <QTimeZone>

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
    connect(m_client, &IBKRClient::marketDataUpdated, this, &TradingManager::onTickByTickUpdated);
    connect(m_client, &IBKRClient::orderConfirmed, this, &TradingManager::onOrderConfirmed);
    connect(m_client, &IBKRClient::orderStatusUpdated, this, &TradingManager::onOrderStatusUpdated);
    connect(m_client, &IBKRClient::error, this, &TradingManager::onError);
    connect(m_client, &IBKRClient::positionUpdated, [this](const QString& account, const QString& symbol, double position, double avgCost, double marketPrice, double unrealizedPNL) {
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

    // Start sell order monitor for new symbol
    if (!symbol.isEmpty()) {
        m_sellOrderMonitor->start();
    }
}

void TradingManager::setSymbolExchange(const QString& symbol, const QString& exchange)
{
    if (m_currentSymbol == symbol) {
        m_currentExchange = exchange;
        LOG_INFO(QString("Set exchange for %1: %2").arg(symbol).arg(exchange));
    }
}

void TradingManager::openPosition(int percentage)
{
    LOG_DEBUG(QString("openPosition called with percentage: %1").arg(percentage));
    LOG_DEBUG(QString("Current symbol: %1, askPrice: %2, currentPrice: %3, budget: %4")
        .arg(m_currentSymbol).arg(m_askPrice).arg(m_currentPrice).arg(getBudget()));

    if (m_currentSymbol.isEmpty()) {
        LOG_WARNING("No symbol selected");
        emit warning("No symbol selected");
        return;
    }

    // Check if already has open position
    if (getCurrentPosition() > 0) {
        LOG_WARNING(QString("Position already exists: %1 shares").arg(getCurrentPosition()));
        emit warning("Cannot open new position. Position already exists. Use Add buttons to increase position.");
        return;
    }

    int shares = calculateSharesFromPercentage(percentage);
    LOG_DEBUG(QString("Calculated shares: %1").arg(shares));

    if (shares <= 0) {
        if (m_askPrice <= 0 && m_currentPrice <= 0) {
            LOG_WARNING("Market data not available");
            emit warning("Market data not available yet. Wait for price updates.");
        } else {
            LOG_WARNING("Calculated share quantity is 0");
            emit warning("Calculated share quantity is 0. Check your budget settings.");
        }
        return;
    }

    // Check if total would exceed 100%
    double pendingBuy = getPendingBuyQuantity();
    if (pendingBuy > 0) {
        // Update existing pending order
        updatePendingOrder(m_pendingBuyOrderId, "BUY", shares, m_askPrice + (getAskOffset() / 100.0));
    } else {
        // Place new order
        placeOrder("BUY", shares, m_askPrice + (getAskOffset() / 100.0));
    }
}

void TradingManager::addToPosition(int percentage)
{
    LOG_DEBUG(QString("addToPosition called with percentage: %1").arg(percentage));
    LOG_DEBUG(QString("Current symbol: %1, askPrice: %2, currentPrice: %3, budget: %4, position: %5")
        .arg(m_currentSymbol).arg(m_askPrice).arg(m_currentPrice).arg(getBudget()).arg(getCurrentPosition()));

    if (m_currentSymbol.isEmpty()) {
        LOG_WARNING("No symbol selected");
        emit warning("No symbol selected");
        return;
    }

    // Check if has open position
    if (getCurrentPosition() <= 0) {
        LOG_WARNING("No open position");
        emit warning("No open position. Use Open buttons to create a position first.");
        return;
    }

    int additionalShares = calculateSharesFromPercentage(percentage);
    LOG_DEBUG(QString("Calculated additional shares: %1").arg(additionalShares));

    if (additionalShares <= 0) {
        if (m_askPrice <= 0 && m_currentPrice <= 0) {
            LOG_WARNING("Market data not available");
            emit warning("Market data not available yet. Wait for price updates.");
        } else {
            LOG_WARNING("Calculated share quantity is 0");
            emit warning("Calculated share quantity is 0. Check your budget settings.");
        }
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
        TradeOrder& order = m_orders[m_pendingBuyOrderId];
        int newQuantity = order.quantity + additionalShares;
        updatePendingOrder(m_pendingBuyOrderId, "BUY", newQuantity, m_askPrice + (getAskOffset() / 100.0));
    } else {
        // Place new order
        placeOrder("BUY", additionalShares, m_askPrice + (getAskOffset() / 100.0));
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
    placeOrder("SELL", sharesToSell, m_bidPrice - (m_currentSellBidOffset / 100.0));
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
    double pos = m_positions.value(m_currentSymbol, 0.0);
    // LOG_DEBUG(QString("getCurrentPosition for %1: %2 (positions map size: %3)")
        .arg(m_currentSymbol).arg(pos).arg(m_positions.size()));
    return pos;
}

double TradingManager::getPendingBuyQuantity() const
{
    if (m_pendingBuyOrderId >= 0 && m_orders.contains(m_pendingBuyOrderId)) {
        const TradeOrder& order = m_orders[m_pendingBuyOrderId];
        if (order.isPending()) {
            return order.quantity;
        }
    }
    return 0.0;
}

double TradingManager::getPendingSellQuantity() const
{
    if (m_pendingSellOrderId >= 0 && m_orders.contains(m_pendingSellOrderId)) {
        const TradeOrder& order = m_orders[m_pendingSellOrderId];
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

    LOG_DEBUG(QString("TradingManager: Price update for reqId=%1, symbol=%2: price=%3, bid=%4, ask=%5")
        .arg(reqId).arg(m_currentSymbol).arg(price).arg(bidPrice).arg(askPrice));

    // Check if pending buy order needs updating
    if (m_pendingBuyOrderId >= 0 && m_orders.contains(m_pendingBuyOrderId)) {
        const TradeOrder& order = m_orders[m_pendingBuyOrderId];
        double targetPrice = m_askPrice + (getAskOffset() / 100.0);

        // If current ask moved beyond our order price, update it
        if (m_askPrice > order.price) {
            updatePendingOrder(m_pendingBuyOrderId, "BUY", order.quantity, targetPrice);
        }
    }
}

void TradingManager::onOrderConfirmed(int orderId, const QString& symbol, const QString& action, int quantity, double price)
{
    LOG_INFO(QString("Order confirmed by TWS: orderId=%1, symbol=%2, action=%3, qty=%4, price=%5")
        .arg(orderId).arg(symbol).arg(action).arg(quantity).arg(price, 0, 'f', 2));

    // Check if we have this order in pending orders
    if (m_orders.contains(orderId)) {
        TradeOrder& order = m_orders[orderId];
        // Now emit to UI - order is confirmed by TWS
        emit orderPlaced(order);
    } else {
        LOG_WARNING(QString("Received confirmation for unknown order: %1").arg(orderId));
    }
}

void TradingManager::onError(int id, int code, const QString& message)
{
    // Handle order-related errors (id corresponds to orderId)
    if (id > 0 && m_orders.contains(id)) {
        LOG_ERROR(QString("Order %1 failed with error %2: %3").arg(id).arg(code).arg(message));

        // Remove failed order from internal tracking
        m_orders.remove(id);

        // Clear pending order IDs
        if (id == m_pendingBuyOrderId) {
            m_pendingBuyOrderId = -1;
        }
        if (id == m_pendingSellOrderId) {
            m_pendingSellOrderId = -1;
            m_currentSellBidOffset = getBidOffset();  // Reset
        }

        // Emit error to show in UI
        emit error(QString("Order failed: %1").arg(message));
    }
}

void TradingManager::onOrderStatusUpdated(int orderId, const QString& status, double filled, double remaining, double avgFillPrice)
{
    if (m_orders.contains(orderId)) {
        TradeOrder& order = m_orders[orderId];

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

    const TradeOrder& order = m_orders[m_pendingSellOrderId];
    if (!order.isPending()) {
        return;
    }

    // Check if bid price moved beyond our sell order price
    double targetPrice = m_bidPrice - (m_currentSellBidOffset / 100.0);

    if (m_bidPrice < order.price) {
        // Bid moved down, update order with doubled offset
        m_currentSellBidOffset *= 2;
        updatePendingOrder(m_pendingSellOrderId, "SELL", order.quantity, m_bidPrice - (m_currentSellBidOffset / 100.0));
    }
}

int TradingManager::calculateSharesFromPercentage(int percentage)
{
    double budget = getBudget();
    double amount = budget * percentage / 100.0;

    // Use ask price for calculations (what we'll actually pay)
    // Fall back to current price if ask is not available
    double priceForCalc = (m_askPrice > 0) ? m_askPrice : m_currentPrice;

    if (priceForCalc <= 0) {
        return 0; // No price data available yet
    }

    int shares = static_cast<int>(amount / priceForCalc);
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

bool TradingManager::isRegularTradingHours() const
{
    // Get current time in US/Eastern timezone (NASDAQ)
    QTimeZone estTz("America/New_York");
    QDateTime nowEst = QDateTime::currentDateTime().toTimeZone(estTz);
    QTime currentTime = nowEst.time();

    // Regular trading hours: 9:30 - 16:00 EST
    QTime marketOpen(9, 30, 0);
    QTime marketClose(16, 0, 0);

    // Check if weekday (Monday-Friday)
    int dayOfWeek = nowEst.date().dayOfWeek();
    bool isWeekday = (dayOfWeek >= 1 && dayOfWeek <= 5);

    return isWeekday && currentTime >= marketOpen && currentTime < marketClose;
}

int TradingManager::placeOrder(const QString& action, int quantity, double price)
{
    bool isRegularHours = isRegularTradingHours();
    QString tif = isRegularHours ? "DAY" : "GTC";
    bool outsideRth = !isRegularHours;

    LOG_INFO(QString("Placing %1 order: symbol=%2, qty=%3, price=%4, tif=%5, exchange=%6")
        .arg(action).arg(m_currentSymbol).arg(quantity).arg(price, 0, 'f', 2).arg(tif).arg(m_currentExchange));

    int orderId = m_client->placeOrder(m_currentSymbol, action, quantity, price, "LMT", tif, outsideRth, m_currentExchange);

    // Store pending order info - will be confirmed via onOrderConfirmed callback
    TradeOrder order;
    order.orderId = orderId;
    order.symbol = m_currentSymbol;
    order.action = (action == "BUY") ? OrderAction::Buy : OrderAction::Sell;
    order.quantity = quantity;
    order.price = price;
    order.status = OrderStatus::Pending;
    order.fillPrice = 0.0;
    order.timestamp = QDateTime::currentDateTime();

    m_orders[orderId] = order;

    if (action == "BUY") {
        m_pendingBuyOrderId = orderId;
    } else {
        m_pendingSellOrderId = orderId;
    }

    // Do NOT emit orderPlaced here - wait for TWS confirmation via onOrderConfirmed
    return orderId;
}

void TradingManager::updatePendingOrder(int& pendingOrderId, const QString& action, int quantity, double price)
{
    if (pendingOrderId < 0) {
        return;
    }

    // Cancel old order
    m_client->cancelOrder(pendingOrderId);
    m_orders.remove(pendingOrderId);

    // Place new order (this will update pendingOrderId via placeOrder)
    pendingOrderId = placeOrder(action, quantity, price);
}