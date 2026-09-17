// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QDebug>
#include <QCommandLineParser>
#include <QTimer>
#include <QLockFile>

#include "src/core/DeepLinkManager.hpp"
#include "src/core/ThroneEngine.hpp"
#include "src/core/ConfigAdapter.hpp"
#include "src/core/DeviceIdentity.hpp"
#include "src/core/RoutingManager.hpp"
#include "src/core/TrafficMonitor.hpp"
#include "src/core/ToastManager.hpp"
#include "src/core/LocalizationManager.hpp"
#include "src/core/AutostartManager.hpp"
#include "src/core/AppPrefs.hpp"
#include "src/ui/Theme.hpp"
#include "src/bridge/MainWindowBridge.hpp"

#if defined(BEAXTY_HAS_WEBENGINE)
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

#include <QActionGroup>

#include <QQuickWindow>
#include <QLocalSocket>
#include <QLocalServer>

// Abstract-namespace local socket a second launch uses to raise the running window.
static const QString kActivationSocketName = QStringLiteral("beaxty-vpn-activation");

static QIcon createMonochromeTrayIcon(bool connected) {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QIcon appIcon(QStringLiteral(":/icons/app_icon.svg"));
    QPixmap iconPix = appIcon.pixmap(QSize(48, 48));

    if (connected) {
        painter.setOpacity(1.0);
        painter.drawPixmap(8, 8, iconPix);

        // Active indicator badge
        painter.setBrush(QColor(0xFF, 0xFF, 0xFF));
        painter.setPen(QPen(QColor(0x08, 0x08, 0x08), 2));
        painter.drawEllipse(46, 46, 14, 14);
    } else {
        painter.setOpacity(0.55);
        painter.drawPixmap(8, 8, iconPix);
    }

    return QIcon(pixmap);
}

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>

static bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return isAdmin != FALSE;
}

static bool relaunchAsAdmin(int argc, char *argv[]) {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, szPath, MAX_PATH) == 0) {
        return false;
    }

    QStringList args;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.contains(QLatin1Char(' ')) || arg.contains(QLatin1Char('\t'))) {
            arg = QStringLiteral("\"") + arg + QStringLiteral("\"");
        }
        args.append(arg);
    }
    std::wstring params = args.join(QStringLiteral(" ")).toStdWString();

    SHELLEXECUTEINFOW sei = { sizeof(SHELLEXECUTEINFOW) };
    sei.lpVerb = L"runas";
    sei.lpFile = szPath;
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_NORMAL;

    return ShellExecuteExW(&sei) != FALSE;
}
#endif

#if defined(__linux__)
#include <malloc.h>
#endif

#if !defined(_WIN32)
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <QSocketNotifier>

static int sigFd[2];

static void signalHandler(int sig) {
    Q_UNUSED(sig);
    char a = 1;
    ::write(sigFd[0], &a, sizeof(a));
}

static void setupUnixSignalHandlers(QObject *parent) {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd) != 0) {
        return;
    }
    auto *sn = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, parent);
    QObject::connect(sn, &QSocketNotifier::activated, [sn]() {
        sn->setEnabled(false);
        char a;
        ::read(sigFd[1], &a, sizeof(a));
        QCoreApplication::quit();
    });

    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}
#endif

