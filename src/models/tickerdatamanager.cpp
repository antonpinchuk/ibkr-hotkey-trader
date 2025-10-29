#include "models/tickerdatamanager.h"
#include "client/ibkrclient.h"
#include "utils/logger.h"
#include <QDateTime>
#include <QTimeZone>

// Helper functions
QString makeTickerKey(const QString& symbol, const QString& exchange) {
    if (exchange.isEmpty()) {
        return symbol;
    }
    return QString("%1@%2").arg(symbol).arg(exchange);
}

QPair<QString, QString> parseTickerKey(const QString& tickerKey) {
    if (tickerKey.contains('@')) {
        QStringList parts = tickerKey.split('@');
        return qMakePair(parts[0], parts[1]);
    }
    return qMakePair(tickerKey, QString());
}

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
    connect(m_client, &IBKRClient::symbolFound, this, &TickerDataManager::onContractDetailsReceived);
    connect(m_client, &IBKRClient::symbolSearchFinished, this, &TickerDataManager::onContractSearchFinished);
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
    // Don't unsubscribe in destructor - m_client may already be destroyed
    // When app is shutting down, TWS connection will be closed automatically
}

QString TickerDataManager::getExchange(const QString& symbol, const QString& exchange) const
{
    QString tickerKey = makeTickerKey(symbol, exchange);
    return m_tickerKeyToExchange.value(tickerKey, exchange);
}

int TickerDataManager::getContractId(const QString& symbol, const QString& exchange) const
{
    QString tickerKey = makeTickerKey(symbol, exchange);
    return m_tickerKeyToContractId.value(tickerKey, 0);
}

void TickerDataManager::activateTicker(const QString& symbol, const QString& exchange)
{
    // Create ticker key
    QString tickerKey = makeTickerKey(symbol, exchange);

    // Store exchange and conId if provided (both old and new maps for compatibility)
    if (!exchange.isEmpty()) {
        m_symbolToExchange[symbol] = exchange;
        m_tickerKeyToExchange[tickerKey] = exchange;
    }

    // If already current, nothing to do
    if (m_currentSymbol == tickerKey) {
        return;
    }

    // Add ticker if new
    bool isNewTicker = !m_tickerData.contains(tickerKey);
    if (isNewTicker) {
        int conId = m_symbolToContractId.value(symbol, 0);
        if (conId == 0) {
            conId = m_tickerKeyToContractId.value(tickerKey, 0);
        }
        m_tickerData[tickerKey] = TickerData{symbol, exchange, conId};
    }

    // Switch to this ticker (will subscribe to tick-by-tick only)
    // Historical data and real-time bars will be loaded after first tick
    setCurrentSymbol(tickerKey);

    // Emit signal so UI can update (chart will show cached data if available)
    emit tickerActivated(symbol, exchange);
}

void TickerDataManager::setExpectedExchange(const QString& symbol, const QString& exchange)
{
    if (!exchange.isEmpty()) {
        m_symbolToExchange[symbol] = exchange;
    }
}

void TickerDataManager::setContractId(const QString& symbol, const QString& exchange, int conId)
{
    if (conId > 0) {
        QString tickerKey = makeTickerKey(symbol, exchange);
        m_symbolToContractId[symbol] = conId;
        m_tickerKeyToContractId[tickerKey] = conId;
    }
}

void TickerDataManager::removeTicker(const QString& symbol, const QString& exchange)
{
    // Create ticker key
    QString tickerKey = makeTickerKey(symbol, exchange);

    if (m_tickerData.contains(tickerKey)) {
        m_tickerData.remove(tickerKey);

        // Clean up request ID mappings
        QList<int> reqIdsToRemove;
        for (auto it = m_reqIdToSymbol.begin(); it != m_reqIdToSymbol.end(); ++it) {
            if (it.value() == tickerKey) {
                reqIdsToRemove.append(it.key());
            }
        }
        for (int reqId : reqIdsToRemove) {
            m_reqIdToSymbol.remove(reqId);
            m_reqIdToTimeframe.remove(reqId);
        }

        // Clean up real-time bars mappings
        QList<int> rtReqIdsToRemove;
        for (auto it = m_realTimeBarsReqIdToSymbol.begin(); it != m_realTimeBarsReqIdToSymbol.end(); ++it) {
            if (it.value() == tickerKey) {
                rtReqIdsToRemove.append(it.key());
            }
        }
        for (int reqId : rtReqIdsToRemove) {
            m_realTimeBarsReqIdToSymbol.remove(reqId);
            m_realTimeBarsLogged.remove(reqId);
        }
    }
}

