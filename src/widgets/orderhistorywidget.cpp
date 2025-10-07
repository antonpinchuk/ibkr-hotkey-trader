#include "widgets/orderhistorywidget.h"
#include "models/uistate.h"
#include "models/settings.h"
#include "utils/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFrame>

OrderHistoryWidget::OrderHistoryWidget(QWidget *parent)
    : QWidget(parent),
      m_showCancelledAndZeroPositions(false)
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

    m_positionsTable = new QTableWidget(0, 8, this);
    m_positionsTable->setHorizontalHeaderLabels({"Symbol", "Qty", "Avg", "Cost", "Last", "Value", "P&L", "P&L %"});
    setupTableColumns(m_positionsTable);
    restoreColumnWidths(m_positionsTable, "positions");
    m_tabWidget->addTab(m_positionsTable, "Portfolio");

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
    m_positionsTable->setRowCount(0);
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

    // Right-align header labels
    header->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
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
    connect(m_positionsTable->horizontalHeader(), &QHeaderView::sectionResized,
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

    // Save positions table widths
    QList<int> positionsWidths;
    for (int i = 0; i < m_positionsTable->columnCount(); ++i) {
        positionsWidths << m_positionsTable->columnWidth(i);
    }
    uiState.saveTableColumnWidths("positions", positionsWidths);
}

void OrderHistoryWidget::setAccount(const QString& account)
{
    m_account->setText("Account: " + account);
}

void OrderHistoryWidget::setBalance(double balance)
{
    m_totalBalance->setText(QString("Balance: $%1").arg(balance, 0, 'f', 2));
}

void OrderHistoryWidget::addOrder(const TradeOrder& order)
{
    m_orders[order.orderId] = order;

    // Add to both tables
    addOrderToTable(m_currentTable, order);
    addOrderToTable(m_allTable, order);

    updateStatistics();
}

void OrderHistoryWidget::updateOrder(const TradeOrder& order)
{
    m_orders[order.orderId] = order;

    // Update in current table
    int currentRow = findOrderRow(m_currentTable, order.orderId);
    if (currentRow >= 0) {
        updateOrderInTable(m_currentTable, currentRow, order);

        // If filled or cancelled, remove from current table
        if (order.isFilled() || order.isCancelled()) {
            m_currentTable->removeRow(currentRow);
            m_currentTableRows.remove(order.orderId);
        }
    }

    // Update in all table
    int allRow = findOrderRow(m_allTable, order.orderId);
    if (allRow >= 0) {
        updateOrderInTable(m_allTable, allRow, order);
    }

    updateStatistics();
}

void OrderHistoryWidget::removeOrder(int orderId)
{
    if (!m_orders.contains(orderId)) {
        return;
    }

    TradeOrder& order = m_orders[orderId];

    // If showing cancelled orders, just update the order status instead of removing
    if (m_showCancelledAndZeroPositions && order.isCancelled()) {
        updateOrder(order);
        return;
    }

    // Remove cancelled orders from both tables (default behavior)
    int currentRow = findOrderRow(m_currentTable, orderId);
    if (currentRow >= 0) {
        m_currentTable->removeRow(currentRow);
        m_currentTableRows.remove(orderId);
    }

    int allRow = findOrderRow(m_allTable, orderId);
    if (allRow >= 0) {
        m_allTable->removeRow(allRow);
        m_allTableRows.remove(orderId);
    }

    m_orders.remove(orderId);
    updateStatistics();
}

