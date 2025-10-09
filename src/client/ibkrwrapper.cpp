#include "client/ibkrwrapper.h"
#include "client/ibkrclient.h"
#include "utils/logger.h"
#include "Execution.h"
#include "Order.h"
#include "OrderState.h"
#include "Decimal.h"
#include <QDebug>

IBKRWrapper::IBKRWrapper(IBKRClient *client)
    : m_client(client)
    , m_accountValueLogged(false)
    , m_portfolioLogged(false)
{
}

void IBKRWrapper::resetSession()
{
    m_accountValueLogged = false;
    m_portfolioLogged = false;
}

void IBKRWrapper::connectAck()
{
    qDebug() << "Connected to TWS";
    emit connected();
}

void IBKRWrapper::connectionClosed()
{
    qDebug() << "Connection closed";
    emit disconnected();
}

void IBKRWrapper::nextValidId(OrderId orderId)
{
    qDebug() << "API ready, next valid order ID:" << orderId;
    emit apiReady(orderId);
}

void IBKRWrapper::error(int id, time_t errorTime, int errorCode, const std::string& errorString, const std::string& advancedOrderRejectJson)
{
    QString msg = QString::fromStdString(errorString);

    // Filter informational "OK" status messages (not errors)
    // 2104: Market data farm connection is OK
    // 2106: HMDS data farm connection is OK
    // 2158: Sec-def data farm connection is OK
    if (errorCode == 2104 || errorCode == 2106 || errorCode == 2158) {
        LOG_DEBUG(QString("TWS Status [code=%1]: %2").arg(errorCode).arg(msg));
    }
    // Filter connection status messages (handled by auto-reconnect)
    // 1100: Connectivity between IB and TWS has been lost
    // 1300: TWS socket port has been reset (relogin)
    // 2110: Connectivity between TWS and server is broken
    else if (errorCode == 1100 || errorCode == 1300 || errorCode == 2110) {
        LOG_DEBUG(QString("Connection status [code=%1]: %2").arg(errorCode).arg(msg));
        // Force socket closure and trigger reconnect (don't stop reconnect timer)
        m_client->disconnect(false);
    }
    // Filter other informational messages
    // 2105: Historical data farm connection is OK
    // 2107: Historical data farm connection is inactive
    // 2108: Market data farm connection is inactive
    else if (errorCode >= 2105 && errorCode <= 2110) {
        LOG_DEBUG(QString("TWS Status [code=%1]: %2").arg(errorCode).arg(msg));
    }
    // Actual errors - log as ERROR
    else {
        LOG_ERROR(QString("TWS Error [id=%1, code=%2]: %3").arg(id).arg(errorCode).arg(msg));
    }

    emit errorOccurred(id, errorCode, msg);
}

void IBKRWrapper::tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attrib)
{
    emit tickPriceReceived(tickerId, field, price);

    // Aggregate market data: LAST=4, BID=1, ASK=2
    auto& cache = m_marketDataCache[tickerId];

    if (field == 4) {  // LAST
        cache.lastPrice = price;
        cache.hasLast = true;
    } else if (field == 1) {  // BID
        cache.bidPrice = price;
        cache.hasBid = true;
    } else if (field == 2) {  // ASK
        cache.askPrice = price;
        cache.hasAsk = true;
    }

    // Emit aggregated signal when we have all three prices
    if (cache.hasLast && cache.hasBid && cache.hasAsk) {
        emit marketDataReceived(tickerId, cache.lastPrice, cache.bidPrice, cache.askPrice);
    }
}

void IBKRWrapper::tickSize(TickerId tickerId, TickType field, Decimal size)
{
    // Not used for now
}

void IBKRWrapper::tickGeneric(TickerId tickerId, TickType tickType, double value)
{
    // Not used for now
}

void IBKRWrapper::tickString(TickerId tickerId, TickType tickType, const std::string& value)
{
    // Not used for now
}

void IBKRWrapper::tickByTickAllLast(int reqId, int tickType, time_t time, double price, Decimal size, const TickAttribLast& tickAttribLast, const std::string& exchange, const std::string& specialConditions)
{
    // Price updated via all last
    emit tickByTickReceived(reqId, price, 0, 0);
}

void IBKRWrapper::tickByTickBidAsk(int reqId, time_t time, double bidPrice, double askPrice, Decimal bidSize, Decimal askSize, const TickAttribBidAsk& tickAttribBidAsk)
{
    LOG_DEBUG(QString("tickByTickBidAsk [reqId=%1]: bid=%2, ask=%3").arg(reqId).arg(bidPrice).arg(askPrice));
    emit tickByTickReceived(reqId, 0, bidPrice, askPrice);
}

