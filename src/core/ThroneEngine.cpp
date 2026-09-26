// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "ThroneEngine.hpp"
#include "LocalPeerCredentials.hpp"
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
#include <QJsonArray>
#include <QStringList>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QLocalSocket>
#include <memory>
#include <algorithm>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <cerrno>
#include <signal.h>
#include <QFile>
#include <QFileDevice>
#endif

namespace {

bool testCurrentCoreRouteBlocking(int timeoutMs, int *latencyMs) {
    if (latencyMs) *latencyMs = 0;
    if (!API::defaultClient) return false;

    const QString configuredUrl = Configs::dataManager && Configs::dataManager->settingsRepo
                                      ? Configs::dataManager->settingsRepo->test_latency_url.trimmed()
                                      : QString();
    QStringList urls;
    if (!configuredUrl.isEmpty()) urls.append(configuredUrl);
    for (const QString &url : {QStringLiteral("http://cp.cloudflare.com/generate_204"),
                               QStringLiteral("https://www.google.com/generate_204")}) {
        if (!urls.contains(url, Qt::CaseInsensitive)) urls.append(url);
    }

    for (const QString &url : urls) {
        libcore::TestReq request;
        request.test_current = true;
        // With test_current=true, the core uses the live "proxy" outbound unless
        // use_default_outbound is explicitly set. Keep this false so the probe
        // measures the tunnel path rather than a direct/default route.
        request.use_default_outbound = false;
        request.max_concurrency = 1;
        request.test_timeout_ms = qBound(500, timeoutMs, 5000);
        request.url = url.toStdString();

        bool rpcOk = false;
        const auto response = API::defaultClient->Test(&rpcOk, request);
        if (!rpcOk) return false;

        if (!response.results.empty() && response.results.front().error.value().empty()) {
            if (latencyMs) *latencyMs = qMax(0, response.results.front().latency_ms.value());
            return true;
        }
    }

    return false;
}

} // namespace

#if defined(Q_OS_LINUX)
namespace {

bool hasSetIdBits(const QString &path);

bool rootOwnedNotWritableByOthers(const QFileInfo &info) {
    const QFile::Permissions writableByOthers = QFile::WriteGroup | QFile::WriteOther;
    return info.ownerId() == 0 && !(info.permissions() & writableByOthers);
}

bool trustedRootDirectoryChain(const QString &directory) {
    QString current = QFileInfo(directory).canonicalFilePath();
    if (current.isEmpty()) return false;
    while (true) {
        const QFileInfo info(current);
        if (!info.isDir() || !rootOwnedNotWritableByOthers(info)) return false;
        const QString parent = info.dir().absolutePath();
        if (parent == current) return true;
        current = parent;
    }
}

QString trustedLibraryDirectory() {
    const QFileInfo info(QStringLiteral("/usr/lib"));
    const QString canonical = info.canonicalFilePath();
    if (!info.isDir() || !trustedRootDirectoryChain(canonical)) return {};
    return canonical;
}

QString trustedSystemExecutable(const QStringList &candidates) {
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        const QString canonical = info.canonicalFilePath();
        if (info.isFile() && info.isExecutable() && rootOwnedNotWritableByOthers(info) &&
            !canonical.isEmpty() && trustedRootDirectoryChain(QFileInfo(canonical).absolutePath())) {
            return canonical;
        }
    }
    return {};
}

QByteArray accessAcl(const QString &path) {
    const QString getfaclPath = trustedSystemExecutable({QStringLiteral("/usr/bin/getfacl"),
                                                         QStringLiteral("/bin/getfacl")});
    if (getfaclPath.isEmpty()) return {};

    QProcess process;
    process.start(getfaclPath, {QStringLiteral("-cpn"), QStringLiteral("--"), path});
    if (!process.waitForFinished(1500) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        return {};
    }
    return process.readAllStandardOutput().trimmed();
}

bool hasExactUserSearchAcl(const QString &path, uid_t uid) {
    const QByteArray expected = QByteArrayLiteral("user::rwx\nuser:") + QByteArray::number(uid) +
                                QByteArrayLiteral(":--x\ngroup::---\nmask::--x\nother::---");
    return accessAcl(path) == expected;
}

bool hasOwnerOnlyDirectoryAcl(const QString &path) {
    return accessAcl(path) == QByteArrayLiteral("user::rwx\ngroup::---\nother::---");
}

QString beaxtySystemCoreDirectory(bool allowMissing = false) {
    const QString libDir = trustedLibraryDirectory();
    if (libDir.isEmpty()) return {};

    const QString expected = QDir(libDir).filePath(QStringLiteral("beaxty-vpn"));
    const QFileInfo info(expected);
    if (!info.exists() && !info.isSymLink()) return allowMissing ? expected : QString();
    if (!info.isDir() || info.canonicalFilePath() != expected ||
        !trustedRootDirectoryChain(expected) || hasSetIdBits(expected)) {
        return {};
    }
    return expected;
}

QString userCoreDirectoryPath(bool allowMissing = false) {
    const QString coreRoot = beaxtySystemCoreDirectory(allowMissing);
    if (coreRoot.isEmpty()) return {};

    const QString expected = QDir(coreRoot).filePath(QString::number(::getuid()));
    const QFileInfo info(expected);
    if (!info.exists() && !info.isSymLink()) return allowMissing ? expected : QString();
    const QFile::Permissions writableByOthers = QFile::WriteGroup | QFile::WriteOther;
    const QFile::Permissions accessibleByOthers = QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
                                                 QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
    if (!info.isDir() || info.ownerId() != 0 || (info.permissions() & writableByOthers) ||
        hasSetIdBits(expected) || info.canonicalFilePath() != expected ||
        !trustedRootDirectoryChain(expected) ||
        (!hasOwnerOnlyDirectoryAcl(expected) && !hasExactUserSearchAcl(expected, ::getuid())) ||
        (hasOwnerOnlyDirectoryAcl(expected) && (info.permissions() & accessibleByOthers))) {
        return {};
    }
    return expected;
}

QByteArray sha256File(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(256 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) return {};
        hash.addData(chunk);
    }
    return hash.result().toHex();
}

bool hasSetIdBits(const QString &path) {
    const QByteArray nativePath = QFile::encodeName(path);
    struct stat st {};
    return ::stat(nativePath.constData(), &st) != 0 || (st.st_mode & (S_ISUID | S_ISGID)) != 0;
}

bool hasFileCapabilities(const QString &path) {
    const QByteArray nativePath = QFile::encodeName(path);
    const ssize_t size = ::getxattr(nativePath.constData(), "security.capability", nullptr, 0);
    if (size >= 0) return true;
    // No attribute (or a filesystem without file-capability support) is safe;
    // other lookup errors fail closed because the privilege state is unknown.
    return errno != ENODATA && errno != ENOTSUP;
}

