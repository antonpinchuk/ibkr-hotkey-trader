#include "widgets/chartwidget.h"
#include "models/uistate.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QTimeZone>
#include <QDebug>
#include <limits>

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_customPlot(nullptr)
    , m_candlesticks(nullptr)
    , m_bidLine(nullptr)
    , m_askLine(nullptr)
    , m_midLine(nullptr)
    , m_bidLabel(nullptr)
    , m_askLabel(nullptr)
    , m_midLabel(nullptr)
    , m_candleSizeCombo(nullptr)
    , m_autoScaleCheckBox(nullptr)
    , m_currentTimeframe(Timeframe::SEC_10) // Default to 10s
    , m_dataManager(nullptr)
    , m_autoScale(true) // Auto-scale enabled by default
    , m_lastBid(0.0)
    , m_lastAsk(0.0)
    , m_lastMid(0.0)
    , m_priceLinesDirty(false)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(5);

    // Setup debounce timer for replot (max 10 FPS for price lines)
    m_replotTimer = new QTimer(this);
    m_replotTimer->setSingleShot(true);
    m_replotTimer->setInterval(100); // 100ms = 10 FPS
    connect(m_replotTimer, &QTimer::timeout, this, &ChartWidget::scheduledReplot);

    // Chart at the top
    m_customPlot = new QCustomPlot(this);
    setupChart();
    mainLayout->addWidget(m_customPlot);

    // Controls at the bottom
    setupControls();
    mainLayout->addLayout(createControlsLayout());
}

QHBoxLayout* ChartWidget::createControlsLayout()
{
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(5, 5, 5, 0);
    controlsLayout->setSpacing(10);

    // Candle Size dropdown
    QLabel *candleSizeLabel = new QLabel("Candle Size:", this);
    controlsLayout->addWidget(candleSizeLabel);
    controlsLayout->addWidget(m_candleSizeCombo);

    // Auto-scale checkbox
    controlsLayout->addWidget(m_autoScaleCheckBox);

    // Spacer to push controls to the left
    controlsLayout->addStretch();

    return controlsLayout;
}

