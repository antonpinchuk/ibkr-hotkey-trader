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
class GlobalHotkeyManager;
class SystemTrayManager;
class RemoteControlServer;

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
    void onSymbolMoveToTop(const QString& symbol);
    void onSymbolDelete(const QString& symbol);
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

    // Ticker activation
    void onTickerActivated(const QString& symbol);
    void onPriceUpdated(const QString& symbol, double price, double changePercent, double bid, double ask, double mid);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolbar();
    void setupPanels();
    void setupConnections();

    void restoreUIState();
    void saveUIState();

    void showToast(const QString& message, const QString& type = "warning");
    void updateTradingButtonsState();

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

    // Trading menu actions
    QAction *m_open100Action;
    QAction *m_open50Action;
    QAction *m_add5Action;
    QAction *m_add10Action;
    QAction *m_add15Action;
    QAction *m_add20Action;
    QAction *m_add25Action;
    QAction *m_add30Action;
    QAction *m_add35Action;
    QAction *m_add40Action;
    QAction *m_add45Action;
    QAction *m_add50Action;
    QAction *m_cancelAction;
    QAction *m_close25Action;
    QAction *m_close50Action;
    QAction *m_close75Action;
    QAction *m_close100Action;

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
    GlobalHotkeyManager *m_globalHotkeyManager;
    SystemTrayManager *m_systemTrayManager;
    RemoteControlServer *m_remoteControlServer;

    QString m_currentSymbol;
    QMap<QString, QString> m_symbolToExchange;   // symbol -> primaryExchange

    // Order sorting and unique IDs
    qint64 m_historicalOrderCounter;             // Counter for historical orders (incremented on each historical order)
    int m_nextHistoricalOrderId;                 // Unique negative ID generator for historical orders (-1, -2, -3, ...)
};

#endif // MAINWINDOW_H