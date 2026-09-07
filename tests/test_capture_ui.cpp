// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <iostream>
#include <vector>
#include <functional>

#include "src/core/ThroneEngine.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/DeviceIdentity.hpp"
#include "src/core/RoutingManager.hpp"
#include "src/core/TrafficMonitor.hpp"
#include "src/core/ToastManager.hpp"
#include "src/core/LocalizationManager.hpp"
#include "src/core/AppPrefs.hpp"
#include "src/ui/Theme.hpp"
#include "src/bridge/MainWindowBridge.hpp"

int main(int argc, char *argv[]) {
    // Force offscreen platform for headless environments (CI / Container / Sandbox)
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QApplication app(argc, argv);

    UI_InitMainWindow();

    Theme theme;
    ToastManager toastManager;
    ThroneEngine engine;
    DeviceIdentity deviceIdentity;
    RoutingManager routingManager;
    ConfigAdapter configAdapter;
    TrafficMonitor trafficMonitor;
    LocalizationManager locManager;
    AppPrefsService appPrefsService;

    QString dbPath = QStringLiteral("/tmp/test_capture_ui.db");
    if (QFile::exists(dbPath)) QFile::remove(dbPath);

    engine.initialize(dbPath);
    routingManager.initializeRouteProfiles();

    configAdapter.setDemoDataEnabled(true);
    configAdapter.reloadServers();

    QQmlApplicationEngine qmlEngine;
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("Theme"), &theme);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("toastManager"), &toastManager);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("throneEngine"), &engine);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("configAdapter"), &configAdapter);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("deviceIdentity"), &deviceIdentity);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("routingManager"), &routingManager);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("trafficMonitor"), &trafficMonitor);
    locManager.initialize(&qmlEngine);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("locManager"), &locManager);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("i18n"), &locManager);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("appPrefs"), &appPrefsService);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("AppPrefs"), &appPrefsService);

    qmlEngine.addImportPath(QStringLiteral("qrc:/qml"));
    qmlEngine.load(QUrl(QStringLiteral("qrc:/qml/App.qml")));

    if (qmlEngine.rootObjects().isEmpty()) {
        std::cerr << "Failed to load App.qml root object" << std::endl;
        return 1;
    }

    QObject *rootObj = qmlEngine.rootObjects().first();
    QQuickWindow *window = qobject_cast<QQuickWindow *>(rootObj);
    if (!window) {
        std::cerr << "Failed to cast root object to QQuickWindow" << std::endl;
        return 1;
    }

    window->show();

    auto grab = [window](const QString &path) {
        QCoreApplication::processEvents();
        window->requestUpdate();
        QCoreApplication::processEvents();
        QImage img = window->grabWindow();
        if (!img.isNull()) {
            img.save(path);
            std::cout << "[UI Capture] " << path.toStdString()
                      << " (" << img.width() << "x" << img.height() << ")" << std::endl;
        }
    };

    auto setView = [window](int index) {
        QMetaObject::invokeMethod(window, "setView", Q_ARG(QVariant, index));
        QCoreApplication::processEvents();
    };

    struct Step {
        std::function<void()> action;
        int waitMs;
    };

    auto steps = std::make_shared<std::vector<Step>>();
    auto stepIdx = std::make_shared<size_t>(0);

    // 1. Dashboard: 960x640 Disconnected
    steps->push_back({[&]() {
        window->resize(960, 640);
        setView(0);
    }, 400});

    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_960x640.png");
        grab("build/beaxty_dashboard.png");
        window->resize(840, 560);
    }, 350});

    // 2. Dashboard: 840x560
    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_840x560.png");
        window->resize(1280, 800);
    }, 350});

    // 3. Dashboard: 1280x800
    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_1280x800.png");
        window->resize(1920, 1080);
    }, 350});

    // 4. Dashboard: 1920x1080
    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_1920x1080.png");
        window->resize(960, 640);
        // Test collapsed sidebar
        appPrefsService.setBool(QStringLiteral("sidebar_collapsed"), true);
    }, 450});

    // 4b. Dashboard with Collapsed Sidebar: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_collapsed.png");
        appPrefsService.setBool(QStringLiteral("sidebar_collapsed"), false);
        engine.toggleConnect();
    }, 450});

    // 5. Dashboard Connected: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_connected.png");
        engine.toggleConnect();
        setView(1); // Nodes View
    }, 450});

    // 6. Nodes View: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_960x640.png");
        grab("build/beaxty_nodes.png");
        // Update pings for servers to test reactive latency badges
        int pingVal = 28;
        for (const auto &serverVar : configAdapter.servers()) {
            int sid = serverVar.toMap()["id"].toInt();
            emit trafficMonitor.serverPingUpdated(sid, pingVal);
            pingVal += 16;
        }
    }, 450});

    // 6b. Nodes View with live pings
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_ping.png");
        // Open Import Sheet in Dark Mode
        QMetaObject::invokeMethod(window, "openImportSheet");
    }, 450});

    // 6c. Import Sheet Dark Mode
    steps->push_back({[&]() {
        grab("build/beaxty_import_sheet_dark.png");
        theme.setThemeMode(Theme::Light);
    }, 450});

    // 6d. Import Sheet Light Mode
    steps->push_back({[&]() {
        grab("build/beaxty_import_sheet_light.png");
        locManager.setLanguage(QStringLiteral("en"));
    }, 450});

    // 6e. Import Sheet English
    steps->push_back({[&]() {
        grab("build/beaxty_import_sheet_en.png");
        locManager.setLanguage(QStringLiteral("ru"));
        theme.setThemeMode(Theme::Dark);
        QMetaObject::invokeMethod(window, "closeImportSheet");
    }, 450});

    // 6f. Open custom styled deleteDialog
    steps->push_back({[&]() {
        QQuickItem *nodesView = window->findChild<QQuickItem *>("nodesView");
        if (nodesView) {
            QMetaObject::invokeMethod(nodesView, "openDeleteDialog",
                                      Q_ARG(QVariant, 1),
                                      Q_ARG(QVariant, QStringLiteral("beaxty VPN 🪽")));
        }
    }, 450});

    // 6g. Grab Delete Dialog and close it
    steps->push_back({[&]() {
        grab("build/beaxty_delete_dialog.png");
        QQuickItem *nodesView = window->findChild<QQuickItem *>("nodesView");
        if (nodesView) {
            QMetaObject::invokeMethod(nodesView, "closeDeleteDialog");
        }
    }, 450});

    // 6h. Test Empty State & Promo Card by clearing groups with demo data disabled
    steps->push_back({[&]() {
        configAdapter.setDemoDataEnabled(false);
        while (!configAdapter.groups().isEmpty()) {
            int gid = configAdapter.groups().first().toMap()["id"].toInt();
            configAdapter.deleteGroup(gid);
        }
    }, 450});

    // 6i. Grab Empty State with Promo Card in Russian
    steps->push_back({[&]() {
        grab("build/beaxty_empty_nodes_promo.png");
        locManager.setLanguage(QStringLiteral("en"));
    }, 450});

    // 6j. Grab Empty State with Promo Card in English, then restore
    steps->push_back({[&]() {
        grab("build/beaxty_empty_nodes_promo_en.png");
        locManager.setLanguage(QStringLiteral("ru"));
        configAdapter.setDemoDataEnabled(true);
        configAdapter.reloadServers();
        window->resize(840, 560);
    }, 450});

    // 7. Nodes View: 840x560
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_840x560.png");
        window->resize(1280, 800);
    }, 350});

    // 8. Nodes View: 1280x800
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_1280x800.png");
        window->resize(1920, 1080);
    }, 350});

    // 9. Nodes View: 1920x1080
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_1920x1080.png");
        window->resize(960, 640);
        setView(2); // Routing View
        routingManager.addApp(QStringLiteral("telegram-desktop"));
        routingManager.addApp(QStringLiteral("firefox"));
        routingManager.setActivePreset(RoutingManager::SplitTunneling);
    }, 450});

    // 10. Routing View (Split Tunneling): 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_routing_960x640.png");
        grab("build/beaxty_routing.png");
        window->resize(1280, 800);
    }, 450});

    // 11. Routing View (Split Tunneling): 1280x800
    steps->push_back({[&]() {
        grab("build/beaxty_routing_1280x800.png");
        window->resize(960, 640);
        QQuickItem *routingView = window->findChild<QQuickItem *>("routingView");
        if (routingView) {
            QMetaObject::invokeMethod(routingView, "refreshRunningApps");
        }
        QQuickItem *dialog = window->findChild<QQuickItem *>("appSelectorDialog");
        if (dialog) {
            dialog->setProperty("visible", true);
        }
    }, 450});

    // 12. App Selector Dialog: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_app_selector.png");
        QQuickItem *dialog = window->findChild<QQuickItem *>("appSelectorDialog");
        if (dialog) {
            dialog->setProperty("visible", false);
        }
        routingManager.setActivePreset(RoutingManager::AdvancedRouting);
        routingManager.addAdvancedRule(QStringLiteral("Domain"), QStringLiteral("openai.com"), QStringLiteral("Proxy"));
        routingManager.addAdvancedRule(QStringLiteral("Process"), QStringLiteral("steam"), QStringLiteral("Direct"));
    }, 450});

    // 13. Advanced Routing View: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_advanced_routing.png");
        setView(3); // Settings View
    }, 450});

    // 14. Settings View: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_settings_960x640.png");
        grab("build/beaxty_settings.png");
        window->resize(1280, 800);
    }, 450});

    // 15. Settings View: 1280x800
    steps->push_back({[&]() {
        grab("build/beaxty_settings_1280x800.png");
        window->resize(960, 640);
        // Switch to Light Theme
        theme.setThemeMode(Theme::Light);
        setView(0); // Dashboard in Light Theme
    }, 450});

    // 16. Dashboard Light Theme: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_light_dashboard.png");
        appPrefsService.setBool(QStringLiteral("sidebar_collapsed"), true);
    }, 450});

    // 16b. Dashboard Light Theme Collapsed Sidebar
    steps->push_back({[&]() {
        grab("build/beaxty_light_dashboard_collapsed.png");
        appPrefsService.setBool(QStringLiteral("sidebar_collapsed"), false);
        setView(1); // Nodes View in Light Theme
    }, 450});

    // 16c. Nodes View Light Theme
    steps->push_back({[&]() {
        grab("build/beaxty_light_nodes.png");
        setView(2); // Routing View in Light Theme
    }, 450});

    // 16d. Routing View Light Theme
    steps->push_back({[&]() {
        grab("build/beaxty_light_routing.png");
        setView(3); // Settings View in Light Theme
    }, 450});

    // 17. Settings Light Theme: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_light_settings.png");
        // Test English localization
        locManager.setLanguage(QStringLiteral("en"));
        theme.setThemeMode(Theme::Dark);
        setView(0); // Dashboard in English
    }, 450});

    // 18. Dashboard English: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_english_dashboard.png");
        setView(3); // Settings in English
    }, 450});

    // 19. Settings English: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_english_settings.png");
        locManager.setLanguage(QStringLiteral("ru"));
    }, 350});

    // 20. Finish
    steps->push_back({[&]() {
        std::cout << "[UI Capture] Completed all captures successfully!" << std::endl;
        _Exit(0);
    }, 0});

    // Sequential step driver
    auto executeNext = std::make_shared<std::function<void()>>();
    *executeNext = [steps, stepIdx, executeNext]() {
        if (*stepIdx >= steps->size()) return;
        const Step &cur = (*steps)[*stepIdx];
        (*stepIdx)++;
        cur.action();
        if (cur.waitMs > 0) {
            QTimer::singleShot(cur.waitMs, *executeNext);
        }
    };

    QTimer::singleShot(500, *executeNext);

    return app.exec();
}
