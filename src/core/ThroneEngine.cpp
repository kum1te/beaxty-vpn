// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "ThroneEngine.hpp"
#include "AppPrefs.hpp"
#include "ToastManager.hpp"
#include "ConfigAdapter.hpp"
#include "DeviceIdentity.hpp"
#include "RoutingManager.hpp"
#include "TrafficMonitor.hpp"

#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/global/Utils.hpp"
#include "3rdparty/throne/include/global/Logger.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"
#include "3rdparty/throne/include/configs/generate.h"
#include "3rdparty/throne/include/configs/common/xrayStreamSetting.h"
#include "3rdparty/throne/include/api/RPC.h"
#include "3rdparty/throne/include/stats/traffic/TrafficLooper.hpp"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QUuid>
#include <QThreadPool>
#include <QDebug>
#include <QFileDialog>
#include <QSysInfo>
#include <QProcess>
#include <QRegularExpression>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>
#endif

#if defined(Q_OS_WIN)
#include <QSettings>
static void setWindowsDnsSmartNameResolution(bool disable) {
    QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient"), QSettings::NativeFormat);
    if (disable) {
        reg.setValue(QStringLiteral("DisableSmartNameResolution"), 1);
    } else {
        reg.remove(QStringLiteral("DisableSmartNameResolution"));
    }
}
#endif

ThroneEngine *ThroneEngine::s_instance = nullptr;

ThroneEngine::ThroneEngine(QObject *parent) : QObject(parent) {
    s_instance = this;
}

ThroneEngine::~ThroneEngine() {
    cleanup();
}

ThroneEngine *ThroneEngine::instance() {
    return s_instance;
}

int ThroneEngine::state() const {
    return m_state;
}

void ThroneEngine::setStateForTesting(int state) {
    if (m_state != state) {
        m_state = static_cast<State>(state);
        emit stateChanged(m_state);
    }
}

QString ThroneEngine::stateString() const {
    switch (m_state) {
        case Disconnected: return QStringLiteral("DISCONNECTED");
        case Connecting:   return QStringLiteral("CONNECTING");
        case Protected:    return QStringLiteral("PROTECTED");
    }
    return QStringLiteral("DISCONNECTED");
}

bool ThroneEngine::isConnected() const {
    return m_state == Protected;
}

bool ThroneEngine::isTunModeEnabled() const {
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        // spmode_vpn is runtime-only in Throne's SettingsRepo, so the durable answer
        // lives in remember_tun; spmode_vpn mirrors it for the config generator.
        return Configs::dataManager->settingsRepo->remember_tun;
    }
    return true; // Default ON
}

void ThroneEngine::setTunModeEnabled(bool enabled) {
    auto *settings = (Configs::dataManager) ? Configs::dataManager->settingsRepo.get() : nullptr;
    if (!settings) return;
    if (settings->remember_tun == enabled && settings->spmode_vpn == enabled) return;

    settings->remember_tun = enabled;
    settings->enable_tun_routing = enabled;
    settings->spmode_vpn = enabled;
    persistSettings();

    emit tunModeChanged(enabled);
    qDebug() << "[ThroneEngine] TUN mode set to:" << enabled;

    // A live tunnel was generated for the old mode, so it has to be rebuilt.
    if (m_state == Protected) {
        if (ToastManager::instance()) {
            ToastManager::instance()->showInfo(QStringLiteral("Reconnecting to apply TUN change..."));
        }
        restartConnection();
    }
}

bool ThroneEngine::autoConnect() const {
    return m_autoConnect;
}

void ThroneEngine::setAutoConnect(bool enabled) {
    if (m_autoConnect == enabled) return;
    m_autoConnect = enabled;
    AppPrefs::setBool(QStringLiteral("auto_connect"), enabled);
    emit autoConnectChanged(enabled);
}

bool ThroneEngine::killSwitch() const {
    return m_killSwitch;
}

void ThroneEngine::setKillSwitch(bool enabled) {
    if (m_killSwitch == enabled) return;
    m_killSwitch = enabled;
    AppPrefs::setBool(QStringLiteral("kill_switch"), enabled);
    emit killSwitchChanged(enabled);

    // Turning it off while it is holding traffic down must release immediately.
    if (!enabled && m_killSwitchEngaged) {
        applyKillSwitch(false);
    }
}

bool ThroneEngine::failoverEnabled() const {
    return m_failoverEnabled;
}

void ThroneEngine::setFailoverEnabled(bool enabled) {
    if (m_failoverEnabled == enabled) return;
    m_failoverEnabled = enabled;
    AppPrefs::setBool(QStringLiteral("failover_enabled"), enabled);
    emit failoverEnabledChanged(enabled);
}

void ThroneEngine::persistSettings() {
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        Configs::dataManager->settingsRepo->Save();
    }
}

QString ThroneEngine::statusMessage() const {
    return m_statusMessage;
}

QString ThroneEngine::networkStackLabel() const {
    auto *settings = Configs::dataManager ? Configs::dataManager->settingsRepo.get() : nullptr;
    if (!settings) return QStringLiteral("—");
    if (!settings->remember_tun) {
        return QStringLiteral("SOCKS5 %1:%2")
            .arg(settings->inbound_address)
            .arg(settings->inbound_socks_port);
    }
    QString impl = settings->vpn_implementation;
    if (impl.isEmpty()) impl = QStringLiteral("gvisor");
    return QStringLiteral("TUN / %1").arg(impl);
}

QString ThroneEngine::remoteDnsLabel() const {
    auto *settings = Configs::dataManager ? Configs::dataManager->settingsRepo.get() : nullptr;
    if (!settings || settings->remote_dns.isEmpty()) return QStringLiteral("—");
    return settings->remote_dns;
}

