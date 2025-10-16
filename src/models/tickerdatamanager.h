#ifndef TICKERDATAMANAGER_H
#define TICKERDATAMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QDateTime>

class IBKRClient;

// Timeframe for candles
enum class Timeframe {
    SEC_5,    // 5 seconds (minimum for TWS real-time bars)
    SEC_10,   // 10 seconds
    SEC_30,   // 30 seconds
    MIN_1,    // 1 minute
    MIN_5,    // 5 minutes
    MIN_15,   // 15 minutes
    MIN_30,   // 30 minutes
    HOUR_1,   // 1 hour
    DAY_1,    // 1 day
    WEEK_1,   // 1 week
    MONTH_1,  // 1 month
};

Q_DECLARE_METATYPE(Timeframe)

// Helper functions
QString timeframeToString(Timeframe tf);
QString timeframeToBarSize(Timeframe tf);
int timeframeToSeconds(Timeframe tf);

struct CandleBar {
    qint64 timestamp;
    double open, high, low, close;
    qint64 volume;
    CandleBar() : timestamp(0), open(0), high(0), low(0), close(0), volume(0) {}
    CandleBar(qint64 ts, double o, double h, double l, double c, qint64 v) : timestamp(ts), open(o), high(h), low(l), close(c), volume(v) {}
};

struct TickerData {
    QString symbol;
    QMap<Timeframe, QVector<CandleBar>> barsByTimeframe;
    QMap<Timeframe, bool> isLoadedByTimeframe;
    QMap<Timeframe, qint64> lastBarTimestampByTimeframe;
};

class TickerDataManager : public QObject
{
    Q_OBJECT

public:
    explicit TickerDataManager(IBKRClient* client, QObject* parent = nullptr);
    ~TickerDataManager();

    void addTicker(const QString& symbol, Timeframe timeframe = Timeframe::MIN_1);
    void loadTimeframe(const QString& symbol, Timeframe timeframe);
    const QVector<CandleBar>* getBars(const QString& symbol, Timeframe timeframe) const;
    bool isLoaded(const QString& symbol, Timeframe timeframe) const;
    void setCurrentSymbol(const QString& symbol);
    void setCurrentTimeframe(Timeframe timeframe);
    Timeframe currentTimeframe() const { return m_currentTimeframe; }

signals:
    void tickerDataLoaded(const QString& symbol);
    void barsUpdated(const QString& symbol, Timeframe timeframe);
    void currentBarUpdated(const QString& symbol, const CandleBar& bar); // For live tick updates (not in cache)
    void noPriceUpdate(const QString& symbol); // Emitted when no price update received for previous bar
    void priceUpdateReceived(const QString& symbol); // Emitted when price update received

private slots:
    void onHistoricalBarReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void onHistoricalDataFinished(int reqId);
    void onRealTimeBarReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void onTickByTickUpdate(int reqId, double price, double bid, double ask);
    void onCandleBoundaryCheck(); // Timer to detect new candle start
    void onReconnected();

private:
    void subscribeToCurrentTicker();
    void unsubscribeFromCurrentTicker();
    void requestHistoricalBars(const QString& symbol, int reqId, Timeframe timeframe);
    void requestMissingBars(const QString& symbol, qint64 fromTime, qint64 toTime);
    void finalizeAggregationBar();

    IBKRClient* m_client;
    QMap<QString, TickerData> m_tickerData;
    QMap<int, QString> m_reqIdToSymbol;
    QMap<int, Timeframe> m_reqIdToTimeframe;
    int m_nextReqId;

    QString m_currentSymbol;
    Timeframe m_currentTimeframe;
    int m_tickByTickReqId;
    int m_realTimeBarsReqId;
    QMap<int, QString> m_realTimeBarsReqIdToSymbol; // Map reqId to symbol to prevent race condition
    QMap<int, bool> m_realTimeBarsLogged; // Track first bar logged per reqId

    // For building current dynamic candle from ticks (not in cache)
    QTimer* m_candleBoundaryTimer;
    CandleBar m_currentDynamicBar;
    bool m_hasDynamicBar;
    qint64 m_lastCompletedBarTime; // Track last completed bar to avoid duplicates

    // For aggregating 5s bars into larger timeframes
    CandleBar m_aggregationBar;
    bool m_isAggregating;

    // For tracking price updates per candle (for tray blinking)
    qint64 m_lastPriceUpdateTime;
    qint64 m_currentBarStartTime;
    bool m_hasPriceUpdateForCurrentBar;
};

#endif // TICKERDATAMANAGER_H
