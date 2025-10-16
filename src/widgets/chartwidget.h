#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include "qcustomplot.h"
#include "models/tickerdatamanager.h"

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);
    void setSymbol(const QString& symbol);
    void setTimeframe(Timeframe timeframe);
    void setTickerDataManager(TickerDataManager* manager);
    void updateChart();
    void updatePriceLines(double bid, double ask, double mid);
    void updateCurrentBar(const CandleBar& bar);

private slots:
    void onCandleSizeChanged(int index);
    void onAutoScaleChanged(bool checked);
    void onXRangeChanged(const QCPRange &range);

private:
    void setupChart();
    void setupControls();
    QHBoxLayout* createControlsLayout();
    void clearChart();
    void plotCandles(const QVector<CandleBar>& bars);
    void addSessionBackgrounds(const QVector<CandleBar>& bars);
    void rescaleVerticalAxis();
    void saveHorizontalRange();
    void restoreHorizontalRange();

    QCustomPlot *m_customPlot;
    QCPFinancial *m_candlesticks;

    // Price lines
    QCPItemLine *m_bidLine;
    QCPItemLine *m_askLine;
    QCPItemLine *m_midLine;
    QCPItemText *m_bidLabel;
    QCPItemText *m_askLabel;
    QCPItemText *m_midLabel;

    // Controls
    QComboBox *m_candleSizeCombo;
    QCheckBox *m_autoScaleCheckBox;

    QString m_currentSymbol;
    Timeframe m_currentTimeframe;
    TickerDataManager* m_dataManager;
    bool m_autoScale;
};

#endif // CHARTWIDGET_H