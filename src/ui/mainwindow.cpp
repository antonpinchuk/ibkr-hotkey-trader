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
#include "utils/systemtraymanager.h"
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
    m_systemTrayManager = new SystemTrayManager(this);

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
    updateTradingButtonsState();
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
    m_open100Action = ordersMenu->addAction("Open 100%");
    m_open100Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+O"));
    connect(m_open100Action, &QAction::triggered, this, &MainWindow::onOpen100);

    m_open50Action = ordersMenu->addAction("Open 50%");
    m_open50Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+P"));
    connect(m_open50Action, &QAction::triggered, this, &MainWindow::onOpen50);

    ordersMenu->addSeparator();

    // Adding to position
    m_add5Action = ordersMenu->addAction("Add 5%");
    m_add5Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+1"));
    connect(m_add5Action, &QAction::triggered, this, &MainWindow::onAdd5);

    m_add10Action = ordersMenu->addAction("Add 10%");
    m_add10Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+2"));
    connect(m_add10Action, &QAction::triggered, this, &MainWindow::onAdd10);

    m_add15Action = ordersMenu->addAction("Add 15%");
    m_add15Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+3"));
    connect(m_add15Action, &QAction::triggered, this, &MainWindow::onAdd15);

    m_add20Action = ordersMenu->addAction("Add 20%");
    m_add20Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+4"));
    connect(m_add20Action, &QAction::triggered, this, &MainWindow::onAdd20);

    m_add25Action = ordersMenu->addAction("Add 25%");
    m_add25Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+5"));
    connect(m_add25Action, &QAction::triggered, this, &MainWindow::onAdd25);

    m_add30Action = ordersMenu->addAction("Add 30%");
    m_add30Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+6"));
    connect(m_add30Action, &QAction::triggered, this, &MainWindow::onAdd30);

    m_add35Action = ordersMenu->addAction("Add 35%");
    m_add35Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+7"));
    connect(m_add35Action, &QAction::triggered, this, &MainWindow::onAdd35);

    m_add40Action = ordersMenu->addAction("Add 40%");
    m_add40Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+8"));
    connect(m_add40Action, &QAction::triggered, this, &MainWindow::onAdd40);

    m_add45Action = ordersMenu->addAction("Add 45%");
    m_add45Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+9"));
    connect(m_add45Action, &QAction::triggered, this, &MainWindow::onAdd45);

    m_add50Action = ordersMenu->addAction("Add 50%");
    m_add50Action->setShortcut(QKeySequence("Shift+Ctrl+Alt+0"));
    connect(m_add50Action, &QAction::triggered, this, &MainWindow::onAdd50);

    ordersMenu->addSeparator();

    // Cancel orders
    m_cancelAction = ordersMenu->addAction("Cancel All Orders");
    m_cancelAction->setShortcut(QKeySequence("Ctrl+Alt+Q"));
    connect(m_cancelAction, &QAction::triggered, this, &MainWindow::onCancelOrders);

    ordersMenu->addSeparator();

    // Closing positions
    m_close25Action = ordersMenu->addAction("Close 25%");
    m_close25Action->setShortcut(QKeySequence("Ctrl+Alt+V"));
    connect(m_close25Action, &QAction::triggered, this, &MainWindow::onClose25);

    m_close50Action = ordersMenu->addAction("Close 50%");
    m_close50Action->setShortcut(QKeySequence("Ctrl+Alt+C"));
    connect(m_close50Action, &QAction::triggered, this, &MainWindow::onClose50);

    m_close75Action = ordersMenu->addAction("Close 75%");
    m_close75Action->setShortcut(QKeySequence("Ctrl+Alt+X"));
    connect(m_close75Action, &QAction::triggered, this, &MainWindow::onClose75);

    m_close100Action = ordersMenu->addAction("Close 100%");
    m_close100Action->setShortcut(QKeySequence("Ctrl+Alt+Z"));
    connect(m_close100Action, &QAction::triggered, this, &MainWindow::onClose100);

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
            updateTradingButtonsState();
        }
    });
    connect(m_ibkrClient, &IBKRClient::positionUpdated, this, [this](const QString& account, const QString& symbol, double position, double avgCost, double marketPrice, double unrealizedPNL) {
        m_orderHistory->updatePosition(symbol, position, avgCost, marketPrice, unrealizedPNL);
        updateTradingButtonsState();
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

    // Ticker list
    connect(m_tickerList, &TickerListWidget::symbolSelected, this, [this](const QString& symbol) {
        // Retrieve stored exchange from TickerDataManager
        QString exchange = m_tickerDataManager->getExchange(symbol);
        onSymbolSelected(symbol, exchange);
    });
    connect(m_tickerList, &TickerListWidget::tickerLabelClicked, this, &MainWindow::onSymbolSearchRequested);
    connect(m_tickerList, &TickerListWidget::symbolMoveToTopRequested, this, &MainWindow::onSymbolMoveToTop);
    connect(m_tickerList, &TickerListWidget::symbolDeleteRequested, this, &MainWindow::onSymbolDelete);

    // Splitter state saving
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);
    connect(m_rightSplitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);
    connect(m_rightBottomSplitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);

    // TickerDataManager signals
    connect(m_tickerDataManager, &TickerDataManager::tickerActivated, this, &MainWindow::onTickerActivated);
    connect(m_tickerDataManager, &TickerDataManager::priceUpdated, this, &MainWindow::onPriceUpdated);

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

    // System tray: track price updates for blinking
    connect(m_tickerDataManager, &TickerDataManager::noPriceUpdate, this, [this](const QString& symbol) {
        if (symbol == m_currentSymbol) {
            m_systemTrayManager->startBlinking();
        }
    });
    connect(m_tickerDataManager, &TickerDataManager::priceUpdateReceived, this, [this](const QString& symbol) {
        if (symbol == m_currentSymbol) {
            m_systemTrayManager->stopBlinking();
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
    // TickerDataManager handles all the logic: adding ticker, loading data, and subscribing
    m_tickerDataManager->activateTicker(symbol, exchange);
}

void MainWindow::onTickerActivated(const QString& symbol)
{
    // Update all UI components when ticker is activated
    m_currentSymbol = symbol;
    m_tickerList->setTickerLabel(symbol);
    m_tickerList->addSymbol(symbol);
    m_tickerList->setCurrentSymbol(symbol);
    m_chart->setSymbol(symbol);
    m_tradingManager->setSymbol(symbol);
    m_orderHistory->setCurrentSymbol(symbol);
    m_systemTrayManager->setTickerSymbol(symbol);
    m_systemTrayManager->stopBlinking();

    // Reset price to 0 when switching tickers (buttons will be disabled until first price tick)
    m_orderHistory->resetPrice(symbol);

    // Update trading buttons state
    updateTradingButtonsState();

    // Update TradingManager with exchange info
    QString exchange = m_tickerDataManager->getExchange(symbol);
    if (!exchange.isEmpty()) {
        m_tradingManager->setSymbolExchange(symbol, exchange);
    }
}

void MainWindow::onPriceUpdated(const QString& symbol, double price, double changePercent, double bid, double ask, double mid)
{
    // Update ticker list with new price
    m_tickerList->updateTickerPrice(symbol, price, changePercent);

    // Update order history with current price for PnL calculation
    m_orderHistory->updateCurrentPrice(symbol, price);

    // Update price lines on chart (immediate!)
    if (symbol == m_currentSymbol) {
        m_chart->updatePriceLines(bid, ask, mid);
    }

    // Update trading buttons state (price tick event)
    if (symbol == m_currentSymbol) {
        updateTradingButtonsState();
    }
}

void MainWindow::onSymbolMoveToTop(const QString& symbol)
{
    m_tickerList->moveSymbolToTop(symbol);
}

void MainWindow::onSymbolDelete(const QString& symbol)
{
    LOG_DEBUG(QString("Deleting ticker %1").arg(symbol));

    // Remove from ticker data manager (will unsubscribe from all data feeds)
    m_tickerDataManager->removeTicker(symbol);

    // Remove from ticker list widget
    m_tickerList->removeSymbol(symbol);

    // If this was the current symbol, switch to top ticker or reset to initial state
    if (m_currentSymbol == symbol) {
        QString topSymbol = m_tickerList->getTopSymbol();

        if (!topSymbol.isEmpty()) {
            // Switch to top ticker
            QString exchange = m_tickerDataManager->getExchange(topSymbol);
            onSymbolSelected(topSymbol, exchange);
        } else {
            // No tickers left - reset to initial state
            m_currentSymbol.clear();

            // Unsubscribe from market data
            m_tickerDataManager->setCurrentSymbol("");

            // Clear UI
            m_tickerList->setTickerLabel("N/A");
            m_chart->setSymbol("");
            m_chart->clearChart();
            m_tradingManager->setSymbol("");
            m_orderHistory->setCurrentSymbol("");
            m_systemTrayManager->setTickerSymbol("");
            m_systemTrayManager->stopBlinking();

            // Disable trading buttons
            updateTradingButtonsState();
        }
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
    updateTradingButtonsState();
}

void MainWindow::onDisconnected()
{
    // Don't log here - already logged in IBKRClient
    showToast("Disconnected from TWS. Reconnecting...", "error");
    updateTradingButtonsState();
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

void MainWindow::updateTradingButtonsState()
{
    // Check if connected to TWS and have active symbol
    bool isConnected = m_ibkrClient && m_ibkrClient->isConnected();
    bool hasSymbol = !m_currentSymbol.isEmpty();

    // If not connected or no symbol - disable all buttons and menu actions except Cancel
    if (!isConnected || !hasSymbol) {
        // Buttons
        m_btnOpen100->setEnabled(false);
        m_btnOpen50->setEnabled(false);
        m_btnAdd5->setEnabled(false);
        m_btnAdd10->setEnabled(false);
        m_btnAdd15->setEnabled(false);
        m_btnAdd20->setEnabled(false);
        m_btnAdd25->setEnabled(false);
        m_btnAdd30->setEnabled(false);
        m_btnAdd35->setEnabled(false);
        m_btnAdd40->setEnabled(false);
        m_btnAdd45->setEnabled(false);
        m_btnAdd50->setEnabled(false);
        m_btnClose25->setEnabled(false);
        m_btnClose50->setEnabled(false);
        m_btnClose75->setEnabled(false);
        m_btnClose100->setEnabled(false);
        m_btnCancel->setEnabled(true); // Always enabled

        // Menu actions
        m_open100Action->setEnabled(false);
        m_open50Action->setEnabled(false);
        m_add5Action->setEnabled(false);
        m_add10Action->setEnabled(false);
        m_add15Action->setEnabled(false);
        m_add20Action->setEnabled(false);
        m_add25Action->setEnabled(false);
        m_add30Action->setEnabled(false);
        m_add35Action->setEnabled(false);
        m_add40Action->setEnabled(false);
        m_add45Action->setEnabled(false);
        m_add50Action->setEnabled(false);
        m_close25Action->setEnabled(false);
        m_close50Action->setEnabled(false);
        m_close75Action->setEnabled(false);
        m_close100Action->setEnabled(false);
        m_cancelAction->setEnabled(true); // Always enabled
        return;
    }

    // Get current data
    double price = m_orderHistory->getCurrentPrice(m_currentSymbol);
    double balance = m_orderHistory->getBalance();
    double position = m_tradingManager->getCurrentPosition();

    // Basic requirements
    bool hasPrice = (price > 0.0);
    bool hasBalance = (balance > 0.0);
    bool hasPosition = (position > 0.0);
    bool hasNoPosition = (position == 0.0);

    // Open buttons: enabled ONLY if NO position AND has price AND balance
    // (From REQUIREMENTS: "Спрацює тільки якщо немає відкритих позицій")
    bool canOpen = hasNoPosition && hasPrice && hasBalance;
    m_btnOpen100->setEnabled(canOpen);
    m_btnOpen50->setEnabled(canOpen);
    m_open100Action->setEnabled(canOpen);
    m_open50Action->setEnabled(canOpen);

    // Add buttons: enabled if HAS position AND price AND balance AND doesn't exceed 100% budget
    // (From REQUIREMENTS: "Спрацює тільки коли вже є відкриті позиції" + check budget limit)
    bool canAdd5 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(5);
    bool canAdd10 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(10);
    bool canAdd15 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(15);
    bool canAdd20 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(20);
    bool canAdd25 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(25);
    bool canAdd30 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(30);
    bool canAdd35 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(35);
    bool canAdd40 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(40);
    bool canAdd45 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(45);
    bool canAdd50 = hasPosition && hasPrice && hasBalance && m_tradingManager->canAddPercentage(50);

    m_btnAdd5->setEnabled(canAdd5);
    m_btnAdd10->setEnabled(canAdd10);
    m_btnAdd15->setEnabled(canAdd15);
    m_btnAdd20->setEnabled(canAdd20);
    m_btnAdd25->setEnabled(canAdd25);
    m_btnAdd30->setEnabled(canAdd30);
    m_btnAdd35->setEnabled(canAdd35);
    m_btnAdd40->setEnabled(canAdd40);
    m_btnAdd45->setEnabled(canAdd45);
    m_btnAdd50->setEnabled(canAdd50);
    m_add5Action->setEnabled(canAdd5);
    m_add10Action->setEnabled(canAdd10);
    m_add15Action->setEnabled(canAdd15);
    m_add20Action->setEnabled(canAdd20);
    m_add25Action->setEnabled(canAdd25);
    m_add30Action->setEnabled(canAdd30);
    m_add35Action->setEnabled(canAdd35);
    m_add40Action->setEnabled(canAdd40);
    m_add45Action->setEnabled(canAdd45);
    m_add50Action->setEnabled(canAdd50);

    // Close buttons: enabled if has position AND floor(position * %) >= 1
    // (From REQUIREMENTS: "Ті кнопки неактивні що створять округлену заявку менше однієї акції")
    bool canClose25 = hasPosition && m_tradingManager->canClosePercentage(25);
    bool canClose50 = hasPosition && m_tradingManager->canClosePercentage(50);
    bool canClose75 = hasPosition && m_tradingManager->canClosePercentage(75);
    bool canClose100 = hasPosition && m_tradingManager->canClosePercentage(100);

    m_btnClose25->setEnabled(canClose25);
    m_btnClose50->setEnabled(canClose50);
    m_btnClose75->setEnabled(canClose75);
    m_btnClose100->setEnabled(canClose100);
    m_close25Action->setEnabled(canClose25);
    m_close50Action->setEnabled(canClose50);
    m_close75Action->setEnabled(canClose75);
    m_close100Action->setEnabled(canClose100);

    // Cancel: always enabled (will do nothing if no pending orders)
    m_btnCancel->setEnabled(true);
    m_cancelAction->setEnabled(true);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveUIState();
    QMainWindow::closeEvent(event);
}
