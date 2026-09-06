// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "RoutingManager.hpp"
#include "ThroneEngine.hpp"
#include "ToastManager.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/RoutesRepo.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/database/entities/RouteProfile.h"
#include "3rdparty/throne/include/database/entities/RouteRule.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <unistd.h>
#include <algorithm>

RoutingManager *RoutingManager::s_instance = nullptr;

RoutingManager::RoutingManager(QObject *parent) : QObject(parent) {
    s_instance = this;
    m_customDomains = {
        QStringLiteral("instagram.com"),
        QStringLiteral("facebook.com"),
        QStringLiteral("twitter.com"),
        QStringLiteral("x.com"),
        QStringLiteral("notion.so"),
        QStringLiteral("openai.com"),
        QStringLiteral("chatgpt.com"),
        QStringLiteral("youtube.com")
    };
    m_selectedApps = {
        QStringLiteral("telegram-desktop"),
        QStringLiteral("firefox")
    };
}

RoutingManager *RoutingManager::instance() {
    return s_instance;
}

int RoutingManager::activePreset() const {
    return m_preset;
}

void RoutingManager::setActivePreset(int preset) {
    if (m_preset != preset) {
        m_preset = preset;
        applyPreset(static_cast<Preset>(preset), true);
        emit activePresetChanged(preset);
    }
}

QStringList RoutingManager::customDomains() const {
    return m_customDomains;
}

void RoutingManager::addCustomDomain(const QString &domain) {
    QString trimmed = domain.trimmed().toLower();
    if (!trimmed.isEmpty() && !m_customDomains.contains(trimmed)) {
        m_customDomains.append(trimmed);
        emit customDomainsChanged();
        saveAppRoutingSettings();
        if (m_preset == SplitTunneling) {
            applyPreset(SplitTunneling, true);
        }
    }
}

void RoutingManager::removeCustomDomain(int index) {
    if (index >= 0 && index < m_customDomains.size()) {
        m_customDomains.removeAt(index);
        emit customDomainsChanged();
        saveAppRoutingSettings();
        if (m_preset == SplitTunneling) {
            applyPreset(SplitTunneling, true);
        }
    }
}

void RoutingManager::clearCustomDomains() {
    if (!m_customDomains.isEmpty()) {
        m_customDomains.clear();
        emit customDomainsChanged();
        saveAppRoutingSettings();
        if (m_preset == SplitTunneling) {
            applyPreset(SplitTunneling, true);
        }
    }
}

QStringList RoutingManager::selectedApps() const {
    return m_selectedApps;
}

int RoutingManager::appRoutingMode() const {
    return m_appRoutingMode;
}

void RoutingManager::setAppRoutingMode(int mode) {
    if (m_appRoutingMode != mode) {
        m_appRoutingMode = mode;
        emit appRoutingModeChanged(mode);
        saveAppRoutingSettings();
        if (m_preset == SplitTunneling) {
            applyPreset(SplitTunneling, true);
        }
    }
}

void RoutingManager::addApp(const QString &appName) {
    QString trimmed = appName.trimmed();
    if (!trimmed.isEmpty() && !m_selectedApps.contains(trimmed, Qt::CaseInsensitive)) {
        m_selectedApps.append(trimmed);
        emit selectedAppsChanged();
        saveAppRoutingSettings();
        if (m_preset == SplitTunneling) {
            applyPreset(SplitTunneling, true);
        }
    }
}

void RoutingManager::removeApp(int index) {
    if (index >= 0 && index < m_selectedApps.size()) {
        m_selectedApps.removeAt(index);
        emit selectedAppsChanged();
        saveAppRoutingSettings();
        if (m_preset == SplitTunneling) {
            applyPreset(SplitTunneling, true);
        }
    }
}

void RoutingManager::clearApps() {
    if (!m_selectedApps.isEmpty()) {
        m_selectedApps.clear();
        emit selectedAppsChanged();
        saveAppRoutingSettings();
        if (m_preset == SplitTunneling) {
            applyPreset(SplitTunneling, true);
        }
    }
}

QVariantList RoutingManager::advancedRules() const {
    return m_advancedRules;
}

