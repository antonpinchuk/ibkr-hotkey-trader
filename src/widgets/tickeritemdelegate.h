#ifndef TICKERITEMDELEGATE_H
#define TICKERITEMDELEGATE_H

#include <QStyledItemDelegate>

class TickerItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TickerItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Custom roles for storing ticker data
    enum TickerRoles {
        SymbolRole = Qt::UserRole + 1,
        PriceRole = Qt::UserRole + 2,
        ChangePercentRole = Qt::UserRole + 3,
        IsCurrentRole = Qt::UserRole + 4
    };
};

#endif // TICKERITEMDELEGATE_H