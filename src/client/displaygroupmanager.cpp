#include "client/displaygroupmanager.h"
#include "client/ibkrclient.h"
#include "models/settings.h"
#include "utils/logger.h"

DisplayGroupManager::DisplayGroupManager(IBKRClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_nextReqId(20000) // Start from 20000 to avoid conflicts with other requests
{
    // Connect to Display Groups callbacks
    connect(m_client, &IBKRClient::displayGroupListReceived,
            this, &DisplayGroupManager::onDisplayGroupListReceived);
    connect(m_client, &IBKRClient::displayGroupUpdatedReceived,
            this, &DisplayGroupManager::onDisplayGroupUpdated);
}

void DisplayGroupManager::updateActiveSymbol(const QString& symbol, const QString& exchange, int conId)
{
    Settings& settings = Settings::instance();
    int groupId = settings.displayGroupId();

    // Check if Display Groups are enabled (groupId == 0 means "No Group")
    if (groupId == 0) {
        LOG_DEBUG("Display Groups disabled (No Group selected), skipping TWS UI sync");
        return;
    }

    if (!m_client || !m_client->isConnected()) {
        LOG_WARNING("Cannot update Display Group: not connected to TWS");
        return;
    }
    QString contractInfo = formatContractInfo(symbol, exchange, conId);

    LOG_DEBUG(QString("Syncing TWS Display Group %1: %2 (%3)").arg(groupId).arg(symbol).arg(contractInfo));

    int reqId = m_nextReqId++;

    // Subscribe to group events (if not already subscribed, TWS will handle it)
    m_client->subscribeToGroupEvents(reqId, groupId);

    // Update the display group with new contract
    m_client->updateDisplayGroup(reqId, contractInfo);
}

void DisplayGroupManager::queryDisplayGroups()
{
    if (!m_client || !m_client->isConnected()) {
        LOG_WARNING("Cannot query Display Groups: not connected to TWS");
        return;
    }

    int reqId = m_nextReqId++;
    m_client->queryDisplayGroups(reqId);
}

void DisplayGroupManager::onDisplayGroupListReceived(int reqId, const QString& groups)
{
    if (groups.isEmpty()) {
        LOG_WARNING(QString("TWS Display Groups query returned empty list (reqId=%1)").arg(reqId));
        LOG_WARNING("Make sure TWS windows have Display Groups enabled:");
        LOG_WARNING("  1. Open Market Data / Level 2 / News windows in TWS");
        LOG_WARNING("  2. In each window: View → Display Groups → Select a group (e.g., Group 1)");
        LOG_WARNING("  3. The link icon should become colored (not gray)");
    } else {
        LOG_INFO(QString("TWS Display Groups available (reqId=%1): %2").arg(reqId).arg(groups));
        QStringList groupList = groups.split("|");
        LOG_INFO(QString("  → Found %1 active Display Group(s)").arg(groupList.size()));
        for (const QString& groupId : groupList) {
            LOG_INFO(QString("    - Group ID: %1").arg(groupId));
        }
    }
    emit displayGroupsQueried(groups);
}

void DisplayGroupManager::onDisplayGroupUpdated(int reqId, const QString& contractInfo)
{
    Q_UNUSED(reqId);
    Q_UNUSED(contractInfo);
    // Display group update confirmed - no logging needed (happens frequently)
    emit displayGroupUpdateConfirmed(contractInfo);
}

QString DisplayGroupManager::formatContractInfo(const QString& symbol, const QString& exchange, int conId)
{
    // Format: "conId@SMART" - TWS Display Groups always use SMART routing
    // Examples: "8314@SMART", "365207014@SMART"
    // Note: Using specific exchange (NYSE, NASDAQ) doesn't work reliably with Display Groups

    Q_UNUSED(symbol);
    Q_UNUSED(exchange);

    // If we have contract ID, use it with SMART routing
    if (conId > 0) {
        return QString("%1@SMART").arg(conId);
    }

    // Fallback: use symbol with SMART (less reliable, requires unambiguous symbol in TWS)
    return QString("%1@SMART").arg(symbol);
}