void TickerDataManager::loadTimeframe(const QString& tickerKey, Timeframe timeframe)
{
    if (!m_tickerData.contains(tickerKey)) return;

    TickerData& data = m_tickerData[tickerKey];
    if (data.isLoadedByTimeframe.value(timeframe, false)) {
        emit tickerDataLoaded(data.symbol);
        return;
    }

    int reqId = m_nextReqId++;
    m_reqIdToSymbol[reqId] = tickerKey; // Store ticker key
    m_reqIdToTimeframe[reqId] = timeframe;
    requestHistoricalBars(data.symbol, reqId, timeframe); // TWS API gets pure symbol
}

const QVector<CandleBar>* TickerDataManager::getBars(const QString& tickerKey, Timeframe timeframe) const
{
    auto it = m_tickerData.constFind(tickerKey);
    if (it != m_tickerData.constEnd()) {
        auto barIt = it->barsByTimeframe.constFind(timeframe);
        if (barIt != it->barsByTimeframe.constEnd()) {
            return &barIt.value();
        }
    }
    return nullptr;
}

bool TickerDataManager::isLoaded(const QString& tickerKey, Timeframe timeframe) const
{
    auto it = m_tickerData.find(tickerKey);
    return (it != m_tickerData.end()) ? it->isLoadedByTimeframe.value(timeframe, false) : false;
}

