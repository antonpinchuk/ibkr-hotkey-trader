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
    hotkeysLayout->addRow("Shift+Ctrl+Alt+O:", m_hotkeyOpen100);

    m_hotkeyOpen50 = new QSpinBox();
    m_hotkeyOpen50->setRange(1, 100);
    m_hotkeyOpen50->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+P:", m_hotkeyOpen50);

    QLabel *addLabel = new QLabel("<b>Adding to Position:</b>");
    hotkeysLayout->addRow(addLabel);

    m_hotkeyAdd1 = new QSpinBox();
    m_hotkeyAdd1->setRange(1, 100);
    m_hotkeyAdd1->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+1:", m_hotkeyAdd1);

    m_hotkeyAdd2 = new QSpinBox();
    m_hotkeyAdd2->setRange(1, 100);
    m_hotkeyAdd2->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+2:", m_hotkeyAdd2);

    m_hotkeyAdd3 = new QSpinBox();
    m_hotkeyAdd3->setRange(1, 100);
    m_hotkeyAdd3->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+3:", m_hotkeyAdd3);

    m_hotkeyAdd4 = new QSpinBox();
    m_hotkeyAdd4->setRange(1, 100);
    m_hotkeyAdd4->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+4:", m_hotkeyAdd4);

    m_hotkeyAdd5 = new QSpinBox();
    m_hotkeyAdd5->setRange(1, 100);
    m_hotkeyAdd5->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+5:", m_hotkeyAdd5);

    m_hotkeyAdd6 = new QSpinBox();
    m_hotkeyAdd6->setRange(1, 100);
    m_hotkeyAdd6->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+6:", m_hotkeyAdd6);

    m_hotkeyAdd7 = new QSpinBox();
    m_hotkeyAdd7->setRange(1, 100);
    m_hotkeyAdd7->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+7:", m_hotkeyAdd7);

    m_hotkeyAdd8 = new QSpinBox();
    m_hotkeyAdd8->setRange(1, 100);
    m_hotkeyAdd8->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+8:", m_hotkeyAdd8);

    m_hotkeyAdd9 = new QSpinBox();
    m_hotkeyAdd9->setRange(1, 100);
    m_hotkeyAdd9->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+9:", m_hotkeyAdd9);

    m_hotkeyAdd0 = new QSpinBox();
    m_hotkeyAdd0->setRange(1, 100);
    m_hotkeyAdd0->setSuffix("%");
    hotkeysLayout->addRow("Shift+Ctrl+Alt+0:", m_hotkeyAdd0);

    QLabel *closeLabel = new QLabel("<b>Closing Positions:</b>");
    hotkeysLayout->addRow(closeLabel);

    m_hotkeyClose25 = new QSpinBox();
    m_hotkeyClose25->setRange(1, 100);
    m_hotkeyClose25->setSuffix("%");
    hotkeysLayout->addRow("Ctrl+Alt+V:", m_hotkeyClose25);

    m_hotkeyClose50 = new QSpinBox();
    m_hotkeyClose50->setRange(1, 100);
    m_hotkeyClose50->setSuffix("%");
    hotkeysLayout->addRow("Ctrl+Alt+C:", m_hotkeyClose50);

    m_hotkeyClose75 = new QSpinBox();
    m_hotkeyClose75->setRange(1, 100);
    m_hotkeyClose75->setSuffix("%");
    hotkeysLayout->addRow("Ctrl+Alt+X:", m_hotkeyClose75);

    m_hotkeyClose100 = new QSpinBox();
    m_hotkeyClose100->setRange(1, 100);
    m_hotkeyClose100->setSuffix("%");
    hotkeysLayout->addRow("Ctrl+Alt+Z:", m_hotkeyClose100);

    m_tabWidget->addTab(hotkeysTab, "Hotkeys");

    // Connection tab
    QWidget *connectionTab = new QWidget();
    QVBoxLayout *connectionMainLayout = new QVBoxLayout(connectionTab);

    // IBKR TWS Client section
    QLabel *twsLabel = new QLabel("<b>IBKR TWS Client</b>");
    connectionMainLayout->addWidget(twsLabel);

    QWidget *twsWidget = new QWidget();
    QFormLayout *twsLayout = new QFormLayout(twsWidget);
    twsLayout->setContentsMargins(20, 0, 0, 0);

    m_hostEdit = new QLineEdit();
    twsLayout->addRow("Host:", m_hostEdit);

    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    twsLayout->addRow("Port:", m_portSpin);

    m_clientIdSpin = new QSpinBox();
    m_clientIdSpin->setRange(0, 999);  // 0 is required for binding manual TWS orders
    twsLayout->addRow("Client ID:", m_clientIdSpin);

    connectionMainLayout->addWidget(twsWidget);
    connectionMainLayout->addSpacing(20);

    // Remote Control Server section
    QLabel *remoteLabel = new QLabel("<b>Remote Control Server</b>");
    connectionMainLayout->addWidget(remoteLabel);

    QWidget *remoteWidget = new QWidget();
    QFormLayout *remoteLayout = new QFormLayout(remoteWidget);
    remoteLayout->setContentsMargins(20, 0, 0, 0);

    m_remoteControlPortSpin = new QSpinBox();
    m_remoteControlPortSpin->setRange(1, 65535);
    remoteLayout->addRow("Port:", m_remoteControlPortSpin);

    connectionMainLayout->addWidget(remoteWidget);
    connectionMainLayout->addStretch();

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
    m_remoteControlPortSpin->setValue(settings.remoteControlPort());
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
    settings.setRemoteControlPort(m_remoteControlPortSpin->value());

    settings.save();
}

void SettingsDialog::onAccepted()
{
    saveSettings();
    accept();
}