void IBKRWrapper::tickByTickMidPoint(int reqId, time_t time, double midPoint)
{
    // Not used for now
}

void IBKRWrapper::historicalData(TickerId reqId, const Bar& bar)
{
    // bar.time is a string in format "yyyymmdd  hh:mm:ss", convert to unix timestamp
    long timestamp = std::stol(bar.time);
    emit historicalDataReceived(reqId, timestamp, bar.open, bar.high, bar.low, bar.close, bar.volume);
}

void IBKRWrapper::historicalDataEnd(int reqId, const std::string& startDateStr, const std::string& endDateStr)
{
    qDebug() << "Historical data complete for reqId:" << reqId;
    emit historicalDataComplete(reqId);
}

void IBKRWrapper::orderStatus(OrderId orderId, const std::string& status, Decimal filled, Decimal remaining, double avgFillPrice, long long permId, int parentId, double lastFillPrice, int clientId, const std::string& whyHeld, double mktCapPrice)
{
    QString statusStr = QString::fromStdString(status);
    std::string filledStr = DecimalFunctions::decimalStringToDisplay(filled);
    std::string remainingStr = DecimalFunctions::decimalStringToDisplay(remaining);
    qDebug() << "Order status:" << orderId << statusStr << "filled:" << QString::fromStdString(filledStr) << "remaining:" << QString::fromStdString(remainingStr);

    // Log when order is filled to track if portfolio update follows
    if (statusStr == "Filled") {
        LOG_DEBUG(QString("Order %1 FILLED - awaiting portfolio update from TWS").arg(orderId));
    }

    emit orderStatusChanged(orderId, statusStr, QString::fromStdString(filledStr).toDouble(), QString::fromStdString(remainingStr).toDouble(), avgFillPrice);
}

void IBKRWrapper::openOrder(OrderId orderId, const Contract& contract, const Order& order, const OrderState& orderState)
{
    QString symbol = QString::fromStdString(contract.symbol);
    QString action = QString::fromStdString(order.action);

    double quantity = 0;
    if (order.totalQuantity != UNSET_DECIMAL) {
        quantity = DecimalFunctions::decimalToDouble(order.totalQuantity);
        LOG_INFO(QString("Open order: id=%1, %2 %3 x%4 @ %5, type=%6, status=%7")
            .arg(orderId)
            .arg(action)
            .arg(symbol)
            .arg(quantity)
            .arg(order.lmtPrice)
            .arg(QString::fromStdString(order.orderType))
            .arg(QString::fromStdString(orderState.status)));
    } else {
        LOG_WARNING(QString("Open order with UNSET quantity: orderId=%1, %2 %3 - skipping")
            .arg(orderId).arg(action).arg(symbol));
        return;
    }

    emit orderOpened(orderId, symbol, action, (int)quantity, order.lmtPrice, order.permId);
}

void IBKRWrapper::openOrderEnd()
{
    LOG_DEBUG("Open orders end");
}

void IBKRWrapper::completedOrder(const Contract& contract, const Order& order, const OrderState& orderState)
{
    QString symbol = QString::fromStdString(contract.symbol);
    QString action = QString::fromStdString(order.action);

    double quantity = 0;
    if (order.filledQuantity != UNSET_DECIMAL) {
        quantity = DecimalFunctions::decimalToDouble(order.filledQuantity);
    } else if (order.totalQuantity != UNSET_DECIMAL) {
        quantity = DecimalFunctions::decimalToDouble(order.totalQuantity);
    } else {
        LOG_WARNING(QString("Completed order with UNSET quantity: orderId=%1, %2 %3 - skipping")
            .arg(order.orderId).arg(action).arg(symbol));
        return;
    }

    LOG_INFO(QString("Completed order: id=%1, %2 %3 x%4 @ %5, status=%6, permId=%7")
        .arg(order.orderId)
        .arg(action)
        .arg(symbol)
        .arg(quantity)
        .arg(order.lmtPrice)
        .arg(QString::fromStdString(orderState.status))
        .arg(order.permId));

    emit orderOpened(order.orderId, symbol, action, (int)quantity, order.lmtPrice, order.permId);
}

void IBKRWrapper::completedOrdersEnd()
{
    LOG_DEBUG("Completed orders end");
}

void IBKRWrapper::execDetails(int reqId, const Contract& contract, const Execution& execution)
{
    QString symbol = QString::fromStdString(contract.symbol);
    QString side = QString::fromStdString(execution.side); // "BOT" or "SLD"
    std::string sharesStr = DecimalFunctions::decimalStringToDisplay(execution.shares);
    double shares = QString::fromStdString(sharesStr).toDouble();

    qDebug() << "Execution:" << execution.orderId << symbol << side << "price:" << execution.price << "shares:" << shares;

    emit executionReceived(execution.orderId, symbol, side, execution.price, shares);
}

