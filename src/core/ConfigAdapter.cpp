// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "ConfigAdapter.hpp"
#include "ToastManager.hpp"
#include "ThroneEngine.hpp"
#include <QJsonDocument>
#include <algorithm>
#include <QScopeGuard>
#include "AppPrefs.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/global/HTTPRequestHelper.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/database/GroupsRepo.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/configs/sub/SubscriptionParser.hpp"
#include "3rdparty/throne/include/configs/outbounds/vless.h"
#include "3rdparty/throne/include/configs/outbounds/shadowsocks.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QUrl>
#include <QDateTime>

#include <QDebug>

ConfigAdapter *ConfigAdapter::s_instance = nullptr;

ConfigAdapter::ConfigAdapter(QObject *parent) : QObject(parent) {
    s_instance = this;
    m_autoUpdateSubsMode = AppPrefs::getInt(QStringLiteral("auto_update_subs_mode"), 1);
    m_serverSortMode = AppPrefs::getInt(QStringLiteral("server_sort_mode"), 0);
    m_pingPublishTimer.setSingleShot(true);
    m_pingPublishTimer.setInterval(50);
    connect(&m_pingPublishTimer, &QTimer::timeout, this, &ConfigAdapter::serversChanged);
    m_autoUpdateTimer = new QTimer(this);
    connect(m_autoUpdateTimer, &QTimer::timeout, this, &ConfigAdapter::checkScheduledSubscriptionUpdates);
    m_autoUpdateTimer->start(30 * 60 * 1000);
}

ConfigAdapter::~ConfigAdapter() {
    if (s_instance == this) s_instance = nullptr;
}

ConfigAdapter *ConfigAdapter::instance() {
    return s_instance;
}

bool ConfigAdapter::demoDataRequestedFromEnv() {
    return qEnvironmentVariable("BEAXTY_DEMO_DATA") == QStringLiteral("1");
}

void ConfigAdapter::setDemoDataEnabled(bool enabled) {
    m_demoDataEnabled = enabled;
}

void ConfigAdapter::reloadServers() {
    if (!Configs::dataManager || !Configs::dataManager->profilesRepo) {
        return;
    }

    if (!m_preferencesLoaded) {
        m_preferencesLoaded = true;
        m_autoUpdateSubsMode = std::clamp(AppPrefs::getInt(QStringLiteral("auto_update_subs_mode"), 1), 0, 2);
        m_serverSortMode = std::clamp(AppPrefs::getInt(QStringLiteral("server_sort_mode"), 0), 0, 3);
        emit autoUpdateSubsModeChanged(m_autoUpdateSubsMode);
        emit serverSortModeChanged(m_serverSortMode);
    }
    auto repo = Configs::dataManager->profilesRepo.get();
    auto ids = repo->GetAllProfileIds();

    if (ids.isEmpty() && m_demoDataEnabled) {
        ensureDefaultDemoServers();
        ids = repo->GetAllProfileIds();
    }

    QVariantList list;
    for (const auto &profile : repo->GetProfileBatch(ids)) {
        if (!profile || !profile->outbound) continue;

        QVariantMap item;
        item["id"] = profile->id;
        item["gid"] = profile->gid;
        QString pName = profile->name.trimmed();
        if (pName.isEmpty() && profile->outbound && !profile->outbound->name.trimmed().isEmpty()) {
            pName = profile->outbound->name.trimmed();
            profile->name = pName;
        }
        if (pName.isEmpty() && profile->outbound && !profile->outbound->server.trimmed().isEmpty()) {
            pName = QStringLiteral("%1 %2").arg(profile->type.toUpper(), profile->outbound->server.trimmed());
            profile->name = pName;
        }
        item["name"] = pName.isEmpty() ? QString("Server #%1").arg(profile->id) : pName;
        item["type"] = profile->type.toUpper();
        item["address"] = profile->outbound->server;
        item["port"] = profile->outbound->server_port;
        item["country"] = profile->test_country;
        // 0 means "never measured"; the UI renders that as "--" rather than a number
        // we made up. Latency is only ever a real measurement.
        item["ping"] = m_pings.value(profile->id, profile->latency > 0 ? profile->latency : 0);
        item["isCurrent"] = (profile->id == m_selectedServerId);

        list.append(item);
    }

    m_servers = list;

    // Drop stale selection and ping entries when profiles disappear.
    const int previousSelection = m_selectedServerId;
    QSet<int> available;
    for (const auto &row : m_servers) available.insert(row.toMap()["id"].toInt());
    for (auto it = m_pings.begin(); it != m_pings.end();) {
        if (!available.contains(it.key())) it = m_pings.erase(it);
        else ++it;
    }
    if (!available.contains(m_selectedServerId)) m_selectedServerId = -1;
    // Restore the last used node, else fall back to the first one available.
    if (m_selectedServerId < 0 && !m_servers.isEmpty()) {
        int restored = -1;
        if (Configs::dataManager && Configs::dataManager->settingsRepo) {
            int rememberId = Configs::dataManager->settingsRepo->remember_id;
            if (rememberId >= 0) {
                for (const auto &var : m_servers) {
                    if (var.toMap()["id"].toInt() == rememberId) {
                        restored = rememberId;
                        break;
                    }
                }
            }
        }
        m_selectedServerId = (restored >= 0) ? restored : m_servers.first().toMap()["id"].toInt();

        // Keep the "isCurrent" flags consistent with the selection we just made.
        for (auto &var : m_servers) {
            auto map = var.toMap();
            map["isCurrent"] = (map["id"].toInt() == m_selectedServerId);
            var = map;
        }

    }
    if (previousSelection != m_selectedServerId) {
        if (Configs::dataManager->settingsRepo) {
            Configs::dataManager->settingsRepo->remember_id = m_selectedServerId;
            Configs::dataManager->settingsRepo->Save();
        }
        emit selectedServerIdChanged(m_selectedServerId);
    }
    emit selectedServerChanged();
    emit selectedServerPingChanged(selectedServerPing());
    emit serversChanged();
    emit groupsChanged();

    if (!m_initialAutoUpdateTriggered) {
        m_initialAutoUpdateTriggered = true;
        if (m_autoUpdateSubsMode >= 1) {
            QTimer::singleShot(1500, this, [this]() {
                if (m_autoUpdateSubsMode == 0) return;
                qInfo() << "[ConfigAdapter] Running startup silent subscription refresh";
                refreshSubscriptions(/*silent=*/true);
            });
        }
    }
}

