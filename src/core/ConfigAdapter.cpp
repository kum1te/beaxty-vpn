// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "ConfigAdapter.hpp"
#include "ToastManager.hpp"
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
#include <QThreadPool>
#include <QDebug>

ConfigAdapter *ConfigAdapter::s_instance = nullptr;

ConfigAdapter::ConfigAdapter(QObject *parent) : QObject(parent) {
    s_instance = this;
    m_autoUpdateSubsMode = AppPrefs::getInt(QStringLiteral("auto_update_subs_mode"), 1);
    m_serverSortMode = AppPrefs::getInt(QStringLiteral("server_sort_mode"), 0);
    m_autoUpdateTimer = new QTimer(this);
    connect(m_autoUpdateTimer, &QTimer::timeout, this, &ConfigAdapter::checkScheduledSubscriptionUpdates);
    m_autoUpdateTimer->start(30 * 60 * 1000);
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

    auto repo = Configs::dataManager->profilesRepo.get();
    auto ids = repo->GetAllProfileIds();

    if (ids.isEmpty() && m_demoDataEnabled) {
        ensureDefaultDemoServers();
        ids = repo->GetAllProfileIds();
    }

    QVariantList list;
    for (int id : ids) {
        auto profile = repo->GetProfile(id);
        if (!profile || !profile->outbound) continue;

        QVariantMap item;
        item["id"] = profile->id;
        item["gid"] = profile->gid;
        item["name"] = profile->name.isEmpty() ? QString("Server #%1").arg(profile->id) : profile->name;
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

        emit selectedServerIdChanged(m_selectedServerId);
        emit selectedServerChanged();
    }

    emit serversChanged();
    emit groupsChanged();

    if (!m_initialAutoUpdateTriggered) {
        m_initialAutoUpdateTriggered = true;
        if (m_autoUpdateSubsMode >= 1) {
            QTimer::singleShot(1500, this, [this]() {
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
    return m_pings.value(m_selectedServerId, 0);
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
        Configs::dataManager->profilesRepo->BatchDeleteProfiles(ids);
        if (m_selectedServerId == profileId) {
            m_selectedServerId = -1;
        }
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

    QThreadPool::globalInstance()->start([this, groupId, url, groupName, silent]() {
        bool sendHwid = Configs::dataManager->settingsRepo ? Configs::dataManager->settingsRepo->sub_send_hwid : true;
        auto resp = Configs_network::NetworkRequestHelper::HttpGet(url, sendHwid, false);

        if (!resp.error.isEmpty() || resp.data.isEmpty()) {
            qWarning() << "[ConfigAdapter] Failed to update group" << groupId << ":" << resp.error;
            if (!silent) {
                QMetaObject::invokeMethod(this, [groupName, err = resp.error]() {
                    if (ToastManager::instance()) {
                        ToastManager::instance()->showError(QStringLiteral("Ошибка обновления \"%1\": %2").arg(groupName, err));
                    }
                });
            }
            return;
        }

        auto gRepo = Configs::dataManager->groupsRepo.get();
        auto pRepo = Configs::dataManager->profilesRepo.get();
        auto grp = gRepo->GetGroup(groupId);
        if (!grp) return;

        // 1. Profile Title
        QString profileTitle = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-title"));
        if (profileTitle.startsWith(QStringLiteral("base64:"), Qt::CaseInsensitive)) {
            QByteArray decoded = QByteArray::fromBase64(profileTitle.mid(7).trimmed().toUtf8());
            if (!decoded.isEmpty()) profileTitle = QString::fromUtf8(decoded);
        }
        if (!profileTitle.trimmed().isEmpty()) {
            grp->name = profileTitle.trimmed();
        }
        QString updatedGroupName = grp->name;

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

        grp->info = packGroupInfo(baseInfo, decodedAnnounce, supportUrl, webUrl, intervalHours);
        grp->sub_last_update = QDateTime::currentSecsSinceEpoch();
        gRepo->Save(grp);

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

        // Delete old profiles for this group
        auto allProfiles = pRepo->GetProfileBatch(pRepo->GetAllProfileIds());
        QList<int> oldIds;
        for (const auto &p : allProfiles) {
            if (p && p->gid == groupId) {
                oldIds.append(p->id);
            }
        }
        if (!oldIds.isEmpty()) {
            int prevStartedId = Configs::dataManager->settingsRepo ? Configs::dataManager->settingsRepo->started_id : -1;
            if (Configs::dataManager->settingsRepo) {
                Configs::dataManager->settingsRepo->started_id = -1;
            }
            pRepo->BatchDeleteProfiles(oldIds);
            if (Configs::dataManager->settingsRepo) {
                Configs::dataManager->settingsRepo->started_id = prevStartedId;
            }
        }

        int count = 0;
        QList<std::shared_ptr<Configs::Profile>> newProfiles;
        Subscription::ParseSink sink;
        sink.profile = [&](std::shared_ptr<Configs::Profile> prof) {
            if (prof && prof->outbound) {
                prof->gid = groupId;
                if (prof->name.trimmed().isEmpty()) {
                    prof->name = QStringLiteral("%1 %2").arg(prof->type.toUpper()).arg(count + 1);
                }
                if (prof->test_country.isEmpty()) prof->test_country = QStringLiteral("NL");
                if (pRepo->AddProfile(prof, groupId)) {
                    count++;
                    newProfiles.append(prof);
                }
            }
        };
        sink.log = [](const QString &msg) { qDebug() << "[GroupUpdate]" << msg; };
        sink.warn = [](const QString &w1, const QString &w2) { qWarning() << "[GroupUpdate]" << w1 << w2; };

        Subscription::ParseDocument(resp.data, sink);

        // Re-link the selected server to the newly added matching profile
        int newSelectedId = -1;
        if (!prevAddress.isEmpty() && prevPort > 0) {
            for (const auto &np : newProfiles) {
                if (np && np->outbound && np->outbound->server == prevAddress && np->outbound->server_port == prevPort) {
                    newSelectedId = np->id;
                    break;
                }
            }
        }
        if (newSelectedId == -1 && !prevName.isEmpty()) {
            for (const auto &np : newProfiles) {
                if (np && np->name == prevName) {
                    newSelectedId = np->id;
                    break;
                }
            }
        }

        if (newSelectedId != -1) {
            if (Configs::dataManager->settingsRepo) {
                Configs::dataManager->settingsRepo->started_id = newSelectedId;
                Configs::dataManager->settingsRepo->Save();
            }
            m_selectedServerId = newSelectedId;
        } else if (oldIds.contains(prevSelectedId)) {
            if (!newProfiles.isEmpty()) {
                int firstId = newProfiles.first()->id;
                if (Configs::dataManager->settingsRepo) {
                    Configs::dataManager->settingsRepo->started_id = firstId;
                    Configs::dataManager->settingsRepo->Save();
                }
                m_selectedServerId = firstId;
            } else {
                if (Configs::dataManager->settingsRepo) {
                    Configs::dataManager->settingsRepo->started_id = -1;
                    Configs::dataManager->settingsRepo->Save();
                }
                m_selectedServerId = -1;
            }
        }

        QMetaObject::invokeMethod(this, [this, updatedGroupName, count, silent]() {
            reloadServers();
            if (!silent && ToastManager::instance()) {
                ToastManager::instance()->showSuccess(QStringLiteral("Подписка \"%1\" обновлена: %2 узлов").arg(updatedGroupName).arg(count));
            }
        });
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
                if (m_selectedServerId == p->id) {
                    m_selectedServerId = -1;
                }
            }
        }
        if (!idsToDelete.isEmpty()) {
            if (Configs::dataManager->settingsRepo && idsToDelete.contains(Configs::dataManager->settingsRepo->started_id)) {
                Configs::dataManager->settingsRepo->started_id = -1;
                Configs::dataManager->settingsRepo->Save();
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
    if (!Configs::dataManager || !Configs::dataManager->groupsRepo || !Configs::dataManager->profilesRepo) {
        if (!silent && ToastManager::instance()) ToastManager::instance()->showError(tr("База данных не готова"));
        return;
    }

    if (!silent && ToastManager::instance()) {
        ToastManager::instance()->showInfo(tr("Обновление подписок..."));
    }

    QThreadPool::globalInstance()->start([this, silent]() {
        auto gRepo = Configs::dataManager->groupsRepo.get();
        auto pRepo = Configs::dataManager->profilesRepo.get();
        auto gids = gRepo->GetAllGroupIds();
        int totalUpdated = 0;
        int groupsUpdated = 0;

        bool sendHwid = Configs::dataManager->settingsRepo ? Configs::dataManager->settingsRepo->sub_send_hwid : true;

        for (int gid : gids) {
            auto group = gRepo->GetGroup(gid);
            if (!group || group->url.trimmed().isEmpty()) continue;

            auto resp = Configs_network::NetworkRequestHelper::HttpGet(group->url.trimmed(), sendHwid, false);
            if (!resp.error.isEmpty() || resp.data.isEmpty()) {
                qWarning() << "[ConfigAdapter] Failed to fetch subscription" << group->name << ":" << resp.error;
                continue;
            }

            // 1. Profile Title
            QString profileTitle = Configs_network::NetworkRequestHelper::GetHeader(resp.header, QStringLiteral("profile-title"));
            if (profileTitle.startsWith(QStringLiteral("base64:"), Qt::CaseInsensitive)) {
                QByteArray decoded = QByteArray::fromBase64(profileTitle.mid(7).trimmed().toUtf8());
                if (!decoded.isEmpty()) profileTitle = QString::fromUtf8(decoded);
            }
            if (!profileTitle.trimmed().isEmpty()) {
                group->name = profileTitle.trimmed();
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
            QString baseInfo = !subInfo.isEmpty() ? subInfo : group->info;

            group->info = packGroupInfo(baseInfo, decodedAnnounce, supportUrl, webUrl, intervalHours);
            group->sub_last_update = QDateTime::currentSecsSinceEpoch();
            gRepo->Save(group);

            int prevSelectedId = m_selectedServerId;
            QString prevAddress;
            int prevPort = 0;
            QString prevName;

            if (prevSelectedId > 0) {
                if (auto curProf = pRepo->GetProfile(prevSelectedId)) {
                    prevName = curProf->name;
                    if (curProf->outbound) {
                        prevAddress = curProf->outbound->server;
                        prevPort = curProf->outbound->server_port;
                    }
                }
            }

            // Delete old profiles for this group
            auto allProfiles = pRepo->GetProfileBatch(pRepo->GetAllProfileIds());
            QList<int> oldIds;
            for (const auto &p : allProfiles) {
                if (p && p->gid == gid) {
                    oldIds.append(p->id);
                }
            }
            if (!oldIds.isEmpty()) {
                int prevStartedId = Configs::dataManager->settingsRepo ? Configs::dataManager->settingsRepo->started_id : -1;
                if (Configs::dataManager->settingsRepo) {
                    Configs::dataManager->settingsRepo->started_id = -1;
                }
                pRepo->BatchDeleteProfiles(oldIds);
                if (Configs::dataManager->settingsRepo) {
                    Configs::dataManager->settingsRepo->started_id = prevStartedId;
                }
            }

            int count = 0;
            QList<std::shared_ptr<Configs::Profile>> newProfiles;
            Subscription::ParseSink sink;
            sink.profile = [&](std::shared_ptr<Configs::Profile> prof) {
                if (prof && prof->outbound) {
                    prof->gid = gid;
                    if (prof->name.trimmed().isEmpty()) {
                        prof->name = QStringLiteral("%1 %2").arg(prof->type.toUpper()).arg(count + 1);
                    }
                    if (prof->test_country.isEmpty()) prof->test_country = QStringLiteral("NL");
                    if (pRepo->AddProfile(prof, gid)) {
                        count++;
                        newProfiles.append(prof);
                    }
                }
            };
            sink.log = [](const QString &msg) { qDebug() << "[SubRefresh]" << msg; };
            sink.warn = [](const QString &w1, const QString &w2) { qWarning() << "[SubRefresh]" << w1 << w2; };

            Subscription::ParseDocument(resp.data, sink);
            totalUpdated += count;
            groupsUpdated++;

            // Re-link if previous selected was in this group
            if (oldIds.contains(prevSelectedId)) {
                int newSelectedId = -1;
                if (!prevAddress.isEmpty() && prevPort > 0) {
                    for (const auto &np : newProfiles) {
                        if (np && np->outbound && np->outbound->server == prevAddress && np->outbound->server_port == prevPort) {
                            newSelectedId = np->id;
                            break;
                        }
                    }
                }
                if (newSelectedId == -1 && !prevName.isEmpty()) {
                    for (const auto &np : newProfiles) {
                        if (np && np->name == prevName) {
                            newSelectedId = np->id;
                            break;
                        }
                    }
                }
                if (newSelectedId != -1) {
                    if (Configs::dataManager->settingsRepo) {
                        Configs::dataManager->settingsRepo->started_id = newSelectedId;
                        Configs::dataManager->settingsRepo->Save();
                    }
                    m_selectedServerId = newSelectedId;
                } else if (!newProfiles.isEmpty()) {
                    int firstId = newProfiles.first()->id;
                    if (Configs::dataManager->settingsRepo) {
                        Configs::dataManager->settingsRepo->started_id = firstId;
                        Configs::dataManager->settingsRepo->Save();
                    }
                    m_selectedServerId = firstId;
                }
            }
        }

        QMetaObject::invokeMethod(this, [this, totalUpdated, groupsUpdated, silent]() {
            reloadServers();
            if (!silent) {
                if (groupsUpdated > 0) {
                    QString msg = tr("Обновлено %1 серверов в %2 подписках").arg(totalUpdated).arg(groupsUpdated);
                    if (ToastManager::instance()) ToastManager::instance()->showSuccess(msg);
                } else {
                    if (ToastManager::instance()) ToastManager::instance()->showInfo(tr("Нет подписок по URL для обновления"));
                }
            }
        });
    });
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

    try {
        QString finalGroupName = groupName.trimmed();
        QString subInfo;
        QString decodedAnnounce;
        QString supportUrl;
        QString webUrl;
        int intervalHours = 24;
        QByteArray contentData = trimmed.toUtf8();

        if (trimmed.startsWith(QStringLiteral("http://")) || trimmed.startsWith(QStringLiteral("https://"))) {
            bool sendHwid = Configs::dataManager ? Configs::dataManager->settingsRepo->sub_send_hwid : true;
            auto resp = Configs_network::NetworkRequestHelper::HttpGet(trimmed, sendHwid, false);
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

            if (finalGroupName.isEmpty() || finalGroupName == QStringLiteral("Подписка Beaxty")) {
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
                }

                if (!profileTitle.trimmed().isEmpty()) {
                    finalGroupName = profileTitle.trimmed();
                } else if (!fn.trimmed().isEmpty()) {
                    finalGroupName = fn.trimmed();
                } else {
                    finalGroupName = QStringLiteral("Подписка Beaxty");
                }
            }
        }

        if (finalGroupName.isEmpty()) {
            finalGroupName = QStringLiteral("Подписка Beaxty");
        }

        // Guarantee an existing group in SQLite
        auto group = Configs::GroupsRepo::NewGroup();
        group->name = finalGroupName;
        if (trimmed.startsWith(QStringLiteral("http://")) || trimmed.startsWith(QStringLiteral("https://"))) {
            group->url = trimmed;
        }
        group->info = packGroupInfo(subInfo, decodedAnnounce, supportUrl, webUrl, intervalHours);
        group->sub_last_update = QDateTime::currentSecsSinceEpoch();
        Configs::dataManager->groupsRepo->AddGroup(group);
        int targetGid = group->id;
        Configs::dataManager->settingsRepo->current_group = targetGid;

        int count = 0;
        int firstImportedId = -1;
        Subscription::ParseSink sink;
        sink.profile = [&](std::shared_ptr<Configs::Profile> prof) {
            if (prof && prof->outbound) {
                prof->gid = targetGid;
                if (prof->name.trimmed().isEmpty()) {
                    prof->name = QStringLiteral("%1 %2").arg(prof->type.toUpper()).arg(count + 1);
                }
                // Country is left blank until a real test result comes back; the UI
                // hides the pill rather than showing a guess.
                bool added = Configs::dataManager->profilesRepo->AddProfile(prof, targetGid);
                if (added) {
                    if (firstImportedId < 0) {
                        firstImportedId = prof->id;
                    }
                    count++;
                }
            }
        };
        sink.log = [](const QString &msg) { qDebug() << "[SubImport]" << msg; };
        sink.warn = [](const QString &w1, const QString &w2) { qWarning() << "[SubImport]" << w1 << w2; };

        if (trimmed.startsWith(QStringLiteral("http://")) || trimmed.startsWith(QStringLiteral("https://"))) {
            Subscription::ParseDocument(contentData, sink);
        } else {
            Subscription::ParseText(trimmed, sink);
        }

        if (count == 0) {
            QString err = tr("В содержимом не найдено прокси-узлов");
            if (ToastManager::instance()) ToastManager::instance()->showError(err);
            emit importFinished(false, 0, err);
            return;
        }

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
    m_pings[profileId] = pingMs;
    if (profileId == m_selectedServerId) {
        emit selectedServerPingChanged(pingMs);
    }

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
    if (patched) {
        emit serversChanged();
    } else {
        reloadServers();
    }
}

int ConfigAdapter::autoUpdateSubsMode() const {
    return m_autoUpdateSubsMode;
}

void ConfigAdapter::setAutoUpdateSubsMode(int mode) {
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