int main(int argc, char *argv[]) {
#if defined(BEAXTY_HAS_WEBENGINE)
#if !defined(_WIN32)
    // В AppImage и контейнерах песочница Chromium без SUID-хелпера падает
    if (qEnvironmentVariableIsSet("APPIMAGE") || qEnvironmentVariableIsSet("container")) {
        QByteArray existingFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        if (!existingFlags.contains("--no-sandbox")) {
            QByteArray newFlags = existingFlags.isEmpty() ? QByteArray("--no-sandbox") : (existingFlags + " --no-sandbox");
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", newFlags);
        }
    }
#endif
    QtWebEngineQuick::initialize();
#endif
#if defined(_WIN32)
    if (!isRunningAsAdmin()) {
        relaunchAsAdmin(argc, argv);
        return 0;
    }
#endif
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QApplication app(argc, argv);
#if !defined(_WIN32)
    setupUnixSignalHandlers(&app);
#endif
    app.setApplicationName(QStringLiteral("beaxty VPN"));
    app.setOrganizationName(QStringLiteral("Beaxty"));
    app.setApplicationVersion(QStringLiteral("1.0.7"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("beaxty VPN Desktop Client"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dbOption(QStringList() << QStringLiteral("d") << QStringLiteral("db"),
                                QStringLiteral("Custom SQLite database path"), QStringLiteral("path"));
    parser.addOption(dbOption);

    QCommandLineOption exitAfterOption(QStringLiteral("exit-after"),
                                       QStringLiteral("Exit after specified milliseconds (for test/benchmark)"),
                                       QStringLiteral("ms"));
    parser.addOption(exitAfterOption);

    QCommandLineOption demoDataOption(QStringLiteral("demo-data"),
                                      QStringLiteral("Seed sample nodes when the database is empty (UI testing)"));
    parser.addOption(demoDataOption);

    parser.addPositionalArgument(QStringLiteral("url"), QStringLiteral("Optional deep link URL (beaxty://...)"));

    parser.process(app);

    // Extract deep link if provided in positional arguments
    QString pendingDeepLink;
    for (const QString &posArg : parser.positionalArguments()) {
        if (posArg.startsWith(QStringLiteral("beaxty://"), Qt::CaseInsensitive)) {
            pendingDeepLink = posArg;
            break;
        }
    }

    // Single-instance protection to prevent conflicts over core daemon and network
    // interface. A second launch pokes the running instance to show its window,
    // which is what a user double-clicking the launcher actually expects.
    QString lockPath = QDir::tempPath() + QStringLiteral("/beaxty-vpn.lock");
    QLockFile lockFile(lockPath);
    if (!lockFile.tryLock(100)) {
        QLocalSocket poke;
        poke.connectToServer(kActivationSocketName);
        if (poke.waitForConnected(500)) {
            if (!pendingDeepLink.isEmpty()) {
                poke.write("DEEPLINK " + pendingDeepLink.toUtf8() + "\n");
            } else {
                poke.write("show\n");
            }
            poke.waitForBytesWritten(300);
            poke.disconnectFromServer();
            qInfo() << "[BeaxtyVPN] Already running; forwarded activation/deeplink to existing window.";
        } else {
            qWarning() << "[BeaxtyVPN] Another instance is already running. Exiting.";
        }
        return 0;
    }

    // Initialize headless MainWindow bridge for Throne backend compatibility
    UI_InitMainWindow();

    // Theme and toasts come first: initialize() below reports failures through
    // ToastManager, and a null instance would silently swallow them.
    Theme theme;
    ToastManager toastManager;

    // Core Managers & Facades
    DeepLinkManager deepLinkManager;
    ThroneEngine engine;
    DeviceIdentity deviceIdentity;
    RoutingManager routingManager;
    ConfigAdapter configAdapter;
    TrafficMonitor trafficMonitor;
    LocalizationManager locManager;
    AutostartManager autostartManager;
    AppPrefsService appPrefsService;

    // Register beaxty:// URL scheme handler in OS if enabled or first run
    DeepLinkManager::registerScheme();
    // Wire headless bridge callbacks to facade managers
    BridgeCallbacks::onProfileStart = [&](int id) {
        if (id >= 0) configAdapter.selectServer(id);
        engine.startConnection();
    };
    BridgeCallbacks::onProfileStop = [&](bool manual) {
        Q_UNUSED(manual);
        engine.stopConnection();
    };
    BridgeCallbacks::onGetProfileToStart = [&]() {
        return configAdapter.selectedServerId();
    };
    BridgeCallbacks::onUpdateTraffic = [&](int pd, int pu, int dd, int du) {
        trafficMonitor.updateTraffic(pd, pu, dd, du);
    };
    BridgeCallbacks::onRefreshProxyList = [&]() {
        // Second tick from TrafficLooper: do not reloadServers() here.
        // Server list is reloaded exclusively on actual profile mutations (import/delete/refresh)
        // to prevent 1 Hz QML delegate reconstruction and micro-stutter.
    };
    BridgeCallbacks::onGetRunningConfigName = [&]() {
        return configAdapter.selectedServerName();
    };
    BridgeCallbacks::onSetTunMode = [&](bool enable) {
        engine.setTunModeEnabled(enable);
    };
    BridgeCallbacks::onCleanup = [&]() {
        engine.cleanup();
    };
    BridgeCallbacks::onShowToast = [&](const QString &title, const QString &message, bool isError) {
        QString fullMsg = title.isEmpty() ? message : (title + QStringLiteral(": ") + message);
        if (ToastManager::instance()) {
            if (isError) {
                ToastManager::instance()->showError(fullMsg);
            } else {
                ToastManager::instance()->showInfo(fullMsg);
            }
        }
    };

    // Initialize database, settings, routes, and core daemon
    QString customDb = parser.value(dbOption);
    engine.initialize(customDb);
    routingManager.initializeRouteProfiles();
    configAdapter.setDemoDataEnabled(parser.isSet(demoDataOption) || ConfigAdapter::demoDataRequestedFromEnv());
    configAdapter.reloadServers();

    if (parser.isSet(exitAfterOption)) {
        int ms = parser.value(exitAfterOption).toInt();
        if (ms <= 0) ms = 100;
        QTimer::singleShot(ms, &app, &QApplication::quit);
    }

    // QML Application Engine
    QQmlApplicationEngine qmlEngine;

    // Theme is exposed only as a context property: QML files reach it via `import ".."`.
    // Registering it as a singleton type as well gave two ways to resolve one name.
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
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("autostartManager"), &autostartManager);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("appPrefs"), &appPrefsService);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("AppPrefs"), &appPrefsService);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("deepLinkManager"), &deepLinkManager);
#if defined(BEAXTY_HAS_WEBENGINE)
    bool webEngineAvailable = false;
    for (const QString &importPath : qmlEngine.importPathList()) {
        if (QDir(importPath + QStringLiteral("/QtWebEngine")).exists()) {
            webEngineAvailable = true;
            break;
        }
    }
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("hasWebEngine"), webEngineAvailable);
#else
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("hasWebEngine"), false);
#endif

    // Load main QML file from resource or local file
    const QUrl url(QStringLiteral("qrc:/qml/App.qml"));
    QObject::connect(&qmlEngine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qCritical() << "Fatal: Failed to load QML interface.";
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    // Always run the UI embedded in this build, independent of the launch directory.
    qmlEngine.load(url);

    // If application was launched cold with a deep link argument, process it once event loop starts
    if (!pendingDeepLink.isEmpty()) {
        QTimer::singleShot(250, [&deepLinkManager, pendingDeepLink]() {
            deepLinkManager.handleDeepLink(pendingDeepLink);
        });
    }

#if defined(__linux__)
    malloc_trim(0);
    QTimer::singleShot(1200, []() {
        malloc_trim(0);
    });
#endif

    // Root window handle, used by the tray and the second-instance activation path.
    QQuickWindow *mainWindow = nullptr;
    if (!qmlEngine.rootObjects().isEmpty()) {
        mainWindow = qobject_cast<QQuickWindow *>(qmlEngine.rootObjects().constFirst());
    }

    auto showMainWindow = [&mainWindow]() {
        if (!mainWindow) return;
        mainWindow->show();
        mainWindow->raise();
        mainWindow->requestActivate();
    };

    // Quitting has to bypass the window's close-to-tray handler.
    auto quitApplication = [&app, &mainWindow]() {
        if (mainWindow) {
            mainWindow->setProperty("quitting", true);
        }
        app.quit();
    };

    // System Tray Integration
    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(createMonochromeTrayIcon(false));
    trayIcon.setToolTip(QStringLiteral("beaxty VPN — Отключено"));

    QMenu trayMenu;

    auto rebuildTrayMenu = [&]() {
        trayMenu.clear();

        // 1. Status Header
        QString stateText;
        int curState = engine.state();
        if (curState == ThroneEngine::Protected) {
            stateText = QObject::tr("Подключено");
        } else if (curState == ThroneEngine::Connecting) {
            stateText = QObject::tr("Подключение...");
        } else {
            stateText = QObject::tr("Отключено");
        }
        QAction *headerAction = trayMenu.addAction(QStringLiteral("beaxty VPN • %1").arg(stateText));
        QFont headerFont = headerAction->font();
        headerFont.setBold(true);
        headerAction->setFont(headerFont);
        headerAction->setEnabled(false);

        // 2. Current node item
        QString nodeName = configAdapter.selectedServerName();
        if (nodeName.isEmpty()) {
            nodeName = QObject::tr("Не выбран");
        }
        QString country = configAdapter.selectedServerCountry();
        int ping = configAdapter.selectedServerPing();
        QString pingStr = (ping > 0) ? QStringLiteral("%1 ms").arg(ping) : QStringLiteral("--");
        QString flagEmoji;
        if (country.length() == 2 && country[0].isLetter() && country[1].isLetter()) {
            char32_t ucs[2] = {
                static_cast<char32_t>(0x1F1E6 + (country[0].toUpper().toLatin1() - 'A')),
                static_cast<char32_t>(0x1F1E6 + (country[1].toUpper().toLatin1() - 'A'))
            };
            flagEmoji = QString::fromUcs4(ucs, 2) + QStringLiteral(" ");
        }
        QAction *nodeAction = trayMenu.addAction(QObject::tr("Узел: %1%2 (%3)").arg(flagEmoji, nodeName, pingStr));
        nodeAction->setEnabled(false);

        trayMenu.addSeparator();

        // 3. Connect / Disconnect Action
        if (curState == ThroneEngine::Protected) {
            QAction *disconnectAction = trayMenu.addAction(QObject::tr("Отключить"));
            QObject::connect(disconnectAction, &QAction::triggered, &engine, &ThroneEngine::toggleConnect);
        } else if (curState == ThroneEngine::Connecting) {
            QAction *cancelAction = trayMenu.addAction(QObject::tr("Отменить подключение"));
            QObject::connect(cancelAction, &QAction::triggered, &engine, &ThroneEngine::toggleConnect);
        } else {
            QAction *connectAction = trayMenu.addAction(QObject::tr("Подключить"));
            QObject::connect(connectAction, &QAction::triggered, &engine, &ThroneEngine::toggleConnect);
        }

        // 4. Quick Server Switcher Submenu
        QMenu *serversSubmenu = trayMenu.addMenu(QObject::tr("Сменить сервер"));
        const auto serverList = configAdapter.servers();
        if (serverList.isEmpty()) {
            QAction *emptyAct = serversSubmenu->addAction(QObject::tr("Нет доступных серверов"));
            emptyAct->setEnabled(false);
        } else {
            int selectedId = configAdapter.selectedServerId();
            for (const auto &serverVar : serverList) {
                QVariantMap sm = serverVar.toMap();
                int id = sm["id"].toInt();
                QString sName = sm["name"].toString();
                QString sCountry = sm["country"].toString();
                int sPing = sm["ping"].toInt();
                QString sPingStr = (sPing > 0) ? QStringLiteral(" (%1 ms)").arg(sPing) : QString();

                QString sFlag;
                if (sCountry.length() == 2 && sCountry[0].isLetter() && sCountry[1].isLetter()) {
                    char32_t ucs[2] = {
                        static_cast<char32_t>(0x1F1E6 + (sCountry[0].toUpper().toLatin1() - 'A')),
                        static_cast<char32_t>(0x1F1E6 + (sCountry[1].toUpper().toLatin1() - 'A'))
                    };
                    sFlag = QString::fromUcs4(ucs, 2) + QStringLiteral(" ");
                }

                QString itemText = QStringLiteral("%1%2%3").arg(sFlag, sName, sPingStr);
                QAction *act = serversSubmenu->addAction(itemText);
                act->setCheckable(true);
                act->setChecked(id == selectedId);

                QObject::connect(act, &QAction::triggered, [&engine, &configAdapter, id]() {
                    bool wasConnected = (engine.state() == ThroneEngine::Protected);
                    configAdapter.selectServer(id);
                    if (wasConnected) {
                        engine.restartConnection();
                    }
                });
            }
        }

        trayMenu.addSeparator();

        // 5. Open main window
        QAction *openAction = trayMenu.addAction(QObject::tr("Открыть окно beaxty VPN"));
        QObject::connect(openAction, &QAction::triggered, &app, showMainWindow);

        // 6. Quit
        QAction *quitAction = trayMenu.addAction(QObject::tr("Выход из beaxty VPN"));
        QObject::connect(quitAction, &QAction::triggered, &app, quitApplication);
    };

    QObject::connect(&trayMenu, &QMenu::aboutToShow, &app, rebuildTrayMenu);
    rebuildTrayMenu();

    QObject::connect(&engine, &ThroneEngine::quitRequested, &app, quitApplication);

    // Left-click the tray icon toggles the window, the usual desktop convention.
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &app,
                     [&mainWindow, showMainWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger) return;
        if (mainWindow && mainWindow->isVisible() && mainWindow->isActive()) {
            mainWindow->hide();
        } else {
            showMainWindow();
        }
    });

    QObject::connect(&engine, &ThroneEngine::minimizedToTray, &app, [&trayIcon]() {
        if (QSystemTrayIcon::supportsMessages()) {
            trayIcon.showMessage(QStringLiteral("beaxty VPN"),
                                 QStringLiteral("Приложение продолжает работать в трее."),
                                 QSystemTrayIcon::Information, 3000);
        }
    });

    QObject::connect(&engine, &ThroneEngine::stateChanged, &app, [&](int state) {
        bool connected = (state == ThroneEngine::Protected);
        trayIcon.setIcon(createMonochromeTrayIcon(connected));
        QString status = connected ? QStringLiteral("Защищено") : (state == ThroneEngine::Connecting ? QStringLiteral("Подключение...") : QStringLiteral("Отключено"));
        trayIcon.setToolTip(QStringLiteral("beaxty VPN — ") + status);
    });

    QObject::connect(&configAdapter, &ConfigAdapter::selectedServerChanged, &app, [&]() {
        QString sName = configAdapter.selectedServerName();
        bool connected = (engine.state() == ThroneEngine::Protected);
        QString status = connected ? QStringLiteral("Защищено") : (engine.state() == ThroneEngine::Connecting ? QStringLiteral("Подключение...") : QStringLiteral("Отключено"));
        if (!sName.isEmpty()) {
            trayIcon.setToolTip(QStringLiteral("beaxty VPN: ") + sName + QStringLiteral(" — ") + status);
        } else {
            trayIcon.setToolTip(QStringLiteral("beaxty VPN — ") + status);
        }
    });

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    // Second-instance activation channel: a later launch connects here instead of
    // dying silently, and we surface the existing window.
    QLocalServer activationServer;
    QLocalServer::removeServer(kActivationSocketName);
    if (activationServer.listen(kActivationSocketName)) {
        QObject::connect(&activationServer, &QLocalServer::newConnection, &app,
                         [&activationServer, showMainWindow, &deepLinkManager]() {
            while (auto *conn = activationServer.nextPendingConnection()) {
                QObject::connect(conn, &QLocalSocket::readyRead, conn, [conn, showMainWindow, &deepLinkManager]() {
                    QByteArray data = conn->readAll();
                    QString msg = QString::fromUtf8(data).trimmed();
                    showMainWindow();
                    if (msg.startsWith(QStringLiteral("DEEPLINK "))) {
                        QString url = msg.mid(9).trimmed();
                        if (!url.isEmpty()) {
                            deepLinkManager.handleDeepLink(url);
                        }
                    }
                });
                QObject::connect(conn, &QLocalSocket::disconnected, conn, &QLocalSocket::deleteLater);
                showMainWindow();
            }
        });
    } else {
        qWarning() << "[BeaxtyVPN] Activation channel unavailable:" << activationServer.errorString();
    }

    // Auto-connect on launch, if the user enabled it.
    engine.runPostStartupTasks();

    int ret = app.exec();
    engine.cleanup();
    return ret;
}
