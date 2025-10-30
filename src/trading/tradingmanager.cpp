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
    , m_targetBuyPrice(0.0)
    , m_targetSellPrice(0.0)
    , m_pendingBuyOrderId(-1)
    , m_pendingSellOrderId(-1)
{
    // Connect to IBKR client signals
    connect(m_client, &IBKRClient::tickByTickUpdated, this, &TradingManager::onTickByTickUpdated);
    connect(m_client, &IBKRClient::orderConfirmed, this, &TradingManager::onOrderConfirmed);
    connect(m_client, &IBKRClient::orderStatusUpdated, this, &TradingManager::onOrderStatusUpdated);
    connect(m_client, &IBKRClient::error, this, &TradingManager::onError);
    connect(m_client, &IBKRClient::positionUpdated, [this](const QString& account, const QString& symbol, double position, double avgCost, double marketPrice, double unrealizedPNL) {
        // Only track positions for active account
        if (account == m_client->activeAccount()) {
            m_positions[symbol] = position;
            emit positionUpdated(symbol, position, avgCost);
        }
    });
}

void TradingManager::setSymbol(const QString& symbol)
{
    if (m_currentSymbol == symbol) {
        return;
    }

    m_currentSymbol = symbol;
}

void TradingManager::setSymbolExchange(const QString& symbol, const QString& exchange)
{
    if (m_currentSymbol == symbol) {
        m_currentExchange = exchange;
    }
}

void TradingManager::resetTickLogging(int reqId)
{
    m_tickByTickLogged.remove(reqId);
}

void TradingManager::openPosition(int percentage)
{
    LOG_DEBUG(QString("openPosition called with percentage: %1").arg(percentage));
    LOG_DEBUG(QString("Current symbol: %1, targetBuyPrice: %2, budget: %3")
        .arg(m_currentSymbol).arg(m_targetBuyPrice).arg(getBudget()));

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
        if (m_targetBuyPrice <= 0 && m_currentPrice <= 0) {
            LOG_WARNING("Market data not available");
            emit warning("Market data not available yet. Wait for price updates.");
        } else {
            LOG_WARNING("Calculated share quantity is 0");
            emit warning("Calculated share quantity is 0. Check your budget settings.");
        }
        return;
    }

    // Get order type from settings
    QString orderType = Settings::instance().orderType();
    double targetPrice = (orderType == "LMT") ? m_targetBuyPrice : 0.0;

    // Update existing pending order or place new one
    if (m_pendingBuyOrderId >= 0) {
        updatePendingOrder(m_pendingBuyOrderId, "BUY", shares, targetPrice);
    } else {
        placeOrder("BUY", shares, targetPrice);
    }
}

void TradingManager::addToPosition(int percentage)
{
    LOG_DEBUG(QString("addToPosition called with percentage: %1").arg(percentage));
    LOG_DEBUG(QString("Current symbol: %1, targetBuyPrice: %2, budget: %3, position: %4")
        .arg(m_currentSymbol).arg(m_targetBuyPrice).arg(getBudget()).arg(getCurrentPosition()));

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
        if (m_targetBuyPrice <= 0 && m_currentPrice <= 0) {
            LOG_WARNING("Market data not available");
            emit warning("Market data not available yet. Wait for price updates.");
        } else {
            LOG_WARNING("Calculated share quantity is 0");
            emit warning("Calculated share quantity is 0. Check your budget settings.");
        }
        return;
    }

    // Get order type from settings
    QString orderType = Settings::instance().orderType();
    double targetPrice = (orderType == "LMT") ? m_targetBuyPrice : 0.0;

    // Check if total would exceed 100% of budget
    double currentPosition = getCurrentPosition();
    double pendingBuy = getPendingBuyQuantity();
    double budget = getBudget();
    double currentValue = currentPosition * m_currentPrice;
    double pendingValue = pendingBuy * m_currentPrice;
    double additionalValue = additionalShares * (targetPrice > 0 ? targetPrice : m_currentPrice);

    if (currentValue + pendingValue + additionalValue > budget) {
        emit warning("Cannot exceed 100% of budget");
        return;
    }

    // Update or place buy order
    if (m_pendingBuyOrderId >= 0) {
        // Update existing pending order - add to quantity
        TradeOrder& order = m_orders[m_pendingBuyOrderId];
        int newQuantity = order.quantity + additionalShares;
        updatePendingOrder(m_pendingBuyOrderId, "BUY", newQuantity, targetPrice);
    } else {
        // Place new order
        placeOrder("BUY", additionalShares, targetPrice);
    }
}