QString ThroneEngine::routingIsolationLabel() const {
    auto *settings = Configs::dataManager ? Configs::dataManager->settingsRepo.get() : nullptr;
    if (!settings) return QStringLiteral("—");
    if (!settings->remember_tun) return QStringLiteral("Proxy only");

    QStringList flags;
    if (settings->vpn_strict_route) flags << QStringLiteral("strict route");
    if (settings->vpn_auto_redirect) flags << QStringLiteral("auto-redirect");
    return flags.isEmpty() ? QStringLiteral("Off") : flags.join(QStringLiteral(", "));
}

QString ThroneEngine::remoteDns() const {
    auto *settings = Configs::dataManager ? Configs::dataManager->settingsRepo.get() : nullptr;
    if (!settings || settings->remote_dns.isEmpty()) return QStringLiteral("https://1.1.1.1/dns-query");
    return settings->remote_dns;
}

void ThroneEngine::setRemoteDns(const QString &dns) {
    auto *settings = Configs::dataManager ? Configs::dataManager->settingsRepo.get() : nullptr;
    if (!settings) return;
    QString trimmed = dns.trimmed();
    if (settings->remote_dns != trimmed) {
        settings->remote_dns = trimmed;
        settings->Save();
        emit remoteDnsChanged(trimmed);
        emit dnsPresetChanged(dnsPreset());
        if (isConnected()) {
            restartConnection();
        }
    }
}

int ThroneEngine::dnsPreset() const {
    QString dns = remoteDns().trimmed().toLower();
    if (dns == QStringLiteral("https://1.1.1.1/dns-query") || dns == QStringLiteral("1.1.1.1") || dns == QStringLiteral("1.0.0.1")) return 0;
    if (dns == QStringLiteral("https://8.8.8.8/dns-query") || dns == QStringLiteral("8.8.8.8") || dns == QStringLiteral("8.8.4.4")) return 1;
    if (dns == QStringLiteral("https://dns.quad9.net/dns-query") || dns == QStringLiteral("9.9.9.9") || dns == QStringLiteral("149.112.112.112")) return 2;
    if (dns == QStringLiteral("https://doh.opendns.com/dns-query") || dns == QStringLiteral("208.67.222.222") || dns == QStringLiteral("208.67.220.220")) return 3;
    return 4; // Custom
}

void ThroneEngine::setDnsPreset(int preset) {
    QString targetDns;
    switch (preset) {
        case 0: targetDns = QStringLiteral("https://1.1.1.1/dns-query"); break;
        case 1: targetDns = QStringLiteral("https://8.8.8.8/dns-query"); break;
        case 2: targetDns = QStringLiteral("https://dns.quad9.net/dns-query"); break;
        case 3: targetDns = QStringLiteral("https://doh.opendns.com/dns-query"); break;
        case 4:
            targetDns = customDns();
            if (targetDns.isEmpty()) targetDns = QStringLiteral("https://1.1.1.1/dns-query");
            break;
        default:
            targetDns = QStringLiteral("https://1.1.1.1/dns-query");
            break;
    }
    setRemoteDns(targetDns);
    emit dnsPresetChanged(preset);
}

QString ThroneEngine::customDns() const {
    return AppPrefs::getString(QStringLiteral("custom_dns"), QStringLiteral("1.1.1.1"));
}

void ThroneEngine::setCustomDns(const QString &dns) {
    QString trimmed = dns.trimmed();
    AppPrefs::setString(QStringLiteral("custom_dns"), trimmed);
    emit customDnsChanged(trimmed);
    if (dnsPreset() == 4) {
        setRemoteDns(trimmed);
    }
}

bool ThroneEngine::closeToTray() const {
    return AppPrefs::getBool(QStringLiteral("close_to_tray"), true);
}

void ThroneEngine::setCloseToTray(bool enabled) {
    if (closeToTray() != enabled) {
        AppPrefs::setBool(QStringLiteral("close_to_tray"), enabled);
        emit closeToTrayChanged(enabled);
    }
}