QString matchingInstalledCorePath(const QString &bundledPath, const QByteArray &expectedDigest = {}) {
    const QByteArray digest = expectedDigest.isEmpty() ? sha256File(bundledPath) : expectedDigest;
    if (digest.isEmpty()) return {};

    const QString userDir = userCoreDirectoryPath();
    if (userDir.isEmpty() || !hasExactUserSearchAcl(userDir, ::getuid())) return {};
    const QString candidate = QDir(userDir).filePath(
        QStringLiteral("beaxty-vpn-core-%1").arg(QString::fromLatin1(digest)));
    const QFileInfo info(candidate);
    const QFile::Permissions writableByOthers = QFile::WriteGroup | QFile::WriteOther;
    if (!info.isFile() || !info.isExecutable() || info.ownerId() != 0 ||
        (info.permissions() & writableByOthers) || hasSetIdBits(candidate) ||
        info.canonicalFilePath() != candidate || sha256File(candidate) != digest) {
        return {};
    }
    return candidate;
}

bool hasOnlyNetAdminFileCapability(const QString &path) {
    const QString getcapPath = trustedSystemExecutable({QStringLiteral("/usr/sbin/getcap"),
                                                         QStringLiteral("/sbin/getcap"),
                                                         QStringLiteral("/usr/bin/getcap")});
    if (getcapPath.isEmpty()) return false;

    QProcess process;
    process.start(getcapPath, {path});
    if (!process.waitForFinished(1500) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        return false;
    }
    const QByteArray expected = QFile::encodeName(path) + QByteArrayLiteral(" cap_net_admin=ep");
    return process.readAllStandardOutput().trimmed() == expected;
}

QString installedCorePath(const QString &bundledPath, const QByteArray &expectedDigest = {}) {
    const QString candidate = matchingInstalledCorePath(bundledPath, expectedDigest);
    return !candidate.isEmpty() && hasOnlyNetAdminFileCapability(candidate) ? candidate : QString();
}

} // namespace
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
    m_workerPool.setMaxThreadCount(1);
    m_routeHealthTimer.setInterval(3000);
    connect(&m_routeHealthTimer, &QTimer::timeout, this, &ThroneEngine::probeActiveRoute);
    m_routeConfirmTimer.setSingleShot(true);
    connect(&m_routeConfirmTimer, &QTimer::timeout, this, &ThroneEngine::probeActiveRoute);
    m_failoverRetryTimer.setSingleShot(true);
    connect(&m_failoverRetryTimer, &QTimer::timeout, this, [this]() {
        if (m_failoverEnabled && m_state == Disconnected && !m_failoverInProgress && !m_cleanedUp) {
            triggerFailover();
        }
    });
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

QString ThroneEngine::stateLabel() const {
    switch (m_state) {
        case Disconnected: return tr("ОТКЛЮЧЕНО");
        case Connecting:   return tr("ПОДКЛЮЧЕНИЕ");
        case Protected:    return tr("ЗАЩИЩЕНО");
    }
    return tr("ОТКЛЮЧЕНО");
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
    if (!enabled) m_failoverRetryTimer.stop();
    m_routeFailureDetector.reset();
    m_routeConfirmTimer.stop();
    configureFailoverMonitoring();
}

QVariantList ThroneEngine::failoverServerIds() const {
    QVariantList result;
    result.reserve(m_failoverServerIds.size());
    for (int id : m_failoverServerIds) result.append(id);
    return result;
}

int ThroneEngine::failoverStrategy() const {
    return m_failoverStrategy;
}

void ThroneEngine::setFailoverStrategy(int strategy) {
    const int normalized = qBound(0, strategy, 1);
    if (m_failoverStrategy == normalized) return;
    m_failoverStrategy = normalized;
    AppPrefs::setInt(QStringLiteral("failover_strategy"), normalized);
    emit failoverStrategyChanged(normalized);
}

int ThroneEngine::activeServerId() const {
    return m_activeServerId;
}

bool ThroneEngine::isFailoverServer(int profileId) const {
    return m_failoverServerIds.contains(profileId);
}

void ThroneEngine::setFailoverServer(int profileId, bool enabled) {
    if (profileId < 0 || !Configs::dataManager || !Configs::dataManager->profilesRepo ||
        !Configs::dataManager->profilesRepo->GetProfile(profileId)) {
        return;
    }

    const bool alreadyEnabled = m_failoverServerIds.contains(profileId);
    if (alreadyEnabled == enabled) return;
    if (enabled) {
        m_failoverServerIds.append(profileId);
    } else {
        m_failoverServerIds.removeAll(profileId);
    }
    m_failoverRetryDelayMs = 30000;
    m_routeFailureDetector.reset();
    m_routeConfirmTimer.stop();
    if (m_failoverServerIds.isEmpty()) m_failoverRetryTimer.stop();

    QJsonArray ids;
    for (int id : m_failoverServerIds) ids.append(id);
    AppPrefs::setString(QStringLiteral("failover_server_ids"),
                        QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact)));
    emit failoverServerIdsChanged();
    configureFailoverMonitoring();
}

void ThroneEngine::moveFailoverServer(int profileId, int offset) {
    if (offset != -1 && offset != 1) return;
    const int index = m_failoverServerIds.indexOf(profileId);
    const int target = index + offset;
    if (index < 0 || target < 0 || target >= m_failoverServerIds.size()) return;

    m_failoverServerIds.swapItemsAt(index, target);
    QJsonArray ids;
    for (int id : m_failoverServerIds) ids.append(id);
    AppPrefs::setString(QStringLiteral("failover_server_ids"),
                        QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact)));
    emit failoverServerIdsChanged();
}

bool ThroneEngine::hasEligibleFallbackServers() const {
    if (!m_failoverEnabled || m_failoverServerIds.isEmpty() || !ConfigAdapter::instance()) return false;
    const int currentId = m_activeServerId >= 0 ? m_activeServerId : ConfigAdapter::instance()->selectedServerId();
    for (int id : m_failoverServerIds) {
        if (id != currentId && Configs::dataManager && Configs::dataManager->profilesRepo &&
            Configs::dataManager->profilesRepo->GetProfile(id)) {
            return true;
        }
    }
    return false;
}

void ThroneEngine::configureFailoverMonitoring() {
    if (m_state == Protected && hasEligibleFallbackServers() && !m_failoverInProgress) {
        if (!m_routeHealthTimer.isActive()) {
            m_routeFailureDetector.reset();
            m_routeHealthTimer.start();
        }
    } else {
        m_routeHealthTimer.stop();
        m_routeConfirmTimer.stop();
        if (!m_routeProbeInFlight.load(std::memory_order_acquire)) m_routeFailureDetector.reset();
    }
}

void ThroneEngine::persistSettings() {
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        Configs::dataManager->settingsRepo->Save();
    }
}

QString ThroneEngine::statusMessage() const {
    return m_statusMessage;
}

QString ThroneEngine::localizedStatusMessage() const {
    const QByteArray source = m_statusMessage.toUtf8();
    return tr(source.constData());
}