void OrderHistoryWidget::setShowCancelledAndZeroPositions(bool show)
{
    m_showCancelledAndZeroPositions = show;

    // Rebuild orders tables based on cancelled orders setting
    if (show) {
        // Show all orders including cancelled
        // No need to rebuild - cancelled orders are already in m_orders
    } else {
        // Hide cancelled orders - remove them from tables
        QList<int> cancelledOrderIds;
        for (auto it = m_orders.begin(); it != m_orders.end(); ++it) {
            if (it.value().isCancelled()) {
                cancelledOrderIds.append(it.key());
            }
        }

        for (int orderId : cancelledOrderIds) {
            // Remove from tables
            int currentRow = findOrderRow(m_currentTable, orderId);
            if (currentRow >= 0) {
                m_currentTable->removeRow(currentRow);
                m_currentTableRows.remove(orderId);
            }

            int allRow = findOrderRow(m_allTable, orderId);
            if (allRow >= 0) {
                m_allTable->removeRow(allRow);
                m_allTableRows.remove(orderId);
            }

            m_orders.remove(orderId);
        }

        updateStatistics();
    }

    // Rebuild positions table based on zero positions setting
    // Build list of zero positions to show/hide
    QList<Position> zeroPositions;
    {
        QMutexLocker locker(&m_positionsMutex);
        for (auto it = m_positions.begin(); it != m_positions.end(); ++it) {
            if (it.value().quantity <= 0) {
                zeroPositions.append(it.value());
            }
        }
    }

    if (show) {
        // Show zero positions - add them to table if not already there
        for (const Position& pos : zeroPositions) {
            bool found = false;
            for (int i = 0; i < m_positionsTable->rowCount(); ++i) {
                QTableWidgetItem* item = m_positionsTable->item(i, 0);
                if (item && item->text() == pos.symbol) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                updatePosition(pos.symbol, pos.quantity, pos.avgCost, pos.currentPrice, 0);
            }
        }
    } else {
        // Hide zero positions - remove from table (keep in m_positions)
        for (const Position& pos : zeroPositions) {
            for (int i = 0; i < m_positionsTable->rowCount(); ++i) {
                QTableWidgetItem* item = m_positionsTable->item(i, 0);
                if (item && item->text() == pos.symbol) {
                    m_positionsTable->removeRow(i);
                    break;
                }
            }
        }
    }
}

void OrderHistoryWidget::updateCurrentPrice(const QString& symbol, double price)
{
    // Just update cache - positions are updated via updatePortfolio() callback
    m_currentPrices[symbol] = price;
}

int OrderHistoryWidget::findOrderRow(QTableWidget* table, int orderId)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* item = table->item(row, 0); // Status column has orderId in data
        if (item && item->data(Qt::UserRole).toInt() == orderId) {
            return row;
        }
    }
    return -1;
}

void OrderHistoryWidget::addOrderToTable(QTableWidget* table, const TradeOrder& order)
{
    int row = table->rowCount();
    table->insertRow(row);

    // Status column (0) - store orderId in UserRole
    QString statusText = order.isPending() ? "⏳" : (order.isFilled() ? "✓" : "❌");
    QTableWidgetItem* statusItem = new QTableWidgetItem(statusText);
    statusItem->setData(Qt::UserRole, order.orderId);
    if (order.isFilled()) {
        statusItem->setForeground(Qt::black);
    }
    table->setItem(row, 0, statusItem);

    // Symbol (1)
    table->setItem(row, 1, new QTableWidgetItem(order.symbol));

    // Quantity (2)
    QString qtyText = QString::number(order.quantity);
    if (order.isSell()) {
        qtyText = "-" + qtyText;
    }
    table->setItem(row, 2, new QTableWidgetItem(qtyText));

    // Open $ (3)
    double openPrice = order.isBuy() ? (order.isFilled() ? order.fillPrice : order.price) : 0.0;
    QString openText = openPrice > 0 ? QString("$%1").arg(openPrice, 0, 'f', 2) : "-";
    table->setItem(row, 3, new QTableWidgetItem(openText));

    // Close $ (4)
    double closePrice = order.isSell() ? (order.isFilled() ? order.fillPrice : order.price) : 0.0;
    QString closeText = closePrice > 0 ? QString("$%1").arg(closePrice, 0, 'f', 2) : "-";
    table->setItem(row, 4, new QTableWidgetItem(closeText));

    // PnL $ (5) - calculated later
    table->setItem(row, 5, new QTableWidgetItem("-"));

    // PnL % (6) - calculated later
    table->setItem(row, 6, new QTableWidgetItem("-"));

    // Time (7)
    QString timeText = order.timestamp.toString("hh:mm:ss");
    table->setItem(row, 7, new QTableWidgetItem(timeText));

    // Track row
    if (table == m_currentTable) {
        m_currentTableRows[order.orderId] = row;
    } else {
        m_allTableRows[order.orderId] = row;
    }
}

