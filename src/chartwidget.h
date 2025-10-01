#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QCandlestickSeries>

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);
    void setSymbol(const QString& symbol);

private:
    QChartView *m_chartView;
    QChart *m_chart;
    QCandlestickSeries *m_series;
    QString m_currentSymbol;
};

#endif // CHARTWIDGET_H