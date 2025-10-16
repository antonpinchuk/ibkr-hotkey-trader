#include "models/tickerdatamanager.h"
#include "client/ibkrclient.h"
#include "utils/logger.h"
#include <QDateTime>
#include <QTimeZone>

// Helper functions
QString timeframeToString(Timeframe tf) {
    switch (tf) {
        case Timeframe::SEC_5:   return "5s";
        case Timeframe::SEC_10:  return "10s";
        case Timeframe::SEC_30:  return "30s";
        case Timeframe::MIN_1:   return "1m";
        case Timeframe::MIN_5:   return "5m";
        case Timeframe::MIN_15:  return "15m";
        case Timeframe::MIN_30:  return "30m";
        case Timeframe::HOUR_1:  return "1H";
        case Timeframe::DAY_1:   return "1D";
        case Timeframe::WEEK_1:  return "1W";
        case Timeframe::MONTH_1: return "1M";
        default: return "Unknown";
    }
}

QString timeframeToBarSize(Timeframe tf) {
    switch (tf) {
        case Timeframe::SEC_5:   return "5 secs";
        case Timeframe::SEC_10:  return "10 secs";
        case Timeframe::SEC_30:  return "30 secs";
        case Timeframe::MIN_1:   return "1 min";
        case Timeframe::MIN_5:   return "5 mins";
        case Timeframe::MIN_15:  return "15 mins";
        case Timeframe::MIN_30:  return "30 mins";
        case Timeframe::HOUR_1:  return "1 hour";
        case Timeframe::DAY_1:   return "1 day";
        case Timeframe::WEEK_1:  return "1 week";
        case Timeframe::MONTH_1: return "1 month";
        default: return "1 min";
    }
}

int timeframeToSeconds(Timeframe tf) {
    switch (tf) {
        case Timeframe::SEC_5:   return 5;
        case Timeframe::SEC_10:  return 10;
        case Timeframe::SEC_30:  return 30;
        case Timeframe::MIN_1:   return 60;
        case Timeframe::MIN_5:   return 300;
        case Timeframe::MIN_15:  return 900;
        case Timeframe::MIN_30:  return 1800;
        case Timeframe::HOUR_1:  return 3600;
        case Timeframe::DAY_1:   return 86400;
        case Timeframe::WEEK_1:  return 604800;
        case Timeframe::MONTH_1: return 2592000;
        default: return 60;
    }
}

