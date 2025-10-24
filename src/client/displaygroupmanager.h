#ifndef DISPLAYGROUPMANAGER_H
#define DISPLAYGROUPMANAGER_H

#include <QObject>
#include <QString>

class IBKRClient;
class Settings;

/**
 * @brief Manages TWS Display Groups for UI synchronization
 *
 * Display Groups allow synchronizing the active symbol across TWS windows
 * (Market Data, Level 2, News, Charts) by updating a color-coded group.
 *
 * Usage:
 * 1. In TWS, assign windows to a color group (e.g., Group 1 = pink chain)
 * 2. Enable Display Groups in Settings and select the group ID
 * 3. When ticker changes in the app, DisplayGroupManager updates TWS windows
 */
class DisplayGroupManager : public QObject
{
    Q_OBJECT

public:
    explicit DisplayGroupManager(IBKRClient* client, QObject* parent = nullptr);

    /**
     * @brief Update the active symbol in TWS Display Group
     * @param symbol Stock symbol (e.g., "AAPL")
     * @param exchange Primary exchange (e.g., "NASDAQ", "NYSE")
     * @param conId Contract ID from TWS (optional, can be 0)
     *
     * If Display Groups are enabled in settings, this will update the TWS
     * windows assigned to the configured group to show the specified symbol.
     */
    void updateActiveSymbol(const QString& symbol, const QString& exchange, int conId = 0);

    /**
     * @brief Query available Display Groups from TWS
     *
     * This is useful for debugging or allowing users to select which group to use.
     * Results are received via displayGroupsQueried signal.
     */
    void queryDisplayGroups();

private slots:
    void onDisplayGroupListReceived(int reqId, const QString& groups);
    void onDisplayGroupUpdated(int reqId, const QString& contractInfo);

signals:
    void displayGroupsQueried(const QString& groups);
    void displayGroupUpdateConfirmed(const QString& contractInfo);

private:
    IBKRClient* m_client;
    int m_nextReqId;

    QString formatContractInfo(const QString& symbol, const QString& exchange, int conId);
};

#endif // DISPLAYGROUPMANAGER_H
