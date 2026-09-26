// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <iostream>
#include <chrono>
#include <algorithm>
#include <QApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QFile>

#include "src/core/ThroneEngine.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/RoutingManager.hpp"
#include "src/core/TrafficMonitor.hpp"
#include "src/core/ToastManager.hpp"
#include "src/bridge/MainWindowBridge.hpp"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "============================================================" << std::endl;
    std::cout << "[TEST] Starting Connect Responsiveness & Non-Blocking UI Test" << std::endl;
    std::cout << "============================================================" << std::endl;

    UI_InitMainWindow();

    ToastManager toastManager;
    ThroneEngine engine;
    RoutingManager routingManager;
    ConfigAdapter configAdapter;
    TrafficMonitor trafficMonitor;

    QString testDbPath = QStringLiteral("/tmp/test_responsiveness.db");
    if (QFile::exists(testDbPath)) QFile::remove(testDbPath);

    engine.initialize(testDbPath);
    routingManager.initializeRouteProfiles();
    configAdapter.importSubscription(
        QStringLiteral("vless://11111111-1111-4111-8111-111111111111@198.51.100.10:443?encryption=none&security=reality&sni=example.invalid&fp=chrome&pbk=test-public-key&sid=1a2b3c4d&type=tcp#TestServer"),
        QStringLiteral("Responsiveness-Group")
    );
    configAdapter.reloadServers();

    QElapsedTimer loopTimer;
    loopTimer.start();
    qint64 lastTick = loopTimer.nsecsElapsed();
    qint64 maxLatencyNs = 0;
    int tickCount = 0;

    // 1ms high-resolution timer to measure event-loop jitter and blocking
    QTimer tickTimer;
    tickTimer.setInterval(1);
    QObject::connect(&tickTimer, &QTimer::timeout, [&]() {
        qint64 now = loopTimer.nsecsElapsed();
        qint64 delta = now - lastTick;
        lastTick = now;
        if (delta > maxLatencyNs) {
            maxLatencyNs = delta;
        }
        tickCount++;
    });
    tickTimer.start();

    bool stateConnectingSeen = false;
    QObject::connect(&engine, &ThroneEngine::stateChanged, [&](int s) {
        if (s == ThroneEngine::Connecting) {
            stateConnectingSeen = true;
            std::cout << "  -> Immediate stateChanged(Connecting) received on main thread" << std::endl;
        }
    });

    // Trigger connection after 50ms of baseline
    QTimer::singleShot(50, [&]() {
        std::cout << "[1] Triggering engine.startConnection()..." << std::endl;
        QElapsedTimer callTimer;
        callTimer.start();

        engine.startConnection();

        qint64 callDurationUs = callTimer.nsecsElapsed() / 1000;
        std::cout << "  -> startConnection() call returned in: " << callDurationUs << " µs" << std::endl;

        if (callDurationUs > 50000) { // 50 ms max call duration
            std::cerr << "FAILED: startConnection() took " << callDurationUs << " µs (exceeded 50ms synchronous limit)!" << std::endl;
            app.exit(1);
            return;
        }
    });

    // Stop and evaluate after 1500ms
    QTimer::singleShot(1500, [&]() {
        std::cout << "[2] Stopping connection and evaluating event loop metrics..." << std::endl;
        engine.stopConnection();

        double maxLatencyMs = static_cast<double>(maxLatencyNs) / 1000000.0;
        std::cout << "  -> Watchdog ticks recorded: " << tickCount << std::endl;
        std::cout << "  -> Maximum main-thread event loop latency: " << maxLatencyMs << " ms" << std::endl;

        if (!stateConnectingSeen) {
            std::cerr << "FAILED: Never transitioned to Connecting state!" << std::endl;
            app.exit(2);
            return;
        }

        // 60 FPS requires frame interval < 16.6 ms. Event loop latency < 15 ms guarantees 60 FPS.
        if (maxLatencyMs > 15.0) {
            std::cerr << "FAILED: Event loop latency exceeded 15 ms (" << maxLatencyMs << " ms)!" << std::endl;
            app.exit(3);
            return;
        }

        std::cout << "\n[PASS] UI thread remained completely responsive (< 15 ms latency, 60+ FPS maintained)!" << std::endl;
        std::cout << "============================================================" << std::endl;
        engine.cleanup();
        app.exit(0);
    });

    return app.exec();
}
