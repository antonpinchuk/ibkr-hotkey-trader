#include "ui/mainwindow.h"
#include "client/ibkrclient.h"
#include "trading/tradingmanager.h"
#include "widgets/tickerlistwidget.h"
#include "widgets/chartwidget.h"
#include "widgets/orderhistorywidget.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/symbolsearchdialog.h"
#include "dialogs/debuglogdialog.h"
#include "ui/toastnotification.h"
#include "models/settings.h"
#include "models/uistate.h"
#include "models/tickerdatamanager.h"
#include "utils/logger.h"
#include "utils/globalhotkeymanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QMessageBox>
#include <QFrame>
#include <QApplication>
#include <QScreen>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tickerLabel(nullptr)
    , m_settingsButton(nullptr)
    , m_toolbar(nullptr)
    , m_mainSplitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_rightBottomSplitter(nullptr)
    , m_tickerList(nullptr)
    , m_chart(nullptr)
    , m_orderHistory(nullptr)
    , m_settingsDialog(nullptr)
    , m_symbolSearch(nullptr)
    , m_ibkrClient(nullptr)
    , m_tradingManager(nullptr)
    , m_nextTickerId(1000)  // Start from 1000 to avoid conflicts
    , m_historicalOrderCounter(1)  // Start from 1 for historical orders
    , m_nextHistoricalOrderId(-1)  // Start from -1 for unique historical order IDs
{
    setWindowTitle("IBKR Hotkey Trader");
    resize(1400, 800);

    // Initialize components
    m_ibkrClient = new IBKRClient(this);
    m_tradingManager = new TradingManager(m_ibkrClient, this);
    m_tickerDataManager = new TickerDataManager(m_ibkrClient, this);
    m_settingsDialog = new SettingsDialog(this);
    m_symbolSearch = new SymbolSearchDialog(m_ibkrClient, this);
    m_globalHotkeyManager = new GlobalHotkeyManager(this);

    // Setup 10-second timer for inactive ticker updates
    m_inactiveTickerTimer = new QTimer(this);
    m_inactiveTickerTimer->setInterval(10000); // 10 seconds
    connect(m_inactiveTickerTimer, &QTimer::timeout, this, &MainWindow::updateInactiveTickers);
    m_inactiveTickerTimer->start();

    setupUI();
    setupConnections();
    restoreUIState();

    // Register global hotkeys (macOS only)
    m_globalHotkeyManager->registerHotkeys();

    // Apply saved settings to order history
    Settings& settings = Settings::instance();
    m_orderHistory->setShowCancelledAndZeroPositions(settings.showCancelledOrders());

    // Try to connect to TWS on startup
    m_ibkrClient->connect(settings.host(), settings.port(), settings.clientId());
}

MainWindow::~MainWindow()
{
    // Unregister global hotkeys
    m_globalHotkeyManager->unregisterHotkeys();
}

void MainWindow::setupUI()
{
    setupMenuBar();
    setupToolbar();
    setupPanels();

    // Initially disable trading until connected
    enableTrading(false);
}