static QString packGroupInfo(const QString &baseInfo, const QString &announceText, const QString &supportUrl, const QString &webUrl, int intervalHours = -1) {
    QStringList kept;
    if (!baseInfo.isEmpty()) {
        for (const QString &part : baseInfo.split(';', Qt::SkipEmptyParts)) {
            QString p = part.trimmed();
            int eq = p.indexOf('=');
            if (eq > 0) {
                QString k = p.left(eq).trimmed().toLower();
                if (k == QStringLiteral("upload") || k == QStringLiteral("download") ||
                    k == QStringLiteral("total") || k == QStringLiteral("expire")) {
                    kept.append(p);
                } else if (k == QStringLiteral("announce_b64") && announceText.isEmpty()) {
                    kept.append(p);
                } else if ((k == QStringLiteral("support") || k == QStringLiteral("support_url") || k == QStringLiteral("support-url")) && supportUrl.isEmpty()) {
                    kept.append(p);
                } else if ((k == QStringLiteral("web") || k == QStringLiteral("web_url") || k == QStringLiteral("profile-web-page-url")) && webUrl.isEmpty()) {
                    kept.append(p);
                } else if ((k == QStringLiteral("interval_hours") || k == QStringLiteral("profile-update-interval")) && intervalHours <= 0) {
                    kept.append(p);
                }
            }
        }
    }
    if (!announceText.isEmpty()) {
        QString b64 = QString::fromLatin1(announceText.toUtf8().toBase64());
        kept.append(QStringLiteral("announce_b64=%1").arg(b64));
    }
    if (!supportUrl.isEmpty()) {
        kept.append(QStringLiteral("support=%1").arg(supportUrl.trimmed()));
    }
    if (!webUrl.isEmpty()) {
        kept.append(QStringLiteral("web=%1").arg(webUrl.trimmed()));
    }
    if (intervalHours > 0) {
        kept.append(QStringLiteral("interval_hours=%1").arg(intervalHours));
    }
    return kept.join(QStringLiteral("; "));
}

void ConfigAdapter::ensureDefaultDemoServers() {
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo || !Configs::dataManager->profilesRepo) {
        qWarning() << "[ConfigAdapter] Database manager not ready; skipping demo data seeding.";
        return;
    }

    auto gRepo = Configs::dataManager->groupsRepo.get();
    auto pRepo = Configs::dataManager->profilesRepo.get();

    // Reuse the "Default" group that Configs::initDB() creates, rather than adding
    // a second one. NewGroup() leaves id at -1 and AddGroup() assigns the real id,
    // so "already persisted" is id >= 0, not id == 0.
    std::shared_ptr<Configs::Group> group;
    auto existingGids = gRepo->GetAllGroupIds();
    for (int id : existingGids) {
        auto g = gRepo->GetGroup(id);
        if (g && (g->name == QStringLiteral("Default") || g->name.isEmpty())) {
            group = g;
            break;
        }
    }

    bool isNewGroup = false;
    if (!group) {
        group = Configs::GroupsRepo::NewGroup();
        isNewGroup = true;
    }
    group->name = QStringLiteral("beaxty VPN 🪽");
    group->url = QStringLiteral("https://sub.beaxty.com:8443/z-pB5nbBj37wuqQz");
    QString demoAnnounce = QStringLiteral("🔄 Не забывайте обновлять подписку\n⚡ - Сервера с низким пингом\n🏳️ - Если не работает мобильный инетрнет\nБот: @beaxtyvpnbot | Сайт: cabinet.beaxty.com");
    group->info = packGroupInfo(QStringLiteral("upload=0; download=2107669288917; total=0; expire=0"),
                                demoAnnounce,
                                QStringLiteral("https://t.me/beaxtysupport"),
                                QStringLiteral("https://sub.beaxty.com:8443/z-pB5nbBj37wuqQz"));
    group->sub_last_update = QDateTime::currentSecsSinceEpoch() - 1800;

    if (isNewGroup) {
        if (!gRepo->AddGroup(group)) {
            qWarning() << "[ConfigAdapter] Could not create the demo group; skipping demo data.";
            return;
        }
    } else {
        gRepo->Save(group);
    }
    Configs::dataManager->settingsRepo->current_group = group->id;
    Configs::dataManager->settingsRepo->Save();

    auto addNode = [&](const QString &name, const QString &country, const QString &type, const QString &server, int port, int pingMs) {
        auto p = Configs::ProfilesRepo::NewProfile(type);
        p->name = name;
        p->test_country = country;
        p->gid = group->id;
        if (type == QStringLiteral("shadowsocks")) {
            auto ss = std::make_unique<Configs::shadowsocks>();
            ss->server = server;
            ss->server_port = port;
            ss->method = QStringLiteral("2022-blake3-aes-128-gcm");
            ss->password = QStringLiteral("beaxty-secret-pass-key-2026");
            p->outbound = std::move(ss);
        } else {
            auto vless = std::make_unique<Configs::vless>();
            vless->server = server;
            vless->server_port = port;
            vless->uuid = QStringLiteral("a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d");
            vless->flow = QStringLiteral("xtls-rprx-vision");
            p->outbound = std::move(vless);
        }
        p->outbound->name = p->name;
        pRepo->AddProfile(p, group->id);
        if (pingMs > 0) {
            m_pings[p->id] = pingMs;
        }
    };

    // Realistic production servers matching the Beaxty subscription
    addNode(QStringLiteral("🇩🇪 Германия"), QStringLiteral("DE"), QStringLiteral("xrayvless"), QStringLiteral("fra.beaxtyvpn.net"), 443, 38);
    addNode(QStringLiteral("🇳🇱 Нидерланды"), QStringLiteral("NL"), QStringLiteral("xrayvless"), QStringLiteral("ams.beaxtyvpn.net"), 443, 42);
    addNode(QStringLiteral("🇫🇮 Финляндия"), QStringLiteral("FI"), QStringLiteral("vless"), QStringLiteral("hel.beaxtyvpn.net"), 443, 52);
    addNode(QStringLiteral("🇪🇪 Эстония ⚡"), QStringLiteral("EE"), QStringLiteral("hysteria"), QStringLiteral("tll.beaxtyvpn.net"), 443, 46);
    addNode(QStringLiteral("🇮🇹 [🏳️ Свободный интернет] Италия"), QStringLiteral("IT"), QStringLiteral("xrayvless"), QStringLiteral("mil.beaxtyvpn.net"), 443, 55);
    addNode(QStringLiteral("🇵🇱 [🏳️ Свободный интернет] Польша"), QStringLiteral("PL"), QStringLiteral("xrayvless"), QStringLiteral("waw.beaxtyvpn.net"), 443, 48);
    addNode(QStringLiteral("pl-ovh-test"), QStringLiteral("PL"), QStringLiteral("xrayvless"), QStringLiteral("ovh.beaxtyvpn.net"), 443, 50);

    // A standalone node, to exercise the "custom servers" section. It goes into the
    // Default group: profiles.gid has a FOREIGN KEY onto groups(id), so gid 0 would
    // be rejected outright unless a group with that id happens to exist.
    int customGid = group->id;
    auto defaultGids = gRepo->GetAllGroupIds();
    for (int gid : defaultGids) {
        auto g = gRepo->GetGroup(gid);
        if (g && g->id != group->id) {
            customGid = g->id;
            break;
        }
    }

    auto pCustom = Configs::ProfilesRepo::NewProfile(QStringLiteral("vless"));
    pCustom->name = QStringLiteral("Personal Home Server");
    pCustom->test_country = QStringLiteral("SE");
    pCustom->gid = customGid;
    auto vlessCustom = std::make_unique<Configs::vless>();
    vlessCustom->server = QStringLiteral("vpn.customhome.net");
    vlessCustom->server_port = 8443;
    vlessCustom->uuid = QStringLiteral("c3d4e5f6-a7b8-9c0d-1e2f-3a4b5c6d7e8f");
    vlessCustom->flow = QStringLiteral("xtls-rprx-vision");
    pCustom->outbound = std::move(vlessCustom);
    pCustom->outbound->name = pCustom->name;
    pRepo->AddProfile(pCustom, customGid);
    m_pings[pCustom->id] = 24;
}

