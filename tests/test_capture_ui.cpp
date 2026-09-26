// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFontDatabase>
#include <QTextLayout>
#include <QKeyEvent>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QImage>
#include <iostream>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>

namespace {

double relativeLuminance(const QColor &color) {
    auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) +
           0.7152 * linear(color.greenF()) +
           0.0722 * linear(color.blueF());
}

double contrastRatio(const QColor &first, const QColor &second) {
    const double a = relativeLuminance(first);
    const double b = relativeLuminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

} // namespace

#include "src/core/ThroneEngine.hpp"
#include "src/core/DeepLinkManager.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/DeviceIdentity.hpp"
#include "src/core/RoutingManager.hpp"
#include "src/core/TrafficMonitor.hpp"
#include "src/core/ToastManager.hpp"
#include "src/core/LocalizationManager.hpp"
#include "src/core/AppPrefs.hpp"
#include "src/ui/Theme.hpp"
#include "src/ui/EmojiFont.hpp"
#include "src/bridge/MainWindowBridge.hpp"

#if defined(BEAXTY_HAS_WEBENGINE)
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

int main(int argc, char *argv[]) {
    // Force offscreen platform for headless environments (CI / Container / Sandbox)
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

#if defined(BEAXTY_HAS_WEBENGINE)
    QtWebEngineQuick::initialize();
#endif

    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QApplication app(argc, argv);
    registerEmojiFallback();

    const QStringList commonFallbacks =
        QFontDatabase::applicationFallbackFontFamilies(QChar::Script_Common);
    if (!commonFallbacks.isEmpty()) {
        const QString emojiFamily = commonFallbacks.last();
        const QString sample = QStringLiteral("373 B/s, 16 B/s, 136 B, 38 ms");
        for (const QString &family : {QStringLiteral("sans-serif"), QStringLiteral("monospace")}) {
            QFont numericFont(family);
            numericFont.setPixelSize(11);
            numericFont.setBold(true);
            QTextLayout numericLayout(sample, numericFont);
            numericLayout.beginLayout();
            QTextLine numericLine = numericLayout.createLine();
            numericLine.setLineWidth(500);
            numericLayout.endLayout();
            for (qsizetype i = 0; i < sample.size(); ++i) {
                if (!sample.at(i).isDigit()) continue;
                for (const auto &run : numericLayout.glyphRuns(i, 1)) {
                    if (run.rawFont().familyName() == emojiFamily) {
                        std::cerr << "Numeric text was rendered with the emoji fallback font for "
                                  << family.toStdString() << std::endl;
                        return 1;
                    }
                }
            }
        }
    }

    UI_InitMainWindow();

    ToastManager toastManager;
    DeepLinkManager deepLinkManager;
    ThroneEngine engine;
    RoutingManager routingManager;
    ConfigAdapter configAdapter;
    TrafficMonitor trafficMonitor;
    LocalizationManager locManager;

    if (engine.stateLabel() != QStringLiteral("ОТКЛЮЧЕНО") ||
        engine.localizedStatusMessage() != QStringLiteral("Готов") ||
        engine.connectionModeLabel() != QStringLiteral("TUN АКТИВЕН")) {
        std::cerr << "Russian connection status labels were not localized" << std::endl;
        return 1;
    }
    locManager.setLanguage(QStringLiteral("en"));
    if (engine.stateLabel() != QStringLiteral("DISCONNECTED") ||
        engine.localizedStatusMessage() != QStringLiteral("Ready") ||
        engine.connectionModeLabel() != QStringLiteral("TUN ACTIVE")) {
        std::cerr << "English connection status labels were not localized" << std::endl;
        return 1;
    }
    locManager.setLanguage(QStringLiteral("ru"));

    QTemporaryDir tempDir(QDir(QDir::tempPath()).filePath(QStringLiteral("beaxty-ui-capture-XXXXXX")));
    if (!tempDir.isValid()) {
        std::cerr << "Failed to create an isolated temporary directory" << std::endl;
        return 1;
    }
    const QString dbPath = tempDir.filePath(QStringLiteral("throne.db"));

    // Use a non-ELF executable fixture so the permission gate can be exercised,
    // while QProcess cannot start a core daemon from the source tree or host.
    const QString fakeCorePath = tempDir.filePath(QStringLiteral("beaxty-core-fixture"));
    QFile fakeCore(fakeCorePath);
    if (!fakeCore.open(QIODevice::WriteOnly) ||
        fakeCore.write("not a core executable\n") < 0) {
        std::cerr << "Failed to create isolated fake core fixture" << std::endl;
        return 1;
    }
    fakeCore.close();
    if (!fakeCore.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
        std::cerr << "Failed to mark isolated fake core fixture executable" << std::endl;
        return 1;
    }
    engine.initialize(dbPath, fakeCorePath);
    if (engine.failoverEnabled()) {
        std::cerr << "Automatic failover must be disabled in a fresh profile" << std::endl;
        return 1;
    }
    AppPrefs::setInt(QStringLiteral("theme_mode"), Theme::Light);
    AppPrefs::setBool(QStringLiteral("sidebar_collapsed"), true);
    Theme theme;
    AppPrefsService appPrefsService;
    if (theme.themeMode() != Theme::Light || !appPrefsService.sidebarCollapsed()) {
        std::cerr << "Saved theme or sidebar preference was not loaded after database initialization" << std::endl;
        return 1;
    }
    theme.setThemeMode(Theme::Dark);
    for (const int mode : {Theme::Dark, Theme::Light}) {
        theme.setThemeMode(mode);
        const QList<QColor> surfaces = {theme.bgDark(), theme.cardBg(), theme.cardHover()};
        for (const QColor &surface : surfaces) {
            if (contrastRatio(theme.textSecondary(), surface) < 4.5 ||
                contrastRatio(theme.textMuted(), surface) < 4.5) {
                std::cerr << "Supporting text contrast fell below 4.5:1 in theme mode " << mode << '\n';
                return 1;
            }
        }
    }
    theme.setThemeMode(Theme::Dark);
    appPrefsService.setSidebarCollapsed(false);
    DeviceIdentity deviceIdentity;
    routingManager.initializeRouteProfiles();

    configAdapter.setAutoUpdateSubsMode(0);
    configAdapter.setDemoDataEnabled(true);
    configAdapter.reloadServers();

    QQmlApplicationEngine qmlEngine;
    QObject::connect(&qmlEngine, &QQmlEngine::warnings, &app, [](const QList<QQmlError> &errors) {
        for (const auto &error : errors) std::cerr << error.toString().toStdString() << '\n';
        std::exit(1);
    });
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
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("deepLinkManager"), &deepLinkManager);

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
    window->setProperty("sidebarCollapsed", false);

    // Synthetic rates make multi-digit typography visible in screenshots; this
    // harness uses an invalid executable fixture and never starts a tunnel or
    // Throne process.
    trafficMonitor.updateTraffic(373, 16, 0, 0);
    QTimer::singleShot(350, &app, [&trafficMonitor]() {
        trafficMonitor.updateTraffic(373, 16, 0, 0);
    });

    if (window->title() != QStringLiteral("beaxty VPN")) {
        std::cerr << "Window title mismatch! Expected 'beaxty VPN', got '" << window->title().toStdString() << "'\n";
        return 1;
    }

    int trafficSamples = 0;
    QTimer geometryMonitor;
    QObject::connect(&geometryMonitor, &QTimer::timeout, &app, [&]() {
        auto sidebar = window->findChild<QQuickItem *>("sidebar");
        if (!sidebar || sidebar->width() < 67.5 || sidebar->width() > 220.5) {
            std::cerr << "Sidebar width outside animation bounds\n";
            std::exit(1);
        }
        QList<QQuickItem *> fills = window->findChildren<QQuickItem *>("trafficUsageFill");
        if (fills.isEmpty() && window->contentItem()) {
            std::function<void(QQuickItem *)> scan = [&](QQuickItem *it) {
                if (!it) return;
                if (it->objectName() == QStringLiteral("trafficUsageFill")) fills.append(it);
                for (auto c : it->childItems()) scan(c);
            };
            scan(window->contentItem());
        }
        for (auto fill : fills) {
            if (!fill->isVisible()) continue;
            ++trafficSamples;
            const auto trackWidth = fill->parentItem()->width();
            const auto ratio = fill->property("fillRatio").toDouble();
            if (fill->width() < 0 || fill->width() > trackWidth + 0.1 ||
                std::abs(fill->width() - trackWidth * ratio) > 0.1) {
                std::cerr << "Traffic fill lagged behind its track or overflowed\n";
                std::exit(1);
            }
        }
    });
    geometryMonitor.start(16);

    const QString captureDir = qEnvironmentVariable("BEAXTY_CAPTURE_DIR", "build-local/ui-captures");
    QDir().mkpath(captureDir);
    auto grab = [window, captureDir](const QString &path) {
        QCoreApplication::processEvents();
        window->requestUpdate();
        QCoreApplication::processEvents();
        QImage img = window->grabWindow();
        if (!img.isNull()) {
            if (!img.save(captureDir + "/" + QFileInfo(path).fileName())) {
                std::cerr << "Failed to save screenshot\n";
                std::exit(1);
            }
            std::cout << "[UI Capture] " << path.toStdString()
                      << " (" << img.width() << "x" << img.height() << ")" << std::endl;
        } else {
            std::cerr << "Screenshot capture returned an empty image\n";
            std::exit(1);
        }
    };

    auto setView = [window](int index) {
        QMetaObject::invokeMethod(window, "setView", Q_ARG(QVariant, index));
        QCoreApplication::processEvents();
    };

    // A connection request without the installed Linux TUN permission must
    // stop at an explanatory consent dialog and remain disconnected.
    setView(0);
    engine.startConnection();
    QCoreApplication::processEvents();
    QObject *tunPermissionDialog = window->findChild<QObject *>("tunPermissionDialog");
    if (!tunPermissionDialog || !tunPermissionDialog->property("visible").toBool() ||
        engine.state() != ThroneEngine::Disconnected) {
        std::cerr << "Missing TUN permission did not show consent while remaining disconnected" << std::endl;
        return 1;
    }
    grab("build/beaxty_tun_permission_dialog.png");
    QMetaObject::invokeMethod(tunPermissionDialog, "close");
    QCoreApplication::processEvents();

    // Check that preference updates travel back into the QML-bound switch state.
    setView(4);
    auto settingsView = window->findChild<QQuickItem *>("settingsView");
    auto externalBrowserToggle = window->findChild<QObject *>("cabinetExternalBrowserToggle");
    auto memorySaverToggle = window->findChild<QObject *>("cabinetMemorySaverToggle");
    if (!settingsView || !externalBrowserToggle || !memorySaverToggle ||
        settingsView->property("cabinetExternalBrowser").toBool() ||
        !memorySaverToggle->property("checked").toBool()) {
        std::cerr << "Cabinet preference controls did not initialize from the stored defaults" << std::endl;
        return 1;
    }
    appPrefsService.setBool(QStringLiteral("cabinet_external_browser"), true);
    appPrefsService.setBool(QStringLiteral("cabinet_memory_saver"), false);
    QCoreApplication::processEvents();
    if (!settingsView->property("cabinetExternalBrowser").toBool() ||
        settingsView->property("cabinetMemorySaver").toBool() ||
        memorySaverToggle->property("checked").toBool()) {
        std::cerr << "Cabinet switches did not react to preference changes" << std::endl;
        return 1;
    }
    appPrefsService.setBool(QStringLiteral("cabinet_external_browser"), false);
    appPrefsService.setBool(QStringLiteral("cabinet_memory_saver"), true);
    QCoreApplication::processEvents();

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
        QMetaObject::invokeMethod(window, "toggleSidebar");
    }, 450});

    // 4b. Dashboard with Collapsed Sidebar: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_collapsed.png");
        QMetaObject::invokeMethod(window, "toggleSidebar");
    }, 450});

    // 4c. Dashboard: 960x640 Connected (Protected state)
    steps->push_back({[&]() {
        engine.setStateForTesting(ThroneEngine::Protected);
    }, 450});

    // 5. Grab Connected Dashboard, reset state, and switch to Nodes View: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_dashboard_connected.png");
        engine.setStateForTesting(ThroneEngine::Disconnected);
        setView(1); // Nodes View
    }, 450});

    // Reverse an in-flight animation and resize while the traffic track moves.
    steps->push_back({[&]() {
        QMetaObject::invokeMethod(window, "toggleSidebar");
    }, 70});
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_sidebar_mid_transition.png");
        auto sidebar = window->findChild<QQuickItem *>("sidebar");
        QList<QQuickItem *> navIcons;
        std::function<void(QQuickItem *)> collectNavIcons = [&](QQuickItem *item) {
            if (!item) return;
            if (item->objectName() == QStringLiteral("sidebarNavIcon")) navIcons.append(item);
            for (auto *child : item->childItems()) collectNavIcons(child);
        };
        collectNavIcons(window->contentItem());
        if (!sidebar || navIcons.size() != 5) {
            std::cerr << "Sidebar navigation icons were not available during the transition\n";
            std::exit(1);
        }
        for (auto *icon : navIcons) {
            const qreal iconX = icon->mapToItem(sidebar, QPointF(0, 0)).x();
            if (iconX < 24.0 || iconX > 27.0) {
                std::cerr << "Sidebar icon drifted during collapse: x=" << iconX << '\n';
                std::exit(1);
            }
        }
        QMetaObject::invokeMethod(window, "toggleSidebar");
        window->resize(840, 560);
    }, 70});
    steps->push_back({[&]() {
        QMetaObject::invokeMethod(window, "toggleSidebar");
    }, 300});
    steps->push_back({[&]() {
        auto sidebar = window->findChild<QQuickItem *>("sidebar");
        if (!sidebar || std::abs(sidebar->width() - 68) > 0.5) std::exit(1);
        grab("build/beaxty_nodes_sidebar_collapsed.png");
        QMetaObject::invokeMethod(window, "toggleSidebar");
        window->resize(960, 640);
        setView(3);
    }, 50});
    steps->push_back({[&]() { setView(2); }, 50});
    steps->push_back({[&]() { setView(1); }, 300});
    steps->push_back({[&]() {
        auto sidebar = window->findChild<QQuickItem *>("sidebar");
        if (!sidebar || std::abs(sidebar->width() - 220) > 0.5 || trafficSamples == 0) {
            std::cerr << "Expanded width: " << (sidebar ? sidebar->width() : -1)
                      << "; traffic samples: " << trafficSamples << '\n';
            std::exit(1);
        }
    }, 50});

    // 6. Nodes View: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_960x640.png");
        grab("build/beaxty_nodes.png");
        // Test Ping animation in Nodes View
        trafficMonitor.setTestingPing(true);
    }, 450});

    // 6-ping. Nodes View with active ping testing animation
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_ping_testing.png");
        trafficMonitor.setTestingPing(false);
        // Update pings for servers to test reactive latency badges
        int pingVal = 28;
        for (const auto &serverVar : configAdapter.servers()) {
            int sid = serverVar.toMap()["id"].toInt();
            emit trafficMonitor.serverPingUpdated(sid, pingVal);
            pingVal += 16;
        }
    }, 450});

    // 6-sort-popup. Nodes View with Open Sort Popup
    steps->push_back({[&]() {
        QObject *sortPopup = window->findChild<QObject *>("sortPopup");
        if (sortPopup) {
            QMetaObject::invokeMethod(sortPopup, "open");
        }
    }, 450});

    // 6-sort-grab. Grab sort popup, then switch sort mode to 1
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_sort_popup.png");
        QObject *sortPopup = window->findChild<QObject *>("sortPopup");
        if (sortPopup) {
            QMetaObject::invokeMethod(sortPopup, "close");
        }
        configAdapter.setServerSortMode(1);
    }, 450});

    // 6-sort-active. Grab Nodes View with active sort mode (accent dot visible)
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_sort_active.png");
        configAdapter.setServerSortMode(0);
    }, 450});

    // 6b. Nodes View with live pings
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_ping.png");
        // Open Import Sheet in Dark Mode
        QMetaObject::invokeMethod(window, "openImportSheet");
    }, 450});

    // 6c. Import Sheet Dark Mode
    steps->push_back({[&]() {
        auto input = window->findChild<QQuickItem *>("inputField");
        if (!input || !input->hasActiveFocus()) {
            std::cerr << "Import input did not receive keyboard focus\n";
            std::exit(1);
        }
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
        configAdapter.setAutoUpdateSubsMode(0);
        configAdapter.setDemoDataEnabled(true);
        configAdapter.reloadServers();
        window->resize(840, 560);
    }, 450});

    // 7. Nodes View: 840x560
    steps->push_back({[&]() {
        grab("build/beaxty_nodes_840x560.png");
        toastManager.showInfo(QStringLiteral("Notification layout check"), 3000);
    }, 1});

    steps->push_back({[&]() {
        auto toast = window->findChild<QQuickItem *>("toastNotification");
        auto viewStack = window->findChild<QQuickItem *>("viewStack");
        if (!toast || !viewStack || toast->property("opacity").toReal() <= 0.01) {
            std::cerr << "Notification layout fixture did not become visible" << std::endl;
            std::exit(1);
        }
        const qreal toastBottom = toast->mapToItem(window->contentItem(), QPointF(0, toast->height())).y();
        const qreal pageTop = viewStack->mapToItem(window->contentItem(), QPointF(0, 0)).y();
        if (pageTop < toastBottom + 8.0) {
            std::cerr << "Notification overlaps the page controls: pageTop=" << pageTop
                      << ", toastBottom=" << toastBottom << std::endl;
            std::exit(1);
        }
        grab("build/beaxty_nodes_notification_layout.png");
        window->resize(1280, 800);
    }, 250});

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
        // Keep UI captures deterministic and offline: do not load the live
        // cabinet service from an automated test.
        appPrefsService.setBool(QStringLiteral("cabinet_external_browser"), true);
        setView(3); // Cabinet View
    }, 450});

    // 13b. Cabinet View (Fallback/Web): 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_cabinet_960x640.png");
        setView(4); // Settings View
    }, 450});

    // 14. Enable failover and verify the two-list pool picker and ordering.
    steps->push_back({[&]() {
        const auto fallbackCandidates = configAdapter.servers();
        int firstFallbackId = -1;
        int secondFallbackId = -1;
        for (qsizetype i = 0; i < std::min<qsizetype>(2, fallbackCandidates.size()); ++i) {
            const int id = fallbackCandidates[i].toMap().value(QStringLiteral("id")).toInt();
            if (i == 0) firstFallbackId = id;
            else secondFallbackId = id;
            engine.setFailoverServer(id, true);
        }

        // The selected-list order is the configured priority for the
        // "Configured order" strategy and must persist when reordered.
        engine.moveFailoverServer(secondFallbackId, -1);
        const QVariantList reorderedIds = engine.failoverServerIds();
        const bool reorderWorked = reorderedIds.size() >= 2 &&
                                   reorderedIds[0].toInt() == secondFallbackId &&
                                   reorderedIds[1].toInt() == firstFallbackId;
        engine.moveFailoverServer(secondFallbackId, 1);
        engine.setFailoverEnabled(true);
        QCoreApplication::processEvents();
        auto failoverRow = window->findChild<QQuickItem *>("failoverToggleRow");
        auto failoverControls = window->findChild<QQuickItem *>("failoverControls");
        auto fallbackLists = window->findChild<QQuickItem *>("fallbackLists");
        auto availableFallbackList = window->findChild<QQuickItem *>("availableFallbackList");
        auto selectedFallbackList = window->findChild<QQuickItem *>("selectedFallbackList");
        bool selectedFallbacksPersisted = fallbackCandidates.size() >= 2;
        for (qsizetype i = 0; selectedFallbacksPersisted && i < 2; ++i) {
            selectedFallbacksPersisted = engine.isFailoverServer(
                fallbackCandidates[i].toMap().value(QStringLiteral("id")).toInt());
        }
        const int availableCount = availableFallbackList ? availableFallbackList->property("count").toInt() : -1;
        const int selectedCount = selectedFallbackList ? selectedFallbackList->property("count").toInt() : -1;

        auto availableSearch = window->findChild<QObject *>("fallbackAvailableSearch");
        if (availableSearch) {
            availableSearch->setProperty("text", QStringLiteral("no-server-can-match-this-query"));
            QCoreApplication::processEvents();
        }
        const int filteredAvailableCount = availableFallbackList ? availableFallbackList->property("count").toInt() : -1;
        if (availableSearch) {
            availableSearch->setProperty("text", QString());
            QCoreApplication::processEvents();
        }

        if (fallbackCandidates.size() < 2 || !engine.failoverEnabled() || !failoverRow ||
            !failoverRow->property("checked").toBool() || !failoverControls || !failoverControls->isVisible() ||
            !fallbackLists || fallbackLists->property("columns").toInt() != 2 ||
            !selectedFallbacksPersisted || !reorderWorked || !availableFallbackList || !selectedFallbackList ||
            availableFallbackList->height() <= 0 || selectedFallbackList->height() <= 0 ||
            selectedCount != 2 || availableCount != fallbackCandidates.size() - 2 ||
            (availableSearch && filteredAvailableCount != 0)) {
            std::cerr << "Failover settings did not show its selected server pool: enabled="
                      << engine.failoverEnabled() << ", row="
                      << (failoverRow ? failoverRow->property("checked").toBool() : false)
                      << ", controls=" << (failoverControls && failoverControls->isVisible())
                      << ", selected/available=" << selectedCount << "/" << availableCount
                      << ", columns=" << (fallbackLists ? fallbackLists->property("columns").toInt() : -1)
                      << ", reorder=" << reorderWorked
                      << ", filtered available=" << filteredAvailableCount
                      << std::endl;
            std::exit(1);
        }
    }, 300});

    // Capture the picker at wide, narrow and desktop widths to verify its
    // responsive two-column / stacked layouts.
    steps->push_back({[&]() {
        grab("build/beaxty_settings_failover.png");
        auto fallbackLists = window->findChild<QQuickItem *>("fallbackLists");
        window->resize(840, 560);
        QCoreApplication::processEvents();
        if (!fallbackLists || fallbackLists->property("columns").toInt() != 1) {
            std::cerr << "Fallback picker did not stack at narrow width" << std::endl;
            std::exit(1);
        }
        grab("build/beaxty_settings_failover_narrow.png");
        window->resize(1280, 800);
        QCoreApplication::processEvents();
        if (fallbackLists->property("columns").toInt() != 2) {
            std::cerr << "Fallback picker did not use two columns at desktop width" << std::endl;
            std::exit(1);
        }
        grab("build/beaxty_settings_failover_wide.png");
        window->resize(960, 640);
        QCoreApplication::processEvents();
        engine.setFailoverEnabled(false);
    }, 350});

    // 14b. Settings View: 960x640 with failover disabled.
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
        QMetaObject::invokeMethod(window, "toggleSidebar");
    }, 450});

    // 16b. Dashboard Light Theme Collapsed Sidebar
    steps->push_back({[&]() {
        grab("build/beaxty_light_dashboard_collapsed.png");
        QMetaObject::invokeMethod(window, "toggleSidebar");
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
        setView(4); // Settings View in Light Theme
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
        setView(4); // Settings in English
    }, 450});

    // 19. Settings English: 960x640
    steps->push_back({[&]() {
        grab("build/beaxty_english_settings.png");
        locManager.setLanguage(QStringLiteral("ru"));
    }, 350});

    // Verify the import sheet after a live resize, long input, and keyboard dismissal.
    steps->push_back({[&]() {
        window->resize(840, 560);
        QMetaObject::invokeMethod(window, "openImportSheet");
    }, 350});
    steps->push_back({[&]() {
        auto input = window->findChild<QQuickItem *>("inputField");
        if (!input) std::exit(1);
        input->setProperty("text", QString(1000, 'x'));
        grab("build/beaxty_import_minimum_long_text.png");
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &escape);
    }, 350});
    steps->push_back({[&]() {
        auto sheet = window->findChild<QQuickItem *>("importSheet");
        if (!sheet || sheet->isVisible()) {
            std::cerr << "Escape did not dismiss import sheet\n";
            std::exit(1);
        }
    }, 50});

    // 20. Finish
    steps->push_back({[&]() {
        std::cout << "[UI Capture] Completed all captures successfully!" << std::endl;
        if (!tempDir.remove()) {
            std::cerr << "Could not remove isolated UI capture data" << std::endl;
            _Exit(1);
        }
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