QString ThroneEngine::connectionModeLabel() const {
    return isTunModeEnabled() ? tr("TUN АКТИВЕН") : tr("ТОЛЬКО ПРОКСИ");
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
    out << "App Version: beaxty VPN v1.1.0 (GPL-3.0)\n";
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

    QSaveFile targetFile(savePath);
    if (targetFile.open(QIODevice::WriteOnly)) {
#if defined(Q_OS_UNIX)
        // Diagnostic reports include host/network details. Write the temporary
        // file privately before committing it atomically at the chosen path.
        if (!targetFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            targetFile.cancelWriting();
            if (ToastManager::instance()) {
                ToastManager::instance()->showError(tr("Ошибка защиты диагностического отчета"));
            }
            return;
        }
#endif
        const QByteArray reportUtf8 = report.toUtf8();
        if (targetFile.write(reportUtf8) == reportUtf8.size() && targetFile.commit()) {
            if (ToastManager::instance()) {
                ToastManager::instance()->showSuccess(tr("Диагностический отчет сохранен"));
            }
        } else {
            targetFile.cancelWriting();
            if (ToastManager::instance()) {
                ToastManager::instance()->showError(tr("Ошибка сохранения отчета"));
            }
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
    configureFailoverMonitoring();
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
        m_failoverEnabled = AppPrefs::getBool(QStringLiteral("failover_enabled"), false);
        m_failoverStrategy = qBound(0, AppPrefs::getInt(QStringLiteral("failover_strategy"), 0), 1);
        m_failoverServerIds.clear();
        const auto storedFailoverIds = QJsonDocument::fromJson(
            AppPrefs::getString(QStringLiteral("failover_server_ids"), QStringLiteral("[]")).toUtf8());
        if (storedFailoverIds.isArray() && Configs::dataManager->profilesRepo) {
            QSet<int> seen;
            for (const auto &value : storedFailoverIds.array()) {
                if (!value.isDouble()) continue;
                const int id = value.toInt(-1);
                if (id >= 0 && !seen.contains(id) && Configs::dataManager->profilesRepo->GetProfile(id)) {
                    seen.insert(id);
                    m_failoverServerIds.append(id);
                }
            }
        }
    }

    // Initialize API Client
    if (!API::defaultClient) {
        API::defaultClient = new API::Client();
    }

    // Set Utils logging callback
    MW_show_log = [](const QString &msg) {
        qDebug().noquote() << "[ThroneCore]" << msg;
    };

    // Locate the bundled core first. Linux's optional privileged copy is kept
    // under a root-only per-UID directory; never run the legacy SUID path.
    m_bundledCoreBinaryPath = coreBinaryPath;
    if (m_bundledCoreBinaryPath.isEmpty()) {
        QString appDir = QCoreApplication::applicationDirPath();
        QString binName = QStringLiteral("/beaxty-core");
#ifdef Q_OS_WIN
        binName += QStringLiteral(".exe");
#endif
        QString candidate1 = appDir + binName;
        QString candidate2 = appDir + QStringLiteral("/../bin") + binName;
#if !defined(Q_OS_LINUX)
        QString candidate3 = QStringLiteral("/usr/lib/beaxty-vpn") + binName;
#endif

#ifdef Q_OS_MAC
        QString candidateMac = appDir + QStringLiteral("/../Resources/beaxty-core");
        if (QFile::exists(candidateMac)) m_bundledCoreBinaryPath = candidateMac;
        else
#endif
        if (QFile::exists(candidate1)) m_bundledCoreBinaryPath = candidate1;
        else if (QFile::exists(candidate2)) m_bundledCoreBinaryPath = candidate2;
#if !defined(Q_OS_LINUX)
        else if (QFile::exists(candidate3)) m_bundledCoreBinaryPath = candidate3;
#endif
    }

    m_coreBinaryPath = m_bundledCoreBinaryPath;
#if defined(Q_OS_LINUX)
    if (!m_bundledCoreBinaryPath.isEmpty() && QFileInfo::exists(m_bundledCoreBinaryPath) &&
        (hasSetIdBits(m_bundledCoreBinaryPath) || hasFileCapabilities(m_bundledCoreBinaryPath))) {
        qCritical() << "[ThroneEngine] Refusing to run a bundled network core with SUID/SGID bits or file capabilities.";
        m_unsafeBundledCoreRejected = true;
        m_bundledCoreBinaryPath.clear();
        m_coreBinaryPath.clear();
    }
    const QString libDir = trustedLibraryDirectory();
    if (!libDir.isEmpty()) {
        const QString legacyCore = QDir(libDir).filePath(QStringLiteral("beaxty-vpn/beaxty-core"));
        if (QFileInfo::exists(legacyCore) && (hasSetIdBits(legacyCore) || hasFileCapabilities(legacyCore))) {
            qCritical() << "[ThroneEngine] Found legacy privileged core at:" << legacyCore;
            m_legacySystemCoreDetected = true;
        }
    }
    const QString installedDigest = AppPrefs::getString(QStringLiteral("linux_core_sha256"));
    if (QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(installedDigest).hasMatch()) {
        const QString userDir = userCoreDirectoryPath();
        const QString expectedPath = userDir.isEmpty() ? QString() : QDir(userDir).filePath(
            QStringLiteral("beaxty-vpn-core-%1").arg(installedDigest));
        if (!expectedPath.isEmpty() && QFileInfo::exists(expectedPath)) {
            const QByteArray currentDigest = sha256File(m_bundledCoreBinaryPath);
            if (currentDigest == installedDigest.toLatin1()) {
                const QString privilegedCore = installedCorePath(m_bundledCoreBinaryPath, currentDigest);
                if (!privilegedCore.isEmpty()) {
                    m_coreBinaryPath = privilegedCore;
                } else {
                    AppPrefs::setString(QStringLiteral("linux_core_sha256"), QString());
                }
            } else {
                AppPrefs::setString(QStringLiteral("linux_core_sha256"), QString());
            }
        } else {
            AppPrefs::setString(QStringLiteral("linux_core_sha256"), QString());
        }
    }
#endif

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
    ++m_rpcGeneration;
    m_rpcConnected.store(false, std::memory_order_release);

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
        QLocalSocket *socket = m_localServer->nextPendingConnection();
        if (!socket) return;
#if defined(Q_OS_LINUX)
        const qint64 expectedCorePid = m_coreProcess && m_coreProcess->state() == QProcess::Running
                                          ? m_coreProcess->processId()
                                          : 0;
        if (!LocalPeerCredentials::isExpectedCorePeer(socket->socketDescriptor(), expectedCorePid)) {
            qWarning() << "[ThroneEngine] Rejected local IPC peer that is not the active core child.";
            socket->abort();
            socket->deleteLater();
            return;
        }
#endif
        const uint64_t generation = ++m_rpcGeneration;
        connect(socket, &QLocalSocket::disconnected, this, [this, generation]() {
            // An old socket may report disconnect after a replacement was
            // accepted. Do not let it clear the newer connection state.
            if (m_rpcGeneration == generation) {
                m_rpcConnected.store(false, std::memory_order_release);
            }
        });
        qDebug() << "[ThroneEngine] Core daemon connected to IPC server socket!";
        if (API::defaultClient) {
            // Reconnect detaches the socket and transfers it to RPC's I/O
            // thread. ThroneEngine must not close/delete that socket itself.
            API::defaultClient->Reconnect(socket);
            m_rpcConnected.store(true, std::memory_order_release);
        } else {
            socket->deleteLater();
            m_rpcConnected.store(false, std::memory_order_release);
            return;
        }
        if (Configs::dataManager && Configs::dataManager->settingsRepo) {
            Configs::dataManager->settingsRepo->core_running = true;
        }
        if (m_userWantsConnect && (m_state == Disconnected || m_state == Connecting)) {
            qDebug() << "[ThroneEngine] Core daemon ready, auto-starting requested connection...";
            doStartConnection();
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

bool ThroneEngine::hasVerifiedTunCore() const {
#if defined(Q_OS_LINUX)
    if (m_bundledCoreBinaryPath.isEmpty() || m_coreBinaryPath.isEmpty()) return false;
    const QByteArray digest = sha256File(m_bundledCoreBinaryPath);
    if (digest.isEmpty()) return false;
    const QString verifiedPath = installedCorePath(m_bundledCoreBinaryPath, digest);
    return !verifiedPath.isEmpty() && QFileInfo(m_coreBinaryPath).canonicalFilePath() == verifiedPath;
#else
    return true;
#endif
}

void ThroneEngine::toggleConnect() {
    if (m_state == Protected || m_state == Connecting) {
        stopConnection();
    } else {
        startConnection();
    }
}

void ThroneEngine::connectAfterTunPermissionConsent() {
    if (m_cleanedUp || m_state == Protected || m_state == Connecting) return;
    if (!isTunModeEnabled()) {
        startConnection();
        return;
    }
#if defined(Q_OS_LINUX)
    if (hasVerifiedTunCore()) {
        startConnection();
        return;
    }

    // The core reconnect callback starts the pending connection after the
    // authorized copy is installed and relaunched.
    m_userWantsConnect = true;
    requestElevateCapabilities();
#else
    startConnection();
#endif
}

void ThroneEngine::startConnection() {
    if (m_cleanedUp) return;
    if (m_state == Protected || m_state == Connecting) return;
    m_failoverRetryTimer.stop();
    if (m_unsafeBundledCoreRejected) {
        if (m_failoverInProgress) {
            finishFailoverFailure(tr("Сетевое ядро отклонило запуск резервного подключения из-за небезопасных прав."));
            return;
        }
        const QString error = QStringLiteral("Обнаружены старые привилегии сетевого ядра. Пересоберите core без SUID и file capabilities.");
        if (ToastManager::instance()) ToastManager::instance()->showError(error);
        emit errorOccurred(error);
        return;
    }
#if defined(Q_OS_LINUX)
    if (isTunModeEnabled() && QFileInfo::exists(m_bundledCoreBinaryPath)) {
        if (m_capabilitySetupInProgress) {
            // The user may press Connect while authorizing from Settings; queue
            // one connection behind the same in-flight setup instead of opening
            // a second consent flow.
            m_userWantsConnect = true;
            return;
        }
        if (!hasVerifiedTunCore()) {
            if (m_failoverInProgress) {
                finishFailoverFailure(tr("Для резервного подключения не подтверждены права TUN. Проверьте их в настройках."));
                return;
            }
            m_userWantsConnect = false;
            emit tunPermissionConsentRequested();
            return;
        }
    }
#endif
    m_userWantsConnect = true;

    // The core can exit independently of the GUI. Recreate its IPC endpoint
    // before trying to send Start; otherwise failover/reconnect only retries
    // against a dead socket and can never recover.
    if (!m_coreProcess || m_coreProcess->state() == QProcess::NotRunning) {
        if (!spawnCoreDaemon()) {
            if (m_failoverInProgress) {
                finishFailoverFailure(tr("Не удалось запустить сетевое ядро для резервного подключения."));
                return;
            }
            m_userWantsConnect = false;
            setState(Disconnected);
            const QString error = QStringLiteral("Core daemon could not be started.");
            if (ToastManager::instance()) ToastManager::instance()->showError(error);
            emit errorOccurred(error);
        }
        return;
    }

    // Initialization may have started the process before it connected to the
    // local socket. Wait for newConnection instead of reporting a false RPC
    // failure; that callback will invoke doStartConnection once the socket is
    // ready.
    if (!m_rpcConnected.load(std::memory_order_acquire)) {
        setState(Connecting);
        m_statusMessage = QStringLiteral("Connecting to core...");
        emit statusMessageChanged(m_statusMessage);
        return;
    }
    doStartConnection();
}

void ThroneEngine::doStartConnection() {
    m_intentionalStop = false;
    uint64_t seq = ++m_connectSeq;
    const bool isFailoverAttempt = m_failoverInProgress;
    // Throne's config generator reads the runtime flag, not the persisted one.
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        Configs::dataManager->settingsRepo->spmode_vpn =
            Configs::dataManager->settingsRepo->remember_tun;
    }

    setState(Connecting);
    m_statusMessage = QStringLiteral("Connecting...");
    emit statusMessageChanged(m_statusMessage);

    int profileId = m_activeServerOverride >= 0
                        ? m_activeServerOverride
                        : (ConfigAdapter::instance() ? ConfigAdapter::instance()->selectedServerId() : -1);
    if (profileId < 0) {
        if (isFailoverAttempt) {
            setState(Disconnected);
            tryNextFailoverCandidate();
            return;
        }
        m_userWantsConnect = false;
        setState(Disconnected);
        QString err = QStringLiteral("No server selected. Please choose a node.");
        if (ToastManager::instance()) ToastManager::instance()->showError(err);
        emit errorOccurred(err);
        return;
    }

    // Run config building and RPC Start asynchronously in background thread
    m_workerPool.start([this, profileId, seq, isFailoverAttempt]() {
        if (m_connectSeq != seq || !m_userWantsConnect || m_intentionalStop) {
            return;
        }

        if (!Configs::dataManager || !Configs::dataManager->profilesRepo) {
            QMetaObject::invokeMethod(this, [this, seq, isFailoverAttempt]() {
                if (m_connectSeq != seq) return;
                if (isFailoverAttempt && m_failoverInProgress) {
                    setState(Disconnected);
                    tryNextFailoverCandidate();
                    return;
                }
                m_userWantsConnect = false;
                setState(Disconnected);
                QString err = QStringLiteral("Database not initialized.");
                if (ToastManager::instance()) ToastManager::instance()->showError(err);
                emit errorOccurred(err);
            });
            return;
        }

        auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
        if (!profile) {
            QMetaObject::invokeMethod(this, [this, seq, isFailoverAttempt]() {
                if (m_connectSeq != seq) return;
                if (isFailoverAttempt && m_failoverInProgress) {
                    setState(Disconnected);
                    tryNextFailoverCandidate();
                    return;
                }
                m_userWantsConnect = false;
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
            if (isFailoverAttempt) {
                qWarning() << "[ThroneEngine] Fallback configuration could not be built";
            } else {
                qWarning() << "[ThroneEngine] BuildConfig error:" << result->error;
            }
            QMetaObject::invokeMethod(this, [this, seq, isFailoverAttempt, err = result->error]() {
                if (m_connectSeq != seq) return;
                if (isFailoverAttempt && m_failoverInProgress) {
                    qWarning() << "[ThroneEngine] Fallback configuration failed:" << err;
                    setState(Disconnected);
                    tryNextFailoverCandidate();
                    return;
                }
                m_userWantsConnect = false;
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
        if (API::defaultClient && m_rpcConnected.load(std::memory_order_acquire)) {
            rpcErr = API::defaultClient->Start(&rpcOK, req);
        } else {
            rpcOK = false;
            rpcErr = QStringLiteral("Core daemon RPC is not connected. Check if beaxty-core is running.");
        }

        int verifiedLatency = 0;
        if (rpcOK && isFailoverAttempt) {
            if (!testCurrentCoreRouteBlocking(1400, &verifiedLatency)) {
                bool stopped = false;
                if (API::defaultClient && m_rpcConnected.load(std::memory_order_acquire)) {
                    API::defaultClient->Stop(&stopped);
                }
                rpcOK = false;
                // Keep remote error text out of logs: a user-defined probe URL
                // can contain private path or query data.
                rpcErr = QStringLiteral("Fallback route verification failed");
            }
        }

        if (m_connectSeq != seq || !m_userWantsConnect || m_intentionalStop) {
            if (rpcOK && API::defaultClient && m_rpcConnected.load(std::memory_order_acquire)) {
                bool stopped = false;
                API::defaultClient->Stop(&stopped);
            }
            return;
        }

        auto chainGroups = result->chainGroups;

        // Post completion to main GUI thread
        QMetaObject::invokeMethod(this, [this, seq, rpcOK, rpcErr, profileName, profileId,
                                         verifiedLatency, isFailoverAttempt, chainGroups]() {
            if (m_connectSeq != seq || m_state != Connecting || !m_userWantsConnect) {
                // A Stop is already queued on the serialized worker. Calling
                // Stop from this delayed GUI callback could race a newer Start.
                return;
            }

            if (!rpcOK || !rpcErr.isEmpty()) {
                if (isFailoverAttempt && m_failoverInProgress) {
                    qWarning() << "[ThroneEngine] Fallback core start failed";
                    setState(Disconnected);
                    tryNextFailoverCandidate();
                    return;
                }
                qWarning() << "[ThroneEngine] Core Start failed, rpcOK:" << rpcOK << "err:" << rpcErr;
                setState(Disconnected);

                QString lower = rpcErr.toLower();
                QString userErr = rpcErr.isEmpty() ? QStringLiteral("Failed to start VPN tunnel.") : rpcErr;
#if defined(Q_OS_LINUX)
                const auto *settings = Configs::dataManager && Configs::dataManager->settingsRepo
                                           ? Configs::dataManager->settingsRepo.get()
                                           : nullptr;
                const bool tunEnabled = settings && settings->remember_tun;
                const bool needsTunCapability = tunEnabled &&
                    (lower.contains("operation not permitted") ||
                     lower.contains("permission denied") ||
                     lower.contains("cap_net_admin") ||
                     lower.contains("/dev/net/tun") ||
                     lower.contains("create tun"));
                if (needsTunCapability) {
                    userErr = QStringLiteral("Для TUN не хватает права CAP_NET_ADMIN. Подтвердите его настройку, чтобы продолжить подключение.");
                    m_userWantsConnect = false;
                    emit tunPermissionConsentRequested();
                } else {
                    m_userWantsConnect = false;
                }
#else
                m_userWantsConnect = false;
#endif

                if (ToastManager::instance()) {
                    ToastManager::instance()->showError(userErr);
                }
                emit errorOccurred(userErr);
                return;
            }

            // Initialize traffic groups before switching to Protected or running loop
            if (Stats::trafficLooper) {
                Stats::trafficLooper->SetChainGroups(chainGroups);
                Stats::trafficLooper->stop_requested.store(false, std::memory_order_release);
                Stats::trafficLooper->loop_enabled.store(true, std::memory_order_release);
                if (!m_trafficThread || !m_trafficThread->isRunning()) {
                    m_trafficThread = QThread::create([] {
                        Stats::trafficLooper->Loop();
                    });
                    m_trafficThread->setObjectName(QStringLiteral("TrafficLooperThread"));
                    m_trafficThread->start();
                }
            }

            m_activeServerId = profileId;
            emit activeServerChanged(m_activeServerId);
            if (isFailoverAttempt && m_failoverInProgress) {
                m_failoverInProgress = false;
                m_failoverCandidateIds.clear();
                m_failoverCandidateIndex = 0;
                m_routeFailureDetector.reset();
                m_failedServerIds.clear();
                m_failoverRetryDelayMs = 30000;
                if (ConfigAdapter::instance() && verifiedLatency > 0) {
                    ConfigAdapter::instance()->updateServerPing(profileId, verifiedLatency);
                }
            }
            setState(Protected);
            if (m_killSwitchEngaged) applyKillSwitch(false);
#if defined(Q_OS_WIN)
            setWindowsDnsSmartNameResolution(true);
#endif
            if (ToastManager::instance()) {
                ToastManager::instance()->showSuccess(
                    isFailoverAttempt ? tr("Автоматически переключено на %1").arg(profileName)
                                      : QStringLiteral("Connected to %1").arg(profileName));
            }
        });
    });
}

void ThroneEngine::stopTrafficLooper() {
    if (Stats::trafficLooper) {
        Stats::trafficLooper->loop_enabled.store(false, std::memory_order_release);
        // Loop() polls stop_requested; quit() would be a no-op on a QThread::create
        // thread because it runs no event loop.
        Stats::trafficLooper->stop_requested.store(true, std::memory_order_release);
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
    m_failoverInProgress = false;
    m_failoverCandidateIds.clear();
    m_failoverCandidateIndex = 0;
    m_activeServerOverride = -1;
    if (m_activeServerId >= 0) {
        m_activeServerId = -1;
        emit activeServerChanged(m_activeServerId);
    }
    m_routeHealthTimer.stop();
    m_routeConfirmTimer.stop();
    m_routeFailureDetector.reset();
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

    m_workerPool.start([this]() {
        bool rpcOK = false;
        if (API::defaultClient && m_rpcConnected.load(std::memory_order_acquire)) {
            API::defaultClient->Stop(&rpcOK);
        }
    });

    if (ToastManager::instance()) {
        ToastManager::instance()->showInfo(QStringLiteral("Disconnected"));
    }
}

void ThroneEngine::restartConnection() {
    if (m_cleanedUp) return;
    if (m_state == Disconnected) return;

    ++m_connectSeq;
    m_intentionalStop = true;
    m_userWantsConnect = true;
    setState(Connecting);
    m_statusMessage = QStringLiteral("Connecting...");
    emit statusMessageChanged(m_statusMessage);

    stopTrafficLooper();

    m_workerPool.start([this]() {
        bool rpcOK = false;
        if (API::defaultClient && m_rpcConnected.load(std::memory_order_acquire)) {
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
    if (m_legacySystemCoreDetected && ToastManager::instance()) {
        ToastManager::instance()->showError(
            QStringLiteral("Обнаружена старая привилегированная копия core в /usr/lib. Удалите у неё SUID и file capabilities."));
    }
    if (m_unsafeBundledCoreRejected) {
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(
                QStringLiteral("Обнаружены старые привилегии сетевого ядра. Пересоберите core без SUID и file capabilities."));
        }
        return;
    }
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
        bool coreAcceptedBlockConfig = false;
        // This can request an app-level block only while the user-owned core is
        // still alive. It does not establish or verify an OS firewall rule.
        if (m_coreProcess && m_coreProcess->state() != QProcess::NotRunning &&
            API::defaultClient && m_rpcConnected.load(std::memory_order_acquire)) {
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
            const QString error = API::defaultClient->Start(&ok, req);
            coreAcceptedBlockConfig = ok && error.isEmpty();
        }

        m_statusMessage = coreAcceptedBlockConfig
                              ? QStringLiteral("Core block requested; system firewall is unverified")
                              : QStringLiteral("VPN tunnel lost; system traffic may be unprotected");
        emit statusMessageChanged(m_statusMessage);
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(
                coreAcceptedBlockConfig
                    ? QStringLiteral("Туннель потерян. Ядро приняло запрос блокировки, но системная блокировка трафика не проверена.")
                    : QStringLiteral("Туннель потерян. Системный трафик может идти через обычную сеть.") );
        }
        emit killSwitchTripped();
    } else {
        m_statusMessage = stateString() == QStringLiteral("PROTECTED")
                              ? QStringLiteral("Protected - Tunnel Active")
                              : QStringLiteral("Disconnected");
        emit statusMessageChanged(m_statusMessage);
    }
}

void ThroneEngine::requestElevateCapabilities() {
#ifdef Q_OS_LINUX
    if (m_cleanedUp || m_capabilitySetupInProgress) return;
    if (m_state == Protected || m_state == Connecting) {
        if (ToastManager::instance()) {
            ToastManager::instance()->showInfo(QStringLiteral("Сначала отключитесь, чтобы изменить права сетевого ядра."));
        }
        return;
    }
    if (hasVerifiedTunCore()) {
        if (ToastManager::instance()) {
            ToastManager::instance()->showInfo(QStringLiteral("Права TUN уже настроены для этого приложения."));
        }
        return;
    }

    QString sourcePath = m_bundledCoreBinaryPath;
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) sourcePath = m_coreBinaryPath;
    const QFileInfo sourceInfo(sourcePath);
    const QString canonicalSource = sourceInfo.canonicalFilePath();
    const QFile::Permissions writableByOthers = QFile::WriteGroup | QFile::WriteOther;
    if (canonicalSource.isEmpty() || !sourceInfo.isFile() || !sourceInfo.isExecutable() ||
        (sourceInfo.ownerId() != 0 && sourceInfo.ownerId() != ::getuid()) ||
        (sourceInfo.permissions() & writableByOthers) || hasSetIdBits(canonicalSource) ||
        hasFileCapabilities(canonicalSource)) {
        qWarning() << "[ThroneEngine] Refusing capability setup for an unsafe core file.";
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Не удалось проверить файл сетевого ядра."));
        }
        return;
    }

    const QByteArray digest = sha256File(canonicalSource);
    if (digest.isEmpty()) {
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Не удалось проверить файл сетевого ядра."));
        }
        return;
    }

    const QString coreRoot = beaxtySystemCoreDirectory(true);
    if (coreRoot.isEmpty()) {
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Системный каталог прав сетевого ядра небезопасен или недоступен."));
        }
        return;
    }
    const QString userDir = userCoreDirectoryPath(true);
    if (userDir.isEmpty()) {
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Каталог прав сетевого ядра имеет небезопасные права доступа."));
        }
        return;
    }
    const QString targetPath = QDir(userDir).filePath(
        QStringLiteral("beaxty-vpn-core-%1").arg(QString::fromLatin1(digest)));
    const QString pkexecPath = trustedSystemExecutable({QStringLiteral("/usr/bin/pkexec"),
                                                         QStringLiteral("/bin/pkexec")});
    const QString helperPath = QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("beaxty-vpn-privileged-helper"));
    const QFileInfo helperInfo(helperPath);
    const QFile::Permissions helperWritableByOthers = QFile::WriteGroup | QFile::WriteOther;
    if (pkexecPath.isEmpty() || !helperInfo.isFile() || !helperInfo.isExecutable() ||
        helperInfo.canonicalFilePath() != helperPath ||
        (helperInfo.ownerId() != 0 && helperInfo.ownerId() != ::getuid()) ||
        (helperInfo.permissions() & helperWritableByOthers) || hasSetIdBits(helperPath) ||
        hasFileCapabilities(helperPath)) {
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(
                QStringLiteral("Не удалось проверить системный помощник для настройки TUN."));
        }
        return;
    }

    const QFileInfo coreRootInfo(coreRoot);
    if ((coreRootInfo.exists() || coreRootInfo.isSymLink()) && beaxtySystemCoreDirectory().isEmpty()) {
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Системный каталог прав сетевого ядра имеет небезопасные права доступа."));
        }
        return;
    }
    const QFileInfo userDirInfo(userDir);
    const bool userDirExists = userDirInfo.exists() || userDirInfo.isSymLink();
    if (userDirExists && userCoreDirectoryPath().isEmpty()) {
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Каталог прав сетевого ядра имеет небезопасные права доступа."));
        }
        return;
    }
    const QFileInfo targetInfo(targetPath);
    const bool targetExists = targetInfo.exists() || targetInfo.isSymLink();
    if (targetExists && matchingInstalledCorePath(canonicalSource, digest) != targetPath) {
        m_userWantsConnect = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("В системном каталоге уже есть непроверенная копия сетевого ядра."));
        }
        return;
    }

    auto completed = std::make_shared<bool>(false);
    m_capabilitySetupInProgress = true;
    QProcess *proc = new QProcess(this);
    m_capabilitySetupProcess = proc;
    auto fail = [this, proc, completed](const QString &message) {
        if (*completed) return;
        *completed = true;
        m_capabilitySetupInProgress = false;
        if (m_capabilitySetupProcess == proc) m_capabilitySetupProcess = nullptr;
        proc->deleteLater();
        m_userWantsConnect = false;
        qWarning() << "[ThroneEngine] TUN capability setup failed.";
        if (ToastManager::instance()) ToastManager::instance()->showError(message);
    };

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc, completed, fail, canonicalSource, targetPath, digest]
            (int exitCode, QProcess::ExitStatus exitStatus) {
        if (*completed) return;
        if (m_cleanedUp) {
            *completed = true;
            m_capabilitySetupInProgress = false;
            if (m_capabilitySetupProcess == proc) m_capabilitySetupProcess = nullptr;
            proc->deleteLater();
            return;
        }
        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            fail(QStringLiteral("Установка права CAP_NET_ADMIN отменена или завершилась ошибкой. SUID-root не применялся."));
            return;
        }
        if (installedCorePath(canonicalSource, digest) != targetPath) {
            fail(QStringLiteral("Не удалось проверить установленную копию сетевого ядра."));
            return;
        }

        *completed = true;
        m_capabilitySetupInProgress = false;
        if (m_capabilitySetupProcess == proc) m_capabilitySetupProcess = nullptr;
        proc->deleteLater();
        AppPrefs::setString(QStringLiteral("linux_core_sha256"), QString::fromLatin1(digest));
        m_coreBinaryPath = targetPath;
        if (m_coreProcess) {
            m_intentionalStop = true;
            m_coreProcess->terminate();
            if (!m_coreProcess->waitForFinished(1500)) {
                m_coreProcess->kill();
                m_coreProcess->waitForFinished(500);
            }
        }
        if (!spawnCoreDaemon()) {
            m_intentionalStop = false;
            m_userWantsConnect = false;
            if (ToastManager::instance()) {
                ToastManager::instance()->showError(QStringLiteral("Право TUN установлено, но ядро не удалось перезапустить."));
            }
            return;
        }
        m_intentionalStop = false;
        if (ToastManager::instance()) {
            ToastManager::instance()->showSuccess(QStringLiteral("Сетевое ядро установлено с правом CAP_NET_ADMIN без SUID-root."));
        }
    });
    connect(proc, &QProcess::errorOccurred, this,
            [fail](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            fail(QStringLiteral("Не удалось запустить системный помощник Polkit."));
        }
    });

    if (ToastManager::instance()) {
        ToastManager::instance()->showInfo(
            QStringLiteral("Будет установлена проверяемая копия ядра с CAP_NET_ADMIN, доступная только этому пользователю."));
    }
    proc->start(pkexecPath, {helperPath, QStringLiteral("--install-core"), canonicalSource,
                             QString::fromLatin1(digest)});
