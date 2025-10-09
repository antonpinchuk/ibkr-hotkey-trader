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
      m_showCancelledAndZeroPositions(false),
      m_currentSymbol("")
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_tabWidget = new QTabWidget(this);

    // Current/All tabs - 8 columns
    m_currentTable = new QTableWidget(0, 8, this);
    m_currentTable->setHorizontalHeaderLabels({"Status", "Symbol", "Action", "Qty", "Price", "Cost", "Comm", "Time"});
    setupTableColumns(m_currentTable);
    restoreColumnWidths(m_currentTable, "order_history_all");  // Use same widths as All table
    m_tabWidget->addTab(m_currentTable, "Current");

    m_allTable = new QTableWidget(0, 8, this);
    m_allTable->setHorizontalHeaderLabels({"Status", "Symbol", "Action", "Qty", "Price", "Cost", "Comm", "Time"});
    setupTableColumns(m_allTable);
    restoreColumnWidths(m_allTable, "order_history_all");
    m_tabWidget->addTab(m_allTable, "All");

    // Portfolio tab - 8 columns (same as before)
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
    m_netLiquidationValue = new QLabel("Net Liquidation: $0.00", this);
    m_pnlUnrealized = new QLabel("PnL Unrealized: $0.00 / 0.00%", this);
    m_pnlTotal = new QLabel("PnL Total: $0.00 / 0.00%", this);
    m_numTrades = new QLabel("Trades: 0", this);
    m_winRate = new QLabel("Winrate: 0.00%", this);
    m_largestWin = new QLabel("Largest Win: $0.00 / 0.00%", this);
    m_largestLoss = new QLabel("Largest Loss: $0.00 / 0.00%", this);

    // Left column
    statsLayout->addWidget(m_account, 0, 0);
    statsLayout->addWidget(m_totalBalance, 1, 0);
    statsLayout->addWidget(m_netLiquidationValue, 2, 0); // Only visible on Portfolio tab
    statsLayout->addWidget(m_pnlUnrealized, 3, 0);
    statsLayout->addWidget(m_pnlTotal, 4, 0);

    // Right column
    statsLayout->addWidget(m_numTrades, 0, 1);
    statsLayout->addWidget(m_winRate, 1, 1);
    statsLayout->addWidget(m_largestWin, 2, 1);
    statsLayout->addWidget(m_largestLoss, 3, 1);

    // Hide Net Liquidation by default (only for Portfolio tab)
    m_netLiquidationValue->hide();

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

    // Set initial column widths for order tables (8 columns)
    if (table->columnCount() == 8) {
        // Order tables: Status, Symbol, Action, Qty, Price, Cost, Comm, Time
        header->resizeSection(0, 40);  // Status - narrower
        header->resizeSection(1, 60);  // Symbol
        header->resizeSection(2, 50);  // Action
        header->resizeSection(3, 60);  // Qty
        header->resizeSection(4, 60);  // Price
        header->resizeSection(5, 60);  // Cost
        header->resizeSection(6, 60);  // Comm
        header->resizeSection(7, 60);  // Time

        // Enable auto-sorting
        table->setSortingEnabled(true);
        table->sortItems(7, Qt::DescendingOrder); // Sort by Time column descending
    } else {
        // Default for other tables
        for (int i = 0; i < table->columnCount(); ++i) {
            header->resizeSection(i, 60);
        }
    }

    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::Interactive);

    // Set header alignment for order tables
    if (table->columnCount() == 8) {
        // Order tables: different alignment per column
        QTableWidgetItem *headerItem;

        // Status - center
        headerItem = new QTableWidgetItem("Status");
        headerItem->setTextAlignment(Qt::AlignCenter);
        table->setHorizontalHeaderItem(0, headerItem);

        // Symbol - left
        headerItem = new QTableWidgetItem("Symbol");
        headerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->setHorizontalHeaderItem(1, headerItem);

        // Action - left
        headerItem = new QTableWidgetItem("Action");
        headerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->setHorizontalHeaderItem(2, headerItem);

        // Qty, Price, Cost, Comm, Time - right
        for (int col = 3; col < 8; ++col) {
            headerItem = table->horizontalHeaderItem(col);
            if (headerItem) {
                headerItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
        }
    } else {
        // Default alignment for other tables
        header->setDefaultAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
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
    // For order tables (Current/All), sync widths when All table is resized
    connect(m_allTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int logicalIndex, int oldSize, int newSize) {
        Q_UNUSED(oldSize);
        // Sync width to Current table
        m_currentTable->horizontalHeader()->resizeSection(logicalIndex, newSize);
        // Save widths
        saveColumnWidths();
    });

    // When Current table is resized, sync to All and save
    connect(m_currentTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int logicalIndex, int oldSize, int newSize) {
        Q_UNUSED(oldSize);
        // Sync width to All table
        m_allTable->horizontalHeader()->resizeSection(logicalIndex, newSize);
        // Save widths
        saveColumnWidths();
    });

    connect(m_positionsTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, &OrderHistoryWidget::saveColumnWidths);
}

