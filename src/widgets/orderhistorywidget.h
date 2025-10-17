#ifndef ORDERHISTORYWIDGET_H
#define ORDERHISTORYWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include "models/order.h"

// Custom QTableWidgetItem that sorts numerically instead of lexicographically
class NumericTableWidgetItem : public QTableWidgetItem
{
public:
    NumericTableWidgetItem(const QString& text) : QTableWidgetItem(text) {}

    bool operator<(const QTableWidgetItem& other) const override {
        return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
    }
};

class OrderHistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OrderHistoryWidget(QWidget *parent = nullptr);
    void clear();
    void saveColumnWidths();
    void setAccount(const QString& account);
    void setBalance(double balance);
    void setShowCancelledAndZeroPositions(bool show);
    void setCurrentSymbol(const QString& symbol); // Set current symbol for Current tab filtering

    // Data access for trading button state logic
    double getCurrentPrice(const QString& symbol) const;
    double getCurrentPosition(const QString& symbol) const; // Returns quantity (positive for long, negative for short)
    double getAvgCost(const QString& symbol) const;
    double getBalance() const;
    void resetPrice(const QString& symbol); // Reset price to 0 when switching tickers

public slots:
    void addOrder(const TradeOrder& order);
    void updateOrder(const TradeOrder& order);
    void updateCommission(int orderId, double commission);
    void removeOrder(int orderId);
    void updateCurrentPrice(const QString& symbol, double price);
    void updatePosition(const QString& symbol, double quantity, double avgCost, double marketPrice, double unrealizedPNL);
    void updatePositionQuantityAfterFill(const QString& symbol, const QString& side, int fillQuantity); // Fast update after order fill

private:
    void setupTableColumns(QTableWidget *table);
    void restoreColumnWidths(QTableWidget *table, const QString& tableName);
    void connectColumnResizeSignals();
    void updateStatistics();
    int findOrderRow(QTableWidget* table, int orderId);
    int findOrderByMatch(const TradeOrder& order); // Find order by symbol+qty or symbol+price
    void addOrderToTable(QTableWidget* table, const TradeOrder& order);
    void updateOrderInTable(QTableWidget* table, int row, const TradeOrder& order);
    double calculatePnL(const TradeOrder& buyOrder, const TradeOrder& sellOrder);
    void rebuildCurrentTable(); // Rebuild Current tab based on current symbol
    void rebuildAllTable(); // Rebuild All tab based on filters
    void rebuildTables(); // Rebuild both tables

    QTabWidget *m_tabWidget;
    QTableWidget *m_currentTable;
    QTableWidget *m_allTable;
    QTableWidget *m_positionsTable;

    QLabel *m_account;
    QLabel *m_totalBalance;
    QLabel *m_netLiquidationValue; // Only for Portfolio tab
    QLabel *m_pnlUnrealized;
    QLabel *m_pnlTotal;
    QLabel *m_winRate;
    QLabel *m_numTrades;
    QLabel *m_largestWin;
    QLabel *m_largestLoss;

    // Track orders for statistics and PnL calculation
    QMap<int, TradeOrder> m_orders;
    QMap<QString, double> m_currentPrices; // symbol -> current price
    QMap<int, int> m_currentTableRows; // orderId -> row
    QMap<int, int> m_allTableRows; // orderId -> row

    // Track positions
    struct Position {
        QString symbol;
        double quantity = 0.0;
        double avgCost = 0.0;
        double currentPrice = 0.0;
    };
    QMap<QString, Position> m_positions; // symbol -> Position
    QMutex m_positionsMutex; // Thread-safe access to positions

    bool m_showCancelledAndZeroPositions;
    QString m_currentSymbol; // Current symbol for filtering Current tab
    double m_balance; // Account balance
};

#endif // ORDERHISTORYWIDGET_H