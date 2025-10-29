#include "models/settings.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QVariant>

Settings& Settings::instance()
{
    static Settings instance;
    return instance;
}

Settings::Settings()
{
    initDatabase();
    initDefaults();
    load();
}

Settings::~Settings()
{
    save();
    if (m_db.isOpen()) {
        m_db.close();
    }
}

QString Settings::getDatabasePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("ibkr_hotkey_trader.db");
}

void Settings::initDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(getDatabasePath());

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return;
    }

    // Create settings table
    QSqlQuery query(m_db);
    bool success = query.exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "    key TEXT PRIMARY KEY,"
        "    value TEXT"
        ")"
    );

    if (!success) {
        qCritical() << "Failed to create settings table:" << query.lastError().text();
    }

    qDebug() << "Database initialized at:" << getDatabasePath();
}

void Settings::initDefaults()
{
    m_budget = 1000.0;
    m_askOffset = 10;
    m_bidOffset = 10;

    // Default hotkey percentages
    m_hotkeyPercents["Cmd+O"] = 100;
    m_hotkeyPercents["Cmd+P"] = 50;
    m_hotkeyPercents["Cmd+1"] = 5;
    m_hotkeyPercents["Cmd+2"] = 10;
    m_hotkeyPercents["Cmd+3"] = 15;
    m_hotkeyPercents["Cmd+4"] = 20;
    m_hotkeyPercents["Cmd+5"] = 25;
    m_hotkeyPercents["Cmd+6"] = 30;
    m_hotkeyPercents["Cmd+7"] = 35;
    m_hotkeyPercents["Cmd+8"] = 40;
    m_hotkeyPercents["Cmd+9"] = 45;
    m_hotkeyPercents["Cmd+0"] = 50;
    m_hotkeyPercents["Cmd+Z"] = 100;
    m_hotkeyPercents["Cmd+X"] = 75;
    m_hotkeyPercents["Cmd+C"] = 50;
    m_hotkeyPercents["Cmd+V"] = 25;

    m_host = "127.0.0.1";
    m_port = 7496;
    m_clientId = 0;  // Client ID 0 is required for binding manual orders
    m_remoteControlPort = 8496;
    m_displayGroupId = 0;  // 0 = disabled (No Group)
    m_showCancelledOrders = false;  // Hidden by default
    m_orderType = "LMT";  // Default to limit orders
}

void Settings::setBudget(double budget)
{
    m_budget = budget;
}

void Settings::setAskOffset(int offset)
{
    m_askOffset = offset;
}

void Settings::setBidOffset(int offset)
{
    m_bidOffset = offset;
}

int Settings::hotkeyPercent(const QString& key) const
{
    return m_hotkeyPercents.value(key, 0);
}

void Settings::setHotkeyPercent(const QString& key, int percent)
{
    m_hotkeyPercents[key] = percent;
}

void Settings::setHost(const QString& host)
{
    m_host = host;
}

void Settings::setPort(int port)
{
    m_port = port;
}

void Settings::setClientId(int id)
{
    m_clientId = id;
}

void Settings::setRemoteControlPort(int port)
{
    m_remoteControlPort = port;
}

void Settings::setDisplayGroupId(int groupId)
{
    m_displayGroupId = groupId;
}

void Settings::setShowCancelledOrders(bool show)
{
    m_showCancelledOrders = show;
}

void Settings::setOrderType(const QString& type)
{
    m_orderType = type;
}

QString Settings::getValue(const QString& key, const QString& defaultValue) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = ?");
    query.addBindValue(key);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return defaultValue;
}

void Settings::setValue(const QString& key, const QString& value)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);

    if (!query.exec()) {
        qWarning() << "Failed to save setting" << key << ":" << query.lastError().text();
    }
}

void Settings::load()
{
    m_budget = getValue("budget", "1000.0").toDouble();
    m_askOffset = getValue("ask_offset", "10").toInt();
    m_bidOffset = getValue("bid_offset", "10").toInt();

    // Load hotkey percentages
    for (auto it = m_hotkeyPercents.begin(); it != m_hotkeyPercents.end(); ++it) {
        QString key = "hotkey_" + it.key();
        it.value() = getValue(key, QString::number(it.value())).toInt();
    }

    m_host = getValue("host", "127.0.0.1");
    m_port = getValue("port", "7496").toInt();
    m_clientId = getValue("client_id", "0").toInt();  // Default to 0 for manual order binding
    m_remoteControlPort = getValue("remote_control_port", "8496").toInt();
    m_displayGroupId = getValue("display_group_id", "0").toInt();
    m_showCancelledOrders = getValue("show_cancelled_orders", "0").toInt() == 1;
    m_orderType = getValue("order_type", "LMT");
}

void Settings::save()
{
    setValue("budget", QString::number(m_budget));
    setValue("ask_offset", QString::number(m_askOffset));
    setValue("bid_offset", QString::number(m_bidOffset));

    // Save hotkey percentages
    for (auto it = m_hotkeyPercents.constBegin(); it != m_hotkeyPercents.constEnd(); ++it) {
        QString key = "hotkey_" + it.key();
        setValue(key, QString::number(it.value()));
    }

    setValue("host", m_host);
    setValue("port", QString::number(m_port));
    setValue("client_id", QString::number(m_clientId));
    setValue("remote_control_port", QString::number(m_remoteControlPort));
    setValue("display_group_id", QString::number(m_displayGroupId));
    setValue("show_cancelled_orders", m_showCancelledOrders ? "1" : "0");
    setValue("order_type", m_orderType);
}