void ThroneEngine::exportSupportReport() {
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString defaultFilename = QStringLiteral("beaxty_support_report_%1.txt").arg(timestamp);
    QString savePath = QFileDialog::getSaveFileName(
        nullptr,
        tr("Сохранить отчет для поддержки"),
        QDir::homePath() + "/" + defaultFilename,
        tr("Текстовые отчеты (*.txt);;Все файлы (*)")
    );

    if (savePath.isEmpty()) return;

    QString report;
    QTextStream out(&report);

    out << "=====================================================\n";
    out << "           BEAXTY VPN SUPPORT DIAGNOSTIC REPORT      \n";
    out << "=====================================================\n";
    out << "Generated at: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    // 1. System & App info
    out << "[1. SYSTEM & APPLICATION INFORMATION]\n";
    out << "App Version: beaxty VPN v1.0.11 (GPL-3.0)\n";
    out << "Qt Version: " << QT_VERSION_STR << "\n";
    out << "OS Pretty Name: " << QSysInfo::prettyProductName() << "\n";
    out << "Kernel Type/Version: " << QSysInfo::kernelType() << " " << QSysInfo::kernelVersion() << "\n";
    out << "Architecture: " << QSysInfo::currentCpuArchitecture() << "\n";
    out << "Host Name: " << QSysInfo::machineHostName() << "\n\n";

    // 2. Network interfaces & routes (via ip commands)
    out << "[2. NETWORK INTERFACES (ip link)]\n";
    {
        QProcess proc;
        proc.start(QStringLiteral("ip"), {QStringLiteral("link")});
        if (proc.waitForFinished(2000)) {
            out << proc.readAllStandardOutput();
        } else {
            out << "Unable to execute 'ip link'\n";
        }
    }
    out << "\n[3. ROUTING TABLE (ip route)]\n";
    {
        QProcess proc;
        proc.start(QStringLiteral("ip"), {QStringLiteral("route")});
        if (proc.waitForFinished(2000)) {
            out << proc.readAllStandardOutput();
        } else {
            out << "Unable to execute 'ip route'\n";
        }
    }

    // 4. DNS configuration
    out << "\n[4. SYSTEM RESOLV.CONF]\n";
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly | QIODevice::Text)) {
        out << resolv.readAll();
        resolv.close();
    } else {
        out << "Could not read /etc/resolv.conf\n";
    }

    // Optional resolvectl status
    {
        QProcess proc;
        proc.start(QStringLiteral("resolvectl"), {QStringLiteral("status")});
        if (proc.waitForFinished(1500) && proc.exitCode() == 0) {
            out << "\n[RESOLVECTL STATUS]\n" << proc.readAllStandardOutput();
        }
    }

    // 5. App Settings & Tunnel State
    out << "\n[5. BEAXTY VPN STATE & CONFIGURATION]\n";
    out << "Engine State: " << stateString() << " (code " << m_state << ")\n";
    out << "Status Message: " << m_statusMessage << "\n";
    out << "Network Stack: " << networkStackLabel() << "\n";
    out << "Remote DNS: " << remoteDns() << " (Preset: " << dnsPreset() << ")\n";
    out << "Routing Isolation: " << routingIsolationLabel() << "\n";
    out << "Auto-Connect: " << (autoConnect() ? "Enabled" : "Disabled") << "\n";
    out << "Kill-Switch: " << (killSwitch() ? "Enabled" : "Disabled") << "\n";
    out << "Close to Tray: " << (closeToTray() ? "Enabled" : "Disabled") << "\n\n";

    // 6. Redacted Recent Core Logs
    out << "[6. RECENT CORE LOGS (Credentials Redacted)]\n";
    QStringList lines = Logging::RecentLines(150);
    if (lines.isEmpty()) {
        QString logPath = Logging::LogDir() + "/throne.log";
        if (QFile::exists(logPath)) {
            QFile logFile(logPath);
            if (logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream logIn(&logFile);
                while (!logIn.atEnd()) {
                    lines.append(logIn.readLine());
                    if (lines.size() > 150) lines.removeFirst();
                }
            }
        }
    }
    for (QString line : lines) {
        // Redact passwords, private keys, seeds, public keys, tokens, UUIDs, authorization secrets
        line.replace(QRegularExpression(QStringLiteral("password=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("password=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("secret=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("secret=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("private_key=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("private_key=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("key=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("key=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("seed=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("seed=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("pbk=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("pbk=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("sid=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("sid=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("token=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("token=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("auth=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("auth=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("uuid=[^;&\\s]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("uuid=***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("Bearer\\s+[A-Za-z0-9\\-_.]+"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("Bearer ***REDACTED***"));
        line.replace(QRegularExpression(QStringLiteral("(vless|vmess|trojan|ss|ssr)://[^@\\s]+@")), QStringLiteral("\\1://***REDACTED***@"));
        line.replace(QRegularExpression(QStringLiteral("\\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\\b")), QStringLiteral("***REDACTED-UUID***"));
        out << line << "\n";
    }
    if (lines.isEmpty()) {
        out << "No recent logs available.\n";
    }

    out << "\n================== END OF REPORT ==================\n";

    QFile targetFile(savePath);
    if (targetFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream writer(&targetFile);
        writer << report;
        targetFile.close();
        if (ToastManager::instance()) {
            ToastManager::instance()->showSuccess(tr("Диагностический отчет сохранен"));
        }
    } else {
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(tr("Ошибка сохранения отчета"));
        }
    }
}

void ThroneEngine::setState(State s) {
    if (m_state != s) {
        m_state = s;
        emit stateChanged(static_cast<int>(s));
        if (s == Protected) {
            m_statusMessage = QStringLiteral("Protected - Tunnel Active");
        } else if (s == Connecting) {
            m_statusMessage = QStringLiteral("Connecting to Node...");
        } else {
            m_statusMessage = QStringLiteral("Disconnected");
        }
        emit statusMessageChanged(m_statusMessage);
    }
}

bool ThroneEngine::initialize(const QString &dbPath, const QString &coreBinaryPath) {
    if (m_initialized) return true;

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    QString actualDbPath = dbPath;
    if (actualDbPath.isEmpty()) {
        actualDbPath = dataDir + QStringLiteral("/throne.db");
    }

    qDebug() << "[ThroneEngine] Initializing SQLite database at:" << actualDbPath;
    Configs::initDB(actualDbPath.toStdString());

    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        auto *settings = Configs::dataManager->settingsRepo.get();

        // First run only: seed the out-of-the-box defaults. On later launches the
        // user's own choices win, which is why these are not re-applied blindly.
        if (!AppPrefs::getBool(QStringLiteral("defaults_seeded"), false)) {
            settings->remember_tun = true;          // REQUIREMENT: TUN ON by default
            settings->enable_tun_routing = true;
            settings->sub_send_hwid = true;         // REQUIREMENT: HWID ON by default
            settings->vpn_implementation = QStringLiteral("gvisor");
            settings->vpn_strict_route = true;
            settings->vpn_auto_redirect = true;
            settings->remember_enable = true;
            AppPrefs::setBool(QStringLiteral("defaults_seeded"), true);
            AppPrefs::setBool(QStringLiteral("auto_connect"), false);   // default OFF
            AppPrefs::setBool(QStringLiteral("kill_switch"), false);    // default OFF
            settings->Save();
            qDebug() << "[ThroneEngine] Seeded first-run defaults (TUN ON, HWID ON)";
        }

        // spmode_vpn is runtime-only state in Throne; mirror the persisted choice into it.
        settings->spmode_vpn = settings->remember_tun;

        m_autoConnect = AppPrefs::getBool(QStringLiteral("auto_connect"), false);
        m_killSwitch = AppPrefs::getBool(QStringLiteral("kill_switch"), false);
        m_failoverEnabled = AppPrefs::getBool(QStringLiteral("failover_enabled"), true);
    }

    // Initialize API Client
    if (!API::defaultClient) {
        API::defaultClient = new API::Client();
    }

    // Set Utils logging callback
    MW_show_log = [](const QString &msg) {
        qDebug().noquote() << "[ThroneCore]" << msg;
    };

    // Locate core binary
    m_coreBinaryPath = coreBinaryPath;
    if (m_coreBinaryPath.isEmpty()) {
        QString appDir = QCoreApplication::applicationDirPath();
        QString binName = QStringLiteral("/beaxty-core");
#ifdef Q_OS_WIN
        binName += QStringLiteral(".exe");
#endif
        QString candidate1 = appDir + binName;
        QString candidate2 = appDir + QStringLiteral("/../bin") + binName;
        QString candidate3 = QStringLiteral("/usr/lib/beaxty-vpn") + binName;

#ifdef Q_OS_MAC
        QString candidateMac = appDir + QStringLiteral("/../Resources/beaxty-core");
        if (QFile::exists(candidateMac)) m_coreBinaryPath = candidateMac;
        else
#endif
        if (QFile::exists(candidate1)) m_coreBinaryPath = candidate1;
        else if (QFile::exists(candidate2)) m_coreBinaryPath = candidate2;
        else if (QFile::exists(candidate3)) m_coreBinaryPath = candidate3;
    }

    m_initialized = true;

    // Start background core daemon
    if (!m_coreBinaryPath.isEmpty() && QFile::exists(m_coreBinaryPath)) {
        spawnCoreDaemon();
    } else {
        qWarning() << "[ThroneEngine] Core daemon binary not found at:" << m_coreBinaryPath;
    }

    return true;
}

bool ThroneEngine::spawnCoreDaemon() {
    if (m_coreProcess) {
        m_coreProcess->disconnect(this);
        if (m_coreProcess->state() != QProcess::NotRunning) {
            m_coreProcess->terminate();
            if (!m_coreProcess->waitForFinished(1000)) {
                m_coreProcess->kill();
                m_coreProcess->waitForFinished(500);
            }
        }
        delete m_coreProcess;
        m_coreProcess = nullptr;
    }

    if (m_rpcSocket) {
        m_rpcSocket->disconnect(this);
        m_rpcSocket->close();
        delete m_rpcSocket;
        m_rpcSocket = nullptr;
    }

    m_socketPath = QStringLiteral("beaxtyIPC-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (m_localServer) {
        m_localServer->close();
        delete m_localServer;
        m_localServer = nullptr;
    }

    m_localServer = new QLocalServer(this);
    m_localServer->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_localServer->listen(m_socketPath)) {
        qWarning() << "[ThroneEngine] Failed to listen on local socket:" << m_localServer->errorString();
        return false;
    }

    QString fullSocketName = m_localServer->fullServerName();
    m_socketFullPath = fullSocketName;

    connect(m_localServer, &QLocalServer::newConnection, this, [this]() {
        m_rpcSocket = m_localServer->nextPendingConnection();
        qDebug() << "[ThroneEngine] Core daemon connected to IPC server socket!";
        if (API::defaultClient) {
            API::defaultClient->Reconnect(m_rpcSocket);
        }
        if (Configs::dataManager && Configs::dataManager->settingsRepo) {
            Configs::dataManager->settingsRepo->core_running = true;
        }
        if (m_userWantsConnect && m_state == Disconnected) {
            qDebug() << "[ThroneEngine] Core daemon ready, auto-starting requested connection...";
            startConnection();
        }
    });

    m_coreProcess = new QProcess(this);
#if defined(Q_OS_LINUX)
    m_coreProcess->setChildProcessModifier([]() {
        ::prctl(PR_SET_PDEATHSIG, SIGTERM);
    });
#endif
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("THRONE_CORE_SOCKET"), fullSocketName);
    // Privileged dashboard extraction is constrained by the GUI-selected data
    // directory. The daemon must not accept arbitrary filesystem paths over IPC.
    const QString basePath = QDir(Configs::GetBasePath()).absolutePath();
    env.insert(QStringLiteral("THRONE_BASE_PATH"), basePath);
    env.insert(QStringLiteral("THRONE_DASHBOARD_DIR"),
               QDir(basePath).filePath(QStringLiteral("sb-dashboard")));
    env.insert(QStringLiteral("GOTRACEBACK"), QStringLiteral("crash"));
    m_coreProcess->setProcessEnvironment(env);

    connect(m_coreProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray out = m_coreProcess->readAllStandardOutput();
        qDebug().noquote() << "[CoreDaemon STDOUT]" << out.trimmed();
    });

    connect(m_coreProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray err = m_coreProcess->readAllStandardError();
        qDebug().noquote() << "[CoreDaemon STDERR]" << err.trimmed();
    });

    connect(m_coreProcess, &QProcess::finished, this, [this](int exitCode) {
        onCoreExited(exitCode);
    });

    qDebug() << "[ThroneEngine] Launching core daemon:" << m_coreBinaryPath << "with socket:" << fullSocketName;
    m_coreProcess->start(m_coreBinaryPath, {});

    if (!m_coreProcess->waitForStarted(2000)) {
        qWarning() << "[ThroneEngine] Failed to start core daemon process:" << m_coreProcess->errorString();
        return false;
    }

    return true;
}

bool ThroneEngine::connectToCoreRpc() {
    return true;
}

void ThroneEngine::toggleConnect() {
    if (m_state == Protected || m_state == Connecting) {
        stopConnection();
    } else {
        startConnection();
    }
}

void ThroneEngine::startConnection() {
    if (m_state == Protected || m_state == Connecting) return;
    m_userWantsConnect = true;
    doStartConnection();
}

void ThroneEngine::doStartConnection() {
    m_intentionalStop = false;
    uint64_t seq = ++m_connectSeq;
    if (m_failoverAttempts == 0) {
        m_failedServerIds.clear();
    }
    if (m_killSwitchEngaged) {
        applyKillSwitch(false);
    }

    // Throne's config generator reads the runtime flag, not the persisted one.
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        Configs::dataManager->settingsRepo->spmode_vpn =
            Configs::dataManager->settingsRepo->remember_tun;
    }

    setState(Connecting);
    m_statusMessage = QStringLiteral("Connecting...");
    emit statusMessageChanged(m_statusMessage);

    int profileId = ConfigAdapter::instance()->selectedServerId();
    if (profileId < 0) {
        setState(Disconnected);
        QString err = QStringLiteral("No server selected. Please choose a node.");
        if (ToastManager::instance()) ToastManager::instance()->showError(err);
        emit errorOccurred(err);
        return;
    }

    // Run config building and RPC Start asynchronously in background thread
    QThreadPool::globalInstance()->start([this, profileId, seq]() {
        if (m_connectSeq != seq || !m_userWantsConnect || m_intentionalStop) {
            return;
        }

        if (!Configs::dataManager || !Configs::dataManager->profilesRepo) {
            QMetaObject::invokeMethod(this, [this]() {
                setState(Disconnected);
                QString err = QStringLiteral("Database not initialized.");
                if (ToastManager::instance()) ToastManager::instance()->showError(err);
                emit errorOccurred(err);
            });
            return;
        }

        auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
        if (!profile) {
            QMetaObject::invokeMethod(this, [this]() {
                setState(Disconnected);
                QString err = QStringLiteral("Selected server node not found in database.");
                if (ToastManager::instance()) ToastManager::instance()->showError(err);
                emit errorOccurred(err);
            });
            return;
        }

        QString profileName = profile->name;

        // Build sing-box configuration using Throne's Configs::BuildSingBoxConfig
        auto result = Configs::BuildSingBoxConfig(profile);
        if (m_connectSeq != seq || !m_userWantsConnect || m_intentionalStop) {
            return;
        }

        if (!result->error.isEmpty()) {
            qWarning() << "[ThroneEngine] BuildConfig error:" << result->error;
            QMetaObject::invokeMethod(this, [this, err = result->error]() {
                setState(Disconnected);
                QString userErr = QStringLiteral("Config error: %1").arg(err);
                if (ToastManager::instance()) ToastManager::instance()->showError(userErr);
                emit errorOccurred(userErr);
            });
            return;
        }

        libcore::LoadConfigReq req;
        req.core_config = QJsonObject2QString(result->coreConfig, true).toStdString();
        req.tun_ipv4_cidr = result->tunIPv4CIDR.toStdString();
        req.disable_stats = (Configs::dataManager && Configs::dataManager->settingsRepo) ? Configs::dataManager->settingsRepo->disable_traffic_stats : false;
        req.xray_config = QJsonObject2QString(result->xrayConfig, true).toStdString();
        req.need_xray = !result->xrayConfig.isEmpty();
        for (const auto &full : result->xrayFullConfigs) {
            req.xray_full_configs.push_back(full.toStdString());
        }
        if (req.need_xray || !req.xray_full_configs.empty()) {
            req.xray_outbound_dns_strategy = Configs::getXrayOutboundDomainStrategy().toStdString();
            if (profile->AutoSelector() != nullptr) {
                req.xray_lazy_start = true;
                req.xray_idle_seconds = std::max(120, profile->AutoSelector()->intervalSec * 2);
                req.xray_full_idle_seconds = 0;
            }
        }
        if (result->extraCoreData && !result->extraCoreData->path.isEmpty()) {
            req.need_extra_process = true;
            req.extra_process_path = result->extraCoreData->path.toStdString();
            req.extra_process_args = result->extraCoreData->args.toStdString();
            req.extra_process_conf = result->extraCoreData->config.toStdString();
            req.extra_no_out = result->extraCoreData->noLog;
        }

        if (m_connectSeq != seq || !m_userWantsConnect || m_intentionalStop) {
            return;
        }

        bool rpcOK = false;
        QString rpcErr;
        if (API::defaultClient && m_rpcSocket && m_rpcSocket->isOpen()) {
            rpcErr = API::defaultClient->Start(&rpcOK, req);
        } else {
            rpcOK = false;
            rpcErr = QStringLiteral("Core daemon RPC is not connected. Check if beaxty-core is running.");
        }

        if (m_connectSeq != seq || !m_userWantsConnect || m_intentionalStop) {
            if (rpcOK && API::defaultClient && m_rpcSocket && m_rpcSocket->isOpen()) {
                bool stopped = false;
                API::defaultClient->Stop(&stopped);
            }
            return;
        }

        auto chainGroups = result->chainGroups;

        // Post completion to main GUI thread
        QMetaObject::invokeMethod(this, [this, seq, rpcOK, rpcErr, profileName, chainGroups]() {
            if (m_connectSeq != seq || m_state != Connecting || !m_userWantsConnect) {
                // Connection was stopped/cancelled while start was in-flight
                if (rpcOK && rpcErr.isEmpty() && API::defaultClient && m_rpcSocket && m_rpcSocket->isOpen()) {
                    bool stopped = false;
                    API::defaultClient->Stop(&stopped);
                }
                return;
            }

            if (!rpcOK || !rpcErr.isEmpty()) {
                qWarning() << "[ThroneEngine] Core Start failed, rpcOK:" << rpcOK << "err:" << rpcErr;
                setState(Disconnected);

                QString lower = rpcErr.toLower();
                QString userErr = rpcErr.isEmpty() ? QStringLiteral("Failed to start VPN tunnel.") : rpcErr;
                if (lower.contains("operation not permitted") || 
                    lower.contains("permission denied") || 
                    lower.contains("cap_net_admin") || 
                    lower.contains("not authorized")) {
                    userErr = QStringLiteral("Требуются права суперпользователя для настройки TUN/маршрутизации.");
                    requestElevateCapabilities();
                }

                if (ToastManager::instance()) {
                    ToastManager::instance()->showError(userErr);
                }
                emit errorOccurred(userErr);
                return;
            }

            // Initialize traffic groups before switching to Protected or running loop
            if (Stats::trafficLooper) {
                Stats::trafficLooper->SetChainGroups(chainGroups);
                Stats::trafficLooper->stop_requested = false;
                Stats::trafficLooper->loop_enabled = true;
                if (!m_trafficThread || !m_trafficThread->isRunning()) {
                    m_trafficThread = QThread::create([] {
                        Stats::trafficLooper->Loop();
                    });
                    m_trafficThread->setObjectName(QStringLiteral("TrafficLooperThread"));
                    m_trafficThread->start();
                }
            }

            setState(Protected);
#if defined(Q_OS_WIN)
            setWindowsDnsSmartNameResolution(true);
#endif
            m_failoverAttempts = 0;
            m_failedServerIds.clear();
            if (ToastManager::instance()) {
                ToastManager::instance()->showSuccess(QStringLiteral("Connected to %1").arg(profileName));
            }
        });
    });
}

void ThroneEngine::stopTrafficLooper() {
    if (Stats::trafficLooper) {
        Stats::trafficLooper->loop_enabled = false;
        // Loop() polls stop_requested; quit() would be a no-op on a QThread::create
        // thread because it runs no event loop.
        Stats::trafficLooper->stop_requested = true;
    }

    if (m_trafficThread) {
        if (!m_trafficThread->wait(2000)) {
            qWarning() << "[ThroneEngine] TrafficLooper thread did not stop in time";
            // Deleting a running QThread aborts, so hand it to Qt and let it self-collect.
            connect(m_trafficThread, &QThread::finished, m_trafficThread, &QObject::deleteLater);
        } else {
            delete m_trafficThread;
        }
        m_trafficThread = nullptr;
    }
}

void ThroneEngine::stopConnection() {
    if (m_state == Disconnected) return;

    ++m_connectSeq;
    m_intentionalStop = true;
    m_userWantsConnect = false;
    m_failoverAttempts = 0;
    m_failedServerIds.clear();
    stopTrafficLooper();

    setState(Disconnected);
#if defined(Q_OS_WIN)
    setWindowsDnsSmartNameResolution(false);
#endif
    if (TrafficMonitor::instance()) {
        TrafficMonitor::instance()->resetSessionStats();
    }

    // A user-initiated stop must never leave the kill switch holding traffic down.
    if (m_killSwitchEngaged) {
        applyKillSwitch(false);
    }

    QThreadPool::globalInstance()->start([this]() {
        bool rpcOK = false;
        if (API::defaultClient && m_rpcSocket && m_rpcSocket->isOpen()) {
            API::defaultClient->Stop(&rpcOK);
        }
    });

    if (ToastManager::instance()) {
        ToastManager::instance()->showInfo(QStringLiteral("Disconnected"));
    }
}

void ThroneEngine::restartConnection() {
    if (m_state == Disconnected) return;

    ++m_connectSeq;
    m_intentionalStop = true;
    m_userWantsConnect = true;
    setState(Connecting);
    m_statusMessage = QStringLiteral("Connecting...");
    emit statusMessageChanged(m_statusMessage);

    stopTrafficLooper();

    QThreadPool::globalInstance()->start([this]() {
        bool rpcOK = false;
        if (API::defaultClient && m_rpcSocket && m_rpcSocket->isOpen()) {
            API::defaultClient->Stop(&rpcOK);
        }
        // Yield to allow OS kernel to cleanly tear down previous TUN interface
        QThread::msleep(150);

        QMetaObject::invokeMethod(this, [this]() {
            if (m_userWantsConnect && m_state == Connecting) {
                doStartConnection();
            }
        });
    });
}


void ThroneEngine::requestQuit() {
    emit quitRequested();
}

void ThroneEngine::notifyMinimizedToTray() {
    emit minimizedToTray();
}

void ThroneEngine::runPostStartupTasks() {
    if (!m_autoConnect) return;
    if (!ConfigAdapter::instance() || ConfigAdapter::instance()->selectedServerId() < 0) {
        qDebug() << "[ThroneEngine] Auto-connect is on but no node is selected; skipping.";
        return;
    }
    // Give the core daemon a moment to finish attaching to the IPC socket.
    QTimer::singleShot(1200, this, [this]() {
        if (m_state == Disconnected) {
            qDebug() << "[ThroneEngine] Auto-connect on launch";
            startConnection();
        }
    });
}

void ThroneEngine::applyKillSwitch(bool engaged) {
    if (m_killSwitchEngaged == engaged) return;
    m_killSwitchEngaged = engaged;

    if (engaged) {
        m_statusMessage = QStringLiteral("Kill switch: tunnel lost, traffic is blocked");
        emit statusMessageChanged(m_statusMessage);
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(
                QStringLiteral("Соединение с туннелем потеряно. Kill switch: весь трафик заблокирован."));
        }
        emit killSwitchTripped();

        // 1. If core daemon is running and RPC is connected, send a blackhole config
        if (API::defaultClient && m_rpcSocket && m_rpcSocket->isOpen()) {
            QJsonObject blackholeConfig{
                {"inbounds", QJsonArray{
                    QJsonObject{
                        {"type", "tun"},
                        {"tag", "tun-in"},
                        {"interface_name", "beaxty-tun"},
                        {"auto_route", true},
                        {"strict_route", true},
                        {"address", QJsonArray{"172.19.0.1/30"}}
                    }
                }},
                {"outbounds", QJsonArray{
                    QJsonObject{
                        {"type", "block"},
                        {"tag", "block"}
                    }
                }},
                {"route", QJsonObject{
                    {"rules", QJsonArray{
                        QJsonObject{{"action", "reject"}}
                    }},
                    {"final", "block"}
                }}
            };
            libcore::LoadConfigReq req;
            req.core_config = QJsonObject2QString(blackholeConfig, true).toStdString();
            req.tun_ipv4_cidr = "172.19.0.1/30";
            bool ok = false;
            API::defaultClient->Start(&ok, req);
        }

        // 2. On Linux, enforce kernel blackhole default route
#if defined(Q_OS_LINUX)
        QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("add"), QStringLiteral("blackhole"), QStringLiteral("default"), QStringLiteral("metric"), QStringLiteral("1")});
#endif
    } else {
        m_statusMessage = stateString() == QStringLiteral("PROTECTED")
                              ? QStringLiteral("Protected - Tunnel Active")
                              : QStringLiteral("Disconnected");
        emit statusMessageChanged(m_statusMessage);

#if defined(Q_OS_LINUX)
        QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("del"), QStringLiteral("blackhole"), QStringLiteral("default"), QStringLiteral("metric"), QStringLiteral("1")});
#endif
    }
}

