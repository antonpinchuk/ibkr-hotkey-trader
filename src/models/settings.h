#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QSqlDatabase>
#include <QMap>

class Settings
{
public:
    static Settings& instance();

    // Trading settings
    double budget() const { return m_budget; }
    void setBudget(double budget);

    // Limits
    int askOffset() const { return m_askOffset; }
    void setAskOffset(int offset);

    int bidOffset() const { return m_bidOffset; }
    void setBidOffset(int offset);

    // Hotkeys percentages
    int hotkeyPercent(const QString& key) const;
    void setHotkeyPercent(const QString& key, int percent);

    // Connection settings
    QString host() const { return m_host; }
    void setHost(const QString& host);

    int port() const { return m_port; }
    void setPort(int port);

    int clientId() const { return m_clientId; }
    void setClientId(int id);

    // Remote Control settings
    int remoteControlPort() const { return m_remoteControlPort; }
    void setRemoteControlPort(int port);

    // View settings
    bool showCancelledOrders() const { return m_showCancelledOrders; }
    void setShowCancelledOrders(bool show);

    void load();
    void save();

private:
    Settings();
    ~Settings();
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    QSqlDatabase m_db;

    double m_budget;
    int m_askOffset;
    int m_bidOffset;
    QMap<QString, int> m_hotkeyPercents;
    QString m_host;
    int m_port;
    int m_clientId;
    int m_remoteControlPort;
    bool m_showCancelledOrders;

    void initDatabase();
    void initDefaults();
    QString getDatabasePath() const;
    QString getValue(const QString& key, const QString& defaultValue) const;
    void setValue(const QString& key, const QString& value);
};

#endif // SETTINGS_H