void MainWindow::setupMenuBar()
{
    QMenu *appMenu = menuBar()->addMenu("IBKR Hotkey Trader");

    QAction *aboutAction = appMenu->addAction("About");
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About IBKR Hotkey Trader",
            "<h3>IBKR Hotkey Trader</h3>"
            "<p>Version: 0.1</p>"
            "<p>Author: Kinect.PRO (Anton Pinchuk)</p>"
            "<p><a href='https://kinect-pro.com/solutions/ibkr-hotkey-trader/'>Website</a></p>"
            "<p><a href='https://github.com/kinect-pro/ibkr-hotkey-trader'>GitHub</a></p>"
            "<p>License: MIT</p>");
    });

    appMenu->addSeparator();

    QAction *settingsAction = appMenu->addAction("Settings");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);

    appMenu->addSeparator();

    QAction *quitAction = appMenu->addAction("Quit");
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAction, &QAction::triggered, this, &MainWindow::onQuit);

    QMenu *fileMenu = menuBar()->addMenu("File");

    QAction *newSymbolAction = fileMenu->addAction("New Symbol");
    newSymbolAction->setShortcut(QKeySequence("Ctrl+K"));
    connect(newSymbolAction, &QAction::triggered, this, &MainWindow::onSymbolSearchRequested);

    fileMenu->addSeparator();

    QAction *resetAction = fileMenu->addAction("Reset Session");
    connect(resetAction, &QAction::triggered, this, &MainWindow::onResetSession);

    QMenu *ordersMenu = menuBar()->addMenu("Orders");

    // Opening positions
    QAction *open100Action = ordersMenu->addAction("Open 100%");
    open100Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+O"));
    connect(open100Action, &QAction::triggered, this, &MainWindow::onOpen100);

    QAction *open50Action = ordersMenu->addAction("Open 50%");
    open50Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+P"));
    connect(open50Action, &QAction::triggered, this, &MainWindow::onOpen50);

    ordersMenu->addSeparator();

    // Adding to position
    QAction *add5Action = ordersMenu->addAction("Add 5%");
    add5Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+1"));
    connect(add5Action, &QAction::triggered, this, &MainWindow::onAdd5);

    QAction *add10Action = ordersMenu->addAction("Add 10%");
    add10Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+2"));
    connect(add10Action, &QAction::triggered, this, &MainWindow::onAdd10);

    QAction *add15Action = ordersMenu->addAction("Add 15%");
    add15Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+3"));
    connect(add15Action, &QAction::triggered, this, &MainWindow::onAdd15);

    QAction *add20Action = ordersMenu->addAction("Add 20%");
    add20Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+4"));
    connect(add20Action, &QAction::triggered, this, &MainWindow::onAdd20);

    QAction *add25Action = ordersMenu->addAction("Add 25%");
    add25Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+5"));
    connect(add25Action, &QAction::triggered, this, &MainWindow::onAdd25);

    QAction *add30Action = ordersMenu->addAction("Add 30%");
    add30Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+6"));
    connect(add30Action, &QAction::triggered, this, &MainWindow::onAdd30);

    QAction *add35Action = ordersMenu->addAction("Add 35%");
    add35Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+7"));
    connect(add35Action, &QAction::triggered, this, &MainWindow::onAdd35);

    QAction *add40Action = ordersMenu->addAction("Add 40%");
    add40Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+8"));
    connect(add40Action, &QAction::triggered, this, &MainWindow::onAdd40);

    QAction *add45Action = ordersMenu->addAction("Add 45%");
    add45Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+9"));
    connect(add45Action, &QAction::triggered, this, &MainWindow::onAdd45);

    QAction *add50Action = ordersMenu->addAction("Add 50%");
    add50Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+0"));
    connect(add50Action, &QAction::triggered, this, &MainWindow::onAdd50);

    ordersMenu->addSeparator();

    // Cancel orders
    QAction *cancelAction = ordersMenu->addAction("Cancel All Orders");
    cancelAction->setShortcut(QKeySequence("Ctrl+Alt+Q"));
    connect(cancelAction, &QAction::triggered, this, &MainWindow::onCancelOrders);

    ordersMenu->addSeparator();

    // Closing positions
    QAction *close25Action = ordersMenu->addAction("Close 25%");
    close25Action->setShortcut(QKeySequence("Ctrl+Alt+V"));
    connect(close25Action, &QAction::triggered, this, &MainWindow::onClose25);

    QAction *close50Action = ordersMenu->addAction("Close 50%");
    close50Action->setShortcut(QKeySequence("Ctrl+Alt+C"));
    connect(close50Action, &QAction::triggered, this, &MainWindow::onClose50);

    QAction *close75Action = ordersMenu->addAction("Close 75%");
    close75Action->setShortcut(QKeySequence("Ctrl+Alt+X"));
    connect(close75Action, &QAction::triggered, this, &MainWindow::onClose75);

    QAction *close100Action = ordersMenu->addAction("Close 100%");
    close100Action->setShortcut(QKeySequence("Ctrl+Alt+Z"));
    connect(close100Action, &QAction::triggered, this, &MainWindow::onClose100);

    // View menu
    QMenu *viewMenu = menuBar()->addMenu("View");

    m_showCancelledOrdersAction = viewMenu->addAction("Show Cancelled && Zero Positions");
    m_showCancelledOrdersAction->setCheckable(true);
    m_showCancelledOrdersAction->setShortcut(QKeySequence("Ctrl+Shift+."));
    m_showCancelledOrdersAction->setChecked(Settings::instance().showCancelledOrders());
    connect(m_showCancelledOrdersAction, &QAction::triggered, this, &MainWindow::onToggleShowCancelledAndZeroPositions);

    QMenu *helpMenu = menuBar()->addMenu("Help");

    QAction *helpAction = helpMenu->addAction("Help documentation");
    connect(helpAction, &QAction::triggered, [this]() {
        QMessageBox::information(this, "Help",
            "<h3>Keyboard Shortcuts</h3>"
            "<h4>Opening Positions (Buy) - Global Hotkeys</h4>"
            "<ul>"
            "<li>Shift+Ctrl+Option+O: Buy 100% of budget</li>"
            "<li>Shift+Ctrl+Option+P: Buy 50% of budget</li>"
            "<li>Shift+Ctrl+Option+1-9: Add 5%-45% to position</li>"
            "<li>Shift+Ctrl+Option+0: Add 50% to position</li>"
            "</ul>"
            "<h4>Closing Positions (Sell) - Global Hotkeys</h4>"
            "<ul>"
            "<li>Ctrl+Option+Z: Sell 100% of position</li>"
            "<li>Ctrl+Option+X: Sell 75% of position</li>"
            "<li>Ctrl+Option+C: Sell 50% of position</li>"
            "<li>Ctrl+Option+V: Sell 25% of position</li>"
            "</ul>"
            "<h4>Other Controls</h4>"
            "<ul>"
            "<li>Cmd+K: Open symbol search</li>"
            "<li>Ctrl+Option+Q: Cancel all pending orders (Global)</li>"
            "</ul>"
            "<p><b>Note:</b> Trading hotkeys work globally from any application!</p>");
    });

    helpMenu->addSeparator();

    QAction *debugAction = helpMenu->addAction("Debug");
    debugAction->setShortcut(QKeySequence("Ctrl+Alt+I"));
    connect(debugAction, &QAction::triggered, this, &MainWindow::onDebugLogs);
}

