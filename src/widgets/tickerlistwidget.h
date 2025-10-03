#ifndef TICKERLISTWIDGET_H
#define TICKERLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QEvent>

class TickerListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TickerListWidget(QWidget *parent = nullptr);
    void addSymbol(const QString& symbol);
    void setCurrentSymbol(const QString& symbol);
    void setTickerLabel(const QString& symbol);
    void updateTickerPrice(const QString& symbol, double price, double changePercent);
    void clear();

signals:
    void symbolSelected(const QString& symbol);
    void tickerLabelClicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

private:
    QLabel *m_tickerLabel;
    QListWidget *m_listWidget;
    QString m_currentSymbol;
};

#endif // TICKERLISTWIDGET_H