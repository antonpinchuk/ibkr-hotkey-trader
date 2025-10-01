#ifndef TICKERLISTWIDGET_H
#define TICKERLISTWIDGET_H

#include <QWidget>
#include <QListWidget>

class TickerListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TickerListWidget(QWidget *parent = nullptr);
    void addSymbol(const QString& symbol);
    void clear();

signals:
    void symbolSelected(const QString& symbol);

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    QListWidget *m_listWidget;
};

#endif // TICKERLISTWIDGET_H