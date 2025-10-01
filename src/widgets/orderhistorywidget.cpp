#include "widgets/orderhistorywidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFrame>

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

    // Statistics panel with separator
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    QWidget *statsWidget = new QWidget(this);
    QGridLayout *statsLayout = new QGridLayout(statsWidget);
    statsLayout->setContentsMargins(10, 5, 10, 20);
    statsLayout->setSpacing(5);

    m_totalBalance = new QLabel("Balance: $0.00", this);
    m_totalPnL = new QLabel("PnL: $0.00", this);
    m_totalPnLPercent = new QLabel("PnL: 0.00%", this);
    m_winRate = new QLabel("Win Rate: 0.00%", this);
    m_numTrades = new QLabel("Trades: 0", this);
    m_largestWin = new QLabel("Largest Win: $0.00", this);
    m_largestLoss = new QLabel("Largest Loss: $0.00", this);

    // Left column
    statsLayout->addWidget(m_totalBalance, 0, 0);
    statsLayout->addWidget(m_totalPnL, 1, 0);
    statsLayout->addWidget(m_totalPnLPercent, 2, 0);
    statsLayout->addWidget(m_numTrades, 3, 0);

    // Right column
    statsLayout->addWidget(m_winRate, 0, 1);
    statsLayout->addWidget(m_largestWin, 1, 1);
    statsLayout->addWidget(m_largestLoss, 2, 1);

    mainLayout->addWidget(statsWidget);
}

void OrderHistoryWidget::clear()
{
    m_currentTable->setRowCount(0);
    m_allTable->setRowCount(0);
}