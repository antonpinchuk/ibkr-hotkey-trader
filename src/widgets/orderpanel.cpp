#include "widgets/orderpanel.h"
#include "models/settings.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleValidator>
#include <QEvent>
#include <QStandardItemModel>

OrderPanel::OrderPanel(QWidget *parent)
    : QWidget(parent)
    , m_buyPriceUserEdited(false)
    , m_sellPriceUserEdited(false)
{
    // Fixed height panel
    setFixedHeight(40);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);

    // Order type label and combo
    QLabel* orderTypeLabel = new QLabel("Order type:", this);
    layout->addWidget(orderTypeLabel);

    m_orderTypeCombo = new QComboBox(this);
    m_orderTypeCombo->addItem("LMT");
    m_orderTypeCombo->addItem("MKT");
    m_orderTypeCombo->setFixedWidth(80);
    layout->addWidget(m_orderTypeCombo);

    // Load saved order type
    Settings& settings = Settings::instance();
    QString savedOrderType = settings.orderType();
    m_orderTypeCombo->setCurrentText(savedOrderType);

    layout->addSpacing(20);

    // Buy price
    QLabel* buyLabel = new QLabel("Buy:", this);
    layout->addWidget(buyLabel);

    m_buyPriceEdit = new QLineEdit(this);
    m_buyPriceEdit->setFixedWidth(100);
    m_buyPriceEdit->setPlaceholderText("Price");
    QDoubleValidator* buyValidator = new QDoubleValidator(0.0, 999999.99, 2, this);
    buyValidator->setNotation(QDoubleValidator::StandardNotation);
    m_buyPriceEdit->setValidator(buyValidator);
    layout->addWidget(m_buyPriceEdit);

    layout->addSpacing(20);

    // Sell price
    QLabel* sellLabel = new QLabel("Sell:", this);
    layout->addWidget(sellLabel);

    m_sellPriceEdit = new QLineEdit(this);
    m_sellPriceEdit->setFixedWidth(100);
    m_sellPriceEdit->setPlaceholderText("Price");
    QDoubleValidator* sellValidator = new QDoubleValidator(0.0, 999999.99, 2, this);
    sellValidator->setNotation(QDoubleValidator::StandardNotation);
    m_sellPriceEdit->setValidator(sellValidator);
    layout->addWidget(m_sellPriceEdit);

    layout->addSpacing(20);

    // Reset button to re-enable auto-update
    m_resetButton = new QPushButton("Auto", this);
    m_resetButton->setFixedWidth(50);
    m_resetButton->setToolTip("Reset prices to auto-update from ticks");
    m_resetButton->setEnabled(false);  // Disabled by default
    layout->addWidget(m_resetButton);

    layout->addStretch();

    // Connect signals
    connect(m_orderTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OrderPanel::onOrderTypeChanged);
    connect(m_buyPriceEdit, &QLineEdit::textEdited, this, &OrderPanel::onBuyPriceEdited);
    connect(m_sellPriceEdit, &QLineEdit::textEdited, this, &OrderPanel::onSellPriceEdited);
    connect(m_resetButton, &QPushButton::clicked, this, &OrderPanel::resetPriceFields);

    // Track focus changes to detect user interaction
    m_buyPriceEdit->installEventFilter(this);
    m_sellPriceEdit->installEventFilter(this);

    // Update initial state
    updatePriceFieldsState();
}

QString OrderPanel::orderType() const
{
    return m_orderTypeCombo->currentText();
}

void OrderPanel::setOrderType(const QString& type)
{
    m_orderTypeCombo->setCurrentText(type);
}

double OrderPanel::buyPrice() const
{
    return m_buyPriceEdit->text().toDouble();
}

double OrderPanel::sellPrice() const
{
    return m_sellPriceEdit->text().toDouble();
}

void OrderPanel::setBuyPrice(double price)
{
    // Don't update if user has focus on the field or manually edited it
    if (m_buyPriceEdit->hasFocus() || m_buyPriceUserEdited) {
        return;
    }

    m_buyPriceEdit->setText(QString::number(price, 'f', 2));
}