void OrderHistoryWidget::saveColumnWidths()
{
    UIState& uiState = UIState::instance();

    // Save All table widths (Current uses the same widths)
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
    // Deduplication logic
    int existingOrderId = -1;

    // If orderId is 0, try to match by symbol+qty or symbol+price
    if (order.orderId == 0) {
        existingOrderId = findOrderByMatch(order);
        if (existingOrderId >= 0) {
            LOG_DEBUG(QString("Matched order with orderId=0 to existing order %1").arg(existingOrderId));
            // Update existing order instead of creating new one
            TradeOrder updatedOrder = order;
            updatedOrder.orderId = existingOrderId; // Use existing order ID
            updateOrder(updatedOrder);
            return;
        }
    } else if (m_orders.contains(order.orderId)) {
        // Order already exists with this ID - update it
        LOG_DEBUG(QString("Order %1 already exists, updating").arg(order.orderId));
        updateOrder(order);
        return;
    }

    // Add new order to memory (always keep in memory)
    m_orders[order.orderId] = order;

    // Rebuild tables to show the new order (respecting filters)
    rebuildTables();
}

void OrderHistoryWidget::updateOrder(const TradeOrder& order)
{
    // Update order in memory (always keep in memory)
    m_orders[order.orderId] = order;

    // Rebuild tables to reflect the update (respecting filters)
    rebuildTables();
}

void OrderHistoryWidget::removeOrder(int orderId)
{
    if (!m_orders.contains(orderId)) {
        return;
    }

    // Remove order from memory completely
    m_orders.remove(orderId);

    // Rebuild tables
    rebuildTables();
}