void OrderHistoryWidget::updateOrderInTable(QTableWidget* table, int row, const TradeOrder& order)
{
    // Update status
    QString statusText = order.isPending() ? "⏳" : (order.isFilled() ? "✓" : "❌");
    table->item(row, 0)->setText(statusText);
    if (order.isFilled()) {
        table->item(row, 0)->setForeground(Qt::black);
    }

    // Update Open $
    if (order.isBuy() && order.isFilled()) {
        QString openText = QString("$%1").arg(order.fillPrice, 0, 'f', 2);
        table->item(row, 3)->setText(openText);
    }

    // Update Close $
    if (order.isSell() && order.isFilled()) {
        QString closeText = QString("$%1").arg(order.fillPrice, 0, 'f', 2);
        table->item(row, 4)->setText(closeText);
    }
}

double OrderHistoryWidget::calculatePnL(const TradeOrder& buyOrder, const TradeOrder& sellOrder)
{
    if (!buyOrder.isFilled() || !sellOrder.isFilled()) {
        return 0.0;
    }

    int quantity = qMin(buyOrder.quantity, sellOrder.quantity);
    double pnl = (sellOrder.fillPrice - buyOrder.fillPrice) * quantity;
    return pnl;
}

void OrderHistoryWidget::updateStatistics()
{
    double totalPnL = 0.0;
    int numTrades = 0;
    int numWins = 0;
    double largestWin = 0.0;
    double largestLoss = 0.0;

    // Calculate statistics from filled orders
    QList<TradeOrder> buyOrders;
    QList<TradeOrder> sellOrders;

    for (const TradeOrder& order : m_orders) {
        if (order.isFilled()) {
            if (order.isBuy()) {
                buyOrders.append(order);
            } else if (order.isSell()) {
                sellOrders.append(order);
            }
        }
    }

    // Match buy/sell orders (simplified - assumes same symbol and FIFO)
    for (const TradeOrder& sellOrder : sellOrders) {
        for (const TradeOrder& buyOrder : buyOrders) {
            if (buyOrder.symbol == sellOrder.symbol && buyOrder.isFilled()) {
                double pnl = calculatePnL(buyOrder, sellOrder);
                totalPnL += pnl;
                numTrades++;

                if (pnl > 0) {
                    numWins++;
                    if (pnl > largestWin) largestWin = pnl;
                } else {
                    if (pnl < largestLoss) largestLoss = pnl;
                }
                break;
            }
        }
    }

    // Update labels - simple text without styling to avoid Qt crashes
    m_totalPnL->setText(QString("PnL: $%1").arg(totalPnL, 0, 'f', 2));

    double winRate = numTrades > 0 ? (numWins * 100.0 / numTrades) : 0.0;
    m_winRate->setText(QString("Win Rate: %1%").arg(winRate, 0, 'f', 2));

    m_numTrades->setText(QString("Trades: %1").arg(numTrades));
    m_largestWin->setText(QString("Largest Win: $%1").arg(largestWin, 0, 'f', 2));
    m_largestLoss->setText(QString("Largest Loss: $%1").arg(largestLoss, 0, 'f', 2));
}
void OrderHistoryWidget::updatePositionQuantityAfterFill(const QString& symbol, const QString& side, int fillQuantity)
{
    // Fast update after order fill - update quantity immediately in memory
    // side: "BOT" (bought) or "SLD" (sold)
    QMutexLocker locker(&m_positionsMutex);

    Position& pos = m_positions[symbol];

    if (side == "BOT") {
        pos.quantity += fillQuantity;
    } else if (side == "SLD") {
        pos.quantity -= fillQuantity;
    }

    // Note: avgCost and PNL will be updated later by updatePortfolio() from TWS
}

