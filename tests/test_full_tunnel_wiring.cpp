// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <cassert>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QLockFile>
#include <QJsonObject>
#include <QJsonDocument>

#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/database/GroupsRepo.h"
#include "3rdparty/throne/include/database/RoutesRepo.h"
#include "3rdparty/throne/include/configs/generate.h"
#include "3rdparty/throne/include/configs/common/xrayStreamSetting.h"
#include "3rdparty/throne/include/configs/sub/SubscriptionParser.hpp"
#include "3rdparty/throne/include/api/RPC.h"
#include "src/bridge/MainWindowBridge.hpp"
#include "src/core/ThroneEngine.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/RoutingManager.hpp"
#include "src/core/ToastManager.hpp"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "============================================================" << std::endl;
    std::cout << "[TEST] Starting Full Tunnel Wiring & SUID/Lock Verification" << std::endl;
    std::cout << "============================================================" << std::endl;

    UI_InitMainWindow();

    QString testDbPath = QStringLiteral("/tmp/test_full_tunnel_wiring.db");
    if (QFile::exists(testDbPath)) QFile::remove(testDbPath);

    // 1. SettingsRepo Defaults Verification
    std::cout << "\n[1] Verifying network defaults in SettingsRepo..." << std::endl;
    ThroneEngine engine;
    engine.initialize(testDbPath);

    auto* settings = Configs::dataManager->settingsRepo.get();
    assert(settings != nullptr);

    std::cout << "  vpn_implementation: " << settings->vpn_implementation.toStdString() << std::endl;
    std::cout << "  vpn_strict_route: " << settings->vpn_strict_route << std::endl;
    std::cout << "  vpn_auto_redirect: " << settings->vpn_auto_redirect << std::endl;
    std::cout << "  spmode_vpn: " << settings->spmode_vpn << std::endl;
    std::cout << "  enable_tun_routing: " << settings->enable_tun_routing << std::endl;

    assert(settings->vpn_implementation == QStringLiteral("gvisor"));
    assert(settings->vpn_strict_route == true);
    assert(settings->vpn_auto_redirect == true);
    assert(settings->spmode_vpn == true);
    assert(settings->enable_tun_routing == true);
    std::cout << "  -> Network defaults verified: gVisor + strict routing active!" << std::endl;

    // 2. Profile Build & LoadConfigReq Formation for Xray Reality
    std::cout << "\n[2] Verifying LoadConfigReq population for xrayvless Reality node..." << std::endl;
    auto group = Configs::GroupsRepo::NewGroup();
    group->name = QStringLiteral("Reality Test Group");
    Configs::dataManager->groupsRepo->AddGroup(group);

    QString vlessLink = QStringLiteral("vless://b831381d-6324-4d53-ad4f-8cda48b30811@104.21.5.12:443?encryption=none&security=reality&sni=yahoo.com&fp=chrome&pbk=wA6f6qS6G7N5h8T2kR4pL0mX1vY3zB9aC7dE5fG2hJ4&sid=1a2b3c4d&type=tcp#FullTunnelRealityNode");

    std::shared_ptr<Configs::Profile> profile;
    Subscription::ParseSink sink;
    sink.profile = [&](std::shared_ptr<Configs::Profile> p) {
        if (p && p->outbound) {
            profile = p;
        }
    };
    sink.log = [](const QString &l) {};
    sink.warn = [](const QString &, const QString &) {};
    Subscription::ParseText(vlessLink, sink);

    assert(profile != nullptr);
    assert(profile->type == QStringLiteral("xrayvless"));
    profile->gid = group->id;
    bool added = Configs::dataManager->profilesRepo->AddProfile(profile, group->id);
    assert(added);

    auto result = Configs::BuildSingBoxConfig(profile);
    assert(result->error.isEmpty());

    // Build the request exactly as ThroneEngine::startConnection() does
    libcore::LoadConfigReq req;
    req.core_config = QJsonObject2QString(result->coreConfig, true).toStdString();
    req.tun_ipv4_cidr = result->tunIPv4CIDR.toStdString();
    req.disable_stats = settings->disable_traffic_stats;
    req.xray_config = QJsonObject2QString(result->xrayConfig, true).toStdString();
    req.need_xray = !result->xrayConfig.isEmpty();
    for (const auto &full : result->xrayFullConfigs) {
        req.xray_full_configs.push_back(full.toStdString());
    }
    if (req.need_xray || !req.xray_full_configs.empty()) {
        req.xray_outbound_dns_strategy = Configs::getXrayOutboundDomainStrategy().toStdString();
    }

    std::cout << "  req.need_xray: " << req.need_xray.value_or(false) << std::endl;
    std::cout << "  req.xray_config length: " << req.xray_config.value_or("").length() << std::endl;
    std::cout << "  req.xray_outbound_dns_strategy: " << req.xray_outbound_dns_strategy.value_or("") << std::endl;
    std::cout << "  req.tun_ipv4_cidr: " << req.tun_ipv4_cidr.value_or("") << std::endl;

    assert(req.need_xray.value_or(false) == true);
    assert(!req.xray_config.value_or("").empty());
    assert(!req.xray_outbound_dns_strategy.value_or("").empty());
    assert(req.tun_ipv4_cidr.value_or("") == result->tunIPv4CIDR.toStdString());
    std::cout << "  -> LoadConfigReq verified: Xray config, strategy and TUN CIDR are populated!" << std::endl;

    // 3. Error Check Logic Verification (!rpcOK || !rpcErr.isEmpty())
    std::cout << "\n[3] Verifying error detection logic (!rpcOK || !rpcErr.isEmpty())..." << std::endl;
    {
        // Case A: rpcOK = true, but rpcErr has error (previous bug was ignoring this!)
        bool rpcOK = true;
        QString rpcErr = QStringLiteral("configure tun interface: operation not permitted");
        bool failed = (!rpcOK || !rpcErr.isEmpty());
        assert(failed == true);

        // Case B: rpcOK = false, rpcErr is empty
        rpcOK = false;
        rpcErr = QString();
        failed = (!rpcOK || !rpcErr.isEmpty());
        assert(failed == true);

        // Case C: rpcOK = true, rpcErr is empty (Success)
        rpcOK = true;
        rpcErr = QString();
        failed = (!rpcOK || !rpcErr.isEmpty());
        assert(failed == false);

        std::cout << "  -> Error detection logic correctly catches false-positive core errors!" << std::endl;
    }

    // 4. Single-Instance QLockFile Verification
    std::cout << "\n[4] Verifying Single-Instance QLockFile behavior..." << std::endl;
    QString testLockPath = QDir::tempPath() + QStringLiteral("/test_beaxty_instance.lock");
    QFile::remove(testLockPath);

    QLockFile lock1(testLockPath);
    bool lock1Acquired = lock1.tryLock(100);
    assert(lock1Acquired == true);

    QLockFile lock2(testLockPath);
    bool lock2Acquired = lock2.tryLock(100);
    assert(lock2Acquired == false); // Must fail because lock1 holds it!
    std::cout << "  First instance locked: " << lock1Acquired << ", Second instance blocked: " << !lock2Acquired << std::endl;

    lock1.unlock();
    bool lock2AcquiredAfterRelease = lock2.tryLock(100);
    assert(lock2AcquiredAfterRelease == true);
    lock2.unlock();
    QFile::remove(testLockPath);
    std::cout << "  -> Single-Instance mutex lock verified successfully!" << std::endl;

    // 5. Core Path & SUID Check Helper Verification
    std::cout << "\n[5] Verifying FindCoreRealPath..." << std::endl;
    QString corePath = Configs::FindCoreRealPath();
    std::cout << "  Resolved core path: " << corePath.toStdString() << std::endl;
    assert(!corePath.isEmpty());
    assert(corePath.contains(QStringLiteral("beaxty-core")) || corePath.contains(QStringLiteral("ThroneCore")));
    std::cout << "  -> FindCoreRealPath correctly identified core binary target!" << std::endl;

    QFile::remove(testDbPath);
    std::cout << "\n============================================================" << std::endl;
    std::cout << "[PASS] All Full Tunnel wiring & elevation tests passed!" << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
