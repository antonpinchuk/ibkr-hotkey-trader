#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QSplitter>
#include <QTimer>
#include <QMap>

class IBKRClient;
class TradingManager;
class TickerListWidget;
class ChartWidget;
class OrderHistoryWidget;
class SettingsDialog;
class SymbolSearchDialog;
class ToastNotification;
class DebugLogDialog;
class TickerDataManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSymbolSearchRequested();
    void onSymbolSelected(const QString& symbol, const QString& exchange = QString());
    void onSettingsClicked();
    void onResetSession();
    void onQuit();
    void onDebugLogs();

    void onConnected();
    void onDisconnected();
    void onError(int id, int code, const QString& message);

    // Trading button slots
    void onOpen100();
    void onOpen50();
    void onAdd5();
    void onAdd10();
    void onAdd15();
    void onAdd20();
    void onAdd25();
    void onAdd30();
    void onAdd35();
    void onAdd40();
    void onAdd45();
    void onAdd50();
    void onClose25();
    void onClose50();
    void onClose75();
    void onClose100();
    void onCancelOrders();
    void onToggleShowCancelledAndZeroPositions(bool checked);
    void onSplitterMoved();

    // Price update slots
    void onTickByTickUpdated(int reqId, double price, double bidPrice, double askPrice);
    void updateInactiveTickers();

    // Error handling
    void onMarketDataError(int tickerId);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolbar();
    void setupPanels();
    void setupConnections();

    void restoreUIState();
    void saveUIState();

    void showToast(const QString& message, const QString& type = "warning");
    void enableTrading(bool enabled);

    // UI Components
    QLabel *m_tickerLabel;
    QPushButton *m_settingsButton;

    // Trading buttons
    QPushButton *m_btnOpen100;
    QPushButton *m_btnOpen50;
    QPushButton *m_btnAdd5;
    QPushButton *m_btnAdd10;
    QPushButton *m_btnAdd15;
    QPushButton *m_btnAdd20;
    QPushButton *m_btnAdd25;
    QPushButton *m_btnAdd30;
    QPushButton *m_btnAdd35;
    QPushButton *m_btnAdd40;
    QPushButton *m_btnAdd45;
    QPushButton *m_btnAdd50;
    QPushButton *m_btnCancel;
    QPushButton *m_btnClose25;
    QPushButton *m_btnClose50;
    QPushButton *m_btnClose75;
    QPushButton *m_btnClose100;

    // Menu actions
    QAction *m_showCancelledOrdersAction;

    QToolBar *m_toolbar;
    QSplitter *m_mainSplitter;        // Left: ticker list, Right: everything else
    QSplitter *m_rightSplitter;       // Top: toolbar, Bottom: chart + orders
    QSplitter *m_rightBottomSplitter; // Left: chart, Right: orders

    // Widgets
    TickerListWidget *m_tickerList;
    ChartWidget *m_chart;
    OrderHistoryWidget *m_orderHistory;

    // Dialogs
    SettingsDialog *m_settingsDialog;
    SymbolSearchDialog *m_symbolSearch;

    // Business logic
    IBKRClient *m_ibkrClient;
    TradingManager *m_tradingManager;
    TickerDataManager *m_tickerDataManager;

    QString m_currentSymbol;
    QString m_previousSymbol;  // Track previous symbol for rollback on error
    QString m_pendingSymbol;   // Symbol waiting for market data confirmation

    // Price tracking
    QTimer *m_inactiveTickerTimer;
    QMap<QString, double> m_lastPrices;          // symbol -> last price
    QMap<QString, double> m_tenSecondAgoPrices;  // symbol -> price 10 seconds ago

    // Ticker ID management
    int m_nextTickerId;
    QMap<QString, int> m_symbolToTickerId;       // symbol -> tickerId
    QMap<int, QString> m_tickerIdToSymbol;       // tickerId -> symbol
    QMap<QString, QString> m_symbolToExchange;   // symbol -> primaryExchange
};

#endif // MAINWINDOW_H