QVariantList ConfigAdapter::servers() const {
    return m_servers;
}

int ConfigAdapter::selectedServerId() const {
    return m_selectedServerId;
}

QString ConfigAdapter::selectedServerName() const {
    for (const auto &var : m_servers) {
        auto map = var.toMap();
        if (map["id"].toInt() == m_selectedServerId) {
            return map["name"].toString();
        }
    }
    return m_servers.isEmpty() ? QStringLiteral("No nodes") : QStringLiteral("Select Node");
}

QString ConfigAdapter::selectedServerType() const {
    for (const auto &var : m_servers) {
        auto map = var.toMap();
        if (map["id"].toInt() == m_selectedServerId) {
            return map["type"].toString();
        }
    }
    return QString();
}

int ConfigAdapter::selectedServerPing() const {
    // 0 = not measured yet. Never invent a latency.
    for (const auto &row : m_servers) {
        const auto map = row.toMap();
        if (map["id"].toInt() == m_selectedServerId) return map["ping"].toInt();
    }
    return 0;
}

QString ConfigAdapter::selectedServerCountry() const {
    for (const auto &var : m_servers) {
        auto map = var.toMap();
        if (map["id"].toInt() == m_selectedServerId) {
            return map["country"].toString();
        }
    }
    return QString();
}

int ConfigAdapter::serverCount() const {
    return m_servers.size();
}

void ConfigAdapter::selectServer(int profileId) {
    if (!Configs::dataManager || !Configs::dataManager->profilesRepo ||
        !Configs::dataManager->profilesRepo->GetProfile(profileId)) return;
    if (m_selectedServerId != profileId) {
        m_selectedServerId = profileId;
        if (Configs::dataManager && Configs::dataManager->settingsRepo) {
            Configs::dataManager->settingsRepo->started_id = profileId;
            Configs::dataManager->settingsRepo->remember_id = profileId;
            // remember_id is persisted; without Save() the choice is lost on exit.
            Configs::dataManager->settingsRepo->Save();
        }
        emit selectedServerIdChanged(profileId);
        emit selectedServerChanged();
        emit selectedServerPingChanged(selectedServerPing());
        reloadServers();
    }
}

void ConfigAdapter::deleteServer(int profileId) {
    if (Configs::dataManager && Configs::dataManager->profilesRepo) {
        QList<int> ids = {profileId};
        auto settings = Configs::dataManager->settingsRepo.get();
        if (settings && settings->started_id == profileId) {
            if (ThroneEngine::instance()) ThroneEngine::instance()->stopConnection();
            settings->started_id = -1;
            settings->Save();
        }
        if (m_selectedServerId == profileId) {
            m_selectedServerId = -1;
            emit selectedServerIdChanged(m_selectedServerId);
        }
        Configs::dataManager->profilesRepo->BatchDeleteProfiles(ids);
        reloadServers();
    }
}

static QString formatByteSize(qint64 bytes) {
    if (bytes <= 0) return QStringLiteral("0 B");
    double val = static_cast<double>(bytes);
    const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB"), QStringLiteral("TB")};
    int u = 0;
    while (val >= 1024.0 && u < units.size() - 1) {
        val /= 1024.0;
        u++;
    }
    return QStringLiteral("%1 %2").arg(QString::number(val, 'f', (u >= 3 ? 1 : 0)), units[u]);
}