#else
    if (ToastManager::instance()) {
        ToastManager::instance()->showInfo(QStringLiteral("Разрешения TUN на этой платформе обрабатываются системой."));
    }
#endif
}

void ThroneEngine::onCoreExited(int exitCode) {
    qWarning() << "[ThroneEngine] Core daemon exited with code:" << exitCode;
    bool wasProtected = (m_state == Protected);
    const bool wasConnecting = (m_state == Connecting);
    const bool connectionPending = m_userWantsConnect;
    if (m_state != Disconnected) {
        setState(Disconnected);
    }
    stopTrafficLooper();

    // An exit we did not ask for while the tunnel was up is exactly the drop the
    // failover / kill switch exists for.
    if (m_intentionalStop || m_cleanedUp) return;

    if (wasConnecting && connectionPending && m_failoverInProgress) {
        ++m_connectSeq;
        m_intentionalStop = false;
        setState(Disconnected);
        tryNextFailoverCandidate();
        return;
    }

    if (wasProtected) {
        if (m_killSwitch) applyKillSwitch(true);
        if (m_failoverEnabled && triggerFailover()) return;
        m_userWantsConnect = false;
        m_activeServerOverride = -1;
        if (m_activeServerId >= 0) {
            m_activeServerId = -1;
            emit activeServerChanged(m_activeServerId);
        }
        if (!m_killSwitch) {
            const QString error = QStringLiteral("VPN core stopped unexpectedly. The system may now use its normal network connection.");
            if (ToastManager::instance()) ToastManager::instance()->showError(error);
            emit errorOccurred(error);
        }
    } else if (wasConnecting && connectionPending) {
        ++m_connectSeq;
        m_userWantsConnect = false;
        m_activeServerOverride = -1;
        const QString error = QStringLiteral("VPN core stopped while connecting. Check TUN permissions and try again.");
        if (ToastManager::instance()) ToastManager::instance()->showError(error);
        emit errorOccurred(error);
    }
}

