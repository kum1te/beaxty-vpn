// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <cassert>
#include <QApplication>
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

#include "src/core/RoutingManager.hpp"
#include "src/core/ToastManager.hpp"
#include "src/bridge/MainWindowBridge.hpp"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "============================================================" << std::endl;
    std::cout << "[TEST] Starting Per-App & Unified Split Tunneling Unit Test" << std::endl;
    std::cout << "============================================================" << std::endl;

    UI_InitMainWindow();
    MW_show_log = [](const QString &msg) { /* silence or std::cout << msg.toStdString() << std::endl; */ };
    MW_dialog_message = [](MwMessage, QStringList) {};

    QTemporaryDir tempDir(QStringLiteral("beaxty-per-app-routing-XXXXXX"));
    if (!tempDir.isValid()) return 1;
    const QString testDb = tempDir.filePath(QStringLiteral("throne.db"));

    Configs::initDB(testDb.toStdString());

    // 1. Process Scanner Verification (/proc and .desktop parsing)
    std::cout << "\n[1] Testing getRunningApplications() scanner..." << std::endl;
    RoutingManager routingMgr;
    routingMgr.initializeRouteProfiles();

    QVariantList runningApps = routingMgr.getRunningApplications();
    std::cout << "  Found " << runningApps.size() << " running applications from /proc." << std::endl;
    assert(!runningApps.isEmpty());

    bool foundValidStructure = false;
    for (int i = 0; i < qMin(5, runningApps.size()); ++i) {
        QVariantMap item = runningApps[i].toMap();
        std::cout << "    - Process: " << item.value("processName").toString().toStdString()
                  << " | Display: " << item.value("displayName").toString().toStdString() << std::endl;
        if (!item.value("processName").toString().isEmpty() && !item.value("displayName").toString().isEmpty()) {
            foundValidStructure = true;
        }
    }
    assert(foundValidStructure);
    std::cout << "  Process scanner verified successfully!" << std::endl;

    // 2. Selected Apps List Management
    std::cout << "\n[2] Testing addApp, removeApp, clearApps..." << std::endl;
    routingMgr.clearApps();
    assert(routingMgr.selectedApps().isEmpty());

    routingMgr.addApp(QStringLiteral("telegram-desktop"));
    routingMgr.addApp(QStringLiteral("firefox"));
    routingMgr.addApp(QStringLiteral("discord"));
    assert(routingMgr.selectedApps().size() == 3);
    assert(routingMgr.selectedApps().contains(QStringLiteral("telegram-desktop")));
    assert(routingMgr.selectedApps().contains(QStringLiteral("firefox")));
    assert(routingMgr.selectedApps().contains(QStringLiteral("discord")));

    // Test duplicate avoidance
    routingMgr.addApp(QStringLiteral("firefox"));
    assert(routingMgr.selectedApps().size() == 3);

    // Test removeApp
    int idxFirefox = routingMgr.selectedApps().indexOf(QStringLiteral("firefox"));
    assert(idxFirefox >= 0);
    routingMgr.removeApp(idxFirefox);
    assert(routingMgr.selectedApps().size() == 2);
    assert(!routingMgr.selectedApps().contains(QStringLiteral("firefox")));
    std::cout << "  App list mutation verified!" << std::endl;

    // 3. Preset Switching: Unified Split Tunneling (Proxy Mode vs Bypass Mode)
    std::cout << "\n[3] Testing Unified Split Tunneling (Proxy mode and Bypass mode)..." << std::endl;
    routingMgr.setAppRoutingMode(0); // Proxy selected
    routingMgr.setActivePreset(RoutingManager::SplitTunneling);
    assert(routingMgr.activePreset() == RoutingManager::SplitTunneling);
    assert(routingMgr.appRoutingMode() == 0);

    auto routesRepo = Configs::dataManager->routesRepo.get();
    int currentRouteId = Configs::dataManager->settingsRepo->current_route_id;
    auto splitProf = routesRepo->GetRouteProfile(currentRouteId);
    assert(splitProf != nullptr);
    assert(splitProf->name == QStringLiteral("Split Tunneling"));
    assert(splitProf->defaultOutboundID == Configs::directID);
    assert(!splitProf->Rules.isEmpty());
    // Rule 1: processes
    assert(splitProf->Rules[0]->outboundID == Configs::proxyID);
    assert(splitProf->Rules[0]->process_name.contains(QStringLiteral("telegram-desktop")));
    // Rule 2: domains
    assert(splitProf->Rules.size() >= 2);
    assert(splitProf->Rules[1]->outboundID == Configs::proxyID);
    assert(!splitProf->Rules[1]->domain_suffix.isEmpty());
    std::cout << "  SplitTunneling (Proxy mode) verified (sequential rules: process -> domain -> direct default)" << std::endl;

    // Switch to Bypass Mode (mode 1)
    routingMgr.setAppRoutingMode(1);
    assert(routingMgr.appRoutingMode() == 1);
    splitProf = routesRepo->GetRouteProfile(currentRouteId);
    assert(splitProf != nullptr);
    assert(splitProf->defaultOutboundID == Configs::proxyID);
    assert(splitProf->Rules[0]->outboundID == Configs::directID);
    assert(splitProf->Rules[0]->process_name.contains(QStringLiteral("telegram-desktop")));
    std::cout << "  SplitTunneling (Bypass mode) verified (default: proxy, apps & domains: direct)" << std::endl;

    // 4. Persistence Test across Application Restart
    std::cout << "\n[4] Testing persistence of unified split tunneling settings in SQLite..." << std::endl;
    RoutingManager restartedMgr;
    restartedMgr.initializeRouteProfiles();
    assert(restartedMgr.activePreset() == RoutingManager::SplitTunneling);
    assert(restartedMgr.appRoutingMode() == 1);
    assert(restartedMgr.selectedApps().size() == 2);
    assert(restartedMgr.selectedApps().contains(QStringLiteral("telegram-desktop")));
    assert(restartedMgr.selectedApps().contains(QStringLiteral("discord")));
    std::cout << "  SQLite persistence across restart verified successfully!" << std::endl;

    // 5. Sing-Box Config Generation: route["find_process"] == true
    std::cout << "\n[5] Testing Sing-Box JSON generation with find_process..." << std::endl;
    auto group = Configs::GroupsRepo::NewGroup();
    group->name = QStringLiteral("Test App Group");
    Configs::dataManager->groupsRepo->AddGroup(group);

    auto prof = Configs::ProfilesRepo::NewProfile(QStringLiteral("vless"));
    prof->name = QStringLiteral("Per-App VLESS Node");
    prof->gid = group->id;

    auto vless = std::make_unique<Configs::vless>();
    vless->server = QStringLiteral("perapp.beaxtyvpn.net");
    vless->server_port = 443;
    vless->uuid = QStringLiteral("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    prof->outbound = std::move(vless);

    Configs::dataManager->settingsRepo->current_group = group->id;
    Configs::dataManager->profilesRepo->AddProfile(prof, group->id);

    // Build sing-box config
    auto buildResult = Configs::BuildSingBoxConfig(prof);
    if (!buildResult->error.isEmpty()) {
        std::cerr << "FAIL: BuildSingBoxConfig returned error: " << buildResult->error.toStdString() << std::endl;
        return 1;
    }

    QJsonObject coreConfig = buildResult->coreConfig;
    assert(coreConfig.contains("route"));
    QJsonObject routeObj = coreConfig["route"].toObject();

    std::cout << "  route.find_process: " << (routeObj["find_process"].toBool() ? "true" : "false") << std::endl;
    if (!routeObj["find_process"].toBool()) {
        std::cerr << "FAIL: route.find_process must be true when per-app process rules are active!" << std::endl;
        return 1;
    }

    // Verify rules inside route object have process_name matching our selected apps
    QJsonArray routeRules = routeObj["rules"].toArray();
    bool foundProcessRule = false;
    for (const auto &rVal : routeRules) {
        QJsonObject r = rVal.toObject();
        if (r.contains("process_name")) {
            QJsonArray procArr = r["process_name"].toArray();
            std::cout << "  Found process_name rule with " << procArr.size() << " entries: ";
            for (const auto &p : procArr) {
                std::cout << p.toString().toStdString() << " ";
            }
            std::cout << std::endl;
            if (procArr.contains(QJsonValue(QStringLiteral("telegram-desktop")))) {
                foundProcessRule = true;
            }
        }
    }
    assert(foundProcessRule);
    std::cout << "  find_process & process_name rule presence verified in generated Sing-Box config!" << std::endl;

    QFile::remove(testDb);

    std::cout << "\n============================================================" << std::endl;
    std::cout << "[SUCCESS] ALL PER-APP & SPLIT TUNNELING UNIT TESTS PASSED!" << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