void OrderHistoryWidget::setShowCancelledAndZeroPositions(bool show)
{
    m_showCancelledAndZeroPositions = show;

    // Rebuild order tables with new filter setting
    rebuildTables();

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

    // Status column (0) - center aligned, store orderId in UserRole
    QString statusText = order.isPending() ? "⏳" : (order.isFilled() ? "✅" : "✖️");
    QTableWidgetItem* statusItem = new QTableWidgetItem(statusText);
    statusItem->setTextAlignment(Qt::AlignCenter);
    statusItem->setData(Qt::UserRole, order.orderId);
    if (order.isCancelled()) {
        statusItem->setForeground(Qt::gray);
    } else if (order.isFilled()) {
        statusItem->setForeground(Qt::black);
    }
    table->setItem(row, 0, statusItem);

    // Symbol (1) - left aligned
    QTableWidgetItem* symbolItem = new QTableWidgetItem(order.symbol);
    symbolItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setItem(row, 1, symbolItem);

    // Action (2) - left aligned, colored
    QString actionText = order.isBuy() ? "Buy" : "Sell";
    QTableWidgetItem* actionItem = new QTableWidgetItem(actionText);
    actionItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (order.isBuy()) {
        actionItem->setForeground(Qt::darkGreen);
    } else {
        actionItem->setForeground(Qt::red);
    }
    table->setItem(row, 2, actionItem);

    // Qty (3) - right aligned
    QTableWidgetItem* qtyItem = new QTableWidgetItem(QString::number(order.quantity));
    qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 3, qtyItem);

    // Price (4) - right aligned, filled price or limit price for pending
    double price = order.isFilled() ? order.fillPrice : order.price;
    QString priceText = QString("$%1").arg(price, 0, 'f', 2);
    QTableWidgetItem* priceItem = new QTableWidgetItem(priceText);
    priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 4, priceItem);

    // Cost (5) - right aligned, Qty × Price
    double cost = order.quantity * price;
    QString costText = QString("$%1").arg(cost, 0, 'f', 2);
    QTableWidgetItem* costItem = new QTableWidgetItem(costText);
    costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 5, costItem);

    // Commission (6) - right aligned
    QString commissionText = order.commission > 0 ? QString("$%1").arg(order.commission, 0, 'f', 2) : "";
    QTableWidgetItem* commItem = new QTableWidgetItem(commissionText);
    commItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 6, commItem);

    // Time (7) - use sortOrder for sorting
    // sortOrder contains: timestamp (msecs) for new orders, counter for historical orders
    // Show time only for orders with valid timestamp
    QString timeText = "";
    if (order.isFilled() && order.fillTime.isValid()) {
        timeText = order.fillTime.toString("hh:mm:ss");
    } else if (order.timestamp.isValid()) {
        timeText = order.timestamp.toString("hh:mm:ss");
    }

    NumericTableWidgetItem* timeItem = new NumericTableWidgetItem(timeText);
    timeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    timeItem->setData(Qt::UserRole, (double)order.sortOrder);
    table->setItem(row, 7, timeItem);

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
    QString statusText = order.isPending() ? "⏳" : (order.isFilled() ? "✅" : "✖️");
    table->item(row, 0)->setText(statusText);
    if (order.isCancelled()) {
        table->item(row, 0)->setForeground(Qt::gray);
    } else if (order.isFilled()) {
        table->item(row, 0)->setForeground(Qt::black);
    }

    // Update Price if filled
    if (order.isFilled()) {
        QString priceText = QString("$%1").arg(order.fillPrice, 0, 'f', 2);
        table->item(row, 4)->setText(priceText);

        // Update Cost
        double cost = order.quantity * order.fillPrice;
        QString costText = QString("$%1").arg(cost, 0, 'f', 2);
        table->item(row, 5)->setText(costText);

        // Update Commission
        QString commissionText = QString("$%1").arg(order.commission, 0, 'f', 2);
        table->item(row, 6)->setText(commissionText);
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
    // TODO: Implement proper statistics calculation based on current tab
    // Current tab: по поточному тікеру
    // All tab: по всім тікерам за день
    // Portfolio tab: з IBKR API updateAccountValue

    // Placeholder values - will be implemented later
    double pnlUnrealized = 0.0;
    double pnlUnrealizedPercent = 0.0;
    double pnlTotal = 0.0;
    double pnlTotalPercent = 0.0;
    int numTrades = 0;
    double winRate = 0.0;
    double largestWin = 0.0;
    double largestWinPercent = 0.0;
    double largestLoss = 0.0;
    double largestLossPercent = 0.0;

    // Update labels with new format
    m_pnlUnrealized->setText(QString("PnL Unrealized: $%1 / %2%")
        .arg(pnlUnrealized, 0, 'f', 2)
        .arg(pnlUnrealizedPercent, 0, 'f', 2));

    m_pnlTotal->setText(QString("PnL Total: $%1 / %2%")
        .arg(pnlTotal, 0, 'f', 2)
        .arg(pnlTotalPercent, 0, 'f', 2));

    m_numTrades->setText(QString("Trades: %1").arg(numTrades));

    m_winRate->setText(QString("Winrate: %1%").arg(winRate, 0, 'f', 2));

    m_largestWin->setText(QString("Largest Win: $%1 / %2%")
        .arg(largestWin, 0, 'f', 2)
        .arg(largestWinPercent, 0, 'f', 2));

    m_largestLoss->setText(QString("Largest Loss: $%1 / %2%")
        .arg(largestLoss, 0, 'f', 2)
        .arg(largestLossPercent, 0, 'f', 2));
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

void OrderHistoryWidget::setCurrentSymbol(const QString& symbol)
{
    m_currentSymbol = symbol;
    // Only rebuild Current table (All table is not affected by symbol change)
    rebuildCurrentTable();
}

void OrderHistoryWidget::rebuildCurrentTable()
{
    // Disable sorting during rebuild for performance
    bool sortingEnabled = m_currentTable->isSortingEnabled();
    m_currentTable->setSortingEnabled(false);

    // Clear table
    m_currentTable->setRowCount(0);
    m_currentTableRows.clear();

    // Add orders that match filters:
    // - symbol == m_currentSymbol
    // - if cancelled, only show if m_showCancelledAndZeroPositions == true
    for (auto it = m_orders.begin(); it != m_orders.end(); ++it) {
        const TradeOrder& order = it.value();

        // Filter by symbol
        if (order.symbol != m_currentSymbol) {
            continue;
        }

        // Filter by cancelled status
        if (order.isCancelled() && !m_showCancelledAndZeroPositions) {
            continue;
        }

        addOrderToTable(m_currentTable, order);
    }

    // Re-enable sorting
    m_currentTable->setSortingEnabled(sortingEnabled);
}

void OrderHistoryWidget::rebuildAllTable()
{
    // Disable sorting during rebuild for performance
    bool sortingEnabled = m_allTable->isSortingEnabled();
    m_allTable->setSortingEnabled(false);

    // Clear table
    m_allTable->setRowCount(0);
    m_allTableRows.clear();

    // Add all orders that match filters:
    // - if cancelled, only show if m_showCancelledAndZeroPositions == true
    for (auto it = m_orders.begin(); it != m_orders.end(); ++it) {
        const TradeOrder& order = it.value();

        // Filter by cancelled status
        if (order.isCancelled() && !m_showCancelledAndZeroPositions) {
            continue;
        }

        addOrderToTable(m_allTable, order);
    }

    // Re-enable sorting
    m_allTable->setSortingEnabled(sortingEnabled);
}

void OrderHistoryWidget::rebuildTables()
{
    rebuildCurrentTable();
    rebuildAllTable();
    updateStatistics();
}

int OrderHistoryWidget::findOrderByMatch(const TradeOrder& order)
{
    // Try to find existing order by matching symbol + (qty or price)
    // This is for orders with orderId=0 from completedOrders
    for (auto it = m_orders.begin(); it != m_orders.end(); ++it) {
        const TradeOrder& existing = it.value();

        // Same symbol and same action (Buy/Sell)
        if (existing.symbol == order.symbol && existing.action == order.action) {
            // Match by quantity (same qty, status PreSubmitted)
            if (existing.quantity == order.quantity && existing.isPending()) {
                return it.key();
            }
            // Match by price (same price, status PreSubmitted)
            if (qAbs(existing.price - order.price) < 0.01 && existing.isPending()) {
                return it.key();
            }
        }
    }

    return -1; // Not found
}

void OrderHistoryWidget::updateCommission(int orderId, double commission)
{
    if (!m_orders.contains(orderId)) {
        LOG_WARNING(QString("Commission report for unknown order: %1").arg(orderId));
        return;
    }

    // Update commission in memory
    TradeOrder& order = m_orders[orderId];
    order.commission = commission;

    // Rebuild tables to show updated commission
    rebuildTables();
}