void ChartWidget::setupChart()
{
    // Create candlestick graph
    m_candlesticks = new QCPFinancial(m_customPlot->xAxis, m_customPlot->yAxis);
    m_candlesticks->setChartStyle(QCPFinancial::csCandlestick);

    // Style for candlesticks
    m_candlesticks->setBrushPositive(QColor(26, 188, 156));  // Green for up candles
    m_candlesticks->setBrushNegative(QColor(239, 83, 80));   // Red for down candles
    m_candlesticks->setPenPositive(QPen(QColor(26, 188, 156)));
    m_candlesticks->setPenNegative(QPen(QColor(239, 83, 80)));

    // Configure axes
    m_customPlot->xAxis->setLabel("Time");
    m_customPlot->yAxis->setLabel(""); // No label on left
    m_customPlot->yAxis2->setLabel("Price");
    m_customPlot->yAxis2->setVisible(true); // Show right axis

    // Enable interactions: horizontal drag/zoom only
    m_customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_customPlot->axisRect()->setRangeDrag(Qt::Horizontal);
    m_customPlot->axisRect()->setRangeZoom(Qt::Horizontal);

    // Use time format for x-axis (without seconds)
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm");
    m_customPlot->xAxis->setTicker(dateTicker);

    // Connect range changed signal for auto-scale and saving state
    connect(m_customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(onXRangeChanged(QCPRange)));

    // Light theme
    m_customPlot->setBackground(Qt::white);
    m_customPlot->xAxis->setBasePen(QPen(QColor(60, 60, 60)));
    m_customPlot->yAxis->setBasePen(QPen(QColor(60, 60, 60)));
    m_customPlot->yAxis2->setBasePen(QPen(QColor(60, 60, 60)));
    m_customPlot->xAxis->setTickPen(QPen(QColor(60, 60, 60)));
    m_customPlot->yAxis->setTickPen(QPen(QColor(60, 60, 60)));
    m_customPlot->yAxis2->setTickPen(QPen(QColor(60, 60, 60)));
    m_customPlot->xAxis->setSubTickPen(QPen(QColor(140, 140, 140)));
    m_customPlot->yAxis->setSubTickPen(QPen(QColor(140, 140, 140)));
    m_customPlot->yAxis2->setSubTickPen(QPen(QColor(140, 140, 140)));
    m_customPlot->xAxis->setTickLabelColor(QColor(60, 60, 60));
    m_customPlot->yAxis->setTickLabelColor(QColor(60, 60, 60));
    m_customPlot->yAxis2->setTickLabelColor(QColor(60, 60, 60));
    m_customPlot->xAxis->setLabelColor(QColor(40, 40, 40));
    m_customPlot->yAxis->setLabelColor(QColor(40, 40, 40));
    m_customPlot->yAxis2->setLabelColor(QColor(40, 40, 40));
    m_customPlot->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
    m_customPlot->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));

    // Setup price lines (bid, ask, mid)
    m_bidLine = new QCPItemLine(m_customPlot);
    m_bidLine->setPen(QPen(QColor(239, 83, 80), 1, Qt::DashLine));
    m_bidLabel = new QCPItemText(m_customPlot);
    m_bidLabel->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_bidLabel->setColor(QColor(239, 83, 80));
    m_bidLabel->setFont(QFont("sans", 9));
    m_bidLabel->setPadding(QMargins(5, 2, 5, 2));
    m_bidLabel->setBrush(QBrush(QColor(255, 255, 255, 200)));

    m_askLine = new QCPItemLine(m_customPlot);
    m_askLine->setPen(QPen(QColor(26, 188, 156), 1, Qt::DashLine));
    m_askLabel = new QCPItemText(m_customPlot);
    m_askLabel->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_askLabel->setColor(QColor(26, 188, 156));
    m_askLabel->setFont(QFont("sans", 9));
    m_askLabel->setPadding(QMargins(5, 2, 5, 2));
    m_askLabel->setBrush(QBrush(QColor(255, 255, 255, 200)));

    m_midLine = new QCPItemLine(m_customPlot);
    m_midLine->setPen(QPen(QColor(52, 152, 219), 1, Qt::SolidLine));
    m_midLabel = new QCPItemText(m_customPlot);
    m_midLabel->setPositionAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_midLabel->setColor(QColor(52, 152, 219));
    m_midLabel->setFont(QFont("sans", 9, QFont::Bold));
    m_midLabel->setPadding(QMargins(5, 2, 5, 2));
    m_midLabel->setBrush(QBrush(QColor(255, 255, 255, 200)));

    m_bidLine->setVisible(false);
    m_askLine->setVisible(false);
    m_midLine->setVisible(false);
    m_bidLabel->setVisible(false);
    m_askLabel->setVisible(false);
    m_midLabel->setVisible(false);
}

void ChartWidget::setupControls()
{
    m_candleSizeCombo = new QComboBox(this);
    m_candleSizeCombo->setMinimumWidth(80);
    m_candleSizeCombo->addItem("5s", QVariant::fromValue(Timeframe::SEC_5));
    m_candleSizeCombo->addItem("10s", QVariant::fromValue(Timeframe::SEC_10));
    m_candleSizeCombo->addItem("30s", QVariant::fromValue(Timeframe::SEC_30));
    m_candleSizeCombo->addItem("1m", QVariant::fromValue(Timeframe::MIN_1));
    m_candleSizeCombo->addItem("5m", QVariant::fromValue(Timeframe::MIN_5));
    m_candleSizeCombo->addItem("15m", QVariant::fromValue(Timeframe::MIN_15));
    m_candleSizeCombo->addItem("30m", QVariant::fromValue(Timeframe::MIN_30));
    m_candleSizeCombo->addItem("1H", QVariant::fromValue(Timeframe::HOUR_1));

    m_candleSizeCombo->setCurrentIndex(1);

    connect(m_candleSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartWidget::onCandleSizeChanged);

    m_autoScaleCheckBox = new QCheckBox("Auto-scale", this);
    m_autoScaleCheckBox->setChecked(true);
    connect(m_autoScaleCheckBox, &QCheckBox::toggled, this, &ChartWidget::onAutoScaleChanged);
}

void ChartWidget::setSymbol(const QString& symbol, const QString& exchange)
{
    m_currentSymbol = symbol;
    m_currentTickerKey = makeTickerKey(symbol, exchange);
    clearChart();

    if (!m_currentSymbol.isEmpty()) {
        m_customPlot->plotLayout()->insertRow(0);
        QString title = QString("%1 - %2 Candles").arg(m_currentSymbol).arg(timeframeToString(m_currentTimeframe));
        m_customPlot->plotLayout()->addElement(0, 0,
            new QCPTextElement(m_customPlot, title, QFont("sans", 12, QFont::Bold)));

        updateChart();
    }
}