void RoutingManager::addAdvancedRule(const QString &type, const QString &value, const QString &action) {
    QString trimmedVal = value.trimmed();
    if (trimmedVal.isEmpty()) return;

    QVariantMap rule;
    rule[QStringLiteral("type")] = type.trimmed();
    rule[QStringLiteral("value")] = trimmedVal;
    rule[QStringLiteral("action")] = action.trimmed();

    m_advancedRules.append(rule);
    emit advancedRulesChanged();
    saveAppRoutingSettings();

    if (m_preset == AdvancedRouting) {
        applyPreset(AdvancedRouting, true);
    }
}

void RoutingManager::deleteAdvancedRule(int index) {
    if (index >= 0 && index < m_advancedRules.size()) {
        m_advancedRules.removeAt(index);
        emit advancedRulesChanged();
        saveAppRoutingSettings();

        if (m_preset == AdvancedRouting) {
            applyPreset(AdvancedRouting, true);
        }
    }
}

void RoutingManager::clearAdvancedRules() {
    if (!m_advancedRules.isEmpty()) {
        m_advancedRules.clear();
        emit advancedRulesChanged();
        saveAppRoutingSettings();

        if (m_preset == AdvancedRouting) {
            applyPreset(AdvancedRouting, true);
        }
    }
}

