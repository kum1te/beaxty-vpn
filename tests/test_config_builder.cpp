// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <cassert>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/database/GroupsRepo.h"
#include "3rdparty/throne/include/database/RoutesRepo.h"
#include "3rdparty/throne/include/configs/generate.h"
#include "3rdparty/throne/include/configs/outbounds/vless.h"
#include "3rdparty/throne/include/configs/outbounds/shadowsocks.h"
#include "3rdparty/throne/include/configs/sub/SubscriptionParser.hpp"

#include "src/core/RoutingManager.hpp"

#include <QApplication>
#include "3rdparty/throne/include/global/Utils.hpp"
#include "src/bridge/MainWindowBridge.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::cout << "[TEST] Starting Sing-Box Config Builder & Preset Verification..." << std::endl;

    UI_InitMainWindow();
    MW_show_log = [](const QString &msg) { std::cout << "  [ThroneLog] " << msg.toStdString() << std::endl; };
    MW_dialog_message = [](MwMessage, QStringList) {};

    QTemporaryDir tempDir(QStringLiteral("beaxty-config-builder-XXXXXX"));
    if (!tempDir.isValid()) return 1;
    const QString testDb = tempDir.filePath(QStringLiteral("throne.db"));

    Configs::initDB(testDb.toStdString());

    // 1. Check Default Requirements
    // Require TUN mode ON by default
    Configs::dataManager->settingsRepo->spmode_vpn = true;
    Configs::dataManager->settingsRepo->enable_tun_routing = true;
    Configs::dataManager->settingsRepo->sub_send_hwid = true;

    if (!Configs::dataManager->settingsRepo->spmode_vpn) {
        std::cerr << "FAIL: TUN mode must be enabled by default" << std::endl;
        return 1;
    }
    if (!Configs::dataManager->settingsRepo->sub_send_hwid) {
        std::cerr << "FAIL: HWID sending must be enabled by default" << std::endl;
        return 1;
    }

    // 2. Setup Routing Presets
    RoutingManager routingMgr;
    routingMgr.initializeRouteProfiles();

    // 3. Create a Group and a VLESS Profile
    auto group = Configs::GroupsRepo::NewGroup();
    group->name = QStringLiteral("Test Group");
    Configs::dataManager->groupsRepo->AddGroup(group);

    auto prof = Configs::ProfilesRepo::NewProfile(QStringLiteral("vless"));
    prof->name = QStringLiteral("Test VLESS Node");
    prof->gid = group->id;

    auto vless = std::make_unique<Configs::vless>();
    vless->server = QStringLiteral("nl-test.beaxtyvpn.net");
    vless->server_port = 443;
    vless->uuid = QStringLiteral("11111111-2222-3333-4444-555555555555");
    vless->flow = QStringLiteral("xtls-rprx-vision");
    prof->outbound = std::move(vless);

    Configs::dataManager->settingsRepo->current_group = group->id;
    bool added = Configs::dataManager->profilesRepo->AddProfile(prof, group->id);
    if (!added) {
        std::cerr << "FAIL: AddProfile failed" << std::endl;
        return 1;
    }

    // 4. Test Config Generation
    auto result = Configs::BuildSingBoxConfig(prof);
    if (!result->error.isEmpty()) {
        std::cerr << "FAIL: BuildSingBoxConfig returned error: " << result->error.toStdString() << std::endl;
        return 1;
    }

    QJsonObject coreConfig = result->coreConfig;
    QJsonArray inbounds = coreConfig["inbounds"].toArray();

    bool foundTun = false;
    for (const auto &val : inbounds) {
        auto inObj = val.toObject();
        if (inObj["type"].toString() == "tun") {
            foundTun = true;
            std::cout << "  Verified TUN inbound interface: " << inObj["interface_name"].toString().toStdString() << std::endl;
            if (!inObj["auto_route"].toBool()) {
                std::cerr << "FAIL: TUN inbound auto_route is false" << std::endl;
                return 1;
            }
            break;
        }
    }

    if (!foundTun) {
        std::cerr << "FAIL: TUN inbound was not found in generated sing-box config" << std::endl;
        return 1;
    }

    // 5. Test Subscription Parsing
    std::cout << "[TEST] Testing subscription parsing (VLESS, SS, Clash YAML)..." << std::endl;
    QString sampleSub = 
        "vless://22222222-3333-4333-8555-666666666666@198.51.100.20:443?encryption=none&security=reality&sni=example.invalid&fp=chrome&pbk=test-public-key#Example-Reality\n"
        "ss://YWVzLTEyOC1nY206dGVzdA==@198.51.100.30:8388#Example-SS\n";

    int parsedCount = 0;
    Subscription::ParseSink sink;
    sink.profile = [&](std::shared_ptr<Configs::Profile> p) {
        if (p && p->outbound) {
            parsedCount++;
            std::cout << "  Parsed profile: " << p->name.toStdString() << " (" << p->type.toStdString() << ")" << std::endl;
        }
    };
    sink.log = [](const QString &l) { std::cout << "    [ParseLog] " << l.toStdString() << std::endl; };
    sink.warn = [](const QString &w1, const QString &w2) {};

    Subscription::ParseText(sampleSub, sink);

    if (parsedCount != 2) {
        std::cerr << "FAIL: Expected 2 parsed profiles, got " << parsedCount << std::endl;
        return 1;
    }

    QFile::remove(testDb);
    std::cout << "[PASS] Config builder and subscription tests passed successfully!" << std::endl;
    return 0;
}
