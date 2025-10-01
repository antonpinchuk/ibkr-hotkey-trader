#include "widgets/chartwidget.h"
#include <QVBoxLayout>

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_chart = new QChart();
    m_chart->setTitle("Chart");

    m_series = new QCandlestickSeries();
    m_chart->addSeries(m_series);
    m_chart->createDefaultAxes();

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    layout->addWidget(m_chartView);
}

void ChartWidget::setSymbol(const QString& symbol)
{
    m_currentSymbol = symbol;
    m_chart->setTitle(symbol + " - 10s Candles");
}