QVariantList RoutingManager::getRunningApplications() {
    QVariantList result;

    static const QStringList blacklistKeywords = {
        QStringLiteral("daemon"), QStringLiteral("helper"), QStringLiteral("portal"),
        QStringLiteral("extractor"), QStringLiteral("launcher"), QStringLiteral("dbus"),
        QStringLiteral("pipewire"), QStringLiteral("systemd"), QStringLiteral("baloo"),
        QStringLiteral("at-spi"), QStringLiteral("bwrap"), QStringLiteral("agent"),
        QStringLiteral("polkit"), QStringLiteral("kded"), QStringLiteral("kwin"),
        QStringLiteral("xdg"), QStringLiteral("gvfs"), QStringLiteral("dconf"),
        QStringLiteral("beaxty"), QStringLiteral("sing-box"), QStringLiteral("xray"),
        QStringLiteral("sh"), QStringLiteral("bash"), QStringLiteral("zsh"),
        QStringLiteral("sleep"), QStringLiteral("ps"), QStringLiteral("grep"),
        QStringLiteral("cat"), QStringLiteral("wireplumber"), QStringLiteral("plasma"),
        QStringLiteral("kscreen"), QStringLiteral("powerdevil"), QStringLiteral("ksystemstats"),
        QStringLiteral("kaccess"), QStringLiteral("ksmserver"), QStringLiteral("kactivitymanager"),
        QStringLiteral("ksecretd"), QStringLiteral("xembed"), QStringLiteral("xsettings"),
        QStringLiteral("p11-kit"), QStringLiteral("kioworker"), QStringLiteral("worker"),
        QStringLiteral("server"), QStringLiteral("throne"), QStringLiteral("nvidia"),
        QStringLiteral("appimagelauncher"), QStringLiteral("sxhkd"), QStringLiteral("xwayland")
    };

    auto matchesBlacklist = [](const QString &str) -> bool {
        QString lower = str.toLower();
        for (const auto &kw : blacklistKeywords) {
            if (lower.contains(kw)) return true;
        }
        return false;
    };

    struct DesktopInfo {
        QString displayName;
        QString execBin;
        QString icon;
    };
    QMap<QString, DesktopInfo> desktopApps;
    QList<DesktopInfo> allDesktopList;
    QSet<QString> indexedExecs;

    QStringList appDirs = {
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/usr/local/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/applications"),
        QStringLiteral("/var/lib/flatpak/exports/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications")
    };

    for (const auto &dirPath : appDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        const auto files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const auto &file : files) {
            QFile f(dir.absoluteFilePath(file));
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

            QString name, exec, icon;
            bool noDisplay = false;
            bool inDesktopEntry = false;

            QTextStream in(&f);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line == QStringLiteral("[Desktop Entry]")) {
                    inDesktopEntry = true;
                    continue;
                } else if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                    inDesktopEntry = false;
                    continue;
                }
                if (!inDesktopEntry) continue;

                if (line.startsWith(QStringLiteral("Name=")) && name.isEmpty()) {
                    name = line.mid(5).trimmed();
                } else if (line.startsWith(QStringLiteral("Exec=")) && exec.isEmpty()) {
                    exec = line.mid(5).trimmed();
                } else if (line.startsWith(QStringLiteral("Icon=")) && icon.isEmpty()) {
                    icon = line.mid(5).trimmed();
                } else if (line == QStringLiteral("NoDisplay=true")) {
                    noDisplay = true;
                }
            }
            f.close();

            if (noDisplay || name.isEmpty() || exec.isEmpty()) continue;
            if (matchesBlacklist(name) || matchesBlacklist(exec)) continue;

            QString firstToken = exec.split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0);
            firstToken.remove(QLatin1Char('"'));
            firstToken.remove(QLatin1Char('\''));
            QString binName = QFileInfo(firstToken).fileName();
            if (binName.isEmpty()) continue;

            QString lowerBin = binName.toLower();
            if (matchesBlacklist(lowerBin)) continue;

            DesktopInfo dinfo{name, binName, icon};
            desktopApps.insert(lowerBin, dinfo);

            QString baseName = QFileInfo(file).completeBaseName().toLower();
            if (!baseName.isEmpty()) {
                desktopApps.insert(baseName, dinfo);
            }

            if (!indexedExecs.contains(lowerBin)) {
                indexedExecs.insert(lowerBin);
                allDesktopList.append(dinfo);
            }
        }
    }

    uid_t currentUid = getuid();
    QSet<QString> seenProcessKeys;
    QList<QVariantMap> runningApps;

    QDir procDir(QStringLiteral("/proc"));
    const auto entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const auto &entry : entries) {
        bool isPid = false;
        entry.toInt(&isPid);
        if (!isPid) continue;

        QString pidPath = QStringLiteral("/proc/") + entry;
        QFileInfo procInfo(pidPath);
        if (procInfo.ownerId() != currentUid) continue;

        QString exeLink = pidPath + QStringLiteral("/exe");
        QString exeTarget = QFile::symLinkTarget(exeLink);
        if (exeTarget.isEmpty()) continue;

        QString binName = QFileInfo(exeTarget).fileName();
        if (binName.isEmpty()) continue;

        QString lowerBin = binName.toLower();
        if (matchesBlacklist(lowerBin)) continue;

        QString comm;
        QFile commFile(pidPath + QStringLiteral("/comm"));
        if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            comm = QString::fromUtf8(commFile.readAll()).trimmed().toLower();
            commFile.close();
            if (matchesBlacklist(comm)) continue;
        }

        if (seenProcessKeys.contains(lowerBin)) continue;

        bool hasDesktopMatch = false;
        DesktopInfo matchedInfo;

        if (desktopApps.contains(lowerBin)) {
            matchedInfo = desktopApps.value(lowerBin);
            hasDesktopMatch = true;
        } else if (!comm.isEmpty() && desktopApps.contains(comm)) {
            matchedInfo = desktopApps.value(comm);
            hasDesktopMatch = true;
        }

        if (hasDesktopMatch) {
            seenProcessKeys.insert(lowerBin);
            seenProcessKeys.insert(matchedInfo.execBin.toLower());

            QVariantMap item;
            item[QStringLiteral("displayName")] = matchedInfo.displayName;
            item[QStringLiteral("processName")] = binName;
            if (!matchedInfo.icon.isEmpty()) {
                item[QStringLiteral("icon")] = matchedInfo.icon;
            }
            item[QStringLiteral("isRunning")] = true;
            runningApps.append(item);
        }
    }

    QList<QVariantMap> otherInstalledApps;
    for (const auto &dinfo : allDesktopList) {
        QString low = dinfo.execBin.toLower();
        if (seenProcessKeys.contains(low)) continue;
        seenProcessKeys.insert(low);

        QVariantMap item;
        item[QStringLiteral("displayName")] = dinfo.displayName;
        item[QStringLiteral("processName")] = dinfo.execBin;
        if (!dinfo.icon.isEmpty()) {
            item[QStringLiteral("icon")] = dinfo.icon;
        }
        item[QStringLiteral("isRunning")] = false;
        otherInstalledApps.append(item);
    }

    std::sort(runningApps.begin(), runningApps.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("displayName")).toString().localeAwareCompare(
               b.value(QStringLiteral("displayName")).toString()) < 0;
    });

    std::sort(otherInstalledApps.begin(), otherInstalledApps.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("displayName")).toString().localeAwareCompare(
               b.value(QStringLiteral("displayName")).toString()) < 0;
    });

    for (const auto &app : runningApps) {
        result.append(app);
    }
    for (const auto &app : otherInstalledApps) {
        result.append(app);
    }

    return result;
}