void ChartWidget::setTimeframe(Timeframe timeframe)
{
    if (m_currentTimeframe != timeframe) {
        m_currentTimeframe = timeframe;

        if (!m_currentSymbol.isEmpty() && m_customPlot->plotLayout()->rowCount() > 1) {
            QCPTextElement* titleElement = qobject_cast<QCPTextElement*>(m_customPlot->plotLayout()->element(0, 0));
            if (titleElement) {
                QString title = QString("%1 - %2 Candles").arg(m_currentSymbol).arg(timeframeToString(m_currentTimeframe));
                titleElement->setText(title);
            }
        }

        updateChart();
    }
}

void ChartWidget::setTickerDataManager(TickerDataManager* manager)
{
    m_dataManager = manager;

    if (m_dataManager) {
        connect(m_dataManager, &TickerDataManager::barsUpdated, this, [this](const QString& symbol) {
            if (symbol == m_currentSymbol) {
                updateChart();
            }
        });

        connect(m_dataManager, &TickerDataManager::tickerDataLoaded, this, [this](const QString& symbol) {
            if (symbol == m_currentSymbol) {
                updateChart();
            }
        });
    }
}

void ChartWidget::clearChart()
{
    m_candlesticks->data()->clear();

    // Hide price lines
    if (m_bidLine) m_bidLine->setVisible(false);
    if (m_askLine) m_askLine->setVisible(false);
    if (m_midLine) m_midLine->setVisible(false);
    if (m_bidLabel) m_bidLabel->setVisible(false);
    if (m_askLabel) m_askLabel->setVisible(false);
    if (m_midLabel) m_midLabel->setVisible(false);

    // Remove session background rectangles
    for (int i = m_customPlot->itemCount() - 1; i >= 0; --i) {
        QCPAbstractItem* item = m_customPlot->item(i);
        if (!item) continue;

        QCPItemRect* rect = qobject_cast<QCPItemRect*>(item);
        if (rect) {
            m_customPlot->removeItem(rect);
        }
    }

    if (m_customPlot->plotLayout()->rowCount() > 1) {
        m_customPlot->plotLayout()->removeAt(0);
        m_customPlot->plotLayout()->simplify();
    }

    m_customPlot->replot();
}

void ChartWidget::updateChart()
{
    if (m_currentSymbol.isEmpty() || !m_dataManager) {
        return;
    }

    const QVector<CandleBar>* bars = m_dataManager->getBars(m_currentTickerKey, m_currentTimeframe);
    if (bars && !bars->isEmpty()) {
        plotCandles(*bars);
    }
}

void ChartWidget::plotCandles(const QVector<CandleBar>& bars)
{
    m_candlesticks->data()->clear();

    if (!bars.isEmpty()) {
        QDateTime firstTime = QDateTime::fromSecsSinceEpoch(bars.first().timestamp, QTimeZone::utc());
        QDateTime lastTime = QDateTime::fromSecsSinceEpoch(bars.last().timestamp, QTimeZone::utc());
        qDebug() << "ChartWidget: Plotting" << bars.size() << "bars"
                 << "from" << firstTime.toString("yyyy-MM-dd HH:mm:ss")
                 << "to" << lastTime.toString("yyyy-MM-dd HH:mm:ss");
        qDebug() << "ChartWidget: First timestamp:" << bars.first().timestamp
                 << "Last timestamp:" << bars.last().timestamp;
    }

    for (const CandleBar& bar : bars) {
        m_candlesticks->addData(bar.timestamp, bar.open, bar.high, bar.low, bar.close);
    }

    addSessionBackgrounds(bars);

    // Auto-scale X axis to show all candles
    m_candlesticks->rescaleKeyAxis();

    // Restore saved horizontal zoom for this timeframe
    restoreHorizontalRange();

    // Auto-scale Y axis for visible candles
    if (m_autoScale) {
        rescaleVerticalAxis();
    }

    m_customPlot->replot();
}

