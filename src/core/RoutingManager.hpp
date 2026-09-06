// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class RoutingManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(int activePreset READ activePreset WRITE setActivePreset NOTIFY activePresetChanged)
    Q_PROPERTY(QStringList customDomains READ customDomains NOTIFY customDomainsChanged)
    Q_PROPERTY(QStringList selectedApps READ selectedApps NOTIFY selectedAppsChanged)
    Q_PROPERTY(int appRoutingMode READ appRoutingMode WRITE setAppRoutingMode NOTIFY appRoutingModeChanged)
    Q_PROPERTY(QVariantList advancedRules READ advancedRules NOTIFY advancedRulesChanged)

public:
    enum Preset {
        FullTunnel = 0,        // Route All Traffic via VPN
        SplitTunneling = 1,    // Selected domains & apps through VPN
        AdvancedRouting = 2    // Custom granular rules (Domain, IP-CIDR, Process, Port, GeoIP)
    };
    Q_ENUM(Preset)

    explicit RoutingManager(QObject *parent = nullptr);
    static RoutingManager *instance();

    Q_INVOKABLE int activePreset() const;
    Q_INVOKABLE void setActivePreset(int preset);

    QStringList customDomains() const;
    Q_INVOKABLE void addCustomDomain(const QString &domain);
    Q_INVOKABLE void removeCustomDomain(int index);
    Q_INVOKABLE void clearCustomDomains();

    QStringList selectedApps() const;
    int appRoutingMode() const;
    Q_INVOKABLE void setAppRoutingMode(int mode);

    Q_INVOKABLE void addApp(const QString &appName);
    Q_INVOKABLE void removeApp(int index);
    Q_INVOKABLE void clearApps();

    // Advanced Routing API
    QVariantList advancedRules() const;
    Q_INVOKABLE void addAdvancedRule(const QString &type, const QString &value, const QString &action);
    Q_INVOKABLE void deleteAdvancedRule(int index);
    Q_INVOKABLE void clearAdvancedRules();

    // Scans /proc and .desktop entries to find running applications
    Q_INVOKABLE QVariantList getRunningApplications();

    // Ensures route profiles exist in SQLite and applies the chosen preset
    void initializeRouteProfiles();

signals:
    void activePresetChanged(int preset);
    void customDomainsChanged();
    void selectedAppsChanged();
    void appRoutingModeChanged(int mode);
    void advancedRulesChanged();

private:
    void applyPreset(Preset preset, bool notifyAndRestart = false);
    void saveAppRoutingSettings();
    void loadAppRoutingSettings();

    static RoutingManager *s_instance;
    int m_preset = FullTunnel;
    QStringList m_customDomains;
    QStringList m_selectedApps;
    int m_appRoutingMode = 0; // 0 = Proxy selected, 1 = Bypass selected
    QVariantList m_advancedRules;

    int m_fullTunnelProfileId = -1;
    int m_splitTunnelProfileId = -1;
    int m_advancedRoutingProfileId = -1;
};
