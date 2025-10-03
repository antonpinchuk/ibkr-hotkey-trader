#include "tickerdatamanager.h"
#include "client/ibkrclient.h"
#include "utils/logger.h"
#include <QDateTime>

TickerDataManager::TickerDataManager(IBKRClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_nextReqId(2000)  // Start from 2000 to avoid conflicts
{
    // Setup 10-second timer to finalize bars
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(10000);  // 10 seconds
    connect(m_updateTimer, &QTimer::timeout, this, &TickerDataManager::updateAllTickers);
    m_updateTimer->start();

    // Connect to IBKR client signals
    connect(m_client, &IBKRClient::historicalBarReceived, this, &TickerDataManager::onHistoricalBarReceived);
    connect(m_client, &IBKRClient::historicalDataFinished, this, &TickerDataManager::onHistoricalDataFinished);
}

void TickerDataManager::addTicker(const QString& symbol)
{
    if (m_tickerData.contains(symbol)) {
        // Already exists
        return;
    }

    LOG_DEBUG(QString("TickerDataManager: Adding ticker %1").arg(symbol));

    // Create new ticker data entry
    TickerData data;
    data.symbol = symbol;
    data.isLoaded = false;
    m_tickerData[symbol] = data;

    // Request historical 10s bars from start of day
    int reqId = m_nextReqId++;
    m_reqIdToSymbol[reqId] = symbol;
    requestHistoricalBars(symbol, reqId);
}

const QVector<CandleBar>* TickerDataManager::getBars(const QString& symbol) const
{
    auto it = m_tickerData.find(symbol);
    if (it == m_tickerData.end()) {
        return nullptr;
    }
    return &it.value().bars;
}

bool TickerDataManager::isLoaded(const QString& symbol) const
{
    if (!m_tickerData.contains(symbol)) {
        return false;
    }
    return m_tickerData[symbol].isLoaded;
}

void TickerDataManager::updateCurrentBar(const QString& symbol, double price)
{
    if (!m_tickerData.contains(symbol)) {
        return;
    }

    TickerData& data = m_tickerData[symbol];
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 barTimestamp = (currentTime / 10) * 10;  // Round down to 10-second boundary

    if (!data.hasCurrentBar || data.currentBar.timestamp != barTimestamp) {
        // Finalize previous bar if exists
        if (data.hasCurrentBar) {
            finalizeCurrentBar(symbol);
        }

        // Start new bar
        data.currentBar = CandleBar(barTimestamp, price, price, price, price, 0);
        data.hasCurrentBar = true;
    } else {
        // Update current bar
        data.currentBar.high = qMax(data.currentBar.high, price);
        data.currentBar.low = qMin(data.currentBar.low, price);
        data.currentBar.close = price;
    }
}

void TickerDataManager::onHistoricalBarReceived(int reqId, long time, double open, double high, double low, double close, long volume)
{
    if (!m_reqIdToSymbol.contains(reqId)) {
        return;
    }

    QString symbol = m_reqIdToSymbol[reqId];
    if (!m_tickerData.contains(symbol)) {
        return;
    }

    TickerData& data = m_tickerData[symbol];
    CandleBar bar(time, open, high, low, close, volume);
    data.bars.append(bar);
    data.lastBarTimestamp = time;
}

void TickerDataManager::onHistoricalDataFinished(int reqId)
{
    if (!m_reqIdToSymbol.contains(reqId)) {
        return;
    }

    QString symbol = m_reqIdToSymbol[reqId];
    if (!m_tickerData.contains(symbol)) {
        return;
    }

    m_reqIdToSymbol.remove(reqId);

    TickerData& data = m_tickerData[symbol];
    data.isLoaded = true;

    LOG_DEBUG(QString("TickerDataManager: Historical data loaded for %1, %2 bars").arg(symbol).arg(data.bars.size()));
    emit tickerDataLoaded(symbol);
}

void TickerDataManager::updateAllTickers()
{
    // Every 10 seconds, finalize current bars for all tickers
    for (auto it = m_tickerData.begin(); it != m_tickerData.end(); ++it) {
        QString symbol = it.key();
        TickerData& data = it.value();

        if (data.hasCurrentBar) {
            finalizeCurrentBar(symbol);
        }
    }
}

void TickerDataManager::finalizeCurrentBar(const QString& symbol)
{
    if (!m_tickerData.contains(symbol)) {
        return;
    }

    TickerData& data = m_tickerData[symbol];
    if (!data.hasCurrentBar) {
        return;
    }

    // Check if this bar should update the last bar or append as new
    if (!data.bars.isEmpty() && data.bars.last().timestamp == data.currentBar.timestamp) {
        // Update existing bar
        data.bars.last() = data.currentBar;
    } else {
        // Append new bar
        data.bars.append(data.currentBar);
        data.lastBarTimestamp = data.currentBar.timestamp;
    }

    data.hasCurrentBar = false;
    emit barsUpdated(symbol);
}

QString TickerDataManager::getHistoricalDataEndTime() const
{
    // Return empty string to get data up to "now"
    return "";
}

void TickerDataManager::requestHistoricalBars(const QString& symbol, int reqId)
{
    // Request 10-second bars
    // For 10-second bars, IBKR has limitations - max ~2 hours of data
    // We'll request the last 2 hours

    QString endTime = getHistoricalDataEndTime();  // Empty = now
    QString duration = "7200 S";  // 2 hours in seconds
    QString barSize = "10 secs";

    LOG_DEBUG(QString("Requesting historical data for %1: duration=%2, barSize=%3").arg(symbol).arg(duration).arg(barSize));

    m_client->requestHistoricalData(reqId, symbol, endTime, duration, barSize);
}