void ChartWidget::updatePriceLines(double bid, double ask, double mid)
{
    if (!m_customPlot || !m_bidLine || !m_askLine || !m_midLine) {
        return;
    }

    // Store the latest values for debounced update
    m_lastBid = bid;
    m_lastAsk = ask;
    m_lastMid = mid;
    m_priceLinesDirty = true;

    // Schedule a replot if not already scheduled (debouncing at 10 FPS)
    if (!m_replotTimer->isActive()) {
        m_replotTimer->start();
    }
}

void ChartWidget::updateCurrentBar(const CandleBar& bar)
{
    if (!m_dataManager || m_currentSymbol.isEmpty()) {
        return;
    }

    const QVector<CandleBar>* bars = m_dataManager->getBars(m_currentTickerKey, m_currentTimeframe);
    if (!bars || bars->isEmpty()) {
        return;
    }

    bool isNewCandle = (bars->last().timestamp != bar.timestamp);

    if (bars->last().timestamp == bar.timestamp) {
        m_candlesticks->data()->remove(bar.timestamp);
        m_candlesticks->addData(bar.timestamp, bar.open, bar.high, bar.low, bar.close);
    } else {
        m_candlesticks->addData(bar.timestamp, bar.open, bar.high, bar.low, bar.close);
    }

    // Auto-scroll chart when new candle arrives
    if (isNewCandle && m_autoScale) {
        QCPRange currentRange = m_customPlot->xAxis->range();
        double rangeSize = currentRange.size();

        // Shift range to the right by the timeframe width
        int candleWidth = timeframeToSeconds(m_currentTimeframe);
        m_customPlot->xAxis->setRange(currentRange.lower + candleWidth, currentRange.upper + candleWidth);
    }

    // Rescale vertical axis if auto-scale is enabled
    if (m_autoScale) {
        rescaleVerticalAxis();
    }

    m_customPlot->replot();
}

void ChartWidget::addSessionBackgrounds(const QVector<CandleBar>& bars)
{
    if (bars.isEmpty()) {
        return;
    }

    // Remove old session background rectangles (but not price lines)
    QList<QCPItemRect*> rectsToRemove;
    for (int i = 0; i < m_customPlot->itemCount(); ++i) {
        QCPAbstractItem* item = m_customPlot->item(i);
        if (!item) continue;

        QCPItemRect* rect = qobject_cast<QCPItemRect*>(item);
        if (rect) {
            // Check if this is NOT a price line related rect (price lines are QCPItemLine, not rect)
            rectsToRemove.append(rect);
        }
    }

    // Remove collected rectangles
    for (QCPItemRect* rect : rectsToRemove) {
        m_customPlot->removeItem(rect);
    }

    for (const CandleBar& bar : bars) {
        QDateTime barTime = QDateTime::fromSecsSinceEpoch(bar.timestamp, QTimeZone::utc());

        barTime = barTime.addSecs(-5 * 3600);

        int hour = barTime.time().hour();
        int minute = barTime.time().minute();
        double hourDecimal = hour + minute / 60.0;

        if (hourDecimal >= 4.0 && hourDecimal < 9.5) {
            QCPItemRect* rect = new QCPItemRect(m_customPlot);
            rect->topLeft->setCoords(bar.timestamp, m_customPlot->yAxis->range().upper);
            rect->bottomRight->setCoords(bar.timestamp + 10, m_customPlot->yAxis->range().lower);
            rect->setBrush(QBrush(QColor(255, 250, 205, 80)));
            rect->setPen(Qt::NoPen);
        }
        else if (hourDecimal >= 16.0 && hourDecimal < 20.0) {
            QCPItemRect* rect = new QCPItemRect(m_customPlot);
            rect->topLeft->setCoords(bar.timestamp, m_customPlot->yAxis->range().upper);
            rect->bottomRight->setCoords(bar.timestamp + 10, m_customPlot->yAxis->range().lower);
            rect->setBrush(QBrush(QColor(173, 216, 230, 80)));
            rect->setPen(Qt::NoPen);
        }
    }
}

void ChartWidget::onCandleSizeChanged(int index)
{
    if (index < 0 || !m_candleSizeCombo) {
        return;
    }

    Timeframe newTimeframe = m_candleSizeCombo->currentData().value<Timeframe>();

    if (newTimeframe != m_currentTimeframe) {
        setTimeframe(newTimeframe);

        if (m_dataManager) {
            m_dataManager->setCurrentTimeframe(newTimeframe);

            if (!m_currentSymbol.isEmpty()) {
                m_dataManager->loadTimeframe(m_currentTickerKey, newTimeframe);
            }
        }
    }
}

