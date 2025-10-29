#include "widgets/tickerlistwidget.h"
#include "widgets/tickeritemdelegate.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QMenu>

TickerListWidget::TickerListWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Ticker label at the top
    m_tickerLabel = new QLabel("N/A", this);
    m_tickerLabel->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; padding: 10px; background-color: #F5F5F5; border-bottom: 1px solid #DDD; }");
    m_tickerLabel->setAlignment(Qt::AlignCenter);
    m_tickerLabel->setCursor(Qt::PointingHandCursor);
    m_tickerLabel->setMinimumHeight(46);
    m_tickerLabel->setMaximumHeight(46);
    layout->addWidget(m_tickerLabel);

    // Make label clickable
    m_tickerLabel->installEventFilter(this);

    // Ticker list
    m_listWidget = new QListWidget(this);
    m_listWidget->setSpacing(0);  // No spacing between items
    m_listWidget->setItemDelegate(new TickerItemDelegate(this));
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::currentItemChanged,
            this, &TickerListWidget::onCurrentItemChanged);

    // Install event filter on list widget to handle right-click
    m_listWidget->viewport()->installEventFilter(this);
}

void TickerListWidget::addSymbol(const QString& symbol, const QString& exchange)
{
    // Create unique key: symbol@exchange
    QString tickerKey = exchange.isEmpty() ? symbol : QString("%1@%2").arg(symbol).arg(exchange);

    // Check if this symbol@exchange combination already exists
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString itemSymbol = item->data(TickerItemDelegate::SymbolRole).toString();
        QString itemExchange = item->data(TickerItemDelegate::ExchangeRole).toString();
        QString itemKey = itemExchange.isEmpty() ? itemSymbol : QString("%1@%2").arg(itemSymbol).arg(itemExchange);

        if (itemKey == tickerKey) {
            // Already in list, nothing to do
            return;
        }
    }

    // Create new item
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(TickerItemDelegate::SymbolRole, symbol);
    item->setData(TickerItemDelegate::ExchangeRole, exchange);
    item->setData(TickerItemDelegate::PriceRole, 0.0);
    item->setData(TickerItemDelegate::ChangePercentRole, 0.0);
    item->setData(TickerItemDelegate::IsCurrentRole, false);
    m_listWidget->insertItem(0, item);
}

void TickerListWidget::removeSymbol(const QString& symbol, const QString& exchange)
{
    // Create unique key
    QString tickerKey = exchange.isEmpty() ? symbol : QString("%1@%2").arg(symbol).arg(exchange);

    // Find and remove item with this symbol@exchange
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString itemSymbol = item->data(TickerItemDelegate::SymbolRole).toString();
        QString itemExchange = item->data(TickerItemDelegate::ExchangeRole).toString();
        QString itemKey = itemExchange.isEmpty() ? itemSymbol : QString("%1@%2").arg(itemSymbol).arg(itemExchange);

        if (itemKey == tickerKey) {
            delete m_listWidget->takeItem(i);
            break;
        }
    }
}

void TickerListWidget::setCurrentSymbol(const QString& symbol, const QString& exchange)
{
    // Create unique key
    QString tickerKey = exchange.isEmpty() ? symbol : QString("%1@%2").arg(symbol).arg(exchange);
    m_currentSymbol = tickerKey;

    // Block signals to prevent recursion when programmatically setting current item
    m_listWidget->blockSignals(true);

    // Update all items to reflect current symbol and select the matching one
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString itemSymbol = item->data(TickerItemDelegate::SymbolRole).toString();
        QString itemExchange = item->data(TickerItemDelegate::ExchangeRole).toString();
        QString itemKey = itemExchange.isEmpty() ? itemSymbol : QString("%1@%2").arg(itemSymbol).arg(itemExchange);

        bool isCurrent = (itemKey == m_currentSymbol);
        item->setData(TickerItemDelegate::IsCurrentRole, isCurrent);

        // Select the current symbol in the list
        if (isCurrent) {
            m_listWidget->setCurrentItem(item);
        }
    }

    m_listWidget->blockSignals(false);

    // Trigger repaint
    m_listWidget->viewport()->update();
}

void TickerListWidget::setTickerLabel(const QString& symbol)
{
    m_tickerLabel->setText(symbol);
}