void MainWindow::setupToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setFixedHeight(46);
    m_toolbar->setStyleSheet("QToolBar { padding: 2px; }");

    // Center: Trading buttons with labels
    QLabel* openLabel = new QLabel(" Open: ", this);
    m_toolbar->addWidget(openLabel);

    m_btnOpen100 = new QPushButton("100%", this);
    m_btnOpen100->setMinimumWidth(60);
    m_toolbar->addWidget(m_btnOpen100);
    connect(m_btnOpen100, &QPushButton::clicked, this, &MainWindow::onOpen100);

    m_btnOpen50 = new QPushButton("50%", this);
    m_toolbar->addWidget(m_btnOpen50);
    connect(m_btnOpen50, &QPushButton::clicked, this, &MainWindow::onOpen50);

    m_btnAdd5 = new QPushButton("+5%", this);
    m_btnAdd5->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd5);
    connect(m_btnAdd5, &QPushButton::clicked, this, &MainWindow::onAdd5);

    m_btnAdd10 = new QPushButton("+10%", this);
    m_btnAdd10->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd10);
    connect(m_btnAdd10, &QPushButton::clicked, this, &MainWindow::onAdd10);

    m_btnAdd15 = new QPushButton("+15%", this);
    m_btnAdd15->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd15);
    connect(m_btnAdd15, &QPushButton::clicked, this, &MainWindow::onAdd15);

    m_btnAdd20 = new QPushButton("+20%", this);
    m_btnAdd20->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd20);
    connect(m_btnAdd20, &QPushButton::clicked, this, &MainWindow::onAdd20);

    m_btnAdd25 = new QPushButton("+25%", this);
    m_btnAdd25->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd25);
    connect(m_btnAdd25, &QPushButton::clicked, this, &MainWindow::onAdd25);

    m_btnAdd30 = new QPushButton("+30%", this);
    m_btnAdd30->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd30);
    connect(m_btnAdd30, &QPushButton::clicked, this, &MainWindow::onAdd30);

    m_btnAdd35 = new QPushButton("+35%", this);
    m_btnAdd35->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd35);
    connect(m_btnAdd35, &QPushButton::clicked, this, &MainWindow::onAdd35);

    m_btnAdd40 = new QPushButton("+40%", this);
    m_btnAdd40->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd40);
    connect(m_btnAdd40, &QPushButton::clicked, this, &MainWindow::onAdd40);

    m_btnAdd45 = new QPushButton("+45%", this);
    m_btnAdd45->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd45);
    connect(m_btnAdd45, &QPushButton::clicked, this, &MainWindow::onAdd45);

    m_btnAdd50 = new QPushButton("+50%", this);
    m_btnAdd50->setStyleSheet("QPushButton { padding: 4px 4px 4px 4px; }");
    m_toolbar->addWidget(m_btnAdd50);
    connect(m_btnAdd50, &QPushButton::clicked, this, &MainWindow::onAdd50);

    m_toolbar->addSeparator();

    m_btnCancel = new QPushButton("Cancel All Orders", this);
    m_toolbar->addWidget(m_btnCancel);
    connect(m_btnCancel, &QPushButton::clicked, this, &MainWindow::onCancelOrders);

    m_toolbar->addSeparator();

    QLabel* closeLabel = new QLabel(" Close: ", this);
    m_toolbar->addWidget(closeLabel);

    m_btnClose25 = new QPushButton("25%", this);
    m_toolbar->addWidget(m_btnClose25);
    connect(m_btnClose25, &QPushButton::clicked, this, &MainWindow::onClose25);

    m_btnClose50 = new QPushButton("50%", this);
    m_toolbar->addWidget(m_btnClose50);
    connect(m_btnClose50, &QPushButton::clicked, this, &MainWindow::onClose50);

    m_btnClose75 = new QPushButton("75%", this);
    m_toolbar->addWidget(m_btnClose75);
    connect(m_btnClose75, &QPushButton::clicked, this, &MainWindow::onClose75);

    m_btnClose100 = new QPushButton("100%", this);
    m_btnClose100->setMinimumWidth(60);
    m_toolbar->addWidget(m_btnClose100);
    connect(m_btnClose100, &QPushButton::clicked, this, &MainWindow::onClose100);

    // Right: Spacer + Settings button
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_settingsButton = new QPushButton("Settings", this);
    m_toolbar->addWidget(m_settingsButton);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
}

