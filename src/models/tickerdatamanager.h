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
    HOUR_1,   // 1 hour (maximum due to TWS 86400s historical data limit)
};

Q_DECLARE_METATYPE(Timeframe)

// Helper functions
QString timeframeToString(Timeframe tf);
QString timeframeToBarSize(Timeframe tf);
int timeframeToSeconds(Timeframe tf);
QString makeTickerKey(const QString& symbol, const QString& exchange);
QPair<QString, QString> parseTickerKey(const QString& tickerKey); // Returns (symbol, exchange)

struct CandleBar {
    qint64 timestamp;
    double open, high, low, close;
    qint64 volume;
    CandleBar() : timestamp(0), open(0), high(0), low(0), close(0), volume(0) {}
    CandleBar(qint64 ts, double o, double h, double l, double c, qint64 v) : timestamp(ts), open(o), high(h), low(l), close(c), volume(v) {}
};

struct TickerData {
    QString symbol;
    QString exchange;
    int conId;
    QMap<Timeframe, QVector<CandleBar>> barsByTimeframe;
    QMap<Timeframe, bool> isLoadedByTimeframe;
    QMap<Timeframe, qint64> lastBarTimestampByTimeframe;

    TickerData() : conId(0) {}
    TickerData(const QString& sym, const QString& exch = QString(), int contractId = 0)
        : symbol(sym), exchange(exch), conId(contractId) {}
};

class TickerDataManager : public QObject
{
    Q_OBJECT

public:
    explicit TickerDataManager(IBKRClient* client, QObject* parent = nullptr);
    ~TickerDataManager();

    void activateTicker(const QString& symbol, const QString& exchange = QString());
    void removeTicker(const QString& symbol, const QString& exchange = QString());
    void loadTimeframe(const QString& tickerKey, Timeframe timeframe);
    const QVector<CandleBar>* getBars(const QString& tickerKey, Timeframe timeframe) const;
    bool isLoaded(const QString& tickerKey, Timeframe timeframe) const;
    void setCurrentSymbol(const QString& tickerKey);
    void setCurrentTimeframe(Timeframe timeframe);
    Timeframe currentTimeframe() const { return m_currentTimeframe; }
    QString currentSymbol() const { return m_currentSymbol; }
    QString getExchange(const QString& symbol, const QString& exchange) const;
    int getContractId(const QString& symbol, const QString& exchange) const;

    // Set expected exchange and conId before activating ticker (for Remote Control API)
    void setExpectedExchange(const QString& symbol, const QString& exchange);
    void setContractId(const QString& symbol, const QString& exchange, int conId);

signals:
    void tickerDataLoaded(const QString& symbol);
    void tickerActivated(const QString& symbol, const QString& exchange); // Emitted when ticker is ready (UI should update)
    void priceUpdated(const QString& symbol, double price, double changePercent, double bid, double ask, double mid); // For ticker list and price lines
    void barsUpdated(const QString& symbol, Timeframe timeframe);
    void currentBarUpdated(const QString& symbol, const CandleBar& bar); // For live tick updates (not in cache)
    void noPriceUpdate(const QString& symbol); // Emitted when no price update received for previous bar
    void priceUpdateReceived(const QString& symbol); // Emitted when price update received for current bar
    void firstTickReceived(const QString& symbol); // Emitted once when first tick is received for a symbol

private slots:
    void onHistoricalBarReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void onHistoricalDataFinished(int reqId);
    void onRealTimeBarReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void onTickByTickUpdate(int reqId, double price, double bid, double ask);
    void onContractDetailsReceived(int reqId, const QString& symbol, const QString& exchange, int conId);
    void onContractSearchFinished(int reqId); // Called when contract details search is complete
    void onCandleBoundaryCheck(); // Timer to detect new candle start
    void onReconnected();

private:
    void subscribeToCurrentTicker();
    void subscribeToTickByTick();
    void subscribeToRealTimeBars();
    void unsubscribeFromCurrentTicker();
    void requestHistoricalBars(const QString& symbol, int reqId, Timeframe timeframe);
    void requestMissingBars(const QString& symbol, qint64 fromTime, qint64 toTime);
    void finalizeAggregationBar();

    IBKRClient* m_client;
    QMap<QString, TickerData> m_tickerData; // key: tickerKey (symbol@exchange)
    QMap<QString, QString> m_symbolToExchange; // symbol -> exchange (DEPRECATED: use m_tickerKeyToExchange)
    QMap<QString, int> m_symbolToContractId; // symbol -> conId (DEPRECATED: use m_tickerKeyToContractId)
    QMap<QString, QString> m_tickerKeyToExchange; // tickerKey -> exchange
    QMap<QString, int> m_tickerKeyToContractId; // tickerKey -> conId
    QMap<int, QString> m_reqIdToSymbol; // reqId -> symbol
    QMap<int, Timeframe> m_reqIdToTimeframe;
    int m_nextReqId;

    // For contract search logging
    struct ContractSearchInfo {
        QStringList foundContracts; // List of "SYMBOL@EXCHANGE" strings
        int totalCount = 0;
        bool logged = false;
    };
    QMap<int, ContractSearchInfo> m_contractSearches; // reqId -> search info

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