void ThroneEngine::onProfileStopped() {
    bool wasProtected = (m_state == Protected);
    const bool wasConnecting = (m_state == Connecting);
    if (m_state != Disconnected) {
        setState(Disconnected);
    }
    stopTrafficLooper();

    if (wasConnecting && m_failoverInProgress && !m_intentionalStop && !m_cleanedUp) {
        ++m_connectSeq;
        tryNextFailoverCandidate();
        return;
    }

    if (wasProtected && !m_intentionalStop && !m_cleanedUp) {
        if (m_killSwitch) applyKillSwitch(true);
        if (m_failoverEnabled && triggerFailover()) {
            return;
        }
        m_userWantsConnect = false;
        m_activeServerOverride = -1;
        if (m_activeServerId >= 0) {
            m_activeServerId = -1;
            emit activeServerChanged(m_activeServerId);
        }
    }
}

bool ThroneEngine::triggerFailover() {
    if (!m_failoverEnabled || m_failoverInProgress || !ConfigAdapter::instance()) return false;

    const int currentId = m_activeServerId >= 0
                              ? m_activeServerId
                              : ConfigAdapter::instance()->selectedServerId();
    QSet<int> available;
    QMap<int, int> latencies;
    for (const auto &value : ConfigAdapter::instance()->servers()) {
        const auto server = value.toMap();
        const int id = server.value(QStringLiteral("id")).toInt();
        if (id < 0) continue;
        available.insert(id);
        latencies.insert(id, server.value(QStringLiteral("ping")).toInt());
    }

    QSet<int> excluded;
    if (currentId >= 0) excluded.insert(currentId);
    const auto strategy = m_failoverStrategy == 1 ? FailoverPolicy::Strategy::ConfiguredOrder
                                                   : FailoverPolicy::Strategy::LowestLatency;
    m_failoverCandidateIds = FailoverPolicy::orderCandidates(
        m_failoverServerIds, available, latencies, excluded, strategy);
    if (m_failoverCandidateIds.isEmpty()) {
        qWarning() << "[ThroneEngine] Failover has no eligible nodes in the selected pool";
        return false;
    }

    m_failedServerIds.clear();
    if (currentId >= 0) m_failedServerIds.insert(currentId);
    m_failoverCandidateIndex = 0;
    m_failoverInProgress = true;
    m_userWantsConnect = true;
    m_routeFailureDetector.reset();

    const QString message = tr("Связь потеряна. Проверяем резервные серверы...");
    qInfo() << "[ThroneEngine] Confirmed route failure; configured fallback candidates:"
            << m_failoverCandidateIds;
    if (ToastManager::instance()) ToastManager::instance()->showInfo(message);

    if (m_state == Protected) beginFailoverRestart();
    else tryNextFailoverCandidate();
    return true;
}