QVariantList ConfigAdapter::groups() const {
    QVariantList list;
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo) return list;

    auto allGids = Configs::dataManager->groupsRepo->GetAllGroupIds();
    auto allProfiles = Configs::dataManager->profilesRepo ? Configs::dataManager->profilesRepo->GetProfileBatch(Configs::dataManager->profilesRepo->GetAllProfileIds()) : QList<std::shared_ptr<Configs::Profile>>();

    for (int gid : allGids) {
        auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (!group) continue;
        int count = 0;
        for (const auto &p : allProfiles) {
            if (p && p->gid == gid) count++;
        }

        // Hide empty unused Default group
        if (count == 0 && group->url.trimmed().isEmpty() && group->info.trimmed().isEmpty() && group->name == QStringLiteral("Default")) {
            continue;
        }

        qint64 uploadBytes = 0;
        qint64 downloadBytes = 0;
        qint64 totalBytes = 0;
        qint64 expireTimestamp = 0;
        QString announcement;
        QString supportUrl;
        QString webUrl;
        int intervalHours = 24;

        QString rawInfo = group->info.trimmed();
        QStringList parts = rawInfo.split(';', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            QString p = part.trimmed();
            int eqIdx = p.indexOf('=');
            if (eqIdx > 0) {
                QString key = p.left(eqIdx).trimmed().toLower();
                QString val = p.mid(eqIdx + 1).trimmed();
                if (key == QStringLiteral("upload")) {
                    uploadBytes = val.toLongLong();
                } else if (key == QStringLiteral("download")) {
                    downloadBytes = val.toLongLong();
                } else if (key == QStringLiteral("total")) {
                    totalBytes = val.toLongLong();
                } else if (key == QStringLiteral("expire")) {
                    expireTimestamp = val.toLongLong();
                } else if (key == QStringLiteral("announce_b64")) {
                    QByteArray dec = QByteArray::fromBase64(val.toUtf8());
                    if (!dec.isEmpty()) {
                        announcement = QString::fromUtf8(dec);
                    }
                } else if (key == QStringLiteral("announce") || key == QStringLiteral("message") || key == QStringLiteral("notice")) {
                    if (announcement.isEmpty()) {
                        if (val.startsWith(QStringLiteral("base64:"), Qt::CaseInsensitive)) {
                            announcement = QString::fromUtf8(QByteArray::fromBase64(val.mid(7).trimmed().toUtf8()));
                        } else {
                            announcement = val;
                        }
                    }
                } else if (key == QStringLiteral("support") || key == QStringLiteral("support_url") || key == QStringLiteral("support-url")) {
                    supportUrl = val;
                } else if (key == QStringLiteral("web") || key == QStringLiteral("web_url") || key == QStringLiteral("profile-web-page-url")) {
                    webUrl = val;
                } else if (key == QStringLiteral("interval_hours") || key == QStringLiteral("profile-update-interval")) {
                    bool ok = false;
                    int parsed = val.toInt(&ok);
                    if (ok && parsed > 0) intervalHours = parsed;
                }
            } else {
                if (!p.isEmpty() && announcement.isEmpty()) {
                    announcement = p;
                }
            }
        }

        qint64 usedBytes = uploadBytes + downloadBytes;
        bool isUnlimited = (totalBytes <= 0);
        double trafficPercent = (!isUnlimited) ? std::min(1.0, std::max(0.0, static_cast<double>(usedBytes) / static_cast<double>(totalBytes))) : 1.0;
        QString trafficUsedStr = formatByteSize(usedBytes);
        QString trafficTotalStr = (!isUnlimited) ? formatByteSize(totalBytes) : QStringLiteral("Безлимит");

        bool isPerpetual = (expireTimestamp <= 0);
        QString expireDateStr;
        if (!isPerpetual) {
            QDateTime expDt = QDateTime::fromSecsSinceEpoch(expireTimestamp);
            qint64 daysLeft = QDateTime::currentDateTime().daysTo(expDt);
            if (daysLeft > 0) {
                expireDateStr = QStringLiteral("Истекает: %1 (осталось %2 дн.)").arg(expDt.toString(QStringLiteral("dd.MM.yyyy"))).arg(daysLeft);
            } else if (daysLeft == 0) {
                expireDateStr = QStringLiteral("Истекает сегодня");
            } else {
                expireDateStr = QStringLiteral("Истекла %1").arg(expDt.toString(QStringLiteral("dd.MM.yyyy")));
            }
        } else {
            expireDateStr = QStringLiteral("Бессрочно");
        }

        QString lastUpdateStr;
        if (group->sub_last_update > 0) {
            QDateTime dt = QDateTime::fromSecsSinceEpoch(group->sub_last_update);
            if (dt.date() == QDate::currentDate()) {
                lastUpdateStr = QStringLiteral("Обновлено: сегодня в %1").arg(dt.toString(QStringLiteral("HH:mm")));
            } else {
                lastUpdateStr = QStringLiteral("Обновлено: %1").arg(dt.toString(QStringLiteral("dd.MM.yyyy HH:mm")));
            }
        } else {
            lastUpdateStr = QStringLiteral("Не обновлялось");
        }

        QVariantMap g;
        g["id"] = group->id;
        g["name"] = group->name.isEmpty() ? QString("Group #%1").arg(group->id) : group->name;
        g["url"] = group->url;
        g["count"] = count;
        g["info"] = group->info;
        g["sub_last_update"] = group->sub_last_update;
        g["lastUpdateStr"] = lastUpdateStr;
        g["uploadBytes"] = uploadBytes;
        g["downloadBytes"] = downloadBytes;
        g["totalBytes"] = totalBytes;
        g["trafficUsedStr"] = trafficUsedStr;
        g["trafficTotalStr"] = trafficTotalStr;
        g["trafficPercent"] = trafficPercent;
        g["isUnlimited"] = isUnlimited;
        g["isPerpetual"] = isPerpetual;
        g["status"] = QStringLiteral("Активна");
        g["expireDateStr"] = expireDateStr;
        g["announcement"] = announcement;
        g["supportUrl"] = supportUrl;
        g["webUrl"] = webUrl;
        g["updateIntervalHours"] = intervalHours;
        list.append(g);
    }
    return list;
}