void RoutingManager::saveAppRoutingSettings() {
    if (!Configs::dataManager) return;

    try {
        auto &db = Configs::dataManager->getDatabase();
        db.execThrow("CREATE TABLE IF NOT EXISTS app_routing (key TEXT PRIMARY KEY, value TEXT)");

        QJsonArray appsArr;
        for (const auto &app : m_selectedApps) appsArr.append(app);
        QString appsJson = QString::fromUtf8(QJsonDocument(appsArr).toJson(QJsonDocument::Compact));

        QJsonArray domainsArr;
        for (const auto &d : m_customDomains) domainsArr.append(d);
        QString domainsJson = QString::fromUtf8(QJsonDocument(domainsArr).toJson(QJsonDocument::Compact));

        QJsonArray advArr;
        for (const auto &v : m_advancedRules) {
            advArr.append(QJsonObject::fromVariantMap(v.toMap()));
        }
        QString advJson = QString::fromUtf8(QJsonDocument(advArr).toJson(QJsonDocument::Compact));

        db.execThrow("INSERT INTO app_routing (key, value) VALUES ('selected_apps', ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value", appsJson.toStdString());
        db.execThrow("INSERT INTO app_routing (key, value) VALUES ('app_routing_mode', ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value", std::to_string(m_appRoutingMode));
        db.execThrow("INSERT INTO app_routing (key, value) VALUES ('custom_domains', ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value", domainsJson.toStdString());
        db.execThrow("INSERT INTO app_routing (key, value) VALUES ('advanced_rules', ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value", advJson.toStdString());
        db.execThrow("INSERT INTO app_routing (key, value) VALUES ('active_preset', ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value", std::to_string(m_preset));
    } catch (const std::exception &e) {
        qWarning() << "[RoutingManager] Failed to save app routing settings:" << e.what();
    }
}

void RoutingManager::loadAppRoutingSettings() {
    if (!Configs::dataManager) return;

    try {
        auto &db = Configs::dataManager->getDatabase();
        db.execThrow("CREATE TABLE IF NOT EXISTS app_routing (key TEXT PRIMARY KEY, value TEXT)");
        auto query = db.queryThrow("SELECT key, value FROM app_routing");
        while (query->executeStep()) {
            std::string key = query->getColumn(0).getString();
            std::string val = query->getColumn(1).getString();
            if (key == "selected_apps") {
                auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(val));
                if (doc.isArray() && !doc.array().isEmpty()) {
                    m_selectedApps.clear();
                    for (const auto &v : doc.array()) {
                        m_selectedApps.append(v.toString());
                    }
                }
            } else if (key == "app_routing_mode") {
                m_appRoutingMode = std::stoi(val);
            } else if (key == "custom_domains") {
                auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(val));
                if (doc.isArray() && !doc.array().isEmpty()) {
                    m_customDomains.clear();
                    for (const auto &v : doc.array()) {
                        m_customDomains.append(v.toString());
                    }
                }
            } else if (key == "advanced_rules") {
                auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(val));
                if (doc.isArray()) {
                    m_advancedRules.clear();
                    for (const auto &v : doc.array()) {
                        m_advancedRules.append(v.toObject().toVariantMap());
                    }
                }
            } else if (key == "active_preset") {
                m_preset = std::stoi(val);
            }
        }
    } catch (const std::exception &e) {
        qWarning() << "[RoutingManager] Failed to load app routing settings:" << e.what();
    }
}