void MainWindow::setupPanels()
{
    m_tickerList = new TickerListWidget(this);
    m_chart = new ChartWidget(this);
    m_chart->setTickerDataManager(m_tickerDataManager);
    m_orderHistory = new OrderHistoryWidget(this);

    // Bottom-right splitter: chart | order history (horizontal split)
    m_rightBottomSplitter = new QSplitter(Qt::Horizontal, this);
    m_rightBottomSplitter->addWidget(m_chart);
    m_rightBottomSplitter->addWidget(m_orderHistory);
    m_rightBottomSplitter->setStretchFactor(0, 2);  // Chart - wider
    m_rightBottomSplitter->setStretchFactor(1, 1);  // Orders - narrower

    // Right splitter: toolbar / (chart + orders) (vertical split)
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->addWidget(m_toolbar);
    m_rightSplitter->addWidget(m_rightBottomSplitter);
    m_rightSplitter->setStretchFactor(0, 0);  // Toolbar - fixed height
    m_rightSplitter->setStretchFactor(1, 1);  // Bottom - fills remaining space

    // Main splitter: ticker list | everything else (horizontal split)
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_tickerList);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 0);  // Ticker list - narrow
    m_mainSplitter->setStretchFactor(1, 1);  // Right panel - fills remaining space

    setCentralWidget(m_mainSplitter);
}

void MainWindow::setupConnections()
{
    // IBKR Client signals
    connect(m_ibkrClient, &IBKRClient::connected, this, &MainWindow::onConnected);
    connect(m_ibkrClient, &IBKRClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_ibkrClient, &IBKRClient::error, this, &MainWindow::onError);
    connect(m_ibkrClient, &IBKRClient::activeAccountChanged, m_orderHistory, &OrderHistoryWidget::setAccount);
    connect(m_ibkrClient, &IBKRClient::accountUpdated, this, [this](const QString& key, const QString& value, const QString& currency, const QString& account) {
        // Update balance when NetLiquidation value is received
        if (key == "NetLiquidation" && account == m_ibkrClient->activeAccount()) {
            m_orderHistory->setBalance(value.toDouble());
        }
    });
    connect(m_ibkrClient, &IBKRClient::positionUpdated, this, [this](const QString& account, const QString& symbol, double position, double avgCost, double marketPrice, double unrealizedPNL) {
        m_orderHistory->updatePosition(symbol, position, avgCost, marketPrice, unrealizedPNL);
    });
    connect(m_ibkrClient, &IBKRClient::orderFilled, this, [this](int orderId, const QString& symbol, const QString& side, double fillPrice, int fillQuantity) {
        m_orderHistory->updatePositionQuantityAfterFill(symbol, side, fillQuantity);
    });

    // Handle completed orders from TWS (historical orders with orderId=0)
    connect(m_ibkrClient, &IBKRClient::orderConfirmed, this, [this](int orderId, const QString& symbol, const QString& action, int quantity, double price, long long permId) {
        // Only process historical orders (orderId=0) here
        // Orders from TradingManager are handled via orderPlaced signal
        if (orderId == 0) {
            TradeOrder order;
            // Generate unique negative ID for historical orders (since TWS gives them orderId=0)
            order.orderId = m_nextHistoricalOrderId--;  // -1, -2, -3, ...
            order.symbol = symbol;
            order.action = (action == "BUY") ? OrderAction::Buy : OrderAction::Sell;
            order.quantity = quantity;
            order.price = price;
            order.fillPrice = price; // Completed orders are already filled
            // Cancelled orders have quantity=0
            order.status = (quantity > 0) ? OrderStatus::Filled : OrderStatus::Cancelled;
            order.timestamp = QDateTime(); // Empty timestamp for historical orders
            order.fillTime = QDateTime(); // Empty fill time for historical orders
            order.commission = 0.0; // Commission not available in current API
            order.permId = permId; // For sorting
            order.sortOrder = m_historicalOrderCounter++; // Counter: first order = 1, second = 2, etc.

            m_orderHistory->addOrder(order);
        }
    });

    // Symbol search
    connect(m_symbolSearch, &SymbolSearchDialog::symbolSelected, this, &MainWindow::onSymbolSelected);

    // Ticker list (exchange already stored when symbol was added)
    connect(m_tickerList, &TickerListWidget::symbolSelected, this, [this](const QString& symbol) {
        // Retrieve stored exchange or use empty string
        QString exchange = m_symbolToExchange.value(symbol, QString());
        onSymbolSelected(symbol, exchange);
    });
    connect(m_tickerList, &TickerListWidget::tickerLabelClicked, this, &MainWindow::onSymbolSearchRequested);

    // Splitter state saving
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);
    connect(m_rightSplitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);
    connect(m_rightBottomSplitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);

    // Price updates (use marketDataUpdated for free data)
    connect(m_ibkrClient, &IBKRClient::marketDataUpdated, this, &MainWindow::onTickByTickUpdated);

    // Chart updates from TickerDataManager (dynamic candle and bars)
    connect(m_tickerDataManager, &TickerDataManager::currentBarUpdated, this, [this](const QString& symbol, const CandleBar& bar) {
        if (symbol == m_currentSymbol) {
            m_chart->updateCurrentBar(bar);
        }
    });

    // Trading Manager -> Order History
    connect(m_tradingManager, &TradingManager::orderPlaced, m_orderHistory, &OrderHistoryWidget::addOrder);
    connect(m_tradingManager, &TradingManager::orderUpdated, m_orderHistory, &OrderHistoryWidget::updateOrder);
    connect(m_tradingManager, &TradingManager::orderCancelled, m_orderHistory, &OrderHistoryWidget::removeOrder);
    // Position updates come directly from IBKRClient now (line 395)

    // Trading Manager warnings and errors
    connect(m_tradingManager, &TradingManager::warning, this, [this](const QString& message) {
        showToast(message, "warning");
    });
    connect(m_tradingManager, &TradingManager::error, this, [this](const QString& message) {
        showToast(message, "error");
    });

    // Global hotkey manager
    connect(m_globalHotkeyManager, &GlobalHotkeyManager::hotkeyPressed, this, [this](GlobalHotkeyManager::HotkeyAction action) {
        switch (action) {
            case GlobalHotkeyManager::Open100:
                onOpen100();
                break;
            case GlobalHotkeyManager::Open50:
                onOpen50();
                break;
            case GlobalHotkeyManager::Add5:
                onAdd5();
                break;
            case GlobalHotkeyManager::Add10:
                onAdd10();
                break;
            case GlobalHotkeyManager::Add15:
                onAdd15();
                break;
            case GlobalHotkeyManager::Add20:
                onAdd20();
                break;
            case GlobalHotkeyManager::Add25:
                onAdd25();
                break;
            case GlobalHotkeyManager::Add30:
                onAdd30();
                break;
            case GlobalHotkeyManager::Add35:
                onAdd35();
                break;
            case GlobalHotkeyManager::Add40:
                onAdd40();
                break;
            case GlobalHotkeyManager::Add45:
                onAdd45();
                break;
            case GlobalHotkeyManager::Add50:
                onAdd50();
                break;
            case GlobalHotkeyManager::Close25:
                onClose25();
                break;
            case GlobalHotkeyManager::Close50:
                onClose50();
                break;
            case GlobalHotkeyManager::Close75:
                onClose75();
                break;
            case GlobalHotkeyManager::Close100:
                onClose100();
                break;
            case GlobalHotkeyManager::CancelOrders:
                onCancelOrders();
                break;
        }
    });

    // Error handling for market data subscription issues
    connect(m_ibkrClient, &IBKRClient::error, this, [this](int id, int code, const QString& message) {
        // Market data subscription errors: 10089, 10168, 354, 10197, 10167, 162
        if (code == 10089 || code == 10168 || code == 354 || code == 10197 || code == 10167 || code == 162) {
            LOG_DEBUG(QString("Market data subscription error: id=%1, code=%2, pending=%3")
                .arg(id).arg(code).arg(m_pendingSymbol));

            // Check if we have a pending symbol (just added, waiting for data)
            if (!m_pendingSymbol.isEmpty()) {
                // Find ticker ID for pending symbol
                if (m_symbolToTickerId.contains(m_pendingSymbol)) {
                    int tickerId = m_symbolToTickerId[m_pendingSymbol];
                    onMarketDataError(tickerId);
                    m_pendingSymbol.clear();
                }
            }
            // Or check if this error is for a ticker we're tracking (legacy check)
            else if (m_tickerIdToSymbol.contains(id)) {
                onMarketDataError(id);
            }
        }
    });
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Only handle non-trading hotkeys here (trading hotkeys are now global)
    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
            case Qt::Key_K:
                onSymbolSearchRequested();
                return;
        }
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::onSymbolSearchRequested()
{
    m_symbolSearch->show();
    m_symbolSearch->raise();
    m_symbolSearch->activateWindow();
}

