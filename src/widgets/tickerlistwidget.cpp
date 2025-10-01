#include "widgets/tickerlistwidget.h"
#include "widgets/tickeritemdelegate.h"
#include <QVBoxLayout>
#include <QMouseEvent>

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

void TickerListWidget::addSymbol(const QString& symbol)
{
    // Check if symbol already exists
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(TickerItemDelegate::SymbolRole).toString() == symbol) {
            // Symbol already in list, don't add again
            return;
        }
    }

    // Create new item
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(TickerItemDelegate::SymbolRole, symbol);
    item->setData(TickerItemDelegate::PriceRole, 0.0);
    item->setData(TickerItemDelegate::ChangePercentRole, 0.0);
    item->setData(TickerItemDelegate::IsCurrentRole, false);
    m_listWidget->insertItem(0, item);
}

void TickerListWidget::setCurrentSymbol(const QString& symbol)
{
    m_currentSymbol = symbol;

    // Block signals to prevent recursion when programmatically setting current item
    m_listWidget->blockSignals(true);

    // Update all items to reflect current symbol and select the matching one
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        QString itemSymbol = item->data(TickerItemDelegate::SymbolRole).toString();
        bool isCurrent = (itemSymbol == m_currentSymbol);
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

void TickerListWidget::clear()
{
    m_listWidget->clear();
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
                int row = m_listWidget->row(item);
                if (row > 0) {
                    QString symbol = item->data(TickerItemDelegate::SymbolRole).toString();
                    bool wasCurrent = (symbol == m_currentSymbol);

                    m_listWidget->blockSignals(true);
                    m_listWidget->takeItem(row);
                    m_listWidget->insertItem(0, item);

                    // If it was the current item, keep it selected at new position
                    if (wasCurrent) {
                        m_listWidget->setCurrentItem(item);
                    }

                    m_listWidget->blockSignals(false);
                    m_listWidget->viewport()->update();
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
        emit symbolSelected(symbol);
    }
}