void ThroneEngine::probeActiveRoute() {
    if (m_state != Protected || !hasEligibleFallbackServers() || m_failoverInProgress) return;
    if (m_routeProbeInFlight.exchange(true, std::memory_order_acq_rel)) return;

    const uint64_t generation = m_connectSeq.load(std::memory_order_acquire);
    m_workerPool.start([this, generation]() {
        int latencyMs = 0;
        const bool reachable = testCurrentCoreRouteBlocking(1200, &latencyMs);
        QMetaObject::invokeMethod(this, [this, reachable, latencyMs, generation]() {
            m_routeProbeInFlight.store(false, std::memory_order_release);
            handleRouteProbeResult(reachable, latencyMs, generation);
        });
    });
}

void ThroneEngine::handleRouteProbeResult(bool reachable, int latencyMs, uint64_t generation) {
    if (generation != m_connectSeq.load(std::memory_order_acquire) || m_state != Protected ||
        !m_failoverEnabled || m_failoverInProgress) {
        m_routeFailureDetector.reset();
        m_routeConfirmTimer.stop();
        return;
    }

    if (reachable) {
        m_routeFailureDetector.recordProbe(true);
        m_routeConfirmTimer.stop();
        if (m_activeServerId >= 0 && latencyMs > 0 && ConfigAdapter::instance()) {
            ConfigAdapter::instance()->updateServerPing(m_activeServerId, latencyMs);
        }
        return;
    }

    qWarning() << "[ThroneEngine] Active route probe failed; consecutive failures:"
               << (m_routeFailureDetector.consecutiveFailures() + 1);
    if (!m_routeFailureDetector.recordProbe(false)) {
        // Confirm quickly after the first miss, but require a second independent
        // request before changing the user's route.
        m_routeConfirmTimer.start(800);
        return;
    }

    if (triggerFailover()) return;

    m_routeFailureDetector.reset();
    m_routeHealthTimer.stop();
    if (ToastManager::instance()) {
        ToastManager::instance()->showInfo(
            tr("Текущий маршрут не отвечает. Добавьте резервные серверы в настройках, чтобы включить переключение."));
    }
    QTimer::singleShot(15000, this, [this]() { configureFailoverMonitoring(); });
}