void MainWindow::onSymbolSelected(const QString& symbol, const QString& exchange)
{
    // Store exchange for this symbol
    if (!exchange.isEmpty()) {
        m_symbolToExchange[symbol] = exchange;
    }

    // Get or create ticker ID for this symbol
    int tickerId;

    if (m_symbolToTickerId.contains(symbol)) {
        // Existing symbol - just switch UI, market data already running
        tickerId = m_symbolToTickerId[symbol];

        m_currentSymbol = symbol;
        m_tickerDataManager->setCurrentSymbol(symbol);  // Track current symbol for background loading
        m_tickerList->setTickerLabel(symbol);
        m_tickerList->setCurrentSymbol(symbol);
        m_chart->setSymbol(symbol);
        m_tradingManager->setSymbol(symbol);
        m_tradingManager->resetTickLogging(tickerId); // Reset tick logging when switching back to symbol
        m_orderHistory->setCurrentSymbol(symbol);

        // Update TradingManager with exchange info
        if (m_symbolToExchange.contains(symbol)) {
            m_tradingManager->setSymbolExchange(symbol, m_symbolToExchange[symbol]);
        }

        // Note: Don't restart market data - it's already running for this symbol
    } else {
        // New symbol - mark as pending and wait for data confirmation
        // Don't cancel old market data yet - wait until new symbol is confirmed
        tickerId = m_nextTickerId++;
        m_symbolToTickerId[symbol] = tickerId;
        m_tickerIdToSymbol[tickerId] = symbol;
        m_pendingSymbol = symbol;
        m_previousSymbol = m_currentSymbol;  // Save current symbol

        // Add to ticker data manager (will load historical bars)
        m_tickerDataManager->addTicker(symbol);

        // Request market data for new symbol
        m_ibkrClient->requestMarketData(tickerId, symbol);
    }
}

