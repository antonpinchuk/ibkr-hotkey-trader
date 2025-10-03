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

    // Get more data from model
    QString symbol = index.data(SymbolRole).toString();
    double price = index.data(PriceRole).toDouble();
    double changePercent = index.data(ChangePercentRole).toDouble();

    // Format text
    QString priceText = QString("$%1").arg(price, 0, 'f', 2);

    // Format change percent with dynamic precision (max 3 decimal places)
    QString changeText;
    if (qAbs(changePercent) < 0.01) {
        // Very small changes: show 3 decimals
        changeText = QString("%1%2%").arg(changePercent >= 0 ? "+" : "").arg(changePercent, 0, 'f', 3);
    } else if (qAbs(changePercent) < 0.1) {
        // Small changes: show 2 decimals
        changeText = QString("%1%2%").arg(changePercent >= 0 ? "+" : "").arg(changePercent, 0, 'f', 2);
    } else {
        // Larger changes: show 1 decimal
        changeText = QString("%1%2%").arg(changePercent >= 0 ? "+" : "").arg(changePercent, 0, 'f', 1);
    }

    // Calculate positions
    int x = option.rect.x() + 8;
    int y = option.rect.y() + 8;  // Top padding
    int lineHeight = 17;

    // Draw symbol (larger, bold if current)
    QFont symbolFont = painter->font();
    if (isCurrent) {
        symbolFont.setPointSize(13);
        symbolFont.setBold(true);
    } else {
        symbolFont.setPointSize(12);
        symbolFont.setBold(false);
    }
    painter->setFont(symbolFont);
    painter->setPen(option.palette.text().color());  // Always use text color, not highlight
    painter->drawText(x, y + lineHeight, symbol);

    // Draw price (smaller, gray)
    QFont priceFont = painter->font();
    priceFont.setPointSize(10);
    priceFont.setBold(false);
    painter->setFont(priceFont);
    painter->setPen(QColor("#666"));
    painter->drawText(x, y + lineHeight * 2, priceText);

    // Draw change percent (even smaller, colored based on up/down)
    QFont changeFont = painter->font();
    changeFont.setPointSize(8);
    painter->setFont(changeFont);
    QColor changeColor = changePercent >= 0 ? QColor("#4CAF50") : QColor("#F44336");
    painter->setPen(changeColor);
    painter->drawText(x, y + lineHeight * 3, changeText);

    painter->restore();
}

QSize TickerItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(150, 68);  // Increased height to accommodate top and bottom padding
}