void RoutingManager::initializeRouteProfiles() {
    if (!Configs::dataManager || !Configs::dataManager->routesRepo || !Configs::dataManager->settingsRepo) {
        return;
    }

    loadAppRoutingSettings();

    auto repo = Configs::dataManager->routesRepo.get();
    auto ids = repo->GetAllRouteProfileIds();

    for (int id : ids) {
        auto p = repo->GetRouteProfile(id);
        if (!p) continue;
        if (p->name == QStringLiteral("Full Tunnel")) {
            m_fullTunnelProfileId = p->id;
        } else if (p->name == QStringLiteral("Split Tunneling")) {
            m_splitTunnelProfileId = p->id;
        } else if (p->name == QStringLiteral("Advanced Routing")) {
            m_advancedRoutingProfileId = p->id;
        }
    }

    // 1. Create Full Tunnel if not present
    if (m_fullTunnelProfileId < 0) {
        auto full = Configs::RoutesRepo::NewRouteProfile();
        full->name = QStringLiteral("Full Tunnel");
        full->defaultOutboundID = Configs::proxyID;
        repo->AddRouteProfile(full);
        m_fullTunnelProfileId = full->id;
    }

    // 2. Create Split Tunneling if not present
    if (m_splitTunnelProfileId < 0) {
        auto split = Configs::RoutesRepo::NewRouteProfile();
        split->name = QStringLiteral("Split Tunneling");
        split->defaultOutboundID = (m_appRoutingMode == 1) ? Configs::proxyID : Configs::directID;
        int targetOutbound = (m_appRoutingMode == 1) ? Configs::directID : Configs::proxyID;

        // Process rules first
        if (!m_selectedApps.isEmpty()) {
            auto procRule = std::make_shared<Configs::RouteRule>();
            procRule->name = QStringLiteral("Split Apps");
            procRule->type = (m_appRoutingMode == 1) ? Configs::simpleProcessNameBypass : Configs::simpleProcessNameProxy;
            procRule->process_name = m_selectedApps;
            procRule->outboundID = targetOutbound;
            procRule->action = QStringLiteral("route");
            split->Rules.append(procRule);
        }
        // Domain rules second
        if (!m_customDomains.isEmpty()) {
            auto domRule = std::make_shared<Configs::RouteRule>();
            domRule->name = QStringLiteral("Split Domains");
            domRule->type = (m_appRoutingMode == 1) ? Configs::simpleAddressBypass : Configs::simpleAddressProxy;
            domRule->domain_suffix = m_customDomains;
            domRule->outboundID = targetOutbound;
            domRule->action = QStringLiteral("route");
            split->Rules.append(domRule);
        }
        repo->AddRouteProfile(split);
        m_splitTunnelProfileId = split->id;
    }

    // 3. Create Advanced Routing if not present
    if (m_advancedRoutingProfileId < 0) {
        auto adv = Configs::RoutesRepo::NewRouteProfile();
        adv->name = QStringLiteral("Advanced Routing");
        adv->defaultOutboundID = Configs::directID;
        repo->AddRouteProfile(adv);
        m_advancedRoutingProfileId = adv->id;
    }

    // Restore saved preset
    int savedRouteId = Configs::dataManager->settingsRepo->current_route_id;
    if (savedRouteId == m_fullTunnelProfileId) {
        m_preset = FullTunnel;
    } else if (savedRouteId == m_splitTunnelProfileId) {
        m_preset = SplitTunneling;
    } else if (savedRouteId == m_advancedRoutingProfileId) {
        m_preset = AdvancedRouting;
    } else {
        if (m_preset == SplitTunneling) {
            Configs::dataManager->settingsRepo->current_route_id = m_splitTunnelProfileId;
        } else if (m_preset == AdvancedRouting) {
            Configs::dataManager->settingsRepo->current_route_id = m_advancedRoutingProfileId;
        } else {
            m_preset = FullTunnel;
            Configs::dataManager->settingsRepo->current_route_id = m_fullTunnelProfileId;
        }
        Configs::dataManager->settingsRepo->Save();
    }

    emit activePresetChanged(m_preset);
    emit appRoutingModeChanged(m_appRoutingMode);
    emit selectedAppsChanged();
    emit customDomainsChanged();
    emit advancedRulesChanged();

    // Set active profile without notification or restart on initial load
    applyPreset(static_cast<Preset>(m_preset), false);
}