QVariantList ConfigAdapter::serversForGroup(int groupId) const {
    QVariantList res;
    for (const auto &v : m_servers) {
        auto m = v.toMap();
        if (m["gid"].toInt() == groupId) {
            res.append(m);
        }
    }
    return res;
}

QSet<int> ConfigAdapter::subscriptionGroupIds() {
    QSet<int> result;
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo) return result;
    for (int gid : Configs::dataManager->groupsRepo->GetAllGroupIds()) {
        auto g = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (g && !g->url.trimmed().isEmpty()) {
            result.insert(gid);
        }
    }
    return result;
}

QVariantList ConfigAdapter::customServers() const {
    // "Custom" means a manually added node: it lives in a group with no
    // subscription URL. Testing for an unknown gid instead would never match,
    // because profiles.gid carries a FOREIGN KEY onto groups(id) with ON DELETE
    // CASCADE, so a profile can never outlive its group.
    QSet<int> subGids = subscriptionGroupIds();
    QVariantList res;
    for (const auto &v : m_servers) {
        auto m = v.toMap();
        if (!subGids.contains(m["gid"].toInt())) {
            res.append(m);
        }
    }
    return res;
}

void ConfigAdapter::updateGroup(int groupId, bool silent) {
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo || !Configs::dataManager->profilesRepo) {
        if (!silent && ToastManager::instance()) ToastManager::instance()->showError(QStringLiteral("База данных не готова"));
        return;
    }

    auto gRepo = Configs::dataManager->groupsRepo.get();
    auto pRepo = Configs::dataManager->profilesRepo.get();
    auto group = gRepo->GetGroup(groupId);
    if (!group) return;

    QString url = group->url.trimmed();
    if (url.isEmpty()) {
        if (!silent && ToastManager::instance()) ToastManager::instance()->showInfo(QStringLiteral("У подписки \"%1\" нет URL адреса").arg(group->name));
        return;
    }

    QString groupName = group->name;
    if (!silent && ToastManager::instance()) {
        ToastManager::instance()->showInfo(QStringLiteral("Обновление подписки \"%1\"...").arg(groupName));
    }

    if (m_updatingGroups.contains(groupId)) return;
    m_updatingGroups.insert(groupId);
    emit refreshingChanged();
    const bool sendHwid = Configs::dataManager->settingsRepo && Configs::dataManager->settingsRepo->sub_send_hwid;
    Configs_network::NetworkRequestHelper::HttpGetAsync(this, url, sendHwid, false, 16 * 1024 * 1024,
        [this, groupId, url, groupName, silent](Configs_network::HTTPResponse resp) {
        const auto finished = qScopeGuard([this, groupId] {
            m_updatingGroups.remove(groupId);
            emit refreshingChanged();
        });
        if (!resp.error.isEmpty() || resp.data.isEmpty()) {
            if (!silent && ToastManager::instance())
                ToastManager::instance()->showError(tr("Не удалось обновить подписку. Сохранённые серверы не изменены."));
            return;
        }
        try {
        auto gRepo = Configs::dataManager->groupsRepo.get();
        auto pRepo = Configs::dataManager->profilesRepo.get();
        auto grp = gRepo->GetGroup(groupId);
        if (!grp || grp->url.trimmed() != url) return;

        // Parse before touching saved data: HTML errors and invalid subscriptions must
        // never erase working servers or advance the successful-update timestamp.
        QList<std::shared_ptr<Configs::Profile>> incoming;
        Subscription::ParseSink parseSink;
        parseSink.profile = [&](std::shared_ptr<Configs::Profile> profile) {
            if (profile && profile->outbound) {
                if (profile->name.trimmed().isEmpty() && !profile->outbound->name.trimmed().isEmpty()) {
                    profile->name = profile->outbound->name.trimmed();
                }
                incoming.append(profile);
            }
        };
        Subscription::ParseDocument(resp.data, parseSink);
        if (incoming.isEmpty()) {
            if (!silent && ToastManager::instance())
                ToastManager::instance()->showError(tr("В подписке нет узлов. Сохранённые серверы не изменены."));
            return;
        }

        // 1. Profile Title
        QString profileTitle = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-title"));
        if (profileTitle.startsWith(QStringLiteral("base64:"), Qt::CaseInsensitive)) {
            QByteArray decoded = QByteArray::fromBase64(profileTitle.mid(7).trimmed().toUtf8());
            if (!decoded.isEmpty()) profileTitle = QString::fromUtf8(decoded);
        }

        // 2. Announce
        QString announceHeader = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("announce"));
        QString decodedAnnounce;
        if (announceHeader.startsWith(QStringLiteral("base64:"), Qt::CaseInsensitive)) {
            decodedAnnounce = QString::fromUtf8(QByteArray::fromBase64(announceHeader.mid(7).trimmed().toUtf8()));
        } else if (!announceHeader.isEmpty()) {
            decodedAnnounce = announceHeader;
        }

        // 3. Support and Web URLs
        QString supportUrl = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("support-url"));
        QString webUrl = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-web-page-url"));

        // Interval
        QString intervalHeader = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-update-interval"));
        if (intervalHeader.isEmpty()) intervalHeader = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("Profile-Update-Interval"));
        int intervalHours = -1;
        if (!intervalHeader.isEmpty()) {
            bool ok = false;
            int parsed = intervalHeader.trimmed().toInt(&ok);
            if (ok && parsed > 0) intervalHours = parsed;
        }

        // 4. Traffic info
        QString subInfo = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("subscription-userinfo"));
        QString baseInfo = !subInfo.isEmpty() ? subInfo : grp->info;

        const QString updatedInfo = packGroupInfo(baseInfo, decodedAnnounce, supportUrl, webUrl, intervalHours);

        // Capture previously selected server properties to re-link after refresh
        int prevSelectedId = m_selectedServerId;
        QString prevAddress;
        int prevPort = 0;
        QString prevName;
        QString prevType;

        if (prevSelectedId > 0) {
            if (auto curProf = pRepo->GetProfile(prevSelectedId)) {
                prevName = curProf->name;
                prevType = curProf->type;
                if (curProf->outbound) {
                    prevAddress = curProf->outbound->server;
                    prevPort = curProf->outbound->server_port;
                }
            }
        }

        // Reuse unchanged profiles (including IDs, latency and traffic history).
        // Persist new arrivals before deleting stale entries.
        auto oldProfiles = pRepo->GetProfileBatch(pRepo->GetAllProfileIds());
        QList<int> oldIds;
        QList<std::shared_ptr<Configs::Profile>> newProfiles;
        QList<std::shared_ptr<Configs::Profile>> additions;
        for (const auto &old : oldProfiles) {
            if (old && old->gid == groupId) oldIds.append(old->id);
        }
        auto profileKey = [](const std::shared_ptr<Configs::Profile> &profile) {
            return profile->type.toUtf8() + '\0'
                + QJsonDocument(profile->outbound->ExportToJson()).toJson(QJsonDocument::Compact);
        };
        QHash<QByteArray, QList<std::shared_ptr<Configs::Profile>>> byContent;
        for (const auto &old : oldProfiles)
            if (old && old->gid == groupId && old->outbound) byContent[profileKey(old)].append(old);
        QSet<int> claimed;
        for (auto &profile : incoming) {
            std::shared_ptr<Configs::Profile> match;
            auto &matches = byContent[profileKey(profile)];
            if (!matches.isEmpty()) match = matches.takeFirst();
            if (match) {
                claimed.insert(match->id);
                newProfiles.append(match);
            } else {
                profile->gid = groupId;
                additions.append(profile);
                newProfiles.append(profile);
            }
        }
        if (!additions.isEmpty() && !pRepo->AddProfileBatch(additions, groupId))
            throw std::runtime_error("Cannot save subscription profiles");
        QList<int> stale;
        for (int id : oldIds) if (!claimed.contains(id)) stale.append(id);
        auto settings = Configs::dataManager->settingsRepo.get();
        const int startedId = settings ? settings->started_id : -1;
        {
            const auto restore = qScopeGuard([settings, startedId] {
                if (settings) settings->started_id = startedId;
            });
            // A running profile remains available to the traffic/core lifecycle.
            if (settings && (!ThroneEngine::instance() || ThroneEngine::instance()->state() == ThroneEngine::Disconnected))
                settings->started_id = -1;
            if (!stale.isEmpty() && !pRepo->BatchDeleteProfiles(stale))
                throw std::runtime_error("Cannot remove stale subscription profiles");
        }
        const int count = incoming.size();
        if (!profileTitle.trimmed().isEmpty()) grp->name = profileTitle.trimmed();
        grp->info = updatedInfo;
        grp->sub_last_update = QDateTime::currentSecsSinceEpoch();
        gRepo->Save(grp);

        // Re-link the selected server to the newly added matching profile
        int newSelectedId = -1;
        if (oldIds.contains(prevSelectedId) && !prevAddress.isEmpty() && prevPort > 0) {
            for (const auto &np : newProfiles) {
                if (np && np->outbound && np->type == prevType && np->outbound->server == prevAddress && np->outbound->server_port == prevPort) {
                    newSelectedId = np->id;
                    break;
                }
            }
        }
        if (oldIds.contains(prevSelectedId) && newSelectedId == -1 && !prevName.isEmpty()) {
            for (const auto &np : newProfiles) {
                if (np && np->name == prevName) {
                    newSelectedId = np->id;
                    break;
                }
            }
        }

        // Prefer the exact reused ID when several nodes share the same endpoint.
        if (claimed.contains(prevSelectedId)) newSelectedId = prevSelectedId;
        const bool runningSelection = ThroneEngine::instance() &&
            ThroneEngine::instance()->state() != ThroneEngine::Disconnected && prevSelectedId == startedId;
        if (oldIds.contains(prevSelectedId) && !runningSelection) {
            if (newSelectedId < 0 && !newProfiles.isEmpty()) newSelectedId = newProfiles.first()->id;
            if (newSelectedId >= 0) selectServer(newSelectedId);
        }
        reloadServers();
        if (!silent && ToastManager::instance())
            ToastManager::instance()->showSuccess(tr("Подписка обновлена: %1 узлов").arg(count));
        } catch (const std::exception &) {
            reloadServers();
            if (!silent && ToastManager::instance())
                ToastManager::instance()->showError(tr("Не удалось сохранить обновление подписки."));
        }
    });
}