void OrderPanel::setSellPrice(double price)
{
    // Don't update if user has focus on the field or manually edited it
    if (m_sellPriceEdit->hasFocus() || m_sellPriceUserEdited) {
        return;
    }

    m_sellPriceEdit->setText(QString::number(price, 'f', 2));
}

void OrderPanel::updatePriceFieldsState()
{
    bool isLimitOrder = (orderType() == "LMT");
    m_buyPriceEdit->setEnabled(isLimitOrder);
    m_sellPriceEdit->setEnabled(isLimitOrder);

    if (!isLimitOrder) {
        // Clear prices when switching to MKT
        m_buyPriceEdit->clear();
        m_sellPriceEdit->clear();
    }
}

void OrderPanel::resetPriceFields()
{
    m_buyPriceUserEdited = false;
    m_sellPriceUserEdited = false;
    m_buyPriceEdit->clear();
    m_sellPriceEdit->clear();

    // Remove focus from price fields
    m_buyPriceEdit->clearFocus();
    m_sellPriceEdit->clearFocus();

    // Emit signals with 0 to reset target prices in TradingManager
    emit buyPriceChanged(0.0);
    emit sellPriceChanged(0.0);

    // Update reset button state (should be disabled after reset)
    updateResetButtonState();
}

void OrderPanel::setMarketOrdersEnabled(bool enabled)
{
    // Find MKT item in combo box
    int mktIndex = m_orderTypeCombo->findText("MKT");
    if (mktIndex >= 0) {
        // Get the model
        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_orderTypeCombo->model());
        if (model) {
            QStandardItem* item = model->item(mktIndex);
            if (item) {
                // Disable/enable the item
                item->setEnabled(enabled);

                // If MKT is currently selected but should be disabled, switch to LMT
                if (!enabled && m_orderTypeCombo->currentText() == "MKT") {
                    m_orderTypeCombo->setCurrentText("LMT");
                }
            }
        }
    }
}

void OrderPanel::onOrderTypeChanged(int index)
{
    Q_UNUSED(index);

    QString type = orderType();

    // Save to settings
    Settings::instance().setOrderType(type);
    Settings::instance().save();

    // Update price fields state
    updatePriceFieldsState();

    // Reset user edit flags when switching order type
    m_buyPriceUserEdited = false;
    m_sellPriceUserEdited = false;

    // Update reset button state
    updateResetButtonState();

    emit orderTypeChanged(type);
}

void OrderPanel::onBuyPriceEdited()
{
    m_buyPriceUserEdited = true;
    updateResetButtonState();
    emit buyPriceChanged(buyPrice());
}

void OrderPanel::onSellPriceEdited()
{
    m_sellPriceUserEdited = true;
    updateResetButtonState();
    emit sellPriceChanged(sellPrice());
}

void OrderPanel::onBuyPriceFocusChanged(bool hasFocus)
{
    Q_UNUSED(hasFocus);
}

void OrderPanel::onSellPriceFocusChanged(bool hasFocus)
{
    Q_UNUSED(hasFocus);
}

bool OrderPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_buyPriceEdit || obj == m_sellPriceEdit) {
        if (event->type() == QEvent::FocusIn) {
            // User clicked on the field - stop auto-updates
            if (obj == m_buyPriceEdit) {
                m_buyPriceUserEdited = true;
            } else {
                m_sellPriceUserEdited = true;
            }
            updateResetButtonState();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void OrderPanel::updateResetButtonState()
{
    // Enable reset button only if user manually edited at least one price field
    bool hasUserEdits = m_buyPriceUserEdited || m_sellPriceUserEdited;
    m_resetButton->setEnabled(hasUserEdits);
}

void OrderPanel::setOrderPanelEnabled(bool enabled)
{
    m_orderTypeCombo->setEnabled(enabled);

    if (enabled) {
        // When enabling, respect the order type (LMT fields enabled, MKT fields disabled)
        bool isLimitOrder = (orderType() == "LMT");
        m_buyPriceEdit->setEnabled(isLimitOrder);
        m_sellPriceEdit->setEnabled(isLimitOrder);
        updateResetButtonState();
    } else {
        // When disabling, disable everything
        m_buyPriceEdit->setEnabled(false);
        m_sellPriceEdit->setEnabled(false);
        m_resetButton->setEnabled(false);
        // Clear fields when disabling
        resetPriceFields();
    }
}
