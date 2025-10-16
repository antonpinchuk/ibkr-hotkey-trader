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
    void removeSymbol(const QString& symbol);
    void moveSymbolToTop(const QString& symbol);
    void setCurrentSymbol(const QString& symbol);
    void setTickerLabel(const QString& symbol);
    void updateTickerPrice(const QString& symbol, double price, double changePercent);
    void clear();
    QString getTopSymbol() const;

signals:
    void symbolSelected(const QString& symbol);
    void tickerLabelClicked();
    void symbolMoveToTopRequested(const QString& symbol);
    void symbolDeleteRequested(const QString& symbol);

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