void ConfigAdapter::renameGroup(int groupId, const QString &newName) {
    QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) return;

    if (!Configs::dataManager || !Configs::dataManager->groupsRepo) return;
    auto gRepo = Configs::dataManager->groupsRepo.get();
    auto grp = gRepo->GetGroup(groupId);
    if (!grp) return;

    grp->name = trimmed;
    gRepo->Save(grp);
    reloadServers();

    if (ToastManager::instance()) {
        ToastManager::instance()->showSuccess(QStringLiteral("Подписка переименована: \"%1\"").arg(trimmed));
    }
}

void ConfigAdapter::deleteGroup(int groupId) {
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo) return;

    if (Configs::dataManager->profilesRepo) {
        auto allProfiles = Configs::dataManager->profilesRepo->GetProfileBatch(Configs::dataManager->profilesRepo->GetAllProfileIds());
        QList<int> idsToDelete;
        for (const auto &p : allProfiles) {
            if (p && p->gid == groupId) {
                idsToDelete.append(p->id);
            }
        }
        if (!idsToDelete.isEmpty()) {
            if (Configs::dataManager->settingsRepo && idsToDelete.contains(Configs::dataManager->settingsRepo->started_id)) {
                if (ThroneEngine::instance()) ThroneEngine::instance()->stopConnection();
                Configs::dataManager->settingsRepo->started_id = -1;
                Configs::dataManager->settingsRepo->Save();
            }
            if (idsToDelete.contains(m_selectedServerId)) {
                m_selectedServerId = -1;
                emit selectedServerIdChanged(m_selectedServerId);
            }
            Configs::dataManager->profilesRepo->BatchDeleteProfiles(idsToDelete);
        }
    }
    Configs::dataManager->groupsRepo->DeleteGroup(groupId);
    reloadServers();
    if (ToastManager::instance()) {
        ToastManager::instance()->showSuccess(QStringLiteral("Подписка удалена"));
    }
}

