#include <iostream>
#include <QApplication>
#include <QTimer>
#include <QDebug>

#include "src/core/ThroneEngine.hpp"
#include "src/core/TrafficMonitor.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/bridge/MainWindowBridge.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/api/RPC.h"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "[TEST] Running Ping Diagnostic Test..." << std::endl;
    UI_InitMainWindow();

    ThroneEngine engine;
    engine.initialize(QStringLiteral("/home/kumite/.local/share/Beaxty/BeaxtyVPN/throne.db"));

    TrafficMonitor monitor;
    ConfigAdapter configAdapter;
    configAdapter.reloadServers();

    QTimer::singleShot(2000, [&]() {
        auto ids = Configs::dataManager->profilesRepo->GetAllProfileIds();
        std::cout << "Total profile IDs: " << ids.size() << std::endl;
        if (ids.isEmpty()) {
            std::cout << "No profiles found in DB!" << std::endl;
            app.quit();
            return;
        }

        for (int id : ids) {
            auto prof = Configs::dataManager->profilesRepo->GetProfile(id);
            if (prof) {
                std::cout << "Profile " << id << ": name=" << prof->name.toStdString() 
                          << " type=" << prof->type.toStdString() << std::endl;
            }
        }

        std::cout << "Testing API::defaultClient: " << (API::defaultClient != nullptr) << std::endl;
        std::cout << "Triggering monitor.testAllPings()..." << std::endl;
        monitor.testAllPings();
    });

    QObject::connect(&monitor, &TrafficMonitor::serverPingUpdated, [&](int id, int ping) {
        std::cout << ">>> PING RESULT for ID " << id << " = " << ping << " ms" << std::endl;
    });

    QObject::connect(&monitor, &TrafficMonitor::pingTestingChanged, [&](bool testing) {
        std::cout << ">>> pingTestingChanged: " << testing << std::endl;
        if (!testing) {
            std::cout << "All pings finished! Exiting." << std::endl;
            QTimer::singleShot(500, &app, &QApplication::quit);
        }
    });

    QTimer::singleShot(15000, [&]() {
        std::cout << "TIMEOUT (15s) reached!" << std::endl;
        app.quit();
    });

    return app.exec();
}
