#ifndef TICKERDATAMANAGER_H
#define TICKERDATAMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QTimer>
#include <QDateTime>

class IBKRClient;

// Structure to hold a single 10-second bar
struct CandleBar {
    qint64 timestamp;  // Unix timestamp in seconds
    double open;
    double high;
    double low;
    double close;
    qint64 volume;

    CandleBar() : timestamp(0), open(0), high(0), low(0), close(0), volume(0) {}
    CandleBar(qint64 ts, double o, double h, double l, double c, qint64 v)
        : timestamp(ts), open(o), high(h), low(l), close(c), volume(v) {}
};

// Data for a single ticker
struct TickerData {
    QString symbol;
    QVector<CandleBar> bars;  // Historical 10s bars from start of day
    bool isLoaded;            // Whether historical data has been loaded
    qint64 lastBarTimestamp;  // Timestamp of the last complete bar

    // Current bar being built from real-time ticks
    CandleBar currentBar;
    bool hasCurrentBar;

    TickerData() : isLoaded(false), lastBarTimestamp(0), hasCurrentBar(false) {}
};

class TickerDataManager : public QObject
{
    Q_OBJECT

public:
    explicit TickerDataManager(IBKRClient* client, QObject* parent = nullptr);

    // Add a ticker and start loading its data
    void addTicker(const QString& symbol);

    // Get historical bars for a ticker (returns nullptr if ticker not found)
    const QVector<CandleBar>* getBars(const QString& symbol) const;

    // Check if ticker data is loaded
    bool isLoaded(const QString& symbol) const;

    // Update current bar with new tick price
    void updateCurrentBar(const QString& symbol, double price);

signals:
    void tickerDataLoaded(const QString& symbol);
    void barsUpdated(const QString& symbol);

private slots:
    void onHistoricalBarReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void onHistoricalDataFinished(int reqId);
    void updateAllTickers();

private:
    QString getHistoricalDataEndTime() const;
    void requestHistoricalBars(const QString& symbol, int reqId);
    void finalizeCurrentBar(const QString& symbol);

    IBKRClient* m_client;
    QMap<QString, TickerData> m_tickerData;
    QMap<int, QString> m_reqIdToSymbol;  // Map historical data request IDs to symbols
    int m_nextReqId;

    QTimer* m_updateTimer;  // Timer for 10-second bar updates
};

#endif // TICKERDATAMANAGER_H