void ThroneEngine::requestElevateCapabilities() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString corePath = m_coreBinaryPath;
    if (corePath.isEmpty() || !QFile::exists(corePath)) {
        corePath = Configs::FindCoreRealPath();
    }
    if (corePath.isEmpty() || !QFile::exists(corePath)) {
        QString binName = QStringLiteral("/beaxty-core");
#ifdef Q_OS_WIN
        binName += QStringLiteral(".exe");
#endif
        QString c1 = appDir + binName;
        QString c2 = appDir + QStringLiteral("/../bin") + binName;
        QString c3 = QStringLiteral("/usr/lib/beaxty-vpn") + binName;
        if (QFile::exists(c1)) corePath = c1;
        else if (QFile::exists(c2)) corePath = c2;
        else if (QFile::exists(c3)) corePath = c3;
    }

    QFileInfo coreInfo(corePath);
    QString canonicalCore = coreInfo.canonicalFilePath();
    if (canonicalCore.isEmpty() || !coreInfo.exists() || !coreInfo.isFile()) {
        qWarning() << "[ThroneEngine] Core binary not found for elevation:" << corePath;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Core binary not found for elevation"));
        }
        return;
    }

    QString coreFileName = coreInfo.fileName();
    if (coreFileName != QStringLiteral("beaxty-core") && coreFileName != QStringLiteral("beaxty-core.exe")) {
        qWarning() << "[ThroneEngine] Untrusted core binary name for elevation:" << coreFileName;
        return;
    }

