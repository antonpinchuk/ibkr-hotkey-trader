#include "ibkrclient.h"
#include "../external/twsapi/source/cppclient/client/Order.h"
#include <QDebug>

IBKRClient::IBKRClient(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_port(0)
    , m_clientId(0)
    , m_nextOrderId(1)
{
    m_wrapper = std::make_unique<IBKRWrapper>(this);
    m_signal = std::make_unique<EReaderOSSignal>();
    m_socket = std::make_unique<EClientSocket>(m_wrapper.get(), m_signal.get());

    m_messageTimer = new QTimer(this);
    m_messageTimer->setInterval(100);
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
        m_isConnected = true;
        m_reconnectTimer->stop();
        emit connected();
    });

    QObject::connect(m_wrapper.get(), &IBKRWrapper::disconnected, this, [this]() {
        m_isConnected = false;
        m_messageTimer->stop();
        emit disconnected();
        m_reconnectTimer->start();
    });

    QObject::connect(m_wrapper.get(), &IBKRWrapper::errorOccurred, this, &IBKRClient::error);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::tickPriceReceived, this, &IBKRClient::tickPriceUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::tickByTickReceived, this, &IBKRClient::tickByTickUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::historicalDataReceived, this, &IBKRClient::historicalBarReceived);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::historicalDataComplete, this, &IBKRClient::historicalDataFinished);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::orderStatusChanged, this, &IBKRClient::orderStatusUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::executionReceived, this, &IBKRClient::orderFilled);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::accountValueUpdated, this, &IBKRClient::accountUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::positionUpdated, this, &IBKRClient::positionUpdated);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::managedAccountsReceived, this, &IBKRClient::accountsReceived);
    QObject::connect(m_wrapper.get(), &IBKRWrapper::contractDetailsReceived, this, &IBKRClient::symbolFound);
}

void IBKRClient::connect(const QString& host, int port, int clientId)
{
    m_host = host;
    m_port = port;
    m_clientId = clientId;

    qDebug() << "Connecting to" << host << ":" << port << "clientId:" << clientId;

    bool connected = m_socket->eConnect(host.toStdString().c_str(), port, clientId, false);

    if (connected) {
        // Create EReader and start it
        m_reader = std::make_unique<EReader>(m_socket.get(), m_signal.get());
        m_reader->start();

        m_messageTimer->start();
        qDebug() << "Connection initiated";
    } else {
        qDebug() << "Connection failed";
        emit error(-1, -1, "Failed to connect to TWS");
        m_reconnectTimer->start();
    }
}

void IBKRClient::disconnect()
{
    m_messageTimer->stop();
    m_reconnectTimer->stop();

    if (m_socket->isConnected()) {
        m_socket->eDisconnect();
    }

    m_reader.reset();
    m_isConnected = false;
}

void IBKRClient::processMessages()
{
    // Process messages using EReader pattern (non-blocking)
    if (m_reader && m_socket && m_socket->isConnected()) {
        m_reader->processMsgs();
    }
}

void IBKRClient::attemptReconnect()
{
    if (!m_isConnected && !m_host.isEmpty()) {
        qDebug() << "Attempting to reconnect...";
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

int IBKRClient::placeOrder(const QString& symbol, const QString& action, int quantity, double limitPrice)
{
    if (!m_socket->isConnected()) return -1;

    Contract contract;
    contract.symbol = symbol.toStdString();
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";

    Order order;
    order.action = action.toStdString();
    order.totalQuantity = Decimal(quantity);
    order.orderType = "LMT";
    order.lmtPrice = limitPrice;

    int orderId = m_nextOrderId++;
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

void IBKRClient::requestPositions()
{
    if (!m_socket->isConnected()) return;
    m_socket->reqPositions();
}

void IBKRClient::requestManagedAccounts()
{
    if (!m_socket->isConnected()) return;
    m_socket->reqManagedAccts();
}

void IBKRClient::searchSymbol(int reqId, const QString& pattern)
{
    if (!m_socket->isConnected()) return;

    Contract contract;
    contract.symbol = pattern.toStdString();
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";

    m_socket->reqContractDetails(reqId, contract);
}