void ChartWidget::onAutoScaleChanged(bool checked)
{
    m_autoScale = checked;
    if (m_autoScale) {
        rescaleVerticalAxis();
        m_customPlot->replot();
    }
}

void ChartWidget::onXRangeChanged(const QCPRange &range)
{
    Q_UNUSED(range);

    // Auto-scale vertical axis based on visible candles
    if (m_autoScale) {
        rescaleVerticalAxis();
    }

    // Save horizontal range for current timeframe
    saveHorizontalRange();
}

void ChartWidget::rescaleVerticalAxis()
{
    if (!m_candlesticks || m_candlesticks->data()->isEmpty()) {
        return;
    }

    // Get visible range on x-axis
    QCPRange xRange = m_customPlot->xAxis->range();

    double minPrice = std::numeric_limits<double>::max();
    double maxPrice = std::numeric_limits<double>::lowest();

    // Iterate through all candles and find min/max within visible range
    QCPFinancialDataContainer::const_iterator begin = m_candlesticks->data()->constBegin();
    QCPFinancialDataContainer::const_iterator end = m_candlesticks->data()->constEnd();

    for (QCPFinancialDataContainer::const_iterator it = begin; it != end; ++it) {
        double key = it->key;
        if (key >= xRange.lower && key <= xRange.upper) {
            minPrice = qMin(minPrice, it->low);
            maxPrice = qMax(maxPrice, it->high);
        }
    }

    // Add 5% padding
    if (minPrice != std::numeric_limits<double>::max() && maxPrice != std::numeric_limits<double>::lowest()) {
        double range = maxPrice - minPrice;
        double padding = range * 0.05;
        m_customPlot->yAxis->setRange(minPrice - padding, maxPrice + padding);
        m_customPlot->yAxis2->setRange(minPrice - padding, maxPrice + padding);
    }
}

void ChartWidget::saveHorizontalRange()
{
    if (!m_customPlot || m_currentSymbol.isEmpty()) {
        return;
    }

    QCPRange range = m_customPlot->xAxis->range();
    QString timeframeKey = timeframeToString(m_currentTimeframe);
    UIState::instance().saveChartZoom(timeframeKey, range.lower, range.upper);
}

void ChartWidget::restoreHorizontalRange()
{
    if (!m_customPlot || m_currentSymbol.isEmpty()) {
        return;
    }

    QString timeframeKey = timeframeToString(m_currentTimeframe);
    double lower, upper;
    if (UIState::instance().restoreChartZoom(timeframeKey, lower, upper)) {
        m_customPlot->xAxis->setRange(lower, upper);
    }
}

void ChartWidget::scheduledReplot()
{
    if (!m_priceLinesDirty || !m_customPlot || !m_bidLine || !m_askLine || !m_midLine) {
        return;
    }

    double xMin = m_customPlot->xAxis->range().lower;
    double xMax = m_customPlot->xAxis->range().upper;

    // Update bid line
    m_bidLine->start->setCoords(xMin, m_lastBid);
    m_bidLine->end->setCoords(xMax, m_lastBid);
    m_bidLabel->position->setCoords(xMax, m_lastBid);
    m_bidLabel->setText(QString("Bid: %1").arg(m_lastBid, 0, 'f', 2));
    m_bidLine->setVisible(true);
    m_bidLabel->setVisible(true);

    // Update ask line
    m_askLine->start->setCoords(xMin, m_lastAsk);
    m_askLine->end->setCoords(xMax, m_lastAsk);
    m_askLabel->position->setCoords(xMax, m_lastAsk);
    m_askLabel->setText(QString("Ask: %1").arg(m_lastAsk, 0, 'f', 2));
    m_askLine->setVisible(true);
    m_askLabel->setVisible(true);

    // Update mid line
    m_midLine->start->setCoords(xMin, m_lastMid);
    m_midLine->end->setCoords(xMax, m_lastMid);
    m_midLabel->position->setCoords(xMax, m_lastMid);
    m_midLabel->setText(QString("Mid: %1").arg(m_lastMid, 0, 'f', 2));
    m_midLine->setVisible(true);
    m_midLabel->setVisible(true);

    // Replot once for all updates
    m_customPlot->replot();
    m_priceLinesDirty = false;
}