#ifdef Q_OS_LINUX
    // Ownership check: must be owned by root or current user
    if (coreInfo.ownerId() != 0 && coreInfo.ownerId() != ::getuid()) {
        qWarning() << "[ThroneEngine] Core binary is not owned by root or current user:" << canonicalCore;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Небезопасный владелец файла ядра."));
        }
        return;
    }

    qDebug() << "[ThroneEngine] Elevating permissions for core daemon (SUID root):" << canonicalCore;
    QString scriptPath;
    QStringList candidateScripts = {
        appDir + QStringLiteral("/scripts/setup-cap.sh"),
        appDir + QStringLiteral("/../scripts/setup-cap.sh"),
        QStringLiteral("/usr/share/beaxty-vpn/scripts/setup-cap.sh")
    };
    for (const auto &cand : candidateScripts) {
        QFileInfo sInfo(cand);
        if (sInfo.exists() && sInfo.isFile()) {
            if (sInfo.ownerId() == 0 || sInfo.ownerId() == ::getuid()) {
                scriptPath = sInfo.canonicalFilePath();
                break;
            }
        }
    }

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int exitCode, QProcess::ExitStatus exitStatus) {
        proc->deleteLater();
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            qDebug() << "[ThroneEngine] Elevation succeeded. Restarting core daemon...";
            if (ToastManager::instance()) {
                ToastManager::instance()->showSuccess(QStringLiteral("Права успешно повышены."));
            }
            if (m_coreProcess) {
                m_intentionalStop = true;
                m_coreProcess->terminate();
                if (!m_coreProcess->waitForFinished(1500)) {
                    m_coreProcess->kill();
                    m_coreProcess->waitForFinished(500);
                }
            }
            spawnCoreDaemon();
            if (m_userWantsConnect) {
                QTimer::singleShot(600, this, [this]() {
                    if (m_userWantsConnect && m_state == Disconnected && m_rpcSocket && m_rpcSocket->isOpen()) {
                        qDebug() << "[ThroneEngine] Auto-resuming connection after capability elevation...";
                        startConnection();
                    }
                });
            }
        } else {
            qWarning() << "[ThroneEngine] Elevation failed or cancelled, exit code:" << exitCode;
            m_userWantsConnect = false;
            if (ToastManager::instance()) {
                ToastManager::instance()->showError(QStringLiteral("Повышение прав отменено или не удалось."));
            }
        }
    });

    QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (!pkexec.isEmpty()) {
        if (!scriptPath.isEmpty() && QFile::exists(scriptPath)) {
            proc->start(pkexec, {QStringLiteral("/bin/bash"), scriptPath, canonicalCore});
        } else {
            QString cmd = QStringLiteral("chown root:root \"%1\" && chmod 4755 \"%1\"").arg(canonicalCore);
            proc->start(pkexec, {QStringLiteral("sh"), QStringLiteral("-c"), cmd});
        }
    } else {
        if (!scriptPath.isEmpty() && QFile::exists(scriptPath)) {
            proc->start(QStringLiteral("/bin/bash"), {scriptPath, canonicalCore});
        } else {
            proc->start(QStringLiteral("sudo"), {QStringLiteral("sh"), QStringLiteral("-c"),
                        QStringLiteral("chown root:root \"%1\" && chmod 4755 \"%1\"").arg(canonicalCore)});
        }
    }

    if (ToastManager::instance()) {
        ToastManager::instance()->showInfo(QStringLiteral("Запрошено повышение прав TUN (Polkit)..."));
    }