TickerDataManager::TickerDataManager(IBKRClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_nextReqId(2000)
    , m_currentTimeframe(Timeframe::SEC_10)
    , m_tickByTickReqId(-1)
    , m_realTimeBarsReqId(-1)
    , m_hasDynamicBar(false)
    , m_lastCompletedBarTime(0)
    , m_isAggregating(false)
    , m_lastPriceUpdateTime(0)
    , m_currentBarStartTime(0)
    , m_hasPriceUpdateForCurrentBar(false)
{
    connect(m_client, &IBKRClient::historicalBarReceived, this, &TickerDataManager::onHistoricalBarReceived);
    connect(m_client, &IBKRClient::historicalDataFinished, this, &TickerDataManager::onHistoricalDataFinished);
    connect(m_client, &IBKRClient::realTimeBarReceived, this, &TickerDataManager::onRealTimeBarReceived);
    connect(m_client, &IBKRClient::tickByTickUpdated, this, &TickerDataManager::onTickByTickUpdate);
    connect(m_client, &IBKRClient::connected, this, &TickerDataManager::onReconnected);

    // Timer to detect new candle boundaries (aligned to 5s)
    m_candleBoundaryTimer = new QTimer(this);
    connect(m_candleBoundaryTimer, &QTimer::timeout, this, &TickerDataManager::onCandleBoundaryCheck);

    // Align timer to 5s boundaries
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 nextBoundary = ((now / 5) + 1) * 5;
    int msToNext = (nextBoundary - now) * 1000;
    QTimer::singleShot(msToNext, this, [this]() {
        m_candleBoundaryTimer->start(5000); // Start repeating timer
        onCandleBoundaryCheck(); // Fire immediately on alignment
    });
}

TickerDataManager::~TickerDataManager()
{
    unsubscribeFromCurrentTicker();
}

void TickerDataManager::addTicker(const QString& symbol, Timeframe timeframe)
{
    if (!m_tickerData.contains(symbol)) {
        LOG_DEBUG(QString("Adding ticker %1").arg(symbol));
        m_tickerData[symbol] = TickerData{symbol};
    }
    loadTimeframe(symbol, timeframe);
}

void TickerDataManager::loadTimeframe(const QString& symbol, Timeframe timeframe)
{
    if (!m_tickerData.contains(symbol)) return;

    TickerData& data = m_tickerData[symbol];
    if (data.isLoadedByTimeframe.value(timeframe, false)) {
        emit tickerDataLoaded(symbol);
        return;
    }

    LOG_DEBUG(QString("Loading historical data for %1 [%2]").arg(symbol).arg(timeframeToString(timeframe)));
    int reqId = m_nextReqId++;
    m_reqIdToSymbol[reqId] = symbol;
    m_reqIdToTimeframe[reqId] = timeframe;
    requestHistoricalBars(symbol, reqId, timeframe);
}

const QVector<CandleBar>* TickerDataManager::getBars(const QString& symbol, Timeframe timeframe) const
{
    auto it = m_tickerData.constFind(symbol);
    if (it != m_tickerData.constEnd()) {
        auto barIt = it->barsByTimeframe.constFind(timeframe);
        if (barIt != it->barsByTimeframe.constEnd()) {
            return &barIt.value();
        }
    }
    return nullptr;
}

bool TickerDataManager::isLoaded(const QString& symbol, Timeframe timeframe) const
{
    auto it = m_tickerData.find(symbol);
    return (it != m_tickerData.end()) ? it->isLoadedByTimeframe.value(timeframe, false) : false;
}

void TickerDataManager::setCurrentSymbol(const QString& symbol)
{
    if (m_currentSymbol != symbol) {
        unsubscribeFromCurrentTicker();
        m_currentSymbol = symbol;
        m_isAggregating = false; // Reset aggregation state to prevent mixing prices from different tickers
        LOG_INFO(QString("Switched to symbol: %1").arg(symbol));
        loadTimeframe(symbol, m_currentTimeframe);
        subscribeToCurrentTicker();
    }
}

void TickerDataManager::setCurrentTimeframe(Timeframe timeframe)
{
    if (m_currentTimeframe != timeframe) {
        LOG_DEBUG(QString("Switching timeframe to %1").arg(timeframeToString(timeframe)));
        m_currentTimeframe = timeframe;
        m_isAggregating = false; // Reset aggregation on timeframe change
        if (!m_currentSymbol.isEmpty()) {
            loadTimeframe(m_currentSymbol, m_currentTimeframe);
        }
    }
}

void TickerDataManager::subscribeToCurrentTicker()
{
    if (m_currentSymbol.isEmpty() || !m_client || !m_client->isConnected()) return;
    unsubscribeFromCurrentTicker();

    // Subscribe to real-time 5s bars (completed bars go to cache)
    m_realTimeBarsReqId = m_nextReqId++;
    m_realTimeBarsReqIdToSymbol[m_realTimeBarsReqId] = m_currentSymbol; // Map reqId to symbol
    m_realTimeBarsLogged[m_realTimeBarsReqId] = false; // Reset logging for this reqId
    LOG_DEBUG(QString("Subscribing to real-time bars for %1 (reqId: %2)").arg(m_currentSymbol).arg(m_realTimeBarsReqId));
    m_client->requestRealTimeBars(m_realTimeBarsReqId, m_currentSymbol);

    // Subscribe to tick-by-tick for price lines and current dynamic candle
    m_tickByTickReqId = m_nextReqId++;
    LOG_DEBUG(QString("Subscribing to tick-by-tick data for %1 (reqId: %2)").arg(m_currentSymbol).arg(m_tickByTickReqId));
    m_client->requestTickByTick(m_tickByTickReqId, m_currentSymbol);
}

void TickerDataManager::unsubscribeFromCurrentTicker()
{
    if (m_realTimeBarsReqId != -1 && m_client) {
        LOG_DEBUG(QString("Unsubscribing from real-time bars (reqId: %1)").arg(m_realTimeBarsReqId));
        m_client->cancelRealTimeBars(m_realTimeBarsReqId);
        m_realTimeBarsReqIdToSymbol.remove(m_realTimeBarsReqId); // Remove mapping
        m_realTimeBarsLogged.remove(m_realTimeBarsReqId); // Remove logging tracker
        m_realTimeBarsReqId = -1;
    }

    if (m_tickByTickReqId != -1 && m_client) {
        LOG_DEBUG(QString("Unsubscribing from tick-by-tick data (reqId: %1)").arg(m_tickByTickReqId));
        m_client->cancelTickByTick(m_tickByTickReqId);
        m_tickByTickReqId = -1;
    }

    m_hasDynamicBar = false;
    m_lastCompletedBarTime = 0;
}

void TickerDataManager::onHistoricalBarReceived(int reqId, long time, double open, double high, double low, double close, long volume)
{
    if (!m_reqIdToSymbol.contains(reqId)) return;
    QString symbol = m_reqIdToSymbol[reqId];
    Timeframe timeframe = m_reqIdToTimeframe[reqId];
    TickerData& data = m_tickerData[symbol];
    QVector<CandleBar>& bars = data.barsByTimeframe[timeframe];
    if (bars.isEmpty() || bars.last().timestamp < time) {
        bars.append({time, open, high, low, close, volume});
        data.lastBarTimestampByTimeframe[timeframe] = time;
    }
}

void TickerDataManager::onHistoricalDataFinished(int reqId)
{
    if (!m_reqIdToSymbol.contains(reqId)) return;
    QString symbol = m_reqIdToSymbol[reqId];
    Timeframe timeframe = m_reqIdToTimeframe[reqId];
    m_tickerData[symbol].isLoadedByTimeframe[timeframe] = true;
    LOG_DEBUG(QString("Historical data loaded for %1 [%2]").arg(symbol).arg(timeframeToString(timeframe)));
    emit tickerDataLoaded(symbol);
    m_reqIdToSymbol.remove(reqId);
    m_reqIdToTimeframe.remove(reqId);
}

void TickerDataManager::onRealTimeBarReceived(int reqId, long time, double open, double high, double low, double close, long volume)
{
    // Log first bar for each reqId (for debugging)
    if (!m_realTimeBarsLogged.value(reqId, false)) {
        QString mappedSymbol = m_realTimeBarsReqIdToSymbol.value(reqId, "NOT_FOUND");
        LOG_DEBUG(QString("First real-time bar received: reqId=%1, mappedSymbol=%2, currentSymbol=%3, O=%4, H=%5, L=%6, C=%7, V=%8")
            .arg(reqId).arg(mappedSymbol).arg(m_currentSymbol)
            .arg(open).arg(high).arg(low).arg(close).arg(volume));
        m_realTimeBarsLogged[reqId] = true;
    }

    // Verify this bar belongs to a known symbol (prevent race condition on symbol switch)
    if (!m_realTimeBarsReqIdToSymbol.contains(reqId)) {
        LOG_DEBUG(QString("Ignoring real-time bar from unknown reqId %1 (symbol switched)").arg(reqId));
        return;
    }

    QString symbol = m_realTimeBarsReqIdToSymbol[reqId];

    // Only process bars from current symbol
    if (symbol != m_currentSymbol) {
        LOG_DEBUG(QString("Ignoring real-time bar for %1 (current symbol is %2)").arg(symbol).arg(m_currentSymbol));
        return;
    }

    // Ignore duplicates
    if (time == m_lastCompletedBarTime) return;
    m_lastCompletedBarTime = time;

    CandleBar bar{time, open, high, low, close, volume};

    // Add to 5s cache
    TickerData& data = m_tickerData[symbol];
    QVector<CandleBar>& s5_bars = data.barsByTimeframe[Timeframe::SEC_5];
    s5_bars.append(bar);
    data.lastBarTimestampByTimeframe[Timeframe::SEC_5] = time;
    emit barsUpdated(symbol, Timeframe::SEC_5);

    // If viewing 5s, done
    if (m_currentTimeframe == Timeframe::SEC_5) return;

    // Aggregate into larger timeframe (only for current symbol)
    if (symbol != m_currentSymbol) return;

    int barSeconds = timeframeToSeconds(m_currentTimeframe);
    qint64 barTimestamp = (time / barSeconds) * barSeconds;

    if (!m_isAggregating || m_aggregationBar.timestamp != barTimestamp) {
        if (m_isAggregating) {
            finalizeAggregationBar();
        }
        m_aggregationBar = bar;
        m_aggregationBar.timestamp = barTimestamp;
        m_isAggregating = true;
    } else {
        m_aggregationBar.high = qMax(m_aggregationBar.high, bar.high);
        m_aggregationBar.low = qMin(m_aggregationBar.low, bar.low);
        m_aggregationBar.close = bar.close;
        m_aggregationBar.volume += bar.volume;
    }

    // Check if aggregation period complete
    if ((time + 5) % barSeconds == 0) {
        finalizeAggregationBar();
    }
}

void TickerDataManager::onTickByTickUpdate(int reqId, double price, double bid, double ask)
{
    if (reqId != m_tickByTickReqId) return;

    // Only use mid price for dynamic candle if we have both bid and ask
    double midPrice = (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : price;
    if (midPrice <= 0) return;

    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 barTimestamp = (currentTime / 5) * 5; // 5-second boundary

    // Track price update for current bar
    m_lastPriceUpdateTime = currentTime;
    if (!m_hasPriceUpdateForCurrentBar) {
        m_hasPriceUpdateForCurrentBar = true;
        emit priceUpdateReceived(m_currentSymbol);
    }

    if (!m_hasDynamicBar || m_currentDynamicBar.timestamp != barTimestamp) {
        // Start new dynamic candle
        // If we have a previous candle, start with its close
        double startPrice = midPrice;
        if (m_hasDynamicBar && m_currentDynamicBar.close > 0) {
            startPrice = m_currentDynamicBar.close;
        }
        m_currentDynamicBar = {barTimestamp, startPrice, startPrice, startPrice, startPrice, 0};
        m_hasDynamicBar = true;
    }

    // Update with new tick
    m_currentDynamicBar.high = qMax(m_currentDynamicBar.high, midPrice);
    m_currentDynamicBar.low = qMin(m_currentDynamicBar.low, midPrice);
    m_currentDynamicBar.close = midPrice;

    // Emit for chart update (NOT added to cache!)
    emit currentBarUpdated(m_currentSymbol, m_currentDynamicBar);
}

void TickerDataManager::onCandleBoundaryCheck()
{
    if (m_currentSymbol.isEmpty() || !m_hasDynamicBar) return;

    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 currentBoundary = (currentTime / 5) * 5;

    // If dynamic bar is from previous period, start new one
    if (m_currentDynamicBar.timestamp < currentBoundary) {
        // Check if previous bar had any price updates
        if (m_currentBarStartTime > 0 && !m_hasPriceUpdateForCurrentBar) {
            emit noPriceUpdate(m_currentSymbol);
        }

        // Start new bar with close of previous
        double startPrice = m_currentDynamicBar.close;
        m_currentDynamicBar = {currentBoundary, startPrice, startPrice, startPrice, startPrice, 0};
        m_currentBarStartTime = currentBoundary;
        m_hasPriceUpdateForCurrentBar = false; // Reset for new bar
        emit currentBarUpdated(m_currentSymbol, m_currentDynamicBar);
    }
}

void TickerDataManager::finalizeAggregationBar()
{
    if (!m_isAggregating) return;
    TickerData& data = m_tickerData[m_currentSymbol];
    QVector<CandleBar>& bars = data.barsByTimeframe[m_currentTimeframe];
    bars.append(m_aggregationBar);
    data.lastBarTimestampByTimeframe[m_currentTimeframe] = m_aggregationBar.timestamp;
    emit barsUpdated(m_currentSymbol, m_currentTimeframe);
    m_isAggregating = false;
}

void TickerDataManager::requestHistoricalBars(const QString& symbol, int reqId, Timeframe timeframe)
{
    int barSeconds = timeframeToSeconds(timeframe);
    int durationSeconds = barSeconds * 500; // Request 500 bars
    if (timeframe == Timeframe::SEC_10 && durationSeconds > 7200) {
        durationSeconds = 7200;
    }
    QString duration = QString("%1 S").arg(durationSeconds);
    QString barSize = timeframeToBarSize(timeframe);
    LOG_DEBUG(QString("Requesting historical data for %1: duration=%2, barSize=%3").arg(symbol).arg(duration).arg(barSize));
    m_client->requestHistoricalData(reqId, symbol, "", duration, barSize);
}

void TickerDataManager::requestMissingBars(const QString& symbol, qint64 fromTime, qint64 toTime)
{
    if (!m_client || !m_client->isConnected()) return;
    int reqId = m_nextReqId++;
    m_reqIdToSymbol[reqId] = symbol;
    m_reqIdToTimeframe[reqId] = m_currentTimeframe;
    qint64 durationSeconds = toTime - fromTime;
    QString duration = QString("%1 S").arg(durationSeconds);
    QString endTimeStr = QDateTime::fromSecsSinceEpoch(toTime, QTimeZone("UTC")).toString("yyyyMMdd-HH:mm:ss");
    QString barSize = timeframeToBarSize(m_currentTimeframe);
    LOG_DEBUG(QString("Requesting missing bars for %1: from=%2 to=%3").arg(symbol).arg(fromTime).arg(toTime));
    m_client->requestHistoricalData(reqId, symbol, endTimeStr, duration, barSize);
}

void TickerDataManager::onReconnected()
{
    LOG_INFO("Reconnected. Re-subscribing to data.");
    // Clear all logging trackers on reconnect
    m_realTimeBarsLogged.clear();
    subscribeToCurrentTicker();
}