void OrderHistoryWidget::updatePosition(const QString& symbol, double quantity, double avgCost, double marketPrice, double unrealizedPNL)
{
    QMutexLocker locker(&m_positionsMutex);

    // Update current price cache
    m_currentPrices[symbol] = marketPrice;

    // Store position data (always keep in memory)
    Position& pos = m_positions[symbol];
    pos.symbol = symbol;
    pos.quantity = quantity;
    pos.avgCost = avgCost;
    pos.currentPrice = marketPrice;

    // Find existing row by searching for symbol (don't use cached row index - it changes after sorting)
    int row = -1;
    for (int i = 0; i < m_positionsTable->rowCount(); ++i) {
        QTableWidgetItem* item = m_positionsTable->item(i, 0); // Symbol column
        if (item && item->text() == symbol) {
            row = i;
            break;
        }
    }

    // Handle zero positions based on show setting
    if (quantity <= 0 && !m_showCancelledAndZeroPositions) {
        // Hide zero positions - remove from table if exists
        if (row >= 0) {
            m_positionsTable->removeRow(row);
        }
        return;
    }

    // Add new row if position not in table
    if (row < 0) {
        row = m_positionsTable->rowCount();
        m_positionsTable->insertRow(row);
    }

    // Update cells: Symbol, Qty, Avg, Cost, Last, Value, P&L, P&L %
    double costBasis = quantity * avgCost;
    double value = quantity * marketPrice;
    double pnlPercent = avgCost > 0 ? (unrealizedPNL / costBasis * 100.0) : 0.0;

    // Symbol (left-aligned)
    QTableWidgetItem* symbolItem = new QTableWidgetItem(symbol);
    m_positionsTable->setItem(row, 0, symbolItem);

    // Qty (right-aligned, numeric sorting)
    NumericTableWidgetItem* qtyItem = new NumericTableWidgetItem(QString::number(quantity, 'f', 0));
    qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    qtyItem->setData(Qt::UserRole, quantity);
    m_positionsTable->setItem(row, 1, qtyItem);

    // Avg (right-aligned, numeric sorting)
    NumericTableWidgetItem* avgItem = new NumericTableWidgetItem(QString("$%1").arg(avgCost, 0, 'f', 2));
    avgItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    avgItem->setData(Qt::UserRole, avgCost);
    m_positionsTable->setItem(row, 2, avgItem);

    // Cost (right-aligned, dark gray, numeric sorting)
    NumericTableWidgetItem* costItem = new NumericTableWidgetItem(QString("$%1").arg(costBasis, 0, 'f', 2));
    costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    costItem->setForeground(Qt::darkGray);
    costItem->setData(Qt::UserRole, costBasis);
    m_positionsTable->setItem(row, 3, costItem);

    // Last (right-aligned, numeric sorting)
    NumericTableWidgetItem* lastItem = new NumericTableWidgetItem(QString("$%1").arg(marketPrice, 0, 'f', 2));
    lastItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lastItem->setData(Qt::UserRole, marketPrice);
    m_positionsTable->setItem(row, 4, lastItem);

    // Value (right-aligned, dark gray, numeric sorting)
    NumericTableWidgetItem* valueItem = new NumericTableWidgetItem(QString("$%1").arg(value, 0, 'f', 2));
    valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueItem->setForeground(Qt::darkGray);
    valueItem->setData(Qt::UserRole, value);
    m_positionsTable->setItem(row, 5, valueItem);

    // P&L (right-aligned, colored, numeric sorting)
    NumericTableWidgetItem* pnlItem = new NumericTableWidgetItem(QString("$%1").arg(unrealizedPNL, 0, 'f', 2));
    pnlItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pnlItem->setData(Qt::UserRole, unrealizedPNL);
    if (unrealizedPNL >= 0) {
        pnlItem->setForeground(Qt::darkGreen);
    } else {
        pnlItem->setForeground(Qt::red);
    }
    m_positionsTable->setItem(row, 6, pnlItem);

    // P&L % (right-aligned, colored, numeric sorting)
    NumericTableWidgetItem* pnlPercentItem = new NumericTableWidgetItem(QString("%1%").arg(pnlPercent, 0, 'f', 2));
    pnlPercentItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pnlPercentItem->setData(Qt::UserRole, pnlPercent); // Store numeric value for sorting
    if (pnlPercent >= 0) {
        pnlPercentItem->setForeground(Qt::darkGreen);
    } else {
        pnlPercentItem->setForeground(Qt::red);
    }
    m_positionsTable->setItem(row, 7, pnlPercentItem);

    // Enable sorting for user interaction - will sort by P&L % by default
    if (!m_positionsTable->isSortingEnabled()) {
        m_positionsTable->setSortingEnabled(true);
        m_positionsTable->sortItems(7, Qt::DescendingOrder); // Initial sort by P&L %
    }
}
