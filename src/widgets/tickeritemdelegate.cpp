#include "tickeritemdelegate.h"
#include <QPainter>
#include <QApplication>

TickerItemDelegate::TickerItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void TickerItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    // Get data from model
    bool isCurrent = index.data(IsCurrentRole).toBool();
    QString symbol = index.data(SymbolRole).toString();
    QString exchange = index.data(ExchangeRole).toString();
    double price = index.data(PriceRole).toDouble();
    double changePercent = index.data(ChangePercentRole).toDouble();

    // Draw background
    if (option.state & QStyle::State_Selected) {
        // Selected item - light gray
        painter->fillRect(option.rect, QColor("#E8E8E8"));
    } else if (isCurrent) {
        // Current symbol - white
        painter->fillRect(option.rect, QColor("#FFFFFF"));
    } else {
        // Inactive - white
        painter->fillRect(option.rect, QColor("#FFFFFF"));
    }

    // Calculate positions
    int x = option.rect.x() + 8;
    int y = option.rect.y() + 6;  // Top padding
    int lineHeight = 18;

    // LINE 1: Symbol (bold) + Exchange (gray, normal)
    // Draw symbol (bold)
    QFont symbolFont = painter->font();
    symbolFont.setPointSize(isCurrent ? 13 : 12);
    symbolFont.setBold(true);
    painter->setFont(symbolFont);
    painter->setPen(option.palette.text().color());
    painter->drawText(x, y + lineHeight, symbol);

    // Calculate where exchange should start (after symbol)
    QFontMetrics symbolMetrics(symbolFont);
    int symbolWidth = symbolMetrics.horizontalAdvance(symbol);

    // Draw exchange (gray, normal) after symbol with a small gap
    if (!exchange.isEmpty()) {
        QFont exchangeFont = painter->font();
        exchangeFont.setPointSize(isCurrent ? 11 : 10);
        exchangeFont.setBold(false);
        painter->setFont(exchangeFont);
        painter->setPen(QColor("#888"));
        painter->drawText(x + symbolWidth + 6, y + lineHeight, "@" + exchange);
    }

    // LINE 2: Price + Change (for all tickers, not just current)
    if (price > 0) {
        // Format text
        QString priceText = QString("$%1").arg(price, 0, 'f', 2);

        // Format change percent with dynamic precision
        QString changeText;
        if (qAbs(changePercent) < 0.01) {
            changeText = QString("%1%2%").arg(changePercent >= 0 ? "+" : "").arg(changePercent, 0, 'f', 3);
        } else if (qAbs(changePercent) < 0.1) {
            changeText = QString("%1%2%").arg(changePercent >= 0 ? "+" : "").arg(changePercent, 0, 'f', 2);
        } else {
            changeText = QString("%1%2%").arg(changePercent >= 0 ? "+" : "").arg(changePercent, 0, 'f', 1);
        }

        // Draw price (smaller, gray)
        QFont priceFont = painter->font();
        priceFont.setPointSize(10);
        priceFont.setBold(false);
        painter->setFont(priceFont);
        painter->setPen(QColor("#666"));
        painter->drawText(x, y + lineHeight * 2 + 2, priceText);

        // Calculate where change should start (after price)
        QFontMetrics priceMetrics(priceFont);
        int priceWidth = priceMetrics.horizontalAdvance(priceText);

        // Draw change percent (colored based on up/down) after price
        QFont changeFont = painter->font();
        changeFont.setPointSize(10);
        painter->setFont(changeFont);
        QColor changeColor = changePercent >= 0 ? QColor("#4CAF50") : QColor("#F44336");
        painter->setPen(changeColor);
        painter->drawText(x + priceWidth + 8, y + lineHeight * 2 + 2, changeText);
    }

    painter->restore();
}

QSize TickerItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(150, 52);  // Height for 2 lines: symbol+exchange and price+change
}
