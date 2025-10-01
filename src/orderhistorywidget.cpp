#include "orderhistorywidget.h"
#include <QVBoxLayout>
#include <QHeaderView>

OrderHistoryWidget::OrderHistoryWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget(this);

    m_currentTable = new QTableWidget(0, 8, this);
    m_currentTable->setHorizontalHeaderLabels({"Time", "Ticker", "Qty", "Open $", "Close $", "PnL $", "PnL %", "Status"});
    m_currentTable->horizontalHeader()->setStretchLastSection(true);
    m_tabWidget->addTab(m_currentTable, "Current");

    m_allTable = new QTableWidget(0, 8, this);
    m_allTable->setHorizontalHeaderLabels({"Time", "Ticker", "Qty", "Open $", "Close $", "PnL $", "PnL %", "Status"});
    m_allTable->horizontalHeader()->setStretchLastSection(true);
    m_tabWidget->addTab(m_allTable, "All");

    mainLayout->addWidget(m_tabWidget);

    QWidget *statsWidget = new QWidget(this);
    QVBoxLayout *statsLayout = new QVBoxLayout(statsWidget);

    m_totalBalance = new QLabel("Total Balance: $0.00", this);
    m_totalPnL = new QLabel("Total PnL: $0.00", this);
    m_totalPnLPercent = new QLabel("Total PnL: 0.00%", this);
    m_winRate = new QLabel("Win Rate: 0.00%", this);
    m_numTrades = new QLabel("Trades: 0", this);
    m_largestWin = new QLabel("Largest Win: $0.00", this);
    m_largestLoss = new QLabel("Largest Loss: $0.00", this);

    statsLayout->addWidget(m_totalBalance);
    statsLayout->addWidget(m_totalPnL);
    statsLayout->addWidget(m_totalPnLPercent);
    statsLayout->addWidget(m_winRate);
    statsLayout->addWidget(m_numTrades);
    statsLayout->addWidget(m_largestWin);
    statsLayout->addWidget(m_largestLoss);

    mainLayout->addWidget(statsWidget);
}

void OrderHistoryWidget::clear()
{
    m_currentTable->setRowCount(0);
    m_allTable->setRowCount(0);
}