void MainWindow::onSettingsClicked()
{
    m_settingsDialog->exec();
}

void MainWindow::onResetSession()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Reset Session",
        "Are you sure you want to reset the session? This will close all positions and orders.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // TODO: Implement session reset
        m_tradingManager->cancelAllOrders();
        m_currentSymbol.clear();
        m_tickerList->setTickerLabel("N/A");
        m_tickerList->clear();
        m_orderHistory->clear();
    }
}

void MainWindow::onQuit()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Quit",
        "Are you sure you want to quit? All positions and orders will be closed.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // TODO: Close all positions and orders, then quit
        m_tradingManager->cancelAllOrders();
        QApplication::quit();
    }
}

void MainWindow::onDebugLogs()
{
    DebugLogDialog *dialog = new DebugLogDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::onConnected()
{
    LOG_INFO("Connected to TWS");
    showToast("Connected to TWS", "success");
    enableTrading(true);
}

void MainWindow::onDisconnected()
{
    // Don't log here - already logged in IBKRClient
    showToast("Disconnected from TWS. Reconnecting...", "error");
    enableTrading(false);
    m_orderHistory->setBalance(0.0);
}

void MainWindow::onError(int id, int code, const QString& message)
{
    // Note: All errors are already logged in IBKRWrapper, so we don't log here to avoid duplicates

    // Filter out informational TWS status messages (not actual errors)
    // 1100: Connectivity between IB and TWS lost (handled by reconnect)
    // 1300: TWS socket port reset - relogin (handled by reconnect)
    // 2104-2110: Market data farm connection status
    // 2158: Sec-def data farm connection status
    if (code == 1100 || code == 1300) {
        return; // Connection lost - handled by auto-reconnect
    }
    if (code >= 2104 && code <= 2110) {
        return; // Ignore market data farm status messages
    }
    if (code == 2158) {
        return; // Ignore sec-def data farm status messages
    }

    // Market data subscription errors - handled by onMarketDataError
    if (code == 10089 || code == 10168 || code == 354 || code == 10197 || code == 10167 || code == 162) {
        return; // Handled separately with custom message
    }

    // Error 300: Can't find EId - happens when trying to cancel failed market data
    // Error 322: Duplicate ticker id - happens when trying to restart already active market data
    if (code == 300 || code == 322) {
        return; // Side effect errors, not informative
    }

    // Only show toast for actual errors
    showToast(QString("Error %1: %2").arg(code).arg(message), "error");
}

void MainWindow::onOpen100()
{
    m_tradingManager->openPosition(100);
}

void MainWindow::onOpen50()
{
    m_tradingManager->openPosition(50);
}

void MainWindow::onAdd5()
{
    m_tradingManager->addToPosition(5);
}

void MainWindow::onAdd10()
{
    m_tradingManager->addToPosition(10);
}

void MainWindow::onAdd15()
{
    m_tradingManager->addToPosition(15);
}

void MainWindow::onAdd20()
{
    m_tradingManager->addToPosition(20);
}

void MainWindow::onAdd25()
{
    m_tradingManager->addToPosition(25);
}

void MainWindow::onAdd30()
{
    m_tradingManager->addToPosition(30);
}

void MainWindow::onAdd35()
{
    m_tradingManager->addToPosition(35);
}

void MainWindow::onAdd40()
{
    m_tradingManager->addToPosition(40);
}

void MainWindow::onAdd45()
{
    m_tradingManager->addToPosition(45);
}

void MainWindow::onAdd50()
{
    m_tradingManager->addToPosition(50);
}

void MainWindow::onClose25()
{
    m_tradingManager->closePosition(25);
}

void MainWindow::onClose50()
{
    m_tradingManager->closePosition(50);
}

void MainWindow::onClose75()
{
    m_tradingManager->closePosition(75);
}

void MainWindow::onClose100()
{
    m_tradingManager->closePosition(100);
}

void MainWindow::onCancelOrders()
{
    m_tradingManager->cancelAllOrders();
}

void MainWindow::onToggleShowCancelledAndZeroPositions(bool checked)
{
    Settings::instance().setShowCancelledOrders(checked);
    Settings::instance().save();
    m_orderHistory->setShowCancelledAndZeroPositions(checked);
}

void MainWindow::onSplitterMoved()
{
    // Save splitter states whenever they are moved
    UIState& uiState = UIState::instance();
    uiState.saveSplitterSizes("main", m_mainSplitter->sizes());
    uiState.saveSplitterSizes("right", m_rightSplitter->sizes());
    uiState.saveSplitterSizes("right_bottom", m_rightBottomSplitter->sizes());
}

void MainWindow::showToast(const QString& message, const QString& type)
{
    ToastNotification::Type toastType = ToastNotification::Info;
    if (type == "warning") {
        toastType = ToastNotification::Warning;
    } else if (type == "error") {
        toastType = ToastNotification::Error;
    }
    ToastNotification::show(this, message, toastType);
}

void MainWindow::enableTrading(bool enabled)
{
    m_btnOpen100->setEnabled(enabled);
    m_btnOpen50->setEnabled(enabled);
    m_btnAdd5->setEnabled(enabled);
    m_btnAdd10->setEnabled(enabled);
    m_btnAdd15->setEnabled(enabled);
    m_btnAdd20->setEnabled(enabled);
    m_btnAdd25->setEnabled(enabled);
    m_btnAdd30->setEnabled(enabled);
    m_btnAdd35->setEnabled(enabled);
    m_btnAdd40->setEnabled(enabled);
    m_btnAdd45->setEnabled(enabled);
    m_btnAdd50->setEnabled(enabled);
    m_btnCancel->setEnabled(enabled);
    m_btnClose25->setEnabled(enabled);
    m_btnClose50->setEnabled(enabled);
    m_btnClose75->setEnabled(enabled);
    m_btnClose100->setEnabled(enabled);
}

void MainWindow::restoreUIState()
{
    UIState& uiState = UIState::instance();

    // Restore window geometry
    bool isMaximized;
    QString screenName;
    QRect savedGeometry = uiState.restoreWindowGeometry(isMaximized, screenName);

    // Check if saved screen is still available
    QScreen *targetScreen = nullptr;
    for (QScreen *screen : QApplication::screens()) {
        if (screen->name() == screenName) {
            targetScreen = screen;
            break;
        }
    }

    // If screen not found, use primary screen
    if (!targetScreen) {
        targetScreen = QApplication::primaryScreen();
    }

    // Ensure window is visible on target screen
    QRect screenGeometry = targetScreen->availableGeometry();
    if (!screenGeometry.intersects(savedGeometry)) {
        // Window is not visible on any screen, center it on primary screen
        savedGeometry.moveCenter(screenGeometry.center());
    }

    setGeometry(savedGeometry);

    if (isMaximized) {
        showMaximized();
    }

    // Restore splitter states
    QList<int> mainSizes = uiState.restoreSplitterSizes("main");
    if (!mainSizes.isEmpty()) {
        m_mainSplitter->setSizes(mainSizes);
    } else {
        // Default: ticker list 100px, rest takes remaining space
        m_mainSplitter->setSizes(QList<int>() << 100 << 1300);
    }

    QList<int> rightSizes = uiState.restoreSplitterSizes("right");
    if (!rightSizes.isEmpty()) {
        m_rightSplitter->setSizes(rightSizes);
    } else {
        // Default: toolbar 46px (fixed), rest takes remaining space
        m_rightSplitter->setSizes(QList<int>() << 46 << 754);
    }

    QList<int> rightBottomSizes = uiState.restoreSplitterSizes("right_bottom");
    if (!rightBottomSizes.isEmpty()) {
        m_rightBottomSplitter->setSizes(rightBottomSizes);
    } else {
        // Default: chart takes remaining space, orders 480px
        m_rightBottomSplitter->setSizes(QList<int>() << 920 << 480);
    }
}

void MainWindow::saveUIState()
{
    UIState& uiState = UIState::instance();

    // Save window geometry
    QRect geometry = this->geometry();
    bool isMaximized = this->isMaximized();
    QString screenName = "";

    QScreen *screen = this->screen();
    if (screen) {
        screenName = screen->name();
    }

    uiState.saveWindowGeometry(geometry, isMaximized, screenName);

    // Save all splitter states
    uiState.saveSplitterSizes("main", m_mainSplitter->sizes());
    uiState.saveSplitterSizes("right", m_rightSplitter->sizes());
    uiState.saveSplitterSizes("right_bottom", m_rightBottomSplitter->sizes());
}

void MainWindow::onMarketDataError(int tickerId)
{
    if (!m_tickerIdToSymbol.contains(tickerId)) {
        return;
    }

    QString failedSymbol = m_tickerIdToSymbol[tickerId];

    LOG_DEBUG(QString("Market data error for ticker %1 (id=%2)").arg(failedSymbol).arg(tickerId));

    // Remove failed ticker from all tracking structures
    m_tickerIdToSymbol.remove(tickerId);
    m_symbolToTickerId.remove(failedSymbol);

    // Note: Don't call cancelMarketData - TWS never created this ticker ID due to subscription error

    // Check if this was a pending symbol (new symbol waiting for data)
    if (failedSymbol == m_pendingSymbol) {
        // Simply clear pending - don't add to UI at all
        m_pendingSymbol.clear();

        // Current symbol stays unchanged (no rollback needed since we never switched)
        // Show error message to user
        showToast(QString("No market data subscription for %1. Subscribe in IBKR Account Management.").arg(failedSymbol), "error");
    }
    // If it's current symbol that already exists (shouldn't happen normally)
    else if (m_currentSymbol == failedSymbol) {
        m_tickerList->removeSymbol(failedSymbol);

        if (!m_previousSymbol.isEmpty()) {
            // Restore previous symbol
            m_currentSymbol = m_previousSymbol;
            m_tickerList->setTickerLabel(m_previousSymbol);
            m_tickerList->setCurrentSymbol(m_previousSymbol);
            m_chart->setSymbol(m_previousSymbol);
            m_tradingManager->setSymbol(m_previousSymbol);
            m_orderHistory->setCurrentSymbol(m_previousSymbol);
        } else {
            m_currentSymbol.clear();
            m_tickerList->setTickerLabel("N/A");
        }

        showToast(QString("Lost market data for %1").arg(failedSymbol), "error");
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveUIState();
    QMainWindow::closeEvent(event);
}

void MainWindow::onTickByTickUpdated(int reqId, double price, double bidPrice, double askPrice)
{
    // Find which symbol this reqId belongs to
    if (!m_tickerIdToSymbol.contains(reqId)) {
        LOG_DEBUG(QString("onTickByTickUpdated: Unknown reqId=%1").arg(reqId));
        return;
    }

    QString symbol = m_tickerIdToSymbol[reqId];

    // Check if this is first successful data for pending symbol
    if (symbol == m_pendingSymbol) {
        // Data confirmed! Switch to new symbol
        // NOTE: Keep old market data active - allows fast switching between multiple tickers

        // Add to UI and make current
        m_currentSymbol = symbol;
        m_tickerDataManager->setCurrentSymbol(symbol);  // Track current symbol for background loading
        m_tickerList->setTickerLabel(symbol);
        m_tickerList->addSymbol(symbol);
        m_tickerList->setCurrentSymbol(symbol);
        m_chart->setSymbol(symbol);
        m_tradingManager->setSymbol(symbol);
        m_tradingManager->resetTickLogging(reqId); // Reset tick logging for new symbol
        m_orderHistory->setCurrentSymbol(symbol);

        // Update TradingManager with exchange info
        if (m_symbolToExchange.contains(symbol)) {
            m_tradingManager->setSymbolExchange(symbol, m_symbolToExchange[symbol]);
        }

        m_pendingSymbol.clear();
        m_previousSymbol.clear();
    }

    // Use last trade price (includes pre/post market), fallback to mid-price
    double displayPrice = 0.0;
    if (price > 0) {
        displayPrice = price;
    } else if (bidPrice > 0 && askPrice > 0) {
        displayPrice = (bidPrice + askPrice) / 2.0;
    } else {
        // No valid price data
        LOG_DEBUG(QString("onTickByTickUpdated: No valid price data for %1").arg(symbol));
        return;
    }

    // Initialize 10-second-ago price if this is first update
    if (!m_tenSecondAgoPrices.contains(symbol)) {
        m_tenSecondAgoPrices[symbol] = displayPrice;
    }

    // Store the current price
    m_lastPrices[symbol] = displayPrice;

    // Update OrderHistoryWidget with current price for PnL calculation
    m_orderHistory->updateCurrentPrice(symbol, displayPrice);

    // Update price lines if this is the current symbol
    if (symbol == m_currentSymbol) {
        double mid = (bidPrice > 0 && askPrice > 0) ? (bidPrice + askPrice) / 2.0 : displayPrice;
        double bid = bidPrice > 0 ? bidPrice : displayPrice;
        double ask = askPrice > 0 ? askPrice : displayPrice;
        m_chart->updatePriceLines(bid, ask, mid);
    }

    // Calculate change percent from 10 seconds ago (use bars from TickerDataManager)
    double changePercent = 0.0;
    const QVector<CandleBar>* bars = m_tickerDataManager->getBars(symbol, m_tickerDataManager->currentTimeframe());
    if (bars && !bars->isEmpty()) {
        // Compare current price with price from 10 seconds ago (1 bar ago)
        if (bars->size() >= 2) {
            double oldPrice = (*bars)[bars->size() - 2].close;
            if (oldPrice > 0) {
                changePercent = ((displayPrice - oldPrice) / oldPrice) * 100.0;
            }
        }
    } else if (m_tenSecondAgoPrices.contains(symbol)) {
        // Fallback to old method if bars not loaded yet
        double oldPrice = m_tenSecondAgoPrices[symbol];
        if (oldPrice > 0) {
            changePercent = ((displayPrice - oldPrice) / oldPrice) * 100.0;
        }
    }

    // Update the ticker list widget
    m_tickerList->updateTickerPrice(symbol, displayPrice, changePercent);
}

void MainWindow::updateInactiveTickers()
{
    // Every 10 seconds:
    // 1. Update the "10 seconds ago" price snapshot
    // 2. Request snapshot data for inactive tickers

    // Update the 10-second-ago snapshot for all tickers
    m_tenSecondAgoPrices = m_lastPrices;

    // TODO: For inactive tickers, we could request snapshot market data
    // For now, they will only update when they become active
    // In future, we can use reqMktData with snapshot=true for inactive tickers
}
