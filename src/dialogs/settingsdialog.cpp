#include "dialogs/settingsdialog.h"
#include "models/settings.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Settings");
    resize(500, 400);

    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    // Trading tab
    QWidget *tradingTab = new QWidget();
    QFormLayout *tradingLayout = new QFormLayout(tradingTab);

    m_budgetEdit = new QLineEdit();
    tradingLayout->addRow("Budget $:", m_budgetEdit);

    m_tabWidget->addTab(tradingTab, "Trading");

    // Limits tab
    QWidget *limitsTab = new QWidget();
    QFormLayout *limitsLayout = new QFormLayout(limitsTab);

    m_askOffsetSpin = new QSpinBox();
    m_askOffsetSpin->setRange(0, 100);
    m_askOffsetSpin->setSuffix(" cents");
    limitsLayout->addRow("Ask +", m_askOffsetSpin);

    m_bidOffsetSpin = new QSpinBox();
    m_bidOffsetSpin->setRange(0, 100);
    m_bidOffsetSpin->setSuffix(" cents");
    limitsLayout->addRow("Bid -", m_bidOffsetSpin);

    m_tabWidget->addTab(limitsTab, "Limits");

    // Hotkeys tab
    QWidget *hotkeysTab = new QWidget();
    QFormLayout *hotkeysLayout = new QFormLayout(hotkeysTab);

    QLabel *openLabel = new QLabel("<b>Opening Positions:</b>");
    hotkeysLayout->addRow(openLabel);

    m_hotkeyOpen100 = new QSpinBox();
    m_hotkeyOpen100->setRange(1, 100);
    m_hotkeyOpen100->setSuffix("%");
    hotkeysLayout->addRow("Cmd+O:", m_hotkeyOpen100);

    m_hotkeyOpen50 = new QSpinBox();
    m_hotkeyOpen50->setRange(1, 100);
    m_hotkeyOpen50->setSuffix("%");
    hotkeysLayout->addRow("Cmd+P:", m_hotkeyOpen50);

    QLabel *addLabel = new QLabel("<b>Adding to Position:</b>");
    hotkeysLayout->addRow(addLabel);

    m_hotkeyAdd1 = new QSpinBox();
    m_hotkeyAdd1->setRange(1, 100);
    m_hotkeyAdd1->setSuffix("%");
    hotkeysLayout->addRow("Cmd+1:", m_hotkeyAdd1);

    m_hotkeyAdd2 = new QSpinBox();
    m_hotkeyAdd2->setRange(1, 100);
    m_hotkeyAdd2->setSuffix("%");
    hotkeysLayout->addRow("Cmd+2:", m_hotkeyAdd2);

    m_hotkeyAdd3 = new QSpinBox();
    m_hotkeyAdd3->setRange(1, 100);
    m_hotkeyAdd3->setSuffix("%");
    hotkeysLayout->addRow("Cmd+3:", m_hotkeyAdd3);

    m_hotkeyAdd4 = new QSpinBox();
    m_hotkeyAdd4->setRange(1, 100);
    m_hotkeyAdd4->setSuffix("%");
    hotkeysLayout->addRow("Cmd+4:", m_hotkeyAdd4);

    m_hotkeyAdd5 = new QSpinBox();
    m_hotkeyAdd5->setRange(1, 100);
    m_hotkeyAdd5->setSuffix("%");
    hotkeysLayout->addRow("Cmd+5:", m_hotkeyAdd5);

    m_hotkeyAdd6 = new QSpinBox();
    m_hotkeyAdd6->setRange(1, 100);
    m_hotkeyAdd6->setSuffix("%");
    hotkeysLayout->addRow("Cmd+6:", m_hotkeyAdd6);

    m_hotkeyAdd7 = new QSpinBox();
    m_hotkeyAdd7->setRange(1, 100);
    m_hotkeyAdd7->setSuffix("%");
    hotkeysLayout->addRow("Cmd+7:", m_hotkeyAdd7);

    m_hotkeyAdd8 = new QSpinBox();
    m_hotkeyAdd8->setRange(1, 100);
    m_hotkeyAdd8->setSuffix("%");
    hotkeysLayout->addRow("Cmd+8:", m_hotkeyAdd8);

    m_hotkeyAdd9 = new QSpinBox();
    m_hotkeyAdd9->setRange(1, 100);
    m_hotkeyAdd9->setSuffix("%");
    hotkeysLayout->addRow("Cmd+9:", m_hotkeyAdd9);

    m_hotkeyAdd0 = new QSpinBox();
    m_hotkeyAdd0->setRange(1, 100);
    m_hotkeyAdd0->setSuffix("%");
    hotkeysLayout->addRow("Cmd+0:", m_hotkeyAdd0);

    QLabel *closeLabel = new QLabel("<b>Closing Positions:</b>");
    hotkeysLayout->addRow(closeLabel);

    m_hotkeyClose25 = new QSpinBox();
    m_hotkeyClose25->setRange(1, 100);
    m_hotkeyClose25->setSuffix("%");
    hotkeysLayout->addRow("Cmd+V:", m_hotkeyClose25);

    m_hotkeyClose50 = new QSpinBox();
    m_hotkeyClose50->setRange(1, 100);
    m_hotkeyClose50->setSuffix("%");
    hotkeysLayout->addRow("Cmd+C:", m_hotkeyClose50);

    m_hotkeyClose75 = new QSpinBox();
    m_hotkeyClose75->setRange(1, 100);
    m_hotkeyClose75->setSuffix("%");
    hotkeysLayout->addRow("Cmd+X:", m_hotkeyClose75);

    m_hotkeyClose100 = new QSpinBox();
    m_hotkeyClose100->setRange(1, 100);
    m_hotkeyClose100->setSuffix("%");
    hotkeysLayout->addRow("Cmd+Z:", m_hotkeyClose100);

    m_tabWidget->addTab(hotkeysTab, "Hotkeys");

    // Connection tab
    QWidget *connectionTab = new QWidget();
    QFormLayout *connectionLayout = new QFormLayout(connectionTab);

    m_hostEdit = new QLineEdit();
    connectionLayout->addRow("Host:", m_hostEdit);

    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    connectionLayout->addRow("Port:", m_portSpin);

    m_clientIdSpin = new QSpinBox();
    m_clientIdSpin->setRange(1, 999);
    connectionLayout->addRow("Client ID:", m_clientIdSpin);

    m_tabWidget->addTab(connectionTab, "Connection");

    mainLayout->addWidget(m_tabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::loadSettings()
{
    Settings& settings = Settings::instance();

    m_budgetEdit->setText(QString::number(settings.budget()));
    m_askOffsetSpin->setValue(settings.askOffset());
    m_bidOffsetSpin->setValue(settings.bidOffset());

    // Hotkeys - load default values
    m_hotkeyOpen100->setValue(100);
    m_hotkeyOpen50->setValue(50);
    m_hotkeyAdd1->setValue(5);
    m_hotkeyAdd2->setValue(10);
    m_hotkeyAdd3->setValue(15);
    m_hotkeyAdd4->setValue(20);
    m_hotkeyAdd5->setValue(25);
    m_hotkeyAdd6->setValue(30);
    m_hotkeyAdd7->setValue(35);
    m_hotkeyAdd8->setValue(40);
    m_hotkeyAdd9->setValue(45);
    m_hotkeyAdd0->setValue(50);
    m_hotkeyClose25->setValue(25);
    m_hotkeyClose50->setValue(50);
    m_hotkeyClose75->setValue(75);
    m_hotkeyClose100->setValue(100);

    m_hostEdit->setText(settings.host());
    m_portSpin->setValue(settings.port());
    m_clientIdSpin->setValue(settings.clientId());
}

void SettingsDialog::saveSettings()
{
    Settings& settings = Settings::instance();

    settings.setBudget(m_budgetEdit->text().toDouble());
    settings.setAskOffset(m_askOffsetSpin->value());
    settings.setBidOffset(m_bidOffsetSpin->value());

    // TODO: Save hotkey percentages to Settings
    // For now, hotkeys are just displayed with defaults

    settings.setHost(m_hostEdit->text());
    settings.setPort(m_portSpin->value());
    settings.setClientId(m_clientIdSpin->value());
    settings.save();
}

void SettingsDialog::onAccepted()
{
    saveSettings();
    accept();
}