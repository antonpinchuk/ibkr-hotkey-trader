#include "mainwindow.h"
#include "ibkrclient.h"
#include "tradingmanager.h"
#include "tickerlistwidget.h"
#include "chartwidget.h"
#include "orderhistorywidget.h"
#include "settingsdialog.h"
#include "symbolsearchdialog.h"
#include "toastnotification.h"
#include "settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QMessageBox>
#include <QFrame>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tickerLabel(nullptr)
    , m_settingsButton(nullptr)
    , m_toolbar(nullptr)
    , m_mainSplitter(nullptr)
    , m_tickerList(nullptr)
    , m_chart(nullptr)
    , m_orderHistory(nullptr)
    , m_settingsDialog(nullptr)
    , m_symbolSearch(nullptr)
    , m_ibkrClient(nullptr)
    , m_tradingManager(nullptr)
{
    setWindowTitle("IBKR Hotkey Trader");
    resize(1400, 800);

    // Initialize components
    m_ibkrClient = new IBKRClient(this);
    m_tradingManager = new TradingManager(m_ibkrClient, this);
    m_settingsDialog = new SettingsDialog(this);
    m_symbolSearch = new SymbolSearchDialog(m_ibkrClient, this);

    setupUI();
    setupConnections();

    // Try to connect to TWS on startup
    Settings& settings = Settings::instance();
    m_ibkrClient->connect(settings.host(), settings.port(), settings.clientId());
}

MainWindow::~MainWindow()
{
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
    ordersMenu->addAction("See hotkeys in Help documentation");

    QMenu *helpMenu = menuBar()->addMenu("Help");
    QAction *helpAction = helpMenu->addAction("Help documentation");
    connect(helpAction, &QAction::triggered, [this]() {
        QMessageBox::information(this, "Help",
            "<h3>Keyboard Shortcuts</h3>"
            "<h4>Opening Positions (Buy)</h4>"
            "<ul>"
            "<li>Cmd+O: Buy 100% of budget</li>"
            "<li>Cmd+P: Buy 50% of budget</li>"
            "<li>Cmd+1-9: Add 5%-45% to position</li>"
            "<li>Cmd+0: Add 50% to position</li>"
            "</ul>"
            "<h4>Closing Positions (Sell)</h4>"
            "<ul>"
            "<li>Cmd+Z: Sell 100% of position</li>"
            "<li>Cmd+X: Sell 75% of position</li>"
            "<li>Cmd+C: Sell 50% of position</li>"
            "<li>Cmd+V: Sell 25% of position</li>"
            "</ul>"
            "<h4>Other Controls</h4>"
            "<ul>"
            "<li>Cmd+K: Open symbol search</li>"
            "<li>Esc: Cancel all pending orders</li>"
            "</ul>");
    });
}

void MainWindow::setupToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, m_toolbar);

    // Left: Ticker label (clickable)
    m_tickerLabel = new QLabel("No Symbol", this);
    m_tickerLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px 10px; }");
    m_tickerLabel->setCursor(Qt::PointingHandCursor);
    m_tickerLabel->installEventFilter(this);
    m_toolbar->addWidget(m_tickerLabel);

    m_toolbar->addSeparator();

    // Center: Trading buttons
    m_btnOpen100 = new QPushButton("Open 100%", this);
    m_btnOpen100->setMinimumWidth(100);
    m_toolbar->addWidget(m_btnOpen100);
    connect(m_btnOpen100, &QPushButton::clicked, this, &MainWindow::onOpen100);

    m_btnOpen50 = new QPushButton("Open 50%", this);
    m_toolbar->addWidget(m_btnOpen50);
    connect(m_btnOpen50, &QPushButton::clicked, this, &MainWindow::onOpen50);

    m_btnAdd5 = new QPushButton("+5%", this);
    m_toolbar->addWidget(m_btnAdd5);
    connect(m_btnAdd5, &QPushButton::clicked, this, &MainWindow::onAdd5);

    m_btnAdd10 = new QPushButton("+10%", this);
    m_toolbar->addWidget(m_btnAdd10);
    connect(m_btnAdd10, &QPushButton::clicked, this, &MainWindow::onAdd10);

    m_btnAdd15 = new QPushButton("+15%", this);
    m_toolbar->addWidget(m_btnAdd15);
    connect(m_btnAdd15, &QPushButton::clicked, this, &MainWindow::onAdd15);

    m_btnAdd20 = new QPushButton("+20%", this);
    m_toolbar->addWidget(m_btnAdd20);
    connect(m_btnAdd20, &QPushButton::clicked, this, &MainWindow::onAdd20);

    m_btnAdd25 = new QPushButton("+25%", this);
    m_toolbar->addWidget(m_btnAdd25);
    connect(m_btnAdd25, &QPushButton::clicked, this, &MainWindow::onAdd25);

    m_btnAdd30 = new QPushButton("+30%", this);
    m_toolbar->addWidget(m_btnAdd30);
    connect(m_btnAdd30, &QPushButton::clicked, this, &MainWindow::onAdd30);

    m_btnAdd35 = new QPushButton("+35%", this);
    m_toolbar->addWidget(m_btnAdd35);
    connect(m_btnAdd35, &QPushButton::clicked, this, &MainWindow::onAdd35);

    m_btnAdd40 = new QPushButton("+40%", this);
    m_toolbar->addWidget(m_btnAdd40);
    connect(m_btnAdd40, &QPushButton::clicked, this, &MainWindow::onAdd40);

    m_btnAdd45 = new QPushButton("+45%", this);
    m_toolbar->addWidget(m_btnAdd45);
    connect(m_btnAdd45, &QPushButton::clicked, this, &MainWindow::onAdd45);

    m_btnAdd50 = new QPushButton("+50%", this);
    m_toolbar->addWidget(m_btnAdd50);
    connect(m_btnAdd50, &QPushButton::clicked, this, &MainWindow::onAdd50);

    m_toolbar->addSeparator();

    m_btnCancel = new QPushButton("Cancel", this);
    m_toolbar->addWidget(m_btnCancel);
    connect(m_btnCancel, &QPushButton::clicked, this, &MainWindow::onCancelOrders);

    m_toolbar->addSeparator();

    m_btnClose25 = new QPushButton("Close 25%", this);
    m_toolbar->addWidget(m_btnClose25);
    connect(m_btnClose25, &QPushButton::clicked, this, &MainWindow::onClose25);

    m_btnClose50 = new QPushButton("Close 50%", this);
    m_toolbar->addWidget(m_btnClose50);
    connect(m_btnClose50, &QPushButton::clicked, this, &MainWindow::onClose50);

    m_btnClose75 = new QPushButton("Close 75%", this);
    m_toolbar->addWidget(m_btnClose75);
    connect(m_btnClose75, &QPushButton::clicked, this, &MainWindow::onClose75);

    m_btnClose100 = new QPushButton("Close 100%", this);
    m_btnClose100->setMinimumWidth(100);
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
    m_orderHistory = new OrderHistoryWidget(this);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_tickerList);
    m_mainSplitter->addWidget(m_chart);
    m_mainSplitter->addWidget(m_orderHistory);
    m_mainSplitter->setStretchFactor(0, 1);  // Left panel - narrow
    m_mainSplitter->setStretchFactor(1, 4);  // Chart - wide
    m_mainSplitter->setStretchFactor(2, 2);  // Right panel - medium

    setCentralWidget(m_mainSplitter);
}