void ThroneEngine::beginFailoverRestart() {
    if (!m_failoverInProgress || m_state != Protected) return;

    ++m_connectSeq;
    const uint64_t generation = m_connectSeq.load(std::memory_order_acquire);
    m_intentionalStop = true;
    m_userWantsConnect = true;
    stopTrafficLooper();
    setState(Connecting);
    m_statusMessage = tr("Переключение на резервный сервер...");
    emit statusMessageChanged(m_statusMessage);

    m_workerPool.start([this, generation]() {
        bool stopped = false;
        if (API::defaultClient && m_rpcConnected.load(std::memory_order_acquire)) {
            API::defaultClient->Stop(&stopped);
        }
        QThread::msleep(150);
        QMetaObject::invokeMethod(this, [this, generation]() {
            if (generation != m_connectSeq.load(std::memory_order_acquire) || !m_failoverInProgress ||
                m_cleanedUp) {
                return;
            }
            m_intentionalStop = false;
            setState(Disconnected);
            tryNextFailoverCandidate();
        });
    });
}

void ThroneEngine::tryNextFailoverCandidate() {
    if (!m_failoverInProgress || m_cleanedUp) return;
    while (m_failoverCandidateIndex < m_failoverCandidateIds.size()) {
        const int candidateId = m_failoverCandidateIds.at(m_failoverCandidateIndex++);
        if (m_failedServerIds.contains(candidateId) || !Configs::dataManager ||
            !Configs::dataManager->profilesRepo || !Configs::dataManager->profilesRepo->GetProfile(candidateId)) {
            continue;
        }

        m_failedServerIds.insert(candidateId);
        m_activeServerOverride = candidateId;
        m_userWantsConnect = true;
        m_intentionalStop = false;
        qInfo() << "[ThroneEngine] Trying fallback profile" << candidateId;
        startConnection();
        return;
    }

    finishFailoverFailure(tr("Не удалось подключиться ни к одному серверу из резервного пула."));
}

