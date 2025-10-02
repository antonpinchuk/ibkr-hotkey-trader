#include "client/ibkrwrapper.h"
#include "client/ibkrclient.h"
#include "Execution.h"
#include "Decimal.h"
#include <QDebug>

IBKRWrapper::IBKRWrapper(IBKRClient *client)
    : m_client(client)
{
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
    qDebug() << "Error [" << id << "][" << errorCode << "]:" << msg;

    // Detect connection issues that require reconnect
    // 1100: Connectivity between IB and TWS has been lost
    // 1300: TWS socket port has been reset (relogin)
    // 2110: Connectivity between TWS and server is broken
    if (errorCode == 1100 || errorCode == 1300 || errorCode == 2110) {
        qDebug() << "Connection lost error detected (code" << errorCode << "), forcing disconnect and reconnect";
        // Force socket closure and trigger reconnect (don't stop reconnect timer)
        m_client->disconnect(false);
    }

    emit errorOccurred(id, errorCode, msg);
}

void IBKRWrapper::tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attrib)
{
    emit tickPriceReceived(tickerId, field, price);
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
    emit orderStatusChanged(orderId, statusStr, QString::fromStdString(filledStr).toDouble(), QString::fromStdString(remainingStr).toDouble(), avgFillPrice);
}

void IBKRWrapper::openOrder(OrderId orderId, const Contract& contract, const Order& order, const OrderState& orderState)
{
    qDebug() << "Open order:" << orderId << QString::fromStdString(contract.symbol);
}

void IBKRWrapper::openOrderEnd()
{
    qDebug() << "Open orders end";
}

void IBKRWrapper::execDetails(int reqId, const Contract& contract, const Execution& execution)
{
    std::string sharesStr = DecimalFunctions::decimalStringToDisplay(execution.shares);
    qDebug() << "Execution:" << execution.orderId << "price:" << execution.price << "shares:" << QString::fromStdString(sharesStr);
    emit executionReceived(execution.orderId, execution.price, QString::fromStdString(sharesStr).toDouble());
}

void IBKRWrapper::execDetailsEnd(int reqId)
{
    qDebug() << "Executions end";
}

void IBKRWrapper::updateAccountValue(const std::string& key, const std::string& val, const std::string& currency, const std::string& accountName)
{
    emit accountValueUpdated(QString::fromStdString(key), QString::fromStdString(val), QString::fromStdString(currency), QString::fromStdString(accountName));
}

void IBKRWrapper::updatePortfolio(const Contract& contract, Decimal position, double marketPrice, double marketValue, double averageCost, double unrealizedPNL, double realizedPNL, const std::string& accountName)
{
    // Not used for now
}

void IBKRWrapper::updateAccountTime(const std::string& timeStamp)
{
    // Not used for now
}

void IBKRWrapper::accountDownloadEnd(const std::string& accountName)
{
    qDebug() << "Account download end:" << QString::fromStdString(accountName);
}

void IBKRWrapper::position(const std::string& account, const Contract& contract, Decimal position, double avgCost)
{
    std::string posStr = DecimalFunctions::decimalStringToDisplay(position);
    emit positionUpdated(QString::fromStdString(account), QString::fromStdString(contract.symbol), QString::fromStdString(posStr).toDouble(), avgCost);
}

void IBKRWrapper::positionEnd()
{
    qDebug() << "Positions end";
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
