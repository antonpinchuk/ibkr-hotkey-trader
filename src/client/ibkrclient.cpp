#include "client/ibkrclient.h"
#include "../external/twsapi/source/cppclient/client/Order.h"
#include "../external/twsapi/source/cppclient/client/TagValue.h"
#include "utils/logger.h"
#include <QDebug>

IBKRClient::IBKRClient(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_port(0)
    , m_clientId(0)
    , m_nextOrderId(1)
    , m_activeAccount("N/A")
    , m_disconnectLogged(false)
{
    m_wrapper = std::make_unique<IBKRWrapper>(this);
    m_signal = std::make_unique<EReaderOSSignal>();
    m_socket = std::make_unique<EClientSocket>(m_wrapper.get(), m_signal.get());

    m_messageTimer = new QTimer(this);
    m_messageTimer->setInterval(50); // 50ms = fast enough, reduces conflicts with EReader thread
    QObject::connect(m_messageTimer, &QTimer::timeout, this, &IBKRClient::processMessages);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(1000);
    QObject::connect(m_reconnectTimer, &QTimer::timeout, this, &IBKRClient::attemptReconnect);

    setupSignals();
}

IBKRClient::~IBKRClient()
{
    disconnect();
}

void IBKRClient::setupSignals()
{
    QObject::connect(m_wrapper.get(), &IBKRWrapper::connected, this, [this]() {
        LOG_DEBUG("Connection acknowledged by TWS");
    });

    QObject::connect(m_wrapper.get(), &IBKRWrapper::apiReady, this, [this](int nextOrderId) {
        m_isConnected = true;
        m_reconnectTimer->stop();
        m_nextOrderId = nextOrderId;
        m_disconnectLogged = false; // Reset disconnect flag on successful connection
        m_wrapper->resetSession(); // Reset session tracking (logs, etc.)
        LOG_INFO(QString("TWS API ready, next order ID: %1").arg(nextOrderId));
        // Request managed accounts to get active account (after API is ready)
        requestManagedAccounts();
        // Bind manual orders automatically
        m_socket->reqAutoOpenOrders(true);
        // Request open and completed orders
        requestOpenOrders();
        requestCompletedOrders();
        emit connected();
    });

    QObject::connect(m_wrapper.get(), &IBKRWrapper::disconnected, this, [this]() {
        // TWS initiated disconnect (connectionClosed callback)
        disconnect(false); // Don't stop reconnect, let it auto-reconnect
    });

    QObject::connect(m_wrapper.get(), &IBKRWrapper::errorOccurred, this, &IBKRClient::error);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::tickPriceReceived, this, &IBKRClient::tickPriceUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::tickByTickReceived, this, &IBKRClient::tickByTickUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::marketDataReceived, this, &IBKRClient::marketDataUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::historicalDataReceived, this, &IBKRClient::historicalBarReceived);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::historicalDataComplete, this, &IBKRClient::historicalDataFinished);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::orderOpened, this, &IBKRClient::orderConfirmed);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::orderStatusChanged, this, &IBKRClient::orderStatusUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::executionReceived, this, &IBKRClient::orderFilled);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::accountValueUpdated, this, &IBKRClient::accountUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::positionUpdated, this, &IBKRClient::positionUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::managedAccountsReceived, this, [this](const QString& accounts) {
        // Managed accounts callback also indicates successful connection
        if (!m_isConnected) {
            m_isConnected = true;
            m_reconnectTimer->stop();
            m_disconnectLogged = false; // Reset disconnect flag on successful connection
            m_wrapper->resetSession(); // Reset session tracking (logs, etc.)
            LOG_INFO("Connection established via managedAccounts");
            emit connected();
        }

        // Take first account from comma-separated list (active account in TWS)
        QStringList accountList = accounts.split(',', Qt::SkipEmptyParts);
        if (!accountList.isEmpty()) {
            QString newAccount = accountList.first().trimmed();
            if (!newAccount.isEmpty()) {
                m_activeAccount = newAccount;
                LOG_INFO(QString("Active account set to: %1").arg(m_activeAccount));
                emit activeAccountChanged(m_activeAccount);
                // Request account updates (balance + positions)
                LOG_DEBUG(QString("Subscribing to account updates for: %1").arg(m_activeAccount));
                requestAccountUpdates(true, m_activeAccount);
            } else {
                m_activeAccount = "N/A";
                LOG_WARNING("No active account available from TWS");
                emit activeAccountChanged("N/A");
                emit error(-1, 2104, "No account available. Check TWS login and permissions.");
            }
        } else {
            m_activeAccount = "N/A";
            LOG_WARNING("No managed accounts received from TWS");
            emit activeAccountChanged("N/A");
            emit error(-1, 2104, "No account available. Check TWS login and permissions.");
        }
        emit accountsReceived(accounts);
    });
    QObject::connect(m_wrapper.get(), &IBKRWrapper::contractDetailsReceived, this, &IBKRClient::symbolFound);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::symbolSamplesReceived, this, &IBKRClient::symbolSearchResultsReceived);
}