void RoutingManager::applyPreset(Preset preset, bool notifyAndRestart) {
    if (!Configs::dataManager || !Configs::dataManager->settingsRepo || !Configs::dataManager->routesRepo) {
        return;
    }

    auto repo = Configs::dataManager->routesRepo.get();
    int targetProfileId = m_fullTunnelProfileId;

    switch (preset) {
        case FullTunnel: {
            targetProfileId = m_fullTunnelProfileId;
            if (auto full = repo->GetRouteProfile(m_fullTunnelProfileId)) {
                full->ResetRules();
                full->defaultOutboundID = Configs::proxyID;
                repo->Save(full);
            }
            break;
        }
        case SplitTunneling: {
            targetProfileId = m_splitTunnelProfileId;
            if (auto split = repo->GetRouteProfile(m_splitTunnelProfileId)) {
                split->ResetRules();
                split->defaultOutboundID = (m_appRoutingMode == 1) ? Configs::proxyID : Configs::directID;
                int targetOutbound = (m_appRoutingMode == 1) ? Configs::directID : Configs::proxyID;

                // 1. Process rules first
                if (!m_selectedApps.isEmpty()) {
                    auto procRule = std::make_shared<Configs::RouteRule>();
                    procRule->name = QStringLiteral("Split Apps");
                    procRule->type = (m_appRoutingMode == 1) ? Configs::simpleProcessNameBypass : Configs::simpleProcessNameProxy;
                    procRule->process_name = m_selectedApps;
                    procRule->outboundID = targetOutbound;
                    procRule->action = QStringLiteral("route");
                    split->Rules.append(procRule);
                }

                // 2. Domain rules second
                if (!m_customDomains.isEmpty()) {
                    auto domRule = std::make_shared<Configs::RouteRule>();
                    domRule->name = QStringLiteral("Split Domains");
                    domRule->type = (m_appRoutingMode == 1) ? Configs::simpleAddressBypass : Configs::simpleAddressProxy;
                    domRule->domain_suffix = m_customDomains;
                    domRule->outboundID = targetOutbound;
                    domRule->action = QStringLiteral("route");
                    split->Rules.append(domRule);
                }
                repo->Save(split);
            }
            break;
        }
        case AdvancedRouting: {
            targetProfileId = m_advancedRoutingProfileId;
            if (auto adv = repo->GetRouteProfile(m_advancedRoutingProfileId)) {
                adv->ResetRules();
                adv->defaultOutboundID = Configs::directID;

                for (const auto &itemVal : m_advancedRules) {
                    QVariantMap m = itemVal.toMap();
                    QString type = m.value(QStringLiteral("type")).toString();
                    QString val = m.value(QStringLiteral("value")).toString();
                    QString action = m.value(QStringLiteral("action")).toString().toLower();

                    int outId = Configs::directID;
                    if (action == QStringLiteral("proxy")) {
                        outId = Configs::proxyID;
                    } else if (action == QStringLiteral("block")) {
                        outId = Configs::blockID;
                    } else {
                        outId = Configs::directID;
                    }

                    auto rule = std::make_shared<Configs::RouteRule>();
                    rule->name = QStringLiteral("%1: %2").arg(type, val);
                    rule->type = Configs::custom;
                    rule->outboundID = outId;
                    rule->action = (outId == Configs::blockID) ? QStringLiteral("reject") : QStringLiteral("route");

                    if (type == QStringLiteral("Domain")) {
                        rule->domain_suffix.append(val);
                    } else if (type == QStringLiteral("IP-CIDR")) {
                        rule->ip_cidr.append(val);
                    } else if (type == QStringLiteral("Process")) {
                        rule->process_name.append(val);
                    } else if (type == QStringLiteral("Port")) {
                        rule->port.append(val);
                    } else if (type == QStringLiteral("GeoIP")) {
                        if (val.startsWith(QStringLiteral("geoip:"))) {
                            rule->rule_set.append(val);
                        } else {
                            rule->rule_set.append(QStringLiteral("geoip-") + val);
                        }
                    }
                    adv->Rules.append(rule);
                }
                repo->Save(adv);
            }
            break;
        }
    }

    if (targetProfileId > 0) {
        Configs::dataManager->settingsRepo->current_route_id = targetProfileId;
        Configs::dataManager->settingsRepo->Save();
        qDebug() << "[RoutingManager] Switched active route profile to ID:" << targetProfileId << "Preset:" << preset;
    }

    if (notifyAndRestart) {
        if (ThroneEngine::instance() && ThroneEngine::instance()->isConnected()) {
            ThroneEngine::instance()->restartConnection();
        }

        if (ToastManager::instance()) {
            switch (preset) {
                case FullTunnel:
                    ToastManager::instance()->showInfo(QStringLiteral("Режим маршрутизации: Весь трафик через VPN (Full Tunnel)"));
                    break;
                case SplitTunneling:
                    ToastManager::instance()->showInfo(QStringLiteral("Режим маршрутизации: Раздельный туннель (Split Tunneling)"));
                    break;
                case AdvancedRouting:
                    ToastManager::instance()->showInfo(QStringLiteral("Режим маршрутизации: Продвинутая маршрутизация (Advanced Routing)"));
                    break;
            }
        }
    }
}