#else
    qDebug() << "[ThroneEngine] Elevated permissions handled natively on this platform.";
#endif
}

void ThroneEngine::onCoreExited(int exitCode) {
    qWarning() << "[ThroneEngine] Core daemon exited with code:" << exitCode;
    bool wasProtected = (m_state == Protected);
    if (m_state != Disconnected) {
        setState(Disconnected);
    }
    stopTrafficLooper();

    // An exit we did not ask for while the tunnel was up is exactly the drop the
    // failover / kill switch exists for.
    if (wasProtected && !m_intentionalStop && !m_cleanedUp) {
        if (m_failoverEnabled && triggerFailover()) {
            return;
        }
        if (m_killSwitch) {
            applyKillSwitch(true);
        }
    }
}

void ThroneEngine::onProfileStopped() {
    bool wasProtected = (m_state == Protected);
    if (m_state != Disconnected) {
        setState(Disconnected);
    }
    stopTrafficLooper();

    if (wasProtected && !m_intentionalStop && !m_cleanedUp) {
        if (m_failoverEnabled && triggerFailover()) {
            return;
        }
        if (m_killSwitch) {
            applyKillSwitch(true);
        }
    }
}

bool ThroneEngine::triggerFailover() {
    if (!m_failoverEnabled) return false;
    if (!ConfigAdapter::instance()) return false;

    auto servers = ConfigAdapter::instance()->servers();
    if (servers.size() <= 1) {
        qWarning() << "[ThroneEngine] Failover impossible: only" << servers.size() << "servers available";
        m_failoverAttempts = 0;
        m_failedServerIds.clear();
        return false;
    }

    int currentId = ConfigAdapter::instance()->selectedServerId();
    m_failedServerIds.insert(currentId);

    if (m_failoverAttempts >= servers.size()) {
        qWarning() << "[ThroneEngine] Failover exhausted: tried" << m_failoverAttempts << "servers";
        m_failoverAttempts = 0;
        m_failedServerIds.clear();
        QString err = QStringLiteral("Не удалось восстановить подключение (все серверы недоступны)");
        if (ToastManager::instance()) ToastManager::instance()->showError(err);
        return false;
    }

    int bestId = -1;
    QString bestName;
    int bestCategory = 999;
    int bestPing = 999999;

    for (const auto &val : servers) {
        auto m = val.toMap();
        int sId = m["id"].toInt();
        if (m_failedServerIds.contains(sId)) {
            continue;
        }

        int sPing = m["ping"].toInt();
        int category = (sPing > 0) ? 1 : (sPing == 0 ? 2 : 3);

        if (category < bestCategory) {
            bestCategory = category;
            bestPing = sPing;
            bestId = sId;
            bestName = m["name"].toString();
        } else if (category == bestCategory) {
            if (category == 1 && sPing < bestPing) {
                bestPing = sPing;
                bestId = sId;
                bestName = m["name"].toString();
            } else if (bestId == -1) {
                bestId = sId;
                bestName = m["name"].toString();
            }
        }
    }

    if (bestId < 0) {
        qWarning() << "[ThroneEngine] Failover: no untried servers remaining";
        m_failoverAttempts = 0;
        m_failedServerIds.clear();
        QString err = QStringLiteral("Не удалось восстановить подключение (все серверы недоступны)");
        if (ToastManager::instance()) ToastManager::instance()->showError(err);
        return false;
    }

    m_failoverAttempts++;
    m_failedServerIds.insert(bestId);

    QString nodeName = bestName.isEmpty() ? QStringLiteral("Server #%1").arg(bestId) : bestName;
    QString msg = QStringLiteral("Связь потеряна. Переключение на «%1»...").arg(nodeName);
    qInfo() << "[ThroneEngine] Failover attempt" << m_failoverAttempts << "switching to" << bestId << nodeName;
    if (ToastManager::instance()) {
        ToastManager::instance()->showInfo(msg);
    }

    ConfigAdapter::instance()->selectServer(bestId);
    QTimer::singleShot(250, this, [this]() {
        startConnection();
    });
    return true;
}