void IBKRClient::connect(const QString& host, int port, int clientId)
{
    m_host = host;
    m_port = port;
    m_clientId = clientId;

    LOG_DEBUG(QString("Connecting to TWS at %1:%2 with clientId %3").arg(host).arg(port).arg(clientId));

    bool connected = m_socket->eConnect(host.toStdString().c_str(), port, clientId, false);

    if (connected) {
        // Create EReader and start it
        m_reader = std::make_unique<EReader>(m_socket.get(), m_signal.get());
        m_reader->start();

        m_messageTimer->start();
        LOG_DEBUG("TWS connection initiated, waiting for API ready");
    } else {
        // Don't log here - detailed error will come from IBKRWrapper
        // Don't emit error here - detailed error will come from IBKRWrapper
        m_reconnectTimer->start();
    }
}

void IBKRClient::disconnect()
{
    disconnect(true);
}

void IBKRClient::disconnect(bool stopReconnect)
{
    m_messageTimer->stop();
    if (stopReconnect) {
        m_reconnectTimer->stop();
    }

    if (m_socket->isConnected()) {
        m_socket->eDisconnect();
    }

    m_reader.reset();

    if (!m_isConnected) {
        return; // Already disconnected
    }

    m_isConnected = false;
    m_activeAccount = "N/A";
    emit activeAccountChanged("N/A");

    // Log disconnect only once
    if (!m_disconnectLogged) {
        LOG_WARNING("Disconnected from TWS");
        m_disconnectLogged = true;
    }

    emit disconnected();

    if (!stopReconnect) {
        m_reconnectTimer->start();
    }
}

void IBKRClient::processMessages()
{
    // Process messages using EReader pattern (non-blocking)
    if (m_reader && m_socket && m_socket->isConnected()) {
        try {
            m_reader->processMsgs();
        } catch (const std::system_error& e) {
            // Mutex conflict with EReader thread - skip this cycle
            // This can happen when both threads try to access message queue simultaneously
            LOG_DEBUG(QString("Skipped processMsgs() due to mutex conflict: %1").arg(e.what()));
        }
    }
}

void IBKRClient::attemptReconnect()
{
    if (!m_isConnected && !m_host.isEmpty()) {
        // Don't log every reconnect attempt to avoid spam
        // Ensure clean disconnect before reconnecting
        if (m_socket->isConnected()) {
            m_socket->eDisconnect();
            m_reader.reset();
        }
        connect(m_host, m_port, m_clientId);
    }
}

void IBKRClient::requestMarketData(int tickerId, const QString& symbol)
{
    if (!m_socket->isConnected()) return;

    Contract contract;
    contract.symbol = symbol.toStdString();
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";

    m_socket->reqMktData(tickerId, contract, "", false, false, TagValueListSPtr());
}

void IBKRClient::cancelMarketData(int tickerId)
{
    if (!m_socket->isConnected()) return;
    m_socket->cancelMktData(tickerId);
}

void IBKRClient::requestTickByTick(int tickerId, const QString& symbol)
{
    if (!m_socket->isConnected()) return;

    // Reset tick logging for this reqId (allow first tick to be logged again)
    m_wrapper->resetTickByTickLogging(tickerId);

    Contract contract;
    contract.symbol = symbol.toStdString();
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";

    m_socket->reqTickByTickData(tickerId, contract, "BidAsk", 0, true);
}

void IBKRClient::cancelTickByTick(int tickerId)
{
    if (!m_socket->isConnected()) return;

    // Reset tick logging for this reqId (in case we resubscribe later)
    m_wrapper->resetTickByTickLogging(tickerId);

    m_socket->cancelTickByTickData(tickerId);
}