void TickerDataManager::setCurrentSymbol(const QString& tickerKey)
{
    if (m_currentSymbol != tickerKey) {
        unsubscribeFromCurrentTicker();
        m_currentSymbol = tickerKey;
        m_isAggregating = false; // Reset aggregation state to prevent mixing prices from different tickers

        // Extract symbol from tickerKey for logging
        QString symbol = m_tickerData.contains(tickerKey) ? m_tickerData[tickerKey].symbol : tickerKey;
        LOG_INFO(QString("Switched to symbol: %1 (key: %2)").arg(symbol).arg(tickerKey));

        // Subscribe to tick-by-tick ONLY (for immediate price updates)
        // Historical data and real-time bars will be loaded after first tick
        subscribeToTickByTick();
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

void TickerDataManager::subscribeToTickByTick()
{
    if (m_currentSymbol.isEmpty() || !m_client || !m_client->isConnected()) return;

    // Get pure symbol for TWS API (not symbol@exchange)
    QString symbol = m_tickerData.contains(m_currentSymbol) ? m_tickerData[m_currentSymbol].symbol : m_currentSymbol;

    // Subscribe to tick-by-tick for price lines and current dynamic candle
    m_tickByTickReqId = m_nextReqId++;
    m_reqIdToSymbol[m_tickByTickReqId] = m_currentSymbol; // Store ticker key
    LOG_DEBUG(QString("Subscribing to tick-by-tick data for %1 (reqId: %2)").arg(symbol).arg(m_tickByTickReqId));
    m_client->requestTickByTick(m_tickByTickReqId, symbol);
}

void TickerDataManager::subscribeToRealTimeBars()
{
    if (m_currentSymbol.isEmpty() || !m_client || !m_client->isConnected()) return;

    // Get pure symbol for TWS API (not symbol@exchange)
    QString symbol = m_tickerData.contains(m_currentSymbol) ? m_tickerData[m_currentSymbol].symbol : m_currentSymbol;

    // Subscribe to real-time 5s bars (completed bars go to cache)
    m_realTimeBarsReqId = m_nextReqId++;
    m_realTimeBarsReqIdToSymbol[m_realTimeBarsReqId] = m_currentSymbol; // Map reqId to ticker key
    m_realTimeBarsLogged[m_realTimeBarsReqId] = false; // Reset logging for this reqId
    LOG_DEBUG(QString("Subscribing to real-time bars for %1 (reqId: %2)").arg(symbol).arg(m_realTimeBarsReqId));
    m_client->requestRealTimeBars(m_realTimeBarsReqId, symbol);
}

void TickerDataManager::subscribeToCurrentTicker()
{
    subscribeToTickByTick();
    subscribeToRealTimeBars();
}

void TickerDataManager::unsubscribeFromCurrentTicker()
{
    if (m_realTimeBarsReqId != -1 && m_client && m_client->isConnected()) {
        LOG_DEBUG(QString("Unsubscribing from real-time bars (reqId: %1)").arg(m_realTimeBarsReqId));
        m_client->cancelRealTimeBars(m_realTimeBarsReqId);
        m_realTimeBarsReqIdToSymbol.remove(m_realTimeBarsReqId); // Remove mapping
        m_realTimeBarsLogged.remove(m_realTimeBarsReqId); // Remove logging tracker
        m_realTimeBarsReqId = -1;
    }

    if (m_tickByTickReqId != -1 && m_client && m_client->isConnected()) {
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
    QString tickerKey = m_reqIdToSymbol[reqId]; // This is now ticker key (symbol@exchange)
    Timeframe timeframe = m_reqIdToTimeframe[reqId];
    TickerData& data = m_tickerData[tickerKey];
    QVector<CandleBar>& bars = data.barsByTimeframe[timeframe];
    if (bars.isEmpty() || bars.last().timestamp < time) {
        bars.append({time, open, high, low, close, volume});
        data.lastBarTimestampByTimeframe[timeframe] = time;
    }
}

void TickerDataManager::onHistoricalDataFinished(int reqId)
{
    if (!m_reqIdToSymbol.contains(reqId)) return;
    QString tickerKey = m_reqIdToSymbol[reqId]; // This is now ticker key
    Timeframe timeframe = m_reqIdToTimeframe[reqId];
    m_tickerData[tickerKey].isLoadedByTimeframe[timeframe] = true;

    // Count how many bars were loaded
    int barCount = 0;
    if (m_tickerData.contains(tickerKey)) {
        auto it = m_tickerData[tickerKey].barsByTimeframe.find(timeframe);
        if (it != m_tickerData[tickerKey].barsByTimeframe.end()) {
            barCount = it.value().size();
        }
    }

    // Get pure symbol for logging and signal emission
    QString symbol = m_tickerData.contains(tickerKey) ? m_tickerData[tickerKey].symbol : tickerKey;
    LOG_DEBUG(QString("Historical data loaded for %1 (key=%2) [%3]: %4 bars").arg(symbol).arg(tickerKey).arg(timeframeToString(timeframe)).arg(barCount));
    emit tickerDataLoaded(symbol);
    m_reqIdToSymbol.remove(reqId);
    m_reqIdToTimeframe.remove(reqId);
}

void TickerDataManager::onRealTimeBarReceived(int reqId, long time, double open, double high, double low, double close, long volume)
{
    // Log first bar for each reqId (for debugging)
    if (!m_realTimeBarsLogged.value(reqId, false)) {
        QString mappedTickerKey = m_realTimeBarsReqIdToSymbol.value(reqId, "NOT_FOUND");
        LOG_DEBUG(QString("First real-time bar received: reqId=%1, mappedTickerKey=%2, currentSymbol=%3, O=%4, H=%5, L=%6, C=%7, V=%8")
            .arg(reqId).arg(mappedTickerKey).arg(m_currentSymbol)
            .arg(open).arg(high).arg(low).arg(close).arg(volume));
        m_realTimeBarsLogged[reqId] = true;
    }

    // Verify this bar belongs to a known ticker (prevent race condition on ticker switch)
    if (!m_realTimeBarsReqIdToSymbol.contains(reqId)) {
        LOG_DEBUG(QString("Ignoring real-time bar from unknown reqId %1 (ticker switched)").arg(reqId));
        return;
    }

    QString tickerKey = m_realTimeBarsReqIdToSymbol[reqId]; // This is now ticker key

    // Only process bars from current ticker
    if (tickerKey != m_currentSymbol) {
        LOG_DEBUG(QString("Ignoring real-time bar for %1 (current ticker is %2)").arg(tickerKey).arg(m_currentSymbol));
        return;
    }

    // Ignore duplicates
    if (time == m_lastCompletedBarTime) return;
    m_lastCompletedBarTime = time;

    CandleBar bar{time, open, high, low, close, volume};

    // Add to 5s cache
    TickerData& data = m_tickerData[tickerKey];
    QVector<CandleBar>& s5_bars = data.barsByTimeframe[Timeframe::SEC_5];
    s5_bars.append(bar);
    data.lastBarTimestampByTimeframe[Timeframe::SEC_5] = time;

    // Emit signal with pure symbol (not ticker key)
    emit barsUpdated(data.symbol, Timeframe::SEC_5);

    // If viewing 5s, done
    if (m_currentTimeframe == Timeframe::SEC_5) return;

    // Aggregate into larger timeframe (only for current ticker)
    if (tickerKey != m_currentSymbol) return;

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

    // Get pure symbol for signal emission
    QString symbol = m_tickerData.contains(m_currentSymbol) ? m_tickerData[m_currentSymbol].symbol : m_currentSymbol;

    // Check if this is the first tick for current ticker
    // If so, start loading historical data and subscribe to real-time bars
    if (m_realTimeBarsReqId == -1) {
        loadTimeframe(m_currentSymbol, m_currentTimeframe); // Pass ticker key
        subscribeToRealTimeBars();

        // Emit signal for first tick (used for Display Group sync, etc.)
        emit firstTickReceived(symbol); // Emit pure symbol
    }

    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 barTimestamp = (currentTime / 5) * 5; // 5-second boundary

    // Track price update for current bar
    m_lastPriceUpdateTime = currentTime;
    if (!m_hasPriceUpdateForCurrentBar) {
        m_hasPriceUpdateForCurrentBar = true;
        emit priceUpdateReceived(symbol); // Emit pure symbol
    }

    // Only update dynamic candle if we already have at least one real-time bar
    // (need to know the close price of previous bar to start new candle correctly)
    if (m_hasDynamicBar) {
        if (m_currentDynamicBar.timestamp != barTimestamp) {
            // Start new dynamic candle with close of previous
            double startPrice = m_currentDynamicBar.close;
            m_currentDynamicBar = {barTimestamp, startPrice, startPrice, startPrice, startPrice, 0};
        }

        // Update with new tick
        m_currentDynamicBar.high = qMax(m_currentDynamicBar.high, midPrice);
        m_currentDynamicBar.low = qMin(m_currentDynamicBar.low, midPrice);
        m_currentDynamicBar.close = midPrice;

        // Emit for chart update (NOT added to cache!)
        emit currentBarUpdated(symbol, m_currentDynamicBar); // Emit pure symbol
    }

    // Calculate price for ticker list (use last trade price, fallback to mid-price)
    double displayPrice = (price > 0) ? price : midPrice;

    // Calculate change percent from previous bar
    double changePercent = 0.0;
    const QVector<CandleBar>* bars = getBars(m_currentSymbol, m_currentTimeframe); // Pass ticker key
    if (bars && bars->size() >= 2) {
        // Compare with previous bar's close (1 bar ago for 10s = 10 seconds ago)
        double oldPrice = (*bars)[bars->size() - 2].close;
        if (oldPrice > 0) {
            changePercent = ((displayPrice - oldPrice) / oldPrice) * 100.0;
        }
    }

    // Emit price update for ticker list and price lines (immediate!)
    emit priceUpdated(symbol, displayPrice, changePercent, bid, ask, midPrice); // Emit pure symbol
}

void TickerDataManager::onCandleBoundaryCheck()
{
    if (m_currentSymbol.isEmpty() || !m_hasDynamicBar) return;

    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 currentBoundary = (currentTime / 5) * 5;

    // Get pure symbol for signal emission
    QString symbol = m_tickerData.contains(m_currentSymbol) ? m_tickerData[m_currentSymbol].symbol : m_currentSymbol;

    // If dynamic bar is from previous period, start new one
    if (m_currentDynamicBar.timestamp < currentBoundary) {
        // Check if previous bar had any price updates
        if (m_currentBarStartTime > 0 && !m_hasPriceUpdateForCurrentBar) {
            emit noPriceUpdate(symbol); // Emit pure symbol
        }

        // Start new bar with close of previous
        double startPrice = m_currentDynamicBar.close;
        m_currentDynamicBar = {currentBoundary, startPrice, startPrice, startPrice, startPrice, 0};
        m_currentBarStartTime = currentBoundary;
        m_hasPriceUpdateForCurrentBar = false; // Reset for new bar
        emit currentBarUpdated(symbol, m_currentDynamicBar); // Emit pure symbol
    }
}

void TickerDataManager::finalizeAggregationBar()
{
    if (!m_isAggregating) return;
    TickerData& data = m_tickerData[m_currentSymbol]; // m_currentSymbol is now ticker key
    QVector<CandleBar>& bars = data.barsByTimeframe[m_currentTimeframe];
    bars.append(m_aggregationBar);
    data.lastBarTimestampByTimeframe[m_currentTimeframe] = m_aggregationBar.timestamp;
    emit barsUpdated(data.symbol, m_currentTimeframe); // Emit pure symbol
    m_isAggregating = false;
}

void TickerDataManager::requestHistoricalBars(const QString& symbol, int reqId, Timeframe timeframe)
{
    int barSeconds = timeframeToSeconds(timeframe);
    int durationSeconds = barSeconds * 500; // Request 500 bars

    // TWS does not allow historical data requests for more than 86400 seconds (24 hours)
    durationSeconds = qMin(durationSeconds, 86400);

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

    // TWS does not allow historical data requests for more than 86400 seconds (24 hours)
    durationSeconds = qMin(durationSeconds, 86400);

    QString duration = QString("%1 S").arg(durationSeconds);
    QString endTimeStr = QDateTime::fromSecsSinceEpoch(toTime, QTimeZone("UTC")).toString("yyyyMMdd-HH:mm:ss");
    QString barSize = timeframeToBarSize(m_currentTimeframe);
    LOG_DEBUG(QString("Requesting missing bars for %1: from=%2 to=%3").arg(symbol).arg(fromTime).arg(toTime));
    m_client->requestHistoricalData(reqId, symbol, endTimeStr, duration, barSize);
}

void TickerDataManager::onContractDetailsReceived(int reqId, const QString& symbol, const QString& exchange, int conId)
{
    // Initialize search info if needed
    if (!m_contractSearches.contains(reqId)) {
        m_contractSearches[reqId] = ContractSearchInfo();
    }

    ContractSearchInfo& searchInfo = m_contractSearches[reqId];
    searchInfo.totalCount++;

    // Store contractId for Display Groups
    // IMPORTANT: Prioritize exact exchange match if we have expected exchange stored
    bool wasStored = false;
    if (m_symbolToExchange.contains(symbol)) {
        // We have expected exchange - only store if it matches
        QString expectedExchange = m_symbolToExchange[symbol];
        if (expectedExchange == exchange) {
            LOG_DEBUG(QString("Storing conId for %1: conId=%2, exchange=%3 (MATCHED expected: %4)")
                      .arg(symbol).arg(conId).arg(exchange).arg(expectedExchange));
            m_symbolToContractId[symbol] = conId;
            // Also store in tickerKey map
            QString tickerKey = makeTickerKey(symbol, exchange);
            m_tickerKeyToContractId[tickerKey] = conId;
            wasStored = true;
        } else {
            LOG_DEBUG(QString("Skipping conId for %1: conId=%2, exchange=%3 (expected: %4, NOT MATCHED)")
                      .arg(symbol).arg(conId).arg(exchange).arg(expectedExchange));
        }
    } else if (!m_symbolToContractId.contains(symbol)) {
        // No expected exchange - store first contract
        LOG_DEBUG(QString("Storing conId for %1: conId=%2, exchange=%3 (no expected exchange, using first)")
                  .arg(symbol).arg(conId).arg(exchange));
        m_symbolToContractId[symbol] = conId;
        // Also store in tickerKey map
        QString tickerKey = makeTickerKey(symbol, exchange);
        m_tickerKeyToContractId[tickerKey] = conId;
        wasStored = true;
    }

    // Collect up to 5 contracts for summary log
    if (wasStored && searchInfo.foundContracts.size() < 5) {
        searchInfo.foundContracts.append(QString("%1@%2").arg(symbol).arg(exchange));
    }

    // Also update exchange if not set (both old and new maps)
    if (!m_symbolToExchange.contains(symbol)) {
        m_symbolToExchange[symbol] = exchange;
    }
    QString tickerKey = makeTickerKey(symbol, exchange);
    if (!m_tickerKeyToExchange.contains(tickerKey)) {
        m_tickerKeyToExchange[tickerKey] = exchange;
    }
}

void TickerDataManager::onContractSearchFinished(int reqId)
{
    // Log summary if we have any contracts
    if (m_contractSearches.contains(reqId)) {
        const ContractSearchInfo& searchInfo = m_contractSearches[reqId];

        if (searchInfo.totalCount > 0) {
            QString contractList = searchInfo.foundContracts.join(", ");
            if (searchInfo.totalCount > searchInfo.foundContracts.size()) {
                contractList += QString(" ...and %1 more").arg(searchInfo.totalCount - searchInfo.foundContracts.size());
            }
            LOG_DEBUG(QString("Contract search complete: found %1 contract(s) [%2]")
                .arg(searchInfo.totalCount)
                .arg(contractList));
        }

        // Clean up
        m_contractSearches.remove(reqId);
    }
}

void TickerDataManager::onReconnected()
{
    // Clear all logging trackers on reconnect
    m_realTimeBarsLogged.clear();
    subscribeToCurrentTicker();
}
