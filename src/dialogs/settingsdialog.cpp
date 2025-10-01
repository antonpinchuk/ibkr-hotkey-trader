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

    m_accountCombo = new QComboBox();
    m_accountCombo->addItem("DU1234567"); // TODO: Load from IBKR
    tradingLayout->addRow("Account:", m_accountCombo);

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