#include "widgets/tickerlistwidget.h"
#include <QVBoxLayout>

TickerListWidget::TickerListWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &TickerListWidget::onItemDoubleClicked);
}

void TickerListWidget::addSymbol(const QString& symbol)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->text().startsWith(symbol + " ")) {
            QListWidgetItem *item = m_listWidget->takeItem(i);
            m_listWidget->insertItem(0, item);
            return;
        }
    }
    m_listWidget->insertItem(0, symbol + " - $0.00");
}

void TickerListWidget::clear()
{
    m_listWidget->clear();
}

void TickerListWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    QString symbol = item->text().split(" ").first();
    emit symbolSelected(symbol);

    int row = m_listWidget->row(item);
    if (row > 0) {
        QListWidgetItem *item = m_listWidget->takeItem(row);
        m_listWidget->insertItem(0, item);
    }
}