void ConfigAdapter::refreshSubscriptions(bool silent) {
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo) return;
    bool found = false;
    for (int gid : Configs::dataManager->groupsRepo->GetAllGroupIds()) {
        auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group && !group->url.trimmed().isEmpty()) {
            found = true;
            updateGroup(gid, silent);
        }
    }
    if (!found && !silent && ToastManager::instance())
        ToastManager::instance()->showInfo(tr("Нет подписок по URL для обновления"));
}

void ConfigAdapter::importSubscription(const QString &urlOrContent, const QString &groupName) {
    QString trimmed = urlOrContent.trimmed();
    if (trimmed.isEmpty()) {
        if (ToastManager::instance()) ToastManager::instance()->showError(tr("Поле ввода пусто"));
        emit importFinished(false, 0, tr("Поле ввода пусто"));
        return;
    }

    if (!Configs::dataManager || !Configs::dataManager->profilesRepo || !Configs::dataManager->groupsRepo) {
        if (ToastManager::instance()) ToastManager::instance()->showError(tr("База данных не готова"));
        emit importFinished(false, 0, tr("База данных не готова"));
        return;
    }

    if (m_importing) return;
    m_importing = true;
    emit importingChanged();
    const QUrl url(trimmed);
    if (url.scheme().compare("http", Qt::CaseInsensitive) == 0 || url.scheme().compare("https", Qt::CaseInsensitive) == 0) {
        const bool sendHwid = Configs::dataManager->settingsRepo && Configs::dataManager->settingsRepo->sub_send_hwid;
        Configs_network::NetworkRequestHelper::HttpGetAsync(this, trimmed, sendHwid, false, 16 * 1024 * 1024,
            [this, trimmed, groupName](Configs_network::HTTPResponse response) {
                finishImport(trimmed, groupName, response);
            });
    } else {
        finishImport(trimmed, groupName, {});
    }
}

void ConfigAdapter::finishImport(const QString &trimmed, const QString &groupName,
                                const Configs_network::HTTPResponse &response) {
    const auto finished = qScopeGuard([this] {
        m_importing = false;
        emit importingChanged();
    });
    try {
        QString finalGroupName = groupName.trimmed();
        QString subInfo;
        QString decodedAnnounce;
        QString supportUrl;
        QString webUrl;
        int intervalHours = 24;
        QByteArray contentData = trimmed.toUtf8();

        if (trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) || trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
            const auto &resp = response;
            if (!resp.error.isEmpty()) {
                QString err = tr("Ошибка сети при загрузке подписки: %1").arg(resp.error);
                if (ToastManager::instance()) ToastManager::instance()->showError(err);
                emit importFinished(false, 0, err);
                return;
            }
            contentData = resp.data;

            // Extract subscription headers
            subInfo = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("subscription-userinfo"));
            if (subInfo.isEmpty()) subInfo = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("Subscription-Userinfo"));

            QString profileTitle = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-title"));
            if (profileTitle.isEmpty()) profileTitle = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("Profile-Title"));
            if (profileTitle.startsWith(QStringLiteral("base64:"), Qt::CaseInsensitive)) {
                QByteArray decoded = QByteArray::fromBase64(profileTitle.mid(7).trimmed().toUtf8());
                if (!decoded.isEmpty()) profileTitle = QString::fromUtf8(decoded);
            }

            QString announceHeader = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("announce"));
            if (announceHeader.startsWith(QStringLiteral("base64:"), Qt::CaseInsensitive)) {
                decodedAnnounce = QString::fromUtf8(QByteArray::fromBase64(announceHeader.mid(7).trimmed().toUtf8()));
            } else if (!announceHeader.isEmpty()) {
                decodedAnnounce = announceHeader;
            }

            supportUrl = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("support-url"));
            webUrl = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-web-page-url"));

            QString intervalHeader = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-update-interval"));
            if (intervalHeader.isEmpty()) intervalHeader = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("Profile-Update-Interval"));
            if (!intervalHeader.isEmpty()) {
                bool ok = false;
                int parsed = intervalHeader.trimmed().toInt(&ok);
                if (ok && parsed > 0) intervalHours = parsed;
            }

            if (finalGroupName.isEmpty() || finalGroupName == QStringLiteral("Подписка Beaxty") || finalGroupName == QStringLiteral("Подписка beaxty")) {
                QString contentDisp = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("content-disposition"));
                if (contentDisp.isEmpty()) contentDisp = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("Content-Disposition"));
                QString fn;
                int fnIdx = contentDisp.indexOf(QStringLiteral("filename="));
                if (fnIdx >= 0) {
                    fn = contentDisp.mid(fnIdx + 9).trimmed();
                    if (fn.startsWith('"') && fn.indexOf('"', 1) > 0) {
                        fn = fn.mid(1, fn.indexOf('"', 1) - 1);
                    } else if (fn.indexOf(';') > 0) {
                        fn = fn.left(fn.indexOf(';')).trimmed();
                    }
                    fn = QFileInfo(fn).fileName();
                    fn.remove(QRegularExpression(QStringLiteral("[\\\\/\\r\\n\\t\\0]")));
                    while (fn.startsWith('.')) fn.remove(0, 1);
                    if (fn.length() > 80) fn = fn.left(80);
                    fn = fn.trimmed();
                }

                if (!profileTitle.trimmed().isEmpty()) {
                    QString pt = profileTitle.trimmed();
                    pt.remove(QRegularExpression(QStringLiteral("[\\r\\n\\t\\0]")));
                    if (pt.length() > 80) pt = pt.left(80);
                    finalGroupName = pt.trimmed();
                } else if (!fn.isEmpty()) {
                    finalGroupName = fn;
                } else {
                    finalGroupName = QStringLiteral("Подписка beaxty");
                }
            }
        }

        if (finalGroupName.isEmpty()) {
            finalGroupName = QStringLiteral("Подписка beaxty");
        }

        QList<std::shared_ptr<Configs::Profile>> profiles;
        Subscription::ParseSink sink;
        sink.profile = [&](std::shared_ptr<Configs::Profile> profile) {
            if (profile && profile->outbound) {
                if (profile->name.trimmed().isEmpty() && !profile->outbound->name.trimmed().isEmpty()) {
                    profile->name = profile->outbound->name.trimmed();
                }
                profiles.append(profile);
            }
        };
        Subscription::ParseDocument(contentData, sink);
        if (profiles.isEmpty()) {
            const QString err = tr("В содержимом не найдено прокси-узлов");
            if (ToastManager::instance()) ToastManager::instance()->showError(err);
            emit importFinished(false, 0, err);
            return;
        }
        // Guarantee an existing group in SQLite
        auto group = Configs::GroupsRepo::NewGroup();
        group->name = finalGroupName;
        if (trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) || trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
            group->url = trimmed;
        }
        group->info = packGroupInfo(subInfo, decodedAnnounce, supportUrl, webUrl, intervalHours);
        group->sub_last_update = QDateTime::currentSecsSinceEpoch();
        Configs::dataManager->groupsRepo->AddGroup(group);
        int targetGid = group->id;

        if (!Configs::dataManager->profilesRepo->AddProfileBatch(profiles, targetGid)) {
            Configs::dataManager->groupsRepo->DeleteGroup(targetGid);
            throw std::runtime_error("Cannot save imported profiles");
        }
        const int count = profiles.size();
        const int firstImportedId = profiles.first()->id;
        Configs::dataManager->settingsRepo->current_group = targetGid;
        reloadServers();

        // Automatically switch to the newly imported server node
        if (firstImportedId >= 0) {
            selectServer(firstImportedId);
        }

        QString successMsg = tr("Успешно импортировано %1 серверов в \"%2\"").arg(count).arg(finalGroupName);
        if (ToastManager::instance()) ToastManager::instance()->showSuccess(successMsg);
        emit importFinished(true, count, successMsg);

    } catch (const std::exception &ex) {
        QString err = tr("Ошибка базы данных при импорте: %1").arg(ex.what());
        qWarning() << "[ConfigAdapter]" << err;
        if (ToastManager::instance()) ToastManager::instance()->showError(err);
        emit importFinished(false, 0, err);
    }
}

