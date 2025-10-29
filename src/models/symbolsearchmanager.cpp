#include "symbolsearchmanager.h"
#include "client/ibkrclient.h"
#include "models/tickerdatamanager.h"
#include "utils/logger.h"

SymbolSearchManager::SymbolSearchManager(IBKRClient* client, TickerDataManager* tickerDataManager, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_tickerDataManager(tickerDataManager)
    , m_nextReqId(10000) // Start from 10000 to avoid conflicts with other searches
{
    connect(m_client, &IBKRClient::symbolSearchResultsReceived,
            this, &SymbolSearchManager::onSymbolSearchResultsReceived);
}

int SymbolSearchManager::searchSymbolWithExchange(const QString& symbol, const QString& expectedExchange, int callbackId)
{
    int reqId = m_nextReqId++;

    SearchRequest request;
    request.symbol = symbol.toUpper();
    request.expectedExchange = expectedExchange.toUpper();
    request.callbackId = callbackId;

    m_pendingSearches[reqId] = request;

    m_client->searchSymbol(reqId, symbol);

    return reqId;
}

int SymbolSearchManager::searchSymbol(const QString& symbol)
{
    int reqId = m_nextReqId++;

    SearchRequest request;
    request.symbol = symbol.toUpper();
    request.expectedExchange = "";  // No specific exchange expected
    request.callbackId = 0;

    m_pendingSearches[reqId] = request;

    m_client->searchSymbol(reqId, symbol);

    return reqId;
}

void SymbolSearchManager::onSymbolSearchResultsReceived(int reqId,
    const QList<QPair<QString, QPair<QString, QString>>>& results,
    const QMap<QString, int>& symbolToConId)
{
    // Check if this is one of our requests
    if (!m_pendingSearches.contains(reqId)) {
        return;  // Not our request
    }

    SearchRequest request = m_pendingSearches.value(reqId);
    m_pendingSearches.remove(reqId);

    // If no specific exchange expected, just emit all results (for dialog)
    if (request.expectedExchange.isEmpty()) {
        LOG_DEBUG(QString("Symbol search: found %1 results for %2")
                  .arg(results.size()).arg(request.symbol));
        emit symbolSearchResults(reqId, results, symbolToConId);
        return;
    }

    // Search for specific exchange match
    bool found = false;
    QString matchedSymbol;
    QString matchedExchange;
    int matchedConId = 0;

    for (const auto& result : results) {
        QString symbol = result.first;
        QString exchange = result.second.second;

        if (symbol.toUpper() == request.symbol && exchange.toUpper() == request.expectedExchange) {
            found = true;
            matchedSymbol = symbol;
            matchedExchange = exchange;

            // Get conId from map
            QString key = QString("%1@%2").arg(symbol).arg(exchange);
            matchedConId = symbolToConId.value(key, 0);

            LOG_DEBUG(QString("Symbol search: %1@%2 -> conId=%3")
                      .arg(symbol).arg(exchange).arg(matchedConId));
            break;
        }
    }

    if (found) {
        // Store exchange and conId in TickerDataManager
        m_tickerDataManager->setExpectedExchange(matchedSymbol, matchedExchange);
        m_tickerDataManager->setContractId(matchedSymbol, matchedExchange, matchedConId);

        // Emit success
        emit symbolFound(request.callbackId, matchedSymbol, matchedExchange, matchedConId);
    } else {
        LOG_DEBUG(QString("Symbol search: no match for %1@%2")
                  .arg(request.symbol).arg(request.expectedExchange));
        emit symbolNotFound(request.callbackId, request.symbol, request.expectedExchange);
    }
}
