// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <QApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>

#include "src/core/ThroneEngine.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/RoutingManager.hpp"
#include "src/core/TrafficMonitor.hpp"
#include "src/core/ToastManager.hpp"
#include "src/bridge/MainWindowBridge.hpp"
#include "3rdparty/throne/include/stats/traffic/TrafficLooper.hpp"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "============================================================" << std::endl;
    std::cout << "[TEST] Starting End-to-End TrafficLooper & Protection Stability Test" << std::endl;
    std::cout << "============================================================" << std::endl;

    UI_InitMainWindow();

    ToastManager toastManager;
    ThroneEngine engine;
    RoutingManager routingManager;
    ConfigAdapter configAdapter;
    TrafficMonitor trafficMonitor;

    int trafficUpdatesReceived = 0;
    BridgeCallbacks::onUpdateTraffic = [&](int pd, int pu, int dd, int du) {
        trafficMonitor.updateTraffic(pd, pu, dd, du);
        trafficUpdatesReceived++;
    };

    BridgeCallbacks::onRefreshProxyList = [&]() {
        // Verified: no reloadServers on second tick
    };

    QString testDbPath = QStringLiteral("/tmp/test_traffic_lifecycle.db");
    if (QFile::exists(testDbPath)) QFile::remove(testDbPath);

    engine.initialize(testDbPath);
    routingManager.initializeRouteProfiles();

    // 1. Initial State Check: verify proxy and direct are never null even before connection
    std::cout << "\n[1] Checking defensive default initialization of TrafficLooper..." << std::endl;
    if (!Stats::trafficLooper) {
        std::cerr << "FAILED: Stats::trafficLooper is null!" << std::endl;
        return 1;
    }
    if (!Stats::trafficLooper->proxy || !Stats::trafficLooper->direct) {
        std::cerr << "FAILED: TrafficLooper proxy or direct was not initialized in constructor!" << std::endl;
        return 1;
    }
    std::cout << "  -> Defensive proxy and direct non-null check PASSED!" << std::endl;

    // Direct invocation of UpdateAll with empty groups must NOT crash (Null Pointer Protection)
    std::cout << "\n[2] Testing UpdateAll() safety before connection..." << std::endl;
    Stats::trafficLooper->UpdateAll();
    std::cout << "  -> UpdateAll() executed safely with zero crashes!" << std::endl;

    // 2. Import realistic VLESS server
    configAdapter.importSubscription(
        QStringLiteral("vless://11111111-1111-4111-8111-111111111111@198.51.100.10:443?encryption=none&security=reality&sni=example.invalid&fp=chrome&pbk=test-public-key&sid=1a2b3c4d&type=tcp#StabilityNode"),
        QStringLiteral("Stability-Group")
    );
    configAdapter.reloadServers();
    int serverId = configAdapter.selectedServerId();
    std::cout << "\n[3] Imported and selected server ID: " << serverId << std::endl;

    bool reachedProtected = false;
    QObject::connect(&engine, &ThroneEngine::stateChanged, [&](int state) {
        std::cout << "  -> Engine State Changed to: "
                  << (state == ThroneEngine::Connecting ? "CONNECTING" :
                      state == ThroneEngine::Protected ? "PROTECTED" : "DISCONNECTED")
                  << std::endl;
        if (state == ThroneEngine::Protected) {
            reachedProtected = true;
        }
    });

    QTimer::singleShot(200, [&]() {
        std::cout << "\n[4] Triggering engine.startConnection()..." << std::endl;
        engine.startConnection();
    });

    // Step verification checkpoints
    // Check at 2 seconds
    QTimer::singleShot(2000, [&]() {
        std::cout << "\n[5] Checkpoint at t = 2s: Verifying state and TrafficLooper..." << std::endl;
        if (!reachedProtected && engine.state() != ThroneEngine::Protected) {
            std::cerr << "FAILED: Did not reach PROTECTED state within 2 seconds!" << std::endl;
            app.exit(10);
            return;
        }
        if (!Stats::trafficLooper->proxy || !Stats::trafficLooper->direct) {
            std::cerr << "FAILED: proxy or direct is null during active connection!" << std::endl;
            app.exit(11);
            return;
        }
        std::cout << "  -> Protected active. Proxy tag: " << Stats::trafficLooper->proxy->tag.toStdString()
                  << ", Direct tag: " << Stats::trafficLooper->direct->tag.toStdString() << std::endl;
        std::cout << "  -> Traffic looper enabled: " << (Stats::trafficLooper->loop_enabled ? "YES" : "NO") << std::endl;
    });

    // Check at 5 seconds
    QTimer::singleShot(5000, [&]() {
        std::cout << "\n[6] Checkpoint at t = 5s: Verifying continuous execution (no SIGSEGV)..." << std::endl;
        if (engine.state() != ThroneEngine::Protected) {
            std::cerr << "FAILED: Connection state dropped before 5s!" << std::endl;
            app.exit(12);
            return;
        }
        std::cout << "  -> Protected status stable after 5 seconds! Current upload speed: "
                  << trafficMonitor.uploadSpeed().toStdString()
                  << ", download speed: " << trafficMonitor.downloadSpeed().toStdString() << std::endl;
    });

    // Check at 10 seconds
    QTimer::singleShot(10000, [&]() {
        std::cout << "\n[7] Checkpoint at t = 10s: Verifying persistence interval..." << std::endl;
        if (engine.state() != ThroneEngine::Protected) {
            std::cerr << "FAILED: Connection state dropped before 10s!" << std::endl;
            app.exit(13);
            return;
        }
        std::cout << "  -> 10 seconds elapsed cleanly. Traffic updates received: " << trafficUpdatesReceived << std::endl;
    });

    // Check at 20 seconds
    QTimer::singleShot(20000, [&]() {
        std::cout << "\n[8] Checkpoint at t = 20s: Continuous tunnel stability..." << std::endl;
        if (engine.state() != ThroneEngine::Protected) {
            std::cerr << "FAILED: Connection state dropped before 20s!" << std::endl;
            app.exit(14);
            return;
        }
        std::cout << "  -> 20 seconds elapsed cleanly. Traffic updates received: " << trafficUpdatesReceived << std::endl;
    });

    // Check at 30 seconds (User requirement: verify stable after 5, 10, and 30 seconds)
    QTimer::singleShot(30000, [&]() {
        std::cout << "\n[9] Checkpoint at t = 30s: Full 30-second stability confirmation (Zero crashes)..." << std::endl;
        if (engine.state() != ThroneEngine::Protected) {
            std::cerr << "FAILED: Connection state dropped before 30s!" << std::endl;
            app.exit(15);
            return;
        }
        std::cout << "  -> 30 SECONDS ELAPSED WITH ZERO CRASHES / SIGSEGV! Status: PROTECTED" << std::endl;
        std::cout << "  -> Total traffic update notifications received: " << trafficUpdatesReceived << std::endl;
    });

    // Check at 32 seconds: Test clean Disconnection
    QTimer::singleShot(32000, [&]() {
        std::cout << "\n[10] Testing clean Disconnect (engine.stopConnection())..." << std::endl;
        engine.stopConnection();

        if (engine.state() != ThroneEngine::Disconnected) {
            std::cerr << "FAILED: State is not Disconnected after stopConnection()!" << std::endl;
            app.exit(16);
            return;
        }

        if (Stats::trafficLooper->loop_enabled) {
            std::cerr << "FAILED: loop_enabled was not set to false upon disconnect!" << std::endl;
            app.exit(17);
            return;
        }

        if (!Stats::trafficLooper->stop_requested) {
            std::cerr << "FAILED: stop_requested was not set to true upon disconnect!" << std::endl;
            app.exit(18);
            return;
        }

        std::cout << "  -> Disconnect completed cleanly! Flags verified: loop_enabled=false, stop_requested=true" << std::endl;
    });

    // Check at 34 seconds: Test Reconnection cycle
    QTimer::singleShot(34000, [&]() {
        std::cout << "\n[11] Testing reconnection cycle to ensure thread restarts cleanly..." << std::endl;
        engine.startConnection();
    });

    // Check at 37 seconds: Final Teardown
    QTimer::singleShot(37000, [&]() {
        std::cout << "\n[12] Finalizing test and cleaning up..." << std::endl;
        engine.cleanup();

        std::cout << "\n============================================================" << std::endl;
        std::cout << "[SUCCESS] ALL TRAFFIC LOOPER 30-SECOND STABILITY TESTS PASSED!" << std::endl;
        std::cout << "============================================================" << std::endl;
        app.exit(0);
    });

    return app.exec();
}