void ConfigAdapter::importFromClipboard() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        importSubscription(clipboard->text());
    }
}

QString ConfigAdapter::getClipboardText() const {
    QClipboard *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text().trimmed() : QString();
}

void ConfigAdapter::updateServerPing(int profileId, int pingMs) {
    bool exists = false;
    for (const auto &row : m_servers) if (row.toMap()["id"].toInt() == profileId) { exists = true; break; }
    if (!exists) return;
    m_pings[profileId] = pingMs;
    // A full reloadServers() here re-queries every profile and rebuilds every QML
    // delegate; "Ping All" would do that once per node. Patch the one row instead.
    bool patched = false;
    for (auto &var : m_servers) {
        auto map = var.toMap();
        if (map["id"].toInt() == profileId) {
            map["ping"] = pingMs;
            var = map;
            patched = true;
            break;
        }
    }
    if (patched && profileId == m_selectedServerId) emit selectedServerPingChanged(pingMs);
    if (patched && !m_pingPublishTimer.isActive()) {
        m_pingPublishTimer.start();
    }
}

int ConfigAdapter::autoUpdateSubsMode() const {
    return m_autoUpdateSubsMode;
}

void ConfigAdapter::setAutoUpdateSubsMode(int mode) {
    if (mode < 0 || mode > 2) return;
    if (m_autoUpdateSubsMode != mode) {
        m_autoUpdateSubsMode = mode;
        AppPrefs::setInt(QStringLiteral("auto_update_subs_mode"), mode);
        emit autoUpdateSubsModeChanged(mode);
    }
}

int ConfigAdapter::serverSortMode() const {
    return m_serverSortMode;
}

void ConfigAdapter::setServerSortMode(int mode) {
    if (mode < 0 || mode > 3) return;
    if (m_serverSortMode != mode) {
        m_serverSortMode = mode;
        AppPrefs::setInt(QStringLiteral("server_sort_mode"), mode);
        emit serverSortModeChanged(mode);
    }
}

void ConfigAdapter::checkScheduledSubscriptionUpdates() {
    if (m_autoUpdateSubsMode != 2) return;
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo) return;

    auto gRepo = Configs::dataManager->groupsRepo.get();
    auto gids = gRepo->GetAllGroupIds();
    qint64 now = QDateTime::currentSecsSinceEpoch();

    for (int gid : gids) {
        auto group = gRepo->GetGroup(gid);
        if (!group || group->url.trimmed().isEmpty()) continue;

        int intervalHours = 24;
        for (const QString &part : group->info.split(';', Qt::SkipEmptyParts)) {
            QString p = part.trimmed();
            int eq = p.indexOf('=');
            if (eq > 0) {
                QString k = p.left(eq).trimmed().toLower();
                if (k == QStringLiteral("interval_hours") || k == QStringLiteral("profile-update-interval")) {
                    bool ok = false;
                    int parsed = p.mid(eq + 1).trimmed().toInt(&ok);
                    if (ok && parsed > 0) intervalHours = parsed;
                }
            }
        }

        qint64 intervalSecs = static_cast<qint64>(intervalHours) * 3600;
        if (group->sub_last_update <= 0 || (now - group->sub_last_update) >= intervalSecs) {
            qInfo() << "[ConfigAdapter] Auto-updating subscription by schedule:" << group->name;
            updateGroup(gid, /*silent=*/true);
        }
    }
}

