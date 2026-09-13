// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QSet>
#include <QTimer>

namespace Configs_network { struct HTTPResponse; }

class ConfigAdapter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(QVariantList groups READ groups NOTIFY groupsChanged)
    Q_PROPERTY(int selectedServerId READ selectedServerId NOTIFY selectedServerIdChanged)
    Q_PROPERTY(QString selectedServerName READ selectedServerName NOTIFY selectedServerChanged)
    Q_PROPERTY(QString selectedServerType READ selectedServerType NOTIFY selectedServerChanged)
    Q_PROPERTY(int selectedServerPing READ selectedServerPing NOTIFY selectedServerPingChanged)
    Q_PROPERTY(QString selectedServerCountry READ selectedServerCountry NOTIFY selectedServerChanged)
    Q_PROPERTY(int serverCount READ serverCount NOTIFY serversChanged)
    Q_PROPERTY(int autoUpdateSubsMode READ autoUpdateSubsMode WRITE setAutoUpdateSubsMode NOTIFY autoUpdateSubsModeChanged)
    Q_PROPERTY(int serverSortMode READ serverSortMode WRITE setServerSortMode NOTIFY serverSortModeChanged)

public:
    explicit ConfigAdapter(QObject *parent = nullptr);
    ~ConfigAdapter() override;
    static ConfigAdapter *instance();
    bool importing() const { return m_importing; }
    bool refreshing() const { return !m_updatingGroups.isEmpty(); }

    QVariantList servers() const;
    QVariantList groups() const;
    int selectedServerId() const;
    QString selectedServerName() const;
    QString selectedServerType() const;
    int selectedServerPing() const;
    QString selectedServerCountry() const;
    int serverCount() const;
    int autoUpdateSubsMode() const;
    void setAutoUpdateSubsMode(int mode);
    int serverSortMode() const;
    void setServerSortMode(int mode);

    Q_INVOKABLE void selectServer(int profileId);
    Q_INVOKABLE void deleteServer(int profileId);
    Q_INVOKABLE void deleteGroup(int groupId);
    Q_INVOKABLE void renameGroup(int groupId, const QString &newName);
    Q_INVOKABLE void updateGroup(int groupId, bool silent = false);
    Q_INVOKABLE QVariantList serversForGroup(int groupId) const;
    Q_INVOKABLE QVariantList customServers() const;
    Q_INVOKABLE void refreshSubscriptions(bool silent = false);
    Q_INVOKABLE void importSubscription(const QString &urlOrContent, const QString &groupName = "");
    Q_INVOKABLE void importFromClipboard();
    Q_INVOKABLE QString getClipboardText() const;
    Q_INVOKABLE void reloadServers();
    Q_INVOKABLE void updateServerPing(int profileId, int pingMs);
    void checkScheduledSubscriptionUpdates();

    // Opt-in only. Real users start with an empty list and an explicit prompt to
    // import a subscription; the sample nodes exist for UI tests and screenshots.
    // Enabled by --demo-data or BEAXTY_DEMO_DATA=1.
    void setDemoDataEnabled(bool enabled);
    static bool demoDataRequestedFromEnv();

signals:
    void importingChanged();
    void refreshingChanged();
    void serversChanged();
    void groupsChanged();
    void selectedServerIdChanged(int id);
    void selectedServerChanged();
    void selectedServerPingChanged(int ping);
    void autoUpdateSubsModeChanged(int mode);
    void serverSortModeChanged(int mode);
    void importFinished(bool success, int count, const QString &message);

private:
    void finishImport(const QString &trimmed, const QString &groupName,
                      const Configs_network::HTTPResponse &response);
    bool m_importing = false;
    QSet<int> m_updatingGroups;
    void ensureDefaultDemoServers();
    // Groups that came from a subscription URL, i.e. not manually added nodes.
    static QSet<int> subscriptionGroupIds();

    static ConfigAdapter *s_instance;
    QVariantList m_servers;
    int m_selectedServerId = -1;
    QMap<int, int> m_pings;
    bool m_demoDataEnabled = false;
    int m_autoUpdateSubsMode = 1;
    int m_serverSortMode = 0;
    bool m_initialAutoUpdateTriggered = false;
    bool m_preferencesLoaded = false;
    QTimer m_pingPublishTimer;
    QTimer *m_autoUpdateTimer = nullptr;
};
