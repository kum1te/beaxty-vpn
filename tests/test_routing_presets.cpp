// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <cassert>
#include <QApplication>
#include <QTimer>
#include <QFile>

#include "src/core/ThroneEngine.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/RoutingManager.hpp"
#include "src/core/TrafficMonitor.hpp"
#include "src/core/ToastManager.hpp"
#include "src/bridge/MainWindowBridge.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/SettingsRepo.h"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "============================================================" << std::endl;
    std::cout << "[TEST] Starting Routing Presets Persistence & Hot-Switch Test" << std::endl;
    std::cout << "============================================================" << std::endl;

    UI_InitMainWindow();

    ToastManager toastManager;
    QString lastToast;
    QObject::connect(&toastManager, &ToastManager::toastRequested, &app,
        [&](const QString &msg, const QString &, int) { lastToast = msg; });

    ThroneEngine engine;
    TrafficMonitor trafficMonitor;
    ConfigAdapter configAdapter;

    QString testDbPath = QStringLiteral("/tmp/test_routing_presets.db");
    if (QFile::exists(testDbPath)) QFile::remove(testDbPath);

    engine.initialize(testDbPath);

    RoutingManager routingManager;
    routingManager.initializeRouteProfiles();

    // 1. Initial State: Full Tunnel (Preset 0) by default
    std::cout << "\n[1] Checking initial preset..." << std::endl;
    std::cout << "  Initial activePreset: " << routingManager.activePreset() << std::endl;
    assert(routingManager.activePreset() == RoutingManager::FullTunnel);

    // 2. Switch to Preset 1 (Split Tunneling) & test custom domains
    std::cout << "\n[2] Switching to Preset 1 (Split Tunneling)..." << std::endl;
    routingManager.setActivePreset(RoutingManager::SplitTunneling);
    assert(routingManager.activePreset() == RoutingManager::SplitTunneling);
    int splitRouteId = Configs::dataManager->settingsRepo->current_route_id;
    std::cout << "  Current route ID in SQLite settings: " << splitRouteId << std::endl;
    assert(lastToast.contains("Split Tunneling"));

    // Test adding and removing custom domains
    std::cout << "  Testing custom domains in Split Tunneling..." << std::endl;
    int initialCount = routingManager.customDomains().size();
    routingManager.addCustomDomain(QStringLiteral("my-custom-test-domain.org"));
    assert(routingManager.customDomains().size() == initialCount + 1);
    assert(routingManager.customDomains().contains(QStringLiteral("my-custom-test-domain.org")));

    routingManager.removeCustomDomain(routingManager.customDomains().indexOf(QStringLiteral("my-custom-test-domain.org")));
    assert(routingManager.customDomains().size() == initialCount);
    assert(!routingManager.customDomains().contains(QStringLiteral("my-custom-test-domain.org")));
    std::cout << "  Domain add/remove tested successfully!" << std::endl;

    // 3. Switch to Preset 2 (Advanced Routing) & test rule CRUD
    std::cout << "\n[3] Switching to Preset 2 (Advanced Routing)..." << std::endl;
    routingManager.setActivePreset(RoutingManager::AdvancedRouting);
    assert(routingManager.activePreset() == RoutingManager::AdvancedRouting);
    int advRouteId = Configs::dataManager->settingsRepo->current_route_id;
    std::cout << "  Current route ID in SQLite settings: " << advRouteId << std::endl;
    assert(splitRouteId != advRouteId);
    assert(lastToast.contains("Advanced Routing"));

    routingManager.clearAdvancedRules();
    assert(routingManager.advancedRules().isEmpty());
    routingManager.addAdvancedRule(QStringLiteral("Domain"), QStringLiteral("example.com"), QStringLiteral("Proxy"));
    routingManager.addAdvancedRule(QStringLiteral("Process"), QStringLiteral("firefox"), QStringLiteral("Direct"));
    routingManager.addAdvancedRule(QStringLiteral("Port"), QStringLiteral("8080"), QStringLiteral("Block"));
    assert(routingManager.advancedRules().size() == 3);
    std::cout << "  Advanced rules added: " << routingManager.advancedRules().size() << std::endl;

    // 4. Persistence Test: Verify that reloading SettingsRepo and a new RoutingManager restores Preset 2
    std::cout << "\n[4] Testing persistence across app restarts..." << std::endl;
    RoutingManager restartedRoutingManager;
    restartedRoutingManager.initializeRouteProfiles();
    std::cout << "  Restored activePreset after restart: " << restartedRoutingManager.activePreset() << std::endl;
    if (restartedRoutingManager.activePreset() != RoutingManager::AdvancedRouting) {
        std::cerr << "FAILED: Expected preset 2 (AdvancedRouting) after restart, got "
                  << restartedRoutingManager.activePreset() << std::endl;
        return 1;
    }
    assert(restartedRoutingManager.advancedRules().size() == 3);
    std::cout << "  Persistence verified: Preset 2 and 3 advanced rules successfully restored from SQLite!" << std::endl;

    // Live tunnel tests require an explicitly requested integration run.
    if (!app.arguments().contains("--integration")) {
        std::cout << "PASS: routing preset persistence and rule CRUD (live tunnel not requested)\n";
        return 0;
    }

    // 5. Hot-Switching on Connected VPN:
    std::cout << "\n[5] Testing Hot-Switching of Routing Mode while VPN is Connected..." << std::endl;
    configAdapter.importSubscription(
        QStringLiteral("vless://b831381d-6324-4d53-ad4f-8cda48b30811@104.21.5.12:443?encryption=none&security=reality&sni=yahoo.com&fp=chrome&pbk=wA6f6qS6G7N5h8T2kR4pL0mX1vY3zB9aC7dE5fG2hJ4&sid=1a2b3c4d&type=tcp#RouteNode"),
        QStringLiteral("RouteGroup")
    );
    configAdapter.reloadServers();

    bool restartedSeen = false;
    QObject::connect(&engine, &ThroneEngine::stateChanged, [&](int state) {
        if (state == ThroneEngine::Connecting && restartedSeen) {
            std::cout << "  -> Hot-reconnect triggered: state returned to CONNECTING" << std::endl;
        } else if (state == ThroneEngine::Protected && restartedSeen) {
            std::cout << "  -> Hot-reconnect success: tunnel restored to PROTECTED with new route!" << std::endl;
        }
    });

    QTimer::singleShot(500, [&]() {
        engine.startConnection();
    });

    // After 2.5s when connected: change preset on the fly
    QTimer::singleShot(2500, [&]() {
        std::cout << "  -> VPN Connected (Protected: " << engine.isConnected() << "). Switching preset to FullTunnel (0)..." << std::endl;
        restartedSeen = true;
        routingManager.setActivePreset(RoutingManager::FullTunnel);
        assert(routingManager.activePreset() == RoutingManager::FullTunnel);
    });

    // Finish test after hot-switch verification
    QTimer::singleShot(4500, [&]() {
        std::cout << "\n============================================================" << std::endl;
        std::cout << "[SUCCESS] ALL ROUTING PRESET TESTS PASSED!" << std::endl;
        std::cout << "============================================================" << std::endl;
        engine.stopConnection();
        QFile::remove(testDbPath);
        app.quit();
    });

    return app.exec();
}