void MainWindow::setupConnections()
{
    // IBKR Client signals
    connect(m_ibkrClient, &IBKRClient::connected, this, &MainWindow::onConnected);
    connect(m_ibkrClient, &IBKRClient::disconnected, this, &MainWindow::onDisconnected);
    connect(m_ibkrClient, &IBKRClient::error, this, &MainWindow::onError);

    // Symbol search
    connect(m_symbolSearch, &SymbolSearchDialog::symbolSelected, this, &MainWindow::onSymbolSelected);

    // Ticker list
    connect(m_tickerList, &TickerListWidget::symbolSelected, this, &MainWindow::onSymbolSelected);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
            case Qt::Key_K:
                onSymbolSearchRequested();
                return;
            case Qt::Key_O:
                onOpen100();
                return;
            case Qt::Key_P:
                onOpen50();
                return;
            case Qt::Key_1:
                onAdd5();
                return;
            case Qt::Key_2:
                onAdd10();
                return;
            case Qt::Key_3:
                onAdd15();
                return;
            case Qt::Key_4:
                onAdd20();
                return;
            case Qt::Key_5:
                onAdd25();
                return;
            case Qt::Key_6:
                onAdd30();
                return;
            case Qt::Key_7:
                onAdd35();
                return;
            case Qt::Key_8:
                onAdd40();
                return;
            case Qt::Key_9:
                onAdd45();
                return;
            case Qt::Key_0:
                onAdd50();
                return;
            case Qt::Key_Z:
                onClose100();
                return;
            case Qt::Key_X:
                onClose75();
                return;
            case Qt::Key_C:
                onClose50();
                return;
            case Qt::Key_V:
                onClose25();
                return;
        }
    } else if (event->key() == Qt::Key_Escape) {
        onCancelOrders();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::onSymbolSearchRequested()
{
    m_symbolSearch->show();
    m_symbolSearch->raise();
    m_symbolSearch->activateWindow();
}

void MainWindow::onSymbolSelected(const QString& symbol)
{
    // TODO: Check if there are open positions before switching
    m_currentSymbol = symbol;
    m_tickerLabel->setText(symbol);

    // Add to ticker list if not already there
    m_tickerList->addSymbol(symbol);

    // Request market data
    m_chart->setSymbol(symbol);
    m_tradingManager->setSymbol(symbol);
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
        m_tradingManager->closeAllPositions();
        m_tradingManager->cancelAllOrders();
        m_currentSymbol.clear();
        m_tickerLabel->setText("No Symbol");
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
        m_tradingManager->closeAllPositions();
        m_tradingManager->cancelAllOrders();
        QApplication::quit();
    }
}

void MainWindow::onConnected()
{
    showToast("Connected to TWS", "success");
    enableTrading(true);
}

void MainWindow::onDisconnected()
{
    showToast("Disconnected from TWS. Reconnecting...", "error");
    enableTrading(false);
}

void MainWindow::onError(int id, int code, const QString& message)
{
    // Filter out informational TWS status messages (not actual errors)
    // 2104-2110: Market data farm connection status
    // 2158: Sec-def data farm connection status
    if (code >= 2104 && code <= 2110) {
        return; // Ignore market data farm status messages
    }
    if (code == 2158) {
        return; // Ignore sec-def data farm status messages
    }

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