void IBKRWrapper::execDetailsEnd(int reqId)
{
    qDebug() << "Executions end";
}

void IBKRWrapper::updateAccountValue(const std::string& key, const std::string& val, const std::string& currency, const std::string& accountName)
{
    // Log only account number and balance (NetLiquidation) until accountDownloadEnd
    if (!m_accountValueLogged) {
        if (key == "AccountCode") {
            LOG_DEBUG(QString("Account: %1").arg(QString::fromStdString(val)));
        } else if (key == "NetLiquidation") {
            LOG_DEBUG(QString("Balance: $%1 %2")
                .arg(QString::fromStdString(val))
                .arg(QString::fromStdString(currency)));
        }
    }

    emit accountValueUpdated(QString::fromStdString(key), QString::fromStdString(val), QString::fromStdString(currency), QString::fromStdString(accountName));
}

void IBKRWrapper::updatePortfolio(const Contract& contract, Decimal position, double marketPrice, double marketValue, double averageCost, double unrealizedPNL, double realizedPNL, const std::string& accountName)
{
    QString symbol = QString::fromStdString(contract.symbol);
    double quantity = DecimalFunctions::decimalToDouble(position);

    // Log all portfolio updates until accountDownloadEnd
    if (!m_portfolioLogged) {
        LOG_DEBUG(QString("Portfolio: symbol=%1, qty=%2, avgCost=%3, marketPrice=%4, marketValue=%5, unrealizedPNL=%6")
            .arg(symbol)
            .arg(quantity)
            .arg(averageCost)
            .arg(marketPrice)
            .arg(marketValue)
            .arg(unrealizedPNL));
    }

    emit positionUpdated(QString::fromStdString(accountName), symbol, quantity, averageCost, marketPrice, unrealizedPNL);
}

void IBKRWrapper::updateAccountTime(const std::string& timeStamp)
{
    // Not used for now
}

void IBKRWrapper::accountDownloadEnd(const std::string& accountName)
{
    // Disable logging after first account download
    m_accountValueLogged = true;
    m_portfolioLogged = true;
}

void IBKRWrapper::position(const std::string& account, const Contract& contract, Decimal position, double avgCost)
{
    // Not used - we get positions from updatePortfolio() via reqAccountUpdates()
}

void IBKRWrapper::positionEnd()
{
    // Not used - we get positions from updatePortfolio() via reqAccountUpdates()
}

void IBKRWrapper::contractDetails(int reqId, const ContractDetails& contractDetails)
{
    emit contractDetailsReceived(reqId, QString::fromStdString(contractDetails.contract.symbol), QString::fromStdString(contractDetails.contract.exchange), contractDetails.contract.conId);
}

void IBKRWrapper::contractDetailsEnd(int reqId)
{
    qDebug() << "Contract details end for reqId:" << reqId;
}

void IBKRWrapper::bondContractDetails(int reqId, const ContractDetails& contractDetails)
{
    // Not used for now
}

void IBKRWrapper::tickNews(int tickerId, time_t timeStamp, const std::string& providerCode, const std::string& articleId, const std::string& headline, const std::string& extraData)
{
    // Not used for now
}

void IBKRWrapper::managedAccounts(const std::string& accountsList)
{
    qDebug() << "Managed accounts:" << QString::fromStdString(accountsList);
    emit managedAccountsReceived(QString::fromStdString(accountsList));
}

void IBKRWrapper::symbolSamples(int reqId, const std::vector<ContractDescription>& contractDescriptions)
{
    QList<QPair<QString, QPair<QString, QString>>> results;

    for (const auto& desc : contractDescriptions) {
        // Only include stocks
        if (desc.contract.secType != "STK") {
            continue;
        }

        QString symbol = QString::fromStdString(desc.contract.symbol);

        // Get company name from description field
        QString companyName;
        if (!desc.contract.description.empty()) {
            companyName = QString::fromStdString(desc.contract.description);
        } else if (!desc.contract.localSymbol.empty()) {
            companyName = QString::fromStdString(desc.contract.localSymbol);
        } else {
            companyName = symbol;
        }

        // Get primary exchange
        QString exchange;
        if (!desc.contract.primaryExchange.empty()) {
            exchange = QString::fromStdString(desc.contract.primaryExchange);
        } else {
            exchange = QString::fromStdString(desc.contract.exchange);
        }

        results.append(qMakePair(symbol, qMakePair(companyName, exchange)));
    }

    qDebug() << "Symbol samples received for reqId:" << reqId << "count:" << results.size();
    emit symbolSamplesReceived(reqId, results);
}