void IBKRClient::requestHistoricalData(int reqId, const QString& symbol, const QString& endDateTime, const QString& duration, const QString& barSize)
{
    if (!m_socket->isConnected()) return;

    Contract contract;
    contract.symbol = symbol.toStdString();
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";

    m_socket->reqHistoricalData(reqId, contract, endDateTime.toStdString(), duration.toStdString(),
                                 barSize.toStdString(), "TRADES", 1, 1, false, TagValueListSPtr());
}

int IBKRClient::placeOrder(const QString& symbol, const QString& action, int quantity, double limitPrice,
                           const QString& orderType, const QString& tif, bool outsideRth, const QString& primaryExchange)
{
    if (!m_socket->isConnected()) {
        LOG_WARNING("Cannot place order - not connected to TWS");
        return -1;
    }

    LOG_INFO(QString("IBKRClient::placeOrder - symbol=%1, action=%2, qty=%3, limitPrice=%4, type=%5, tif=%6, outsideRth=%7, primaryExch=%8")
        .arg(symbol).arg(action).arg(quantity).arg(limitPrice, 0, 'f', 2).arg(orderType).arg(tif).arg(outsideRth).arg(primaryExchange));

    Contract contract;
    contract.symbol = symbol.toStdString();
    contract.secType = "STK";
    contract.exchange = "SMART";

    // Use provided primaryExchange if available, otherwise default to ISLAND (NASDAQ)
    if (!primaryExchange.isEmpty()) {
        contract.primaryExchange = primaryExchange.toStdString();
    } else {
        contract.primaryExchange = "ISLAND";  // Default to NASDAQ
    }

    contract.currency = "USD";

    // Create TWS API Order (not our TradeOrder)
    // IMPORTANT: Initialize Order properly to avoid copy constructor crash
    Order order;

    // Set all required fields explicitly
    order.action = action.toStdString();
    order.totalQuantity = DecimalFunctions::doubleToDecimal((double)quantity);
    order.orderType = orderType.toStdString();
    order.tif = tif.toStdString();
    order.outsideRth = outsideRth;

    // Set limit price for limit orders, otherwise keep default (0)
    if (orderType == "LMT") {
        order.lmtPrice = limitPrice;
    } else {
        order.lmtPrice = 0;
    }

    // Initialize other fields to prevent uninitialized memory issues
    order.auxPrice = 0;
    order.parentId = 0;
    order.transmit = true;
    order.displaySize = 0;
    order.goodAfterTime = "";
    order.goodTillDate = "";

    int orderId = m_nextOrderId++;

    LOG_INFO(QString("Sending order to TWS: orderId=%1, action=%2, qty=%3, totalQty=%4, type=%5, lmt=%6, tif=%7")
        .arg(orderId).arg(QString::fromStdString(order.action)).arg(quantity)
        .arg(order.totalQuantity).arg(QString::fromStdString(order.orderType))
        .arg(order.lmtPrice, 0, 'f', 2).arg(QString::fromStdString(order.tif)));

    m_socket->placeOrder(orderId, contract, order);

    return orderId;
}

void IBKRClient::cancelOrder(int orderId)
{
    if (!m_socket->isConnected()) return;
    OrderCancel orderCancel;
    orderCancel.manualOrderCancelTime = "";
    m_socket->cancelOrder(orderId, orderCancel);
}

void IBKRClient::cancelAllOrders()
{
    if (!m_socket->isConnected()) return;
    OrderCancel orderCancel;
    orderCancel.manualOrderCancelTime = "";
    m_socket->reqGlobalCancel(orderCancel);
}

void IBKRClient::requestAccountUpdates(bool subscribe, const QString& account)
{
    if (!m_socket->isConnected()) return;
    m_socket->reqAccountUpdates(subscribe, account.toStdString());
}


void IBKRClient::requestManagedAccounts()
{
    if (!m_socket->isConnected()) return;
    m_socket->reqManagedAccts();
}

void IBKRClient::requestOpenOrders()
{
    if (!m_socket->isConnected()) return;
    LOG_DEBUG("Requesting all open orders from TWS");
    m_socket->reqAllOpenOrders();  // Get all orders, not just from this client
}

void IBKRClient::requestCompletedOrders()
{
    if (!m_socket->isConnected()) return;
    LOG_DEBUG("Requesting completed orders from TWS");
    m_socket->reqCompletedOrders(false);  // false = not API-only orders
}

void IBKRClient::searchSymbol(int reqId, const QString& pattern)
{
    if (!m_socket->isConnected()) return;

    // Use reqMatchingSymbols for flexible pattern-based search
    m_socket->reqMatchingSymbols(reqId, pattern.toStdString());
}
