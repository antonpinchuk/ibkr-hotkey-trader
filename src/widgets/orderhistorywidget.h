#ifndef ORDERHISTORYWIDGET_H
#define ORDERHISTORYWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>

class OrderHistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OrderHistoryWidget(QWidget *parent = nullptr);
    void clear();
    void saveColumnWidths();
    void setAccount(const QString& account);
    void setBalance(double balance);

private:
    void setupTableColumns(QTableWidget *table);
    void restoreColumnWidths(QTableWidget *table, const QString& tableName);
    void connectColumnResizeSignals();

    QTabWidget *m_tabWidget;
    QTableWidget *m_currentTable;
    QTableWidget *m_allTable;

    QLabel *m_account;
    QLabel *m_totalBalance;
    QLabel *m_totalPnL;
    QLabel *m_totalPnLPercent;
    QLabel *m_winRate;
    QLabel *m_numTrades;
    QLabel *m_largestWin;
    QLabel *m_largestLoss;
};

#endif // ORDERHISTORYWIDGET_H