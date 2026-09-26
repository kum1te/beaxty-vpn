// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "DeepLinkManager.hpp"
#include "ToastManager.hpp"
#include "3rdparty/throne/include/global/HTTPRequestHelper.hpp"

#include <QUrlQuery>
#include <QHostAddress>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDebug>

#if defined(_WIN32)
#include <QSettings>
#include <shlobj.h>
#endif

DeepLinkManager *DeepLinkManager::s_instance = nullptr;

DeepLinkManager::DeepLinkManager(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

DeepLinkManager::~DeepLinkManager() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

DeepLinkManager *DeepLinkManager::instance() {
    return s_instance;
}

namespace {
bool parseStrictAbsoluteUrl(const QString &rawUrl, QUrl &url) {
    // Do not trim or repair browser-provided URLs. A URL with whitespace or
    // malformed escaping must fail closed instead of changing its meaning.
    if (rawUrl.isEmpty() || rawUrl.size() > 8192 || rawUrl != rawUrl.trimmed()) {
        return false;
    }

    url = QUrl(rawUrl, QUrl::StrictMode);
    return url.isValid() && !url.isRelative() && !url.scheme().isEmpty();
}

bool hasUserInfo(const QUrl &url) {
    // QUrl in the project's minimum Qt release has no hasUserInfo() accessor.
    // In a fully encoded authority, a literal '@' is the user-info separator;
    // an encoded %40 inside a username remains encoded and is not confused.
    return url.authority(QUrl::FullyEncoded).contains(QLatin1Char('@'));
}

QString safeUrlForLog(const QString &rawUrl) {
    const QUrl parsed(rawUrl);
    if (!parsed.isValid() || parsed.scheme().isEmpty() || parsed.host().isEmpty()) {
        return QStringLiteral("<invalid-url>");
    }

    // Subscription tokens commonly live in the path or query. Keep only the
    // origin and never write credentials, paths, fragments, or query strings.
    QString result = parsed.scheme().toLower() + QStringLiteral("://") + parsed.host().toLower();
    if (parsed.port() > 0) result += QStringLiteral(":%1").arg(parsed.port());
    return result;
}
}

bool DeepLinkManager::isBeaxtyUrl(const QString &rawUrl) const {
    QUrl url;
    return parseStrictAbsoluteUrl(rawUrl, url) &&
           url.scheme().compare(QStringLiteral("beaxty"), Qt::CaseInsensitive) == 0;
}

bool DeepLinkManager::isTrustedCabinetUrl(const QString &rawUrl) const {
    QUrl url;
    if (!parseStrictAbsoluteUrl(rawUrl, url) ||
        url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0 ||
        hasUserInfo(url)) {
        return false;
    }

    // Keep the embedded account origin narrow. Other beaxty subdomains can
    // contain unrelated or user-controlled content and belong in a browser.
    if (url.host(QUrl::EncodeUnicode).compare(QStringLiteral("cabinet.beaxty.com"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    const int port = url.port(-1);
    return port == -1 || port == 443;
}

bool DeepLinkManager::isAllowedExternalUrl(const QString &rawUrl) const {
    QUrl url;
    if (!parseStrictAbsoluteUrl(rawUrl, url) ||
        url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0 ||
        url.host().isEmpty() || hasUserInfo(url)) {
        return false;
    }

    // Reject known local hostnames and address literals. DNS resolution is
    // deliberately avoided here: this method runs in the UI thread and a
    // user-clicked link is opened by the user's system browser.
    const QString host = url.host(QUrl::EncodeUnicode).toLower();
    if (host == QStringLiteral("localhost") || host.endsWith(QStringLiteral(".localhost")) ||
        host.endsWith(QStringLiteral(".local")) || host == QStringLiteral("metadata.google.internal")) {
        return false;
    }
    const QHostAddress address(host);
    // Reject IP literals altogether. This covers private, loopback, link-local,
    // mapped IPv4 and less common special ranges without duplicating a CIDR list.
    if (!address.isNull()) {
        return false;
    }

    const int port = url.port(-1);
    return port == -1 || (port > 0 && port <= 65535);
}

bool DeepLinkManager::parseDeepLink(const QString &rawUrl, QString &targetUrl, QString &groupName, QString *outError) {
    targetUrl.clear();
    groupName.clear();

    if (rawUrl.isEmpty()) {
        if (outError) *outError = QStringLiteral("Пустая ссылка");
        return false;
    }
    if (rawUrl.toUtf8().size() > 8192 || rawUrl != rawUrl.trimmed()) {
        if (outError) *outError = QStringLiteral("Некорректная длина ссылки или пробелы по краям");
        return false;
    }

    QUrl url(rawUrl, QUrl::StrictMode);
    if (!url.isValid() || url.scheme().compare(QStringLiteral("beaxty"), Qt::CaseInsensitive) != 0) {
        if (outError) *outError = QStringLiteral("Некорректная схема протокола (ожидается beaxty://)");
        return false;
    }

    // Host or path can represent action:
    // beaxty://import?url=... -> host is "import", path is ""
    // beaxty:///import?url=... -> host is "", path is "/import"
    // beaxty:import?url=...
    QString action = url.host().toLower();
    if (action.isEmpty()) {
        QString p = url.path();
        if (p.startsWith(QLatin1Char('/'))) p = p.mid(1);
        int slashIdx = p.indexOf(QLatin1Char('/'));
        if (slashIdx != -1) {
            action = p.left(slashIdx).toLower();
        } else {
            action = p.toLower();
        }
    }

    if (action != QStringLiteral("import") && action != QStringLiteral("subscribe")) {
        if (outError) *outError = QStringLiteral("Неподдерживаемое действие диплинка: ") + action;
        return false;
    }

    QUrlQuery query(url);
    QString paramUrl = query.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded).trimmed();
    if (paramUrl.isEmpty()) {
        paramUrl = query.queryItemValue(QStringLiteral("sub"), QUrl::FullyDecoded).trimmed();
    }
    if (paramUrl.isEmpty()) {
        paramUrl = query.queryItemValue(QStringLiteral("link"), QUrl::FullyDecoded).trimmed();
    }

    if (paramUrl.isEmpty()) {
        if (outError) *outError = QStringLiteral("В диплинке отсутствует параметр url=");
        return false;
    }

    QString paramName = query.queryItemValue(QStringLiteral("name"), QUrl::FullyDecoded).trimmed();
    if (paramName.isEmpty()) {
        paramName = query.queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded).trimmed();
    }

    // Strict validation of the target subscription URL
    QUrl targetParsed(paramUrl, QUrl::StrictMode);
    if (!targetParsed.isValid() || targetParsed.isRelative()) {
        if (outError) *outError = QStringLiteral("Некорректный URL подписки");
        return false;
    }

    QString scheme = targetParsed.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        if (outError) *outError = QStringLiteral("Недопустимый протокол целевого URL: ") + scheme;
        return false;
    }
    if (targetParsed.host().isEmpty()) {
        if (outError) *outError = QStringLiteral("Некорректный URL подписки");
        return false;
    }

    // Security check: strictly block loopback, private RFC1918 IPs, cloud metadata 169.254.169.254, broadcast, multicast
    QString host = targetParsed.host().trimmed().toLower();
    if (host == QStringLiteral("localhost") || host == QStringLiteral("metadata.google.internal")) {
        if (outError) *outError = QStringLiteral("Целевой адрес отклонён политикой безопасности (SSRF/Localhost)");
        return false;
    }
    QHostAddress addr(host);
    if (!addr.isNull()) {
        if (addr.isLoopback() || addr.isLinkLocal() || addr.isMulticast() || addr.isBroadcast()) {
            if (outError) *outError = QStringLiteral("Целевой IP-адрес отклонён политикой безопасности (SSRF/Private IP)");
            return false;
        }
        QString ipStr = addr.toString();
        if (ipStr == QStringLiteral("169.254.169.254") || ipStr == QStringLiteral("100.100.100.200") || ipStr == QStringLiteral("0.0.0.0")) {
            if (outError) *outError = QStringLiteral("Целевой IP-адрес отклонён политикой безопасности (SSRF/Cloud Metadata)");
            return false;
        }
    }

    if (!Configs_network::NetworkRequestHelper::IsSafePublicUrl(targetParsed)) {
        if (outError) *outError = QStringLiteral("Целевой адрес отклонён политикой безопасности (SSRF/Localhost/Metadata)");
        return false;
    }

    targetUrl = paramUrl;
    groupName = paramName;
    return true;
}

bool DeepLinkManager::handleDeepLink(const QString &rawUrl) {
    QString targetUrl;
    QString groupName;
    QString error;

    if (!parseDeepLink(rawUrl, targetUrl, groupName, &error)) {
        qWarning() << "[DeepLinkManager] Rejected deep link from:" << safeUrlForLog(rawUrl)
                   << "Reason:" << error;
        if (ToastManager::instance()) {
            ToastManager::instance()->showError(QStringLiteral("Ошибка ссылки: %1").arg(error));
        }
        return false;
    }

    qInfo() << "[DeepLinkManager] Valid deep link received. Target origin:"
            << safeUrlForLog(targetUrl) << "Group:" << groupName;
    emit deepLinkReceived(targetUrl, groupName);
    return true;
}

void DeepLinkManager::registerScheme() {
#if defined(_WIN32)
    const QString kClasses = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString openCmd = QStringLiteral("\"%1\" \"%2\"").arg(appPath, QStringLiteral("%1"));

    QSettings scheme(kClasses + QStringLiteral("\\beaxty"), QSettings::NativeFormat);
    scheme.setValue(QStringLiteral("Default"), QStringLiteral("URL:beaxty Protocol"));
    scheme.setValue(QStringLiteral("URL Protocol"), QStringLiteral(""));
    scheme.setValue(QStringLiteral("shell/open/command/Default"), openCmd);
    scheme.sync();
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    qInfo() << "[DeepLinkManager] Registered beaxty:// scheme in Windows registry";

#elif defined(__APPLE__)
    // On macOS, LaunchServices handles registration via CFBundleURLSchemes in Info.plist
    const QString kLsregister = QStringLiteral("/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister");
    QDir d(QCoreApplication::applicationDirPath());
    if (d.cdUp() && d.cdUp()) {
        const QString bundle = d.absolutePath();
        if (bundle.endsWith(QStringLiteral(".app"))) {
            QProcess::execute(kLsregister, {QStringLiteral("-f"), bundle});
            qInfo() << "[DeepLinkManager] Registered beaxty:// scheme with LaunchServices";
        }
    }

#else
    // Linux desktop integration
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QDir().mkpath(appsDir);
    const QString desktopPath = appsDir + QStringLiteral("/beaxty-vpn.desktop");

    auto env = QProcessEnvironment::systemEnvironment();
    QString execTarget = env.contains(QStringLiteral("APPIMAGE")) ? env.value(QStringLiteral("APPIMAGE")) : QCoreApplication::applicationFilePath();

    QFile f(desktopPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "[Desktop Entry]\n"
           << "Name=beaxty VPN\n"
           << "Comment=Minimalist Monochrome VPN Client\n"
           << "Exec=\"" << execTarget << "\" %u\n"
           << "Icon=beaxty-vpn\n"
           << "Terminal=false\n"
           << "Type=Application\n"
           << "Categories=Network;Security;\n"
           << "MimeType=x-scheme-handler/beaxty;\n";
        ts.flush();
        f.close();
    }

    QProcess::execute(QStringLiteral("update-desktop-database"), {appsDir});
    qInfo() << "[DeepLinkManager] Registered beaxty:// scheme desktop handler on Linux";
#endif
}

void DeepLinkManager::unregisterScheme() {
#if defined(_WIN32)
    const QString kClasses = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
    QSettings classes(kClasses, QSettings::NativeFormat);
    classes.remove(QStringLiteral("beaxty"));
    classes.sync();
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
#elif !defined(__APPLE__)
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    const QString desktopPath = appsDir + QStringLiteral("/beaxty-vpn.desktop");
    if (QFile::exists(desktopPath)) {
        QFile::remove(desktopPath);
        QProcess::execute(QStringLiteral("update-desktop-database"), {appsDir});
    }
#endif
}

bool DeepLinkManager::isSchemeRegistered() {
#if defined(_WIN32)
    const QString kClasses = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\beaxty");
    QSettings s(kClasses, QSettings::NativeFormat);
    return s.contains(QStringLiteral("URL Protocol"));
#elif defined(__APPLE__)
    return true;
#else
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    const QString desktopPath = appsDir + QStringLiteral("/beaxty-vpn.desktop");
    QFile f(desktopPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.contains(QStringLiteral("x-scheme-handler/beaxty"))) return true;
    }
    return false;
#endif
}
