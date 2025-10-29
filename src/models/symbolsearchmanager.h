#ifndef SYMBOLSEARCHMANAGER_H
#define SYMBOLSEARCHMANAGER_H

#include <QObject>
#include <QMap>
#include <QPair>

class IBKRClient;
class TickerDataManager;

/**
 * @brief Manages symbol search operations and results
 *
 * This class encapsulates the business logic for searching symbols via TWS API,
 * finding the correct contract by exchange, and storing the results.
 * Used by both SymbolSearchDialog and RemoteControlServer.
 */
class SymbolSearchManager : public QObject
{
    Q_OBJECT

public:
    explicit SymbolSearchManager(IBKRClient* client, TickerDataManager* tickerDataManager, QObject* parent = nullptr);

    /**
     * @brief Search for a symbol and automatically select matching exchange
     * @param symbol Symbol to search for
     * @param expectedExchange Expected exchange (e.g., "NASDAQ")
     * @param callbackId Optional ID to identify the request in callback
     * @return Request ID
     *
     * When results arrive, if a match is found for symbol+exchange:
     * - Emits symbolFound(callbackId, symbol, exchange, conId)
     * - Stores exchange and conId in TickerDataManager
     *
     * If no match found:
     * - Emits symbolNotFound(callbackId, symbol, exchange)
     */
    int searchSymbolWithExchange(const QString& symbol, const QString& expectedExchange, int callbackId = 0);

    /**
     * @brief Search for a symbol without specific exchange (for dialog)
     * @param symbol Symbol to search for
     * @return Request ID
     *
     * Emits symbolSearchResults with all results for user to choose from
     */
    int searchSymbol(const QString& symbol);

signals:
    /**
     * @brief Emitted when symbol with expected exchange is found
     * @param callbackId Callback ID provided in searchSymbolWithExchange
     * @param symbol Found symbol
     * @param exchange Found exchange
     * @param conId Contract ID
     */
    void symbolFound(int callbackId, const QString& symbol, const QString& exchange, int conId);

    /**
     * @brief Emitted when symbol with expected exchange is NOT found
     */
    void symbolNotFound(int callbackId, const QString& symbol, const QString& exchange);

    /**
     * @brief Emitted with all search results (for dialog)
     */
    void symbolSearchResults(int reqId, const QList<QPair<QString, QPair<QString, QString>>>& results, const QMap<QString, int>& symbolToConId);

private slots:
    void onSymbolSearchResultsReceived(int reqId, const QList<QPair<QString, QPair<QString, QString>>>& results, const QMap<QString, int>& symbolToConId);

private:
    struct SearchRequest {
        QString symbol;
        QString expectedExchange;  // Empty if not expecting specific exchange
        int callbackId;
    };

    IBKRClient* m_client;
    TickerDataManager* m_tickerDataManager;

    QMap<int, SearchRequest> m_pendingSearches;  // reqId -> SearchRequest
    int m_nextReqId;
};

#endif // SYMBOLSEARCHMANAGER_H