void ThroneEngine::finishFailoverFailure(const QString &error) {
    qWarning() << "[ThroneEngine]" << error;
    m_failoverInProgress = false;
    m_failoverCandidateIds.clear();
    m_failoverCandidateIndex = 0;
    m_failedServerIds.clear();
    m_activeServerOverride = -1;
    m_userWantsConnect = false;
    m_intentionalStop = false;
    if (m_activeServerId >= 0) {
        m_activeServerId = -1;
        emit activeServerChanged(m_activeServerId);
    }
    setState(Disconnected);
    const int retryDelaySeconds = hasEligibleFallbackServers()
                                      ? m_failoverRetryDelayMs / 1000
                                      : 0;
    const QString userMessage = retryDelaySeconds > 0
                                    ? tr("%1 Повторная попытка через %2 сек.").arg(error).arg(retryDelaySeconds)
                                    : error;
    if (ToastManager::instance()) ToastManager::instance()->showError(userMessage);
    emit errorOccurred(userMessage);
    if (retryDelaySeconds > 0) {
        m_failoverRetryDelayMs = qMin(m_failoverRetryDelayMs * 2, 300000);
        m_failoverRetryTimer.start(retryDelaySeconds * 1000);
    }
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
    m_userWantsConnect = false;
    ++m_connectSeq;
    m_routeHealthTimer.stop();
    m_routeConfirmTimer.stop();
    m_failoverRetryTimer.stop();
    m_workerPool.clear();

    // Do not let a pending Polkit authorization continue changing system
    // privileges after the application is shutting down.
    if (m_capabilitySetupProcess) {
        QProcess *proc = m_capabilitySetupProcess;
        disconnect(proc, nullptr, this, nullptr);
        if (proc->state() != QProcess::NotRunning) {
            proc->terminate();
            if (!proc->waitForFinished(1000)) {
                proc->kill();
                proc->waitForFinished(500);
            }
        }
        m_capabilitySetupProcess = nullptr;
        m_capabilitySetupInProgress = false;
        delete proc;
    }

    stopConnection();
    // Workers use this object and shared Throne repositories. Join them before
    // the engine or its database dependencies can be destroyed.
    m_workerPool.waitForDone();
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

    ++m_rpcGeneration;
    m_rpcConnected.store(false, std::memory_order_release);

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