void ThroneEngine::cleanup() {
    if (m_cleanedUp) return;
    m_cleanedUp = true;

    // Remember what to restore next launch before tearing anything down.
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        auto *settings = Configs::dataManager->settingsRepo.get();
        if (settings->remember_enable && settings->started_id >= 0) {
            settings->remember_id = settings->started_id;
        }
        settings->prepare_exit = true;
        settings->Save();
    }

    m_intentionalStop = true;
    stopConnection();
    stopTrafficLooper();

    if (m_coreProcess) {
        m_coreProcess->terminate();
        if (!m_coreProcess->waitForFinished(1000)) {
            m_coreProcess->kill();
            m_coreProcess->waitForFinished(500);
        }
        delete m_coreProcess;
        m_coreProcess = nullptr;
    }

    m_rpcSocket = nullptr;

    if (m_localServer) {
        m_localServer->close();
        delete m_localServer;
        m_localServer = nullptr;
    }

    // QLocalServer::close() unlinks its own socket; this only covers a stale file
    // left behind by an abnormal exit. m_socketPath is the short name, so the full
    // path from fullServerName() is what actually exists on disk.
    if (!m_socketFullPath.isEmpty()) {
        QFile::remove(m_socketFullPath);
        m_socketFullPath.clear();
    }
}
