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
    void addSymbol(const QString& symbol, const QString& exchange = QString());
    void removeSymbol(const QString& symbol, const QString& exchange = QString());
    void moveSymbolToTop(const QString& symbol, const QString& exchange = QString());
    void setCurrentSymbol(const QString& symbol, const QString& exchange = QString());
    void setTickerLabel(const QString& symbol);
    void updateTickerPrice(const QString& symbol, const QString& exchange, double price, double changePercent);
    void clear();
    QString getTopSymbol() const;
    QStringList getAllSymbols() const;
    QList<QPair<QString, QString>> getAllTickersWithExchange() const; // Returns list of (symbol, exchange) pairs
    bool hasTickerKey(const QString& symbol, const QString& exchange) const;

signals:
    void symbolSelected(const QString& symbol, const QString& exchange);
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