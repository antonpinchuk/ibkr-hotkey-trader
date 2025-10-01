#ifndef IBKRWRAPPER_H
#define IBKRWRAPPER_H

#include "EWrapper.h"
#include <QObject>
#include <memory>

class IBKRClient;

class IBKRWrapper : public QObject, public EWrapper
{
    Q_OBJECT

public:
    explicit IBKRWrapper(IBKRClient *client);
    virtual ~IBKRWrapper() = default;

    // Connection and Server
    void connectAck() override;
    void connectionClosed() override;

    // Market Data
    void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attrib) override;
    void tickSize(TickerId tickerId, TickType field, Decimal size) override;
    void tickGeneric(TickerId tickerId, TickType tickType, double value) override;
    void tickString(TickerId tickerId, TickType tickType, const std::string& value) override;
    void tickByTickAllLast(int reqId, int tickType, time_t time, double price, Decimal size, const TickAttribLast& tickAttribLast, const std::string& exchange, const std::string& specialConditions) override;
    void tickByTickBidAsk(int reqId, time_t time, double bidPrice, double askPrice, Decimal bidSize, Decimal askSize, const TickAttribBidAsk& tickAttribBidAsk) override;
    void tickByTickMidPoint(int reqId, time_t time, double midPoint) override;

    // Historical Data
    void historicalData(TickerId reqId, const Bar& bar) override;
    void historicalDataEnd(int reqId, const std::string& startDateStr, const std::string& endDateStr) override;

    // Orders
    void orderStatus(OrderId orderId, const std::string& status, Decimal filled, Decimal remaining, double avgFillPrice, long long permId, int parentId, double lastFillPrice, int clientId, const std::string& whyHeld, double mktCapPrice) override;
    void openOrder(OrderId orderId, const Contract& contract, const Order& order, const OrderState& orderState) override;
    void openOrderEnd() override;
    void execDetails(int reqId, const Contract& contract, const Execution& execution) override;
    void execDetailsEnd(int reqId) override;

    // Account and Portfolio
    void updateAccountValue(const std::string& key, const std::string& val, const std::string& currency, const std::string& accountName) override;
    void updatePortfolio(const Contract& contract, Decimal position, double marketPrice, double marketValue, double averageCost, double unrealizedPNL, double realizedPNL, const std::string& accountName) override;
    void updateAccountTime(const std::string& timeStamp) override;
    void accountDownloadEnd(const std::string& accountName) override;
    void position(const std::string& account, const Contract& contract, Decimal position, double avgCost) override;
    void positionEnd() override;

    // Contract Details
    void contractDetails(int reqId, const ContractDetails& contractDetails) override;
    void contractDetailsEnd(int reqId) override;
    void bondContractDetails(int reqId, const ContractDetails& contractDetails) override;

    // News
    void tickNews(int tickerId, time_t timeStamp, const std::string& providerCode, const std::string& articleId, const std::string& headline, const std::string& extraData) override;

    // Managed Accounts
    void managedAccounts(const std::string& accountsList) override;

    // Error Handling
    void error(int id, time_t errorTime, int errorCode, const std::string& errorString, const std::string& advancedOrderRejectJson) override;

    // Market Depth - Not used but must be implemented
    void updateMktDepth(TickerId id, int position, int operation, int side, double price, Decimal size) override {}
    void updateMktDepthL2(TickerId id, int position, const std::string& marketMaker, int operation, int side, double price, Decimal size, bool isSmartDepth) override {}

    // News Providers - Not used but must be implemented
    void newsProviders(const std::vector<NewsProvider>& newsProviders) override {}
    void newsArticle(int requestId, int articleType, const std::string& articleText) override {}
    void historicalNews(int requestId, const std::string& time, const std::string& providerCode, const std::string& articleId, const std::string& headline) override {}
    void historicalNewsEnd(int requestId, bool hasMore) override {}

    // Market Scanners - Not used but must be implemented
    void scannerParameters(const std::string& xml) override {}
    void scannerData(int reqId, int rank, const ContractDetails& contractDetails, const std::string& distance, const std::string& benchmark, const std::string& projection, const std::string& legsStr) override {}
    void scannerDataEnd(int reqId) override {}

    // Real Time Bars - Not used but must be implemented
    void realtimeBar(TickerId reqId, long time, double open, double high, double low, double close, Decimal volume, Decimal wap, int count) override {}

    // Current Time - Not used but must be implemented
    void currentTime(long time) override {}

    // Fundamental Data - Not used but must be implemented
    void fundamentalData(TickerId reqId, const std::string& data) override {}

    // Delta-Neutral Validation - Not used but must be implemented
    void deltaNeutralValidation(int reqId, const DeltaNeutralContract& deltaNeutralContract) override {}

    // Tick Snapshot End - Not used but must be implemented
    void tickSnapshotEnd(int reqId) override {}

    // Market Data Type - Not used but must be implemented
    void marketDataType(TickerId reqId, int marketDataType) override {}

    // Commission Report - Not used but must be implemented
    void commissionReport(const CommissionReport& commissionReport) override {}

    // Account Summary - Not used but must be implemented
    void accountSummary(int reqId, const std::string& account, const std::string& tag, const std::string& value, const std::string& currency) override {}
    void accountSummaryEnd(int reqId) override {}

    // Verify and Display Message - Not used but must be implemented
    void verifyMessageAPI(const std::string& apiData) override {}
    void verifyCompleted(bool isSuccessful, const std::string& errorText) override {}
    void verifyAndAuthMessageAPI(const std::string& apiData, const std::string& xyzChallange) override {}
    void verifyAndAuthCompleted(bool isSuccessful, const std::string& errorText) override {}
    void displayGroupList(int reqId, const std::string& groups) override {}
    void displayGroupUpdated(int reqId, const std::string& contractInfo) override {}

    // Position Multi - Not used but must be implemented
    void positionMulti(int reqId, const std::string& account, const std::string& modelCode, const Contract& contract, Decimal pos, double avgCost) override {}
    void positionMultiEnd(int reqId) override {}

    // Account Update Multi - Not used but must be implemented
    void accountUpdateMulti(int reqId, const std::string& account, const std::string& modelCode, const std::string& key, const std::string& value, const std::string& currency) override {}
    void accountUpdateMultiEnd(int reqId) override {}

    // Security Definition Option Parameter - Not used but must be implemented
    void securityDefinitionOptionalParameter(int reqId, const std::string& exchange, int underlyingConId, const std::string& tradingClass, const std::string& multiplier, const std::set<std::string>& expirations, const std::set<double>& strikes) override {}
    void securityDefinitionOptionalParameterEnd(int reqId) override {}

    // Soft Dollar Tiers - Not used but must be implemented
    void softDollarTiers(int reqId, const std::vector<SoftDollarTier>& tiers) override {}

    // Family Codes - Not used but must be implemented
    void familyCodes(const std::vector<FamilyCode>& familyCodes) override {}

    // Symbol Samples - Not used but must be implemented
    void symbolSamples(int reqId, const std::vector<ContractDescription>& contractDescriptions) override {}

    // Market Rule - Not used but must be implemented
    void mktDepthExchanges(const std::vector<DepthMktDataDescription>& depthMktDataDescriptions) override {}

    // Tick Req Params - Not used but must be implemented
    void tickReqParams(int tickerId, double minTick, const std::string& bboExchange, int snapshotPermissions) override {}

    // Smart Components - Not used but must be implemented
    void smartComponents(int reqId, const SmartComponentsMap& theMap) override {}

    // Histogram Data - Not used but must be implemented
    void histogramData(int reqId, const HistogramDataVector& data) override {}

    // Reroute Market Data Req - Not used but must be implemented
    void rerouteMktDataReq(int reqId, int conid, const std::string& exchange) override {}

    // Reroute Market Depth Req - Not used but must be implemented
    void rerouteMktDepthReq(int reqId, int conid, const std::string& exchange) override {}

    // Market Rule - Not used but must be implemented
    void marketRule(int marketRuleId, const std::vector<PriceIncrement>& priceIncrements) override {}

    // PnL - Not used but must be implemented
    void pnl(int reqId, double dailyPnL, double unrealizedPnL, double realizedPnL) override {}
    void pnlSingle(int reqId, Decimal pos, double dailyPnL, double unrealizedPnL, double realizedPnL, double value) override {}

    // Historical Ticks - Not used but must be implemented
    void historicalTicks(int reqId, const std::vector<HistoricalTick>& ticks, bool done) override {}
    void historicalTicksBidAsk(int reqId, const std::vector<HistoricalTickBidAsk>& ticks, bool done) override {}
    void historicalTicksLast(int reqId, const std::vector<HistoricalTickLast>& ticks, bool done) override {}

    // Order Bound - Not used but must be implemented
    void orderBound(long long orderId, int apiClientId, int apiOrderId) override {}

    // Completed Order - Not used but must be implemented
    void completedOrder(const Contract& contract, const Order& order, const OrderState& orderState) override {}
    void completedOrdersEnd() override {}

    // Replace FA End - Not used but must be implemented
    void replaceFAEnd(int reqId, const std::string& text) override {}

    // WSH Meta Data - Not used but must be implemented
    void wshMetaData(int reqId, const std::string& dataJson) override {}
    void wshEventData(int reqId, const std::string& dataJson) override {}

    // Historical Schedule - Not used but must be implemented
    void historicalSchedule(int reqId, const std::string& startDateTime, const std::string& endDateTime, const std::string& timeZone, const std::vector<HistoricalSession>& sessions) override {}

    // User Info - Not used but must be implemented
    void userInfo(int reqId, const std::string& whiteBrandingId) override {}

signals:
    void connected();
    void disconnected();
    void errorOccurred(int id, int code, const QString& message);

    void tickPriceReceived(int tickerId, int field, double price);
    void tickByTickReceived(int reqId, double price, double bidPrice, double askPrice);

    void historicalDataReceived(int reqId, long time, double open, double high, double low, double close, long volume);
    void historicalDataComplete(int reqId);

    void orderStatusChanged(int orderId, const QString& status, double filled, double remaining, double avgFillPrice);
    void executionReceived(int orderId, double fillPrice, int fillQuantity);

    void accountValueUpdated(const QString& key, const QString& value, const QString& currency, const QString& account);
    void positionUpdated(const QString& account, const QString& symbol, double position, double avgCost);

    void managedAccountsReceived(const QString& accounts);

    void contractDetailsReceived(int reqId, const QString& symbol, const QString& exchange, int conId);
    void symbolSearchResults(int reqId, const QStringList& symbols);

private:
    IBKRClient *m_client;
};

#endif // IBKRWRAPPER_H