void TickerListWidget::updateTickerPrice(const QString& symbol, const QString& exchange, double price, double changePercent)
{
    // Create unique key
    QString tickerKey = exchange.isEmpty() ? symbol : QString("%1@%2").arg(symbol).arg(exchange);

    // Find the item with this symbol@exchange and update its price and change percent
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString itemSymbol = item->data(TickerItemDelegate::SymbolRole).toString();
        QString itemExchange = item->data(TickerItemDelegate::ExchangeRole).toString();
        QString itemKey = itemExchange.isEmpty() ? itemSymbol : QString("%1@%2").arg(itemSymbol).arg(itemExchange);

        if (itemKey == tickerKey) {
            item->setData(TickerItemDelegate::PriceRole, price);
            item->setData(TickerItemDelegate::ChangePercentRole, changePercent);
            // Trigger repaint
            m_listWidget->viewport()->update();
            break;
        }
    }
}

void TickerListWidget::clear()
{
    m_listWidget->clear();
}

void TickerListWidget::moveSymbolToTop(const QString& symbol, const QString& exchange)
{
    // Create unique key
    QString tickerKey = exchange.isEmpty() ? symbol : QString("%1@%2").arg(symbol).arg(exchange);

    // Find the item with this symbol@exchange
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString itemSymbol = item->data(TickerItemDelegate::SymbolRole).toString();
        QString itemExchange = item->data(TickerItemDelegate::ExchangeRole).toString();
        QString itemKey = itemExchange.isEmpty() ? itemSymbol : QString("%1@%2").arg(itemSymbol).arg(itemExchange);

        if (itemKey == tickerKey) {
            // Only move if not already at top
            if (i > 0) {
                bool wasCurrent = (tickerKey == m_currentSymbol);

                m_listWidget->blockSignals(true);
                m_listWidget->takeItem(i);
                m_listWidget->insertItem(0, item);

                // If it was the current item, keep it selected at new position
                if (wasCurrent) {
                    m_listWidget->setCurrentItem(item);
                }

                m_listWidget->blockSignals(false);
                m_listWidget->viewport()->update();
            }
            break;
        }
    }
}

QString TickerListWidget::getTopSymbol() const
{
    if (m_listWidget->count() > 0) {
        QListWidgetItem *item = m_listWidget->item(0);
        return item->data(TickerItemDelegate::SymbolRole).toString();
    }
    return QString();
}

QStringList TickerListWidget::getAllSymbols() const
{
    QStringList symbols;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        symbols.append(item->data(TickerItemDelegate::SymbolRole).toString());
    }
    return symbols;
}

QList<QPair<QString, QString>> TickerListWidget::getAllTickersWithExchange() const
{
    QList<QPair<QString, QString>> tickers;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString symbol = item->data(TickerItemDelegate::SymbolRole).toString();
        QString exchange = item->data(TickerItemDelegate::ExchangeRole).toString();
        tickers.append(qMakePair(symbol, exchange));
    }
    return tickers;
}

bool TickerListWidget::hasTickerKey(const QString& symbol, const QString& exchange) const
{
    QString tickerKey = exchange.isEmpty() ? symbol : QString("%1@%2").arg(symbol).arg(exchange);

    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString itemSymbol = item->data(TickerItemDelegate::SymbolRole).toString();
        QString itemExchange = item->data(TickerItemDelegate::ExchangeRole).toString();
        QString itemKey = itemExchange.isEmpty() ? itemSymbol : QString("%1@%2").arg(itemSymbol).arg(itemExchange);

        if (itemKey == tickerKey) {
            return true;
        }
    }
    return false;
}

bool TickerListWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_tickerLabel && event->type() == QEvent::MouseButtonPress) {
        emit tickerLabelClicked();
        return true;
    }

    // Handle right-click on list widget
    if (obj == m_listWidget->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            QListWidgetItem *item = m_listWidget->itemAt(mouseEvent->pos());
            if (item) {
                QString symbol = item->data(TickerItemDelegate::SymbolRole).toString();

                // Create context menu
                QMenu contextMenu(this);

                QAction *moveToTopAction = contextMenu.addAction("Move to Top");
                QAction *deleteAction = contextMenu.addAction("Delete");

                // Execute menu at cursor position
                QAction *selectedAction = contextMenu.exec(mouseEvent->globalPosition().toPoint());

                if (selectedAction == moveToTopAction) {
                    emit symbolMoveToTopRequested(symbol);
                } else if (selectedAction == deleteAction) {
                    emit symbolDeleteRequested(symbol);
                }

                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void TickerListWidget::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (current) {
        QString symbol = current->data(TickerItemDelegate::SymbolRole).toString();
        QString exchange = current->data(TickerItemDelegate::ExchangeRole).toString();
        emit symbolSelected(symbol, exchange);
    }
}