void TradingManager::closePosition(int percentage)
{
    LOG_DEBUG(QString("closePosition called with percentage: %1").arg(percentage));
    LOG_DEBUG(QString("Current symbol: %1, targetSellPrice: %2").arg(m_currentSymbol).arg(m_targetSellPrice));

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
    // Use floor to match REQUIREMENTS: "Ті кнопки неактивні що створять округлену заявку менше однієї акції"
    int sharesToSell = static_cast<int>(currentPosition * percentage / 100.0);
    if (sharesToSell < 1) {
        LOG_WARNING(QString("Cannot close %1%: floor(position * %%) = %2 < 1").arg(percentage).arg(sharesToSell));
        emit warning(QString("Cannot close %1%: would result in less than 1 share").arg(percentage));
        return;
    }

    // Get order type from settings
    QString orderType = Settings::instance().orderType();
    double targetPrice = (orderType == "LMT") ? m_targetSellPrice : 0.0;

    // Update existing pending order or place new one
    if (m_pendingSellOrderId >= 0) {
        updatePendingOrder(m_pendingSellOrderId, "SELL", sharesToSell, targetPrice);
    } else {
        placeOrder("SELL", sharesToSell, targetPrice);
    }
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


double TradingManager::getCurrentPosition() const
{
    return m_positions.value(m_currentSymbol, 0.0);
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

double TradingManager::getPositionPercentageOfBudget() const
{
    double position = getCurrentPosition();
    if (position <= 0 || m_currentPrice <= 0) {
        return 0.0;
    }
    double budget = getBudget();
    if (budget <= 0) {
        return 0.0;
    }
    return (position * m_currentPrice / budget) * 100.0;
}

double TradingManager::getPendingBuyPercentageOfBudget() const
{
    double pendingBuy = getPendingBuyQuantity();
    if (pendingBuy <= 0 || m_currentPrice <= 0) {
        return 0.0;
    }
    double budget = getBudget();
    if (budget <= 0) {
        return 0.0;
    }
    return (pendingBuy * m_currentPrice / budget) * 100.0;
}

bool TradingManager::canAddPercentage(int percentage) const
{
    double positionPct = getPositionPercentageOfBudget();
    double pendingBuyPct = getPendingBuyPercentageOfBudget();
    return (positionPct + pendingBuyPct + percentage) <= 100.0;
}

bool TradingManager::canClosePercentage(int percentage) const
{
    double position = getCurrentPosition();
    int sharesToSell = static_cast<int>(position * percentage / 100.0); // floor
    return sharesToSell >= 1;
}

void TradingManager::onTickByTickUpdated(int reqId, double price, double bidPrice, double askPrice)
{
    m_currentPrice = price;
    m_bidPrice = bidPrice;
    m_askPrice = askPrice;

    // Auto-update target prices with offsets (will be used if not manually set)
    m_targetBuyPrice = m_askPrice + (getAskOffset() / 100.0);
    m_targetSellPrice = m_bidPrice - (getBidOffset() / 100.0);

    // Log only first successful tick for each reqId
    if (!m_tickByTickLogged.value(reqId, false)) {
        LOG_DEBUG(QString("First tick received [reqId=%1, symbol=%2]: bid=%3, ask=%4, price=%5, targetBuy=%6, targetSell=%7")
            .arg(reqId).arg(m_currentSymbol).arg(bidPrice).arg(askPrice).arg(price).arg(m_targetBuyPrice).arg(m_targetSellPrice));
        m_tickByTickLogged[reqId] = true;
    }
}

void TradingManager::setTargetBuyPrice(double price)
{
    m_targetBuyPrice = price;
    // Only log when user sets a limit price (not when resetting to 0)
    if (price > 0) {
        LOG_INFO(QString("Target buy price set to: %1").arg(price));
    }
}

void TradingManager::setTargetSellPrice(double price)
{
    m_targetSellPrice = price;
    // Only log when user sets a limit price (not when resetting to 0)
    if (price > 0) {
        LOG_INFO(QString("Target sell price set to: %1").arg(price));
    }
}

void TradingManager::onOrderConfirmed(int orderId, const QString& symbol, const QString& action, int quantity, double price, long long permId)
{
    // Historical orders (orderId=0) are handled by MainWindow, skip them here
    if (orderId == 0) {
        return;
    }

    LOG_DEBUG(QString("Order confirmed by TWS: orderId=%1, symbol=%2, action=%3, qty=%4, price=%5, permId=%6")
        .arg(orderId).arg(symbol).arg(action).arg(quantity).arg(price, 0, 'f', 2).arg(permId));

    // Check if we have this order in pending orders
    if (m_orders.contains(orderId)) {
        TradeOrder& order = m_orders[orderId];
        bool isUpdate = (order.quantity != quantity || qAbs(order.price - price) > 0.01);

        // Update order fields (important for order updates!)
        order.quantity = quantity;
        order.price = price;
        order.permId = permId; // Store permId for sorting
        order.timestamp = QDateTime::currentDateTime(); // Update timestamp when confirmed by TWS
        order.sortOrder = order.timestamp.toMSecsSinceEpoch(); // Update sortOrder

        // Emit appropriate signal based on whether this is a new order or update
        if (isUpdate) {
            emit orderUpdated(order);
        } else {
            // New order confirmation
            emit orderPlaced(order);
        }
    } else {
        LOG_WARNING(QString("Received confirmation for unknown order: %1").arg(orderId));
    }
}

void TradingManager::onError(int id, int code, const QString& message)
{
    // Handle order-related errors (id corresponds to orderId)
    if (id > 0 && m_orders.contains(id)) {
        // TWS Error codes that are warnings (order is NOT rejected):
        // 2161: Price cap applied (MKT order converted to LMT with price control)
        // Orders with these codes will still be submitted and may fill
        bool isWarning = (code == 2161);

        if (isWarning) {
            LOG_WARNING(QString("Order %1 warning %2: %3").arg(id).arg(code).arg(message));
            // Do NOT remove order - it's still active
            // Show warning to user
            emit warning(QString("Order %1: %2").arg(id).arg(message));
        } else {
            // Real error - order rejected
            LOG_ERROR(QString("Order %1 failed with error %2: %3").arg(id).arg(code).arg(message));

            // Remove failed order from internal tracking
            m_orders.remove(id);

            // Clear pending order IDs
            if (id == m_pendingBuyOrderId) {
                m_pendingBuyOrderId = -1;
            }
            if (id == m_pendingSellOrderId) {
                m_pendingSellOrderId = -1;
            }

            // Emit error to show in UI
            emit error(QString("Order failed: %1").arg(message));
        }
    }
}

void TradingManager::onOrderStatusUpdated(int orderId, const QString& status, double filled, double remaining, double avgFillPrice)
{
    if (m_orders.contains(orderId)) {
        TradeOrder& order = m_orders[orderId];

        // Convert string status to enum
        if (status == "Filled") {
            order.status = OrderStatus::Filled;
            order.fillTime = QDateTime::currentDateTime(); // Set fill time
            // Update timestamp when order changes to filled
            order.timestamp = QDateTime::currentDateTime();
            order.sortOrder = order.fillTime.toMSecsSinceEpoch(); // Use fillTime for sorting
        } else if (status == "Cancelled") {
            order.status = OrderStatus::Cancelled;
            // Update timestamp when order changes to cancelled
            order.timestamp = QDateTime::currentDateTime();
            order.sortOrder = order.timestamp.toMSecsSinceEpoch(); // Update sortOrder
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
                }
            }
        } else if (order.isCancelled()) {
            if (orderId == m_pendingBuyOrderId) {
                m_pendingBuyOrderId = -1;
            }
            if (orderId == m_pendingSellOrderId) {
                m_pendingSellOrderId = -1;
            }
            emit orderCancelled(orderId);
        }

        emit orderUpdated(order);
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

    // Get order type from settings
    QString orderType = Settings::instance().orderType();

    // Market orders cannot be placed outside regular trading hours
    if (orderType == "MKT" && !isRegularHours) {
        LOG_WARNING("Cannot place market order outside regular trading hours");
        emit warning("Market orders can only be placed during regular trading hours (9:30-16:00 EST). Please switch to LMT orders or wait until market opens.");
        return -1;
    }

    // TIF and outsideRth based on trading hours and order type
    QString tif = isRegularHours ? "DAY" : "GTC";
    bool outsideRth = !isRegularHours;

    int orderId = m_client->placeOrder(m_currentSymbol, action, quantity, price, orderType, tif, outsideRth, m_currentExchange);

    LOG_INFO(QString("Order placed: orderId=%1, symbol=%2, action=%3, qty=%4, price=%5, type=%6, tif=%7, outsideRth=%8")
        .arg(orderId).arg(m_currentSymbol).arg(action).arg(quantity).arg(price, 0, 'f', 2).arg(orderType).arg(tif).arg(outsideRth));

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
    order.sortOrder = order.timestamp.toMSecsSinceEpoch(); // Use timestamp for sorting new orders

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

    // Check if order needs updating (price or quantity changed)
    if (m_orders.contains(pendingOrderId)) {
        const TradeOrder& existingOrder = m_orders[pendingOrderId];
        if (existingOrder.quantity == quantity && qAbs(existingOrder.price - price) < 0.01) {
            // Order unchanged, skip update
            LOG_DEBUG(QString("Order %1 unchanged (qty=%2, price=%3), skipping update")
                .arg(pendingOrderId).arg(quantity).arg(price, 0, 'f', 2));
            return;
        }
    }

    LOG_INFO(QString("Updating order %1: qty=%2, price=%3").arg(pendingOrderId).arg(quantity).arg(price, 0, 'f', 2));

    // Update order in TWS (uses same orderId - faster than cancel+create)
    bool isRegularHours = isRegularTradingHours();

    // Get order type from settings
    QString orderType = Settings::instance().orderType();

    // Market orders cannot be updated outside regular trading hours
    if (orderType == "MKT" && !isRegularHours) {
        LOG_WARNING("Cannot update market order outside regular trading hours");
        emit warning("Market orders can only be updated during regular trading hours (9:30-16:00 EST). Please switch to LMT orders or wait until market opens.");
        return;
    }

    // TIF and outsideRth based on trading hours and order type
    QString tif = isRegularHours ? "DAY" : "GTC";
    bool outsideRth = !isRegularHours;

    m_client->updateOrder(pendingOrderId, m_currentSymbol, action, quantity, price, orderType, tif, outsideRth, m_currentExchange);

    // Update order in memory - will be confirmed via onOrderStatusUpdated callback
    if (m_orders.contains(pendingOrderId)) {
        TradeOrder& order = m_orders[pendingOrderId];
        order.quantity = quantity;
        order.price = price;
        order.timestamp = QDateTime::currentDateTime(); // Update timestamp
        order.sortOrder = order.timestamp.toMSecsSinceEpoch();

        // Emit update to UI immediately (optimistic update)
        emit orderUpdated(order);
    }
}