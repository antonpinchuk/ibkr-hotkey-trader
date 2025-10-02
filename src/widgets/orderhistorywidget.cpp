#include "widgets/orderhistorywidget.h"
#include "models/uistate.h"
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
    m_currentTable->setHorizontalHeaderLabels({"Status", "Symbol", "Qty", "Open $", "Close $", "PnL $", "PnL %", "Time"});
    setupTableColumns(m_currentTable);
    restoreColumnWidths(m_currentTable, "order_history_current");
    m_tabWidget->addTab(m_currentTable, "Current");

    m_allTable = new QTableWidget(0, 8, this);
    m_allTable->setHorizontalHeaderLabels({"Status", "Symbol", "Qty", "Open $", "Close $", "PnL $", "PnL %", "Time"});
    setupTableColumns(m_allTable);
    restoreColumnWidths(m_allTable, "order_history_all");
    m_tabWidget->addTab(m_allTable, "All");

    connectColumnResizeSignals();

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

    m_account = new QLabel("Account: N/A", this);
    m_totalBalance = new QLabel("Balance: $0.00", this);
    m_totalPnL = new QLabel("PnL: $0.00", this);
    m_totalPnLPercent = new QLabel("PnL: 0.00%", this);
    m_numTrades = new QLabel("Trades: 0", this);
    m_winRate = new QLabel("Win Rate: 0.00%", this);
    m_largestWin = new QLabel("Largest Win: $0.00", this);
    m_largestLoss = new QLabel("Largest Loss: $0.00", this);

    // Left column
    statsLayout->addWidget(m_account, 0, 0);
    statsLayout->addWidget(m_totalBalance, 1, 0);
    statsLayout->addWidget(m_totalPnL, 2, 0);
    statsLayout->addWidget(m_totalPnLPercent, 3, 0);

    // Right column
    statsLayout->addWidget(m_numTrades, 0, 1);
    statsLayout->addWidget(m_winRate, 1, 1);
    statsLayout->addWidget(m_largestWin, 2, 1);
    statsLayout->addWidget(m_largestLoss, 3, 1);

    mainLayout->addWidget(statsWidget);
}

void OrderHistoryWidget::clear()
{
    m_currentTable->setRowCount(0);
    m_allTable->setRowCount(0);
}

void OrderHistoryWidget::setupTableColumns(QTableWidget *table)
{
    QHeaderView *header = table->horizontalHeader();

    // Make all columns narrower (half default size)
    for (int i = 0; i < table->columnCount(); ++i) {
        header->resizeSection(i, 60); // Default narrower width
    }

    // Sort by Time column (last column, index 7) descending
    table->sortItems(7, Qt::DescendingOrder);

    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::Interactive);
}

void OrderHistoryWidget::restoreColumnWidths(QTableWidget *table, const QString& tableName)
{
    UIState& uiState = UIState::instance();
    QList<int> widths = uiState.restoreTableColumnWidths(tableName);

    if (!widths.isEmpty() && widths.size() == table->columnCount()) {
        QHeaderView *header = table->horizontalHeader();
        for (int i = 0; i < widths.size(); ++i) {
            header->resizeSection(i, widths[i]);
        }
    }
}

void OrderHistoryWidget::connectColumnResizeSignals()
{
    connect(m_currentTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, &OrderHistoryWidget::saveColumnWidths);
    connect(m_allTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, &OrderHistoryWidget::saveColumnWidths);
}

void OrderHistoryWidget::saveColumnWidths()
{
    UIState& uiState = UIState::instance();

    // Save current table widths
    QList<int> currentWidths;
    for (int i = 0; i < m_currentTable->columnCount(); ++i) {
        currentWidths << m_currentTable->columnWidth(i);
    }
    uiState.saveTableColumnWidths("order_history_current", currentWidths);

    // Save all table widths
    QList<int> allWidths;
    for (int i = 0; i < m_allTable->columnCount(); ++i) {
        allWidths << m_allTable->columnWidth(i);
    }
    uiState.saveTableColumnWidths("order_history_all", allWidths);
}

void OrderHistoryWidget::setAccount(const QString& account)
{
    m_account->setText("Account: " + account);
}

void OrderHistoryWidget::setBalance(double balance)
{
    m_totalBalance->setText(QString("Balance: $%1").arg(balance, 0, 'f', 2));
}