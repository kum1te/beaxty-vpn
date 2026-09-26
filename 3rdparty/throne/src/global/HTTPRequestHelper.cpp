#include "include/global/HTTPRequestHelper.hpp"

#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QFile>
#include <QApplication>
#include <QMap>
#include <QStringList>
#include <QHostAddress>
#include <QHostInfo>
#include <QCryptographicHash>

#include "include/global/Configs.hpp"
#include "include/ui/mainwindow.h"
#include "include/global/DeviceDetailsHelper.hpp"

namespace Configs_network {

    namespace {
        bool isPrivateOrSpecialAddress(const QHostAddress &address) {
            if (address.isNull() || address.isLoopback() || address.isLinkLocal() ||
                address.isMulticast() || address.isBroadcast()) {
                return true;
            }

            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                const quint32 ip = address.toIPv4Address();
                const quint32 a = (ip >> 24) & 0xff;
                const quint32 b = (ip >> 16) & 0xff;
                const quint32 c = (ip >> 8) & 0xff;
                return a == 0 || a == 10 || a == 127 ||
                       (a == 100 && b >= 64 && b <= 127) ||
                       (a == 169 && b == 254) ||
                       (a == 172 && b >= 16 && b <= 31) ||
                       (a == 192 && (b == 168 || (b == 0 && c == 0) ||
                                     (b == 0 && c == 2) || (b == 0 && c == 9) ||
                                     (b == 0 && c == 10))) ||
                       (a == 198 && ((b == 18) || (b == 19) || b == 51)) ||
                       (a == 203 && b == 0 && c == 113) ||
                       a >= 224;
            }

            const Q_IPV6ADDR v6 = address.toIPv6Address();
            // fc00::/7 (ULA), plus the unspecified address. Qt's loopback,
            // link-local and multicast checks above cover the other local ranges.
            if ((v6[0] & 0xfe) == 0xfc) return true;
            if (v6[0] == 0 && v6[1] == 0 && v6[2] == 0 && v6[3] == 0 &&
                v6[4] == 0 && v6[5] == 0 && v6[6] == 0 && v6[7] == 0 &&
                v6[8] == 0 && v6[9] == 0 && v6[10] == 0 && v6[11] == 0 &&
                v6[12] == 0 && v6[13] == 0 && v6[14] == 0 && v6[15] == 0) {
                return true;
            }
            // IPv4-mapped addresses must use the same RFC1918 checks.
            if (v6[0] == 0 && v6[1] == 0 && v6[2] == 0 && v6[3] == 0 &&
                v6[4] == 0 && v6[5] == 0 && v6[6] == 0 && v6[7] == 0 &&
                v6[8] == 0 && v6[9] == 0 && v6[10] == 0xff && v6[11] == 0xff) {
                return isPrivateOrSpecialAddress(QHostAddress((quint32(v6[12]) << 24) |
                                                               (quint32(v6[13]) << 16) |
                                                               (quint32(v6[14]) << 8) |
                                                               quint32(v6[15])));
            }
            return false;
        }

        bool isUnsafeHost(const QString &rawHost, bool allowLoopbackForTests) {
            QString host = rawHost.trimmed().toLower();
            while (host.endsWith(QLatin1Char('.'))) host.chop(1);
            if (host.isEmpty() || host == QStringLiteral("localhost") ||
                host == QStringLiteral("metadata.google.internal")) {
                return !allowLoopbackForTests || host != QStringLiteral("localhost");
            }

            const QHostAddress literal(host);
            if (!literal.isNull()) {
                if (literal.isLoopback() && allowLoopbackForTests) return false;
                return isPrivateOrSpecialAddress(literal);
            }

            // Resolve hostnames before opening a request. If any answer points to
            // a local/special range, reject the hostname rather than trusting its
            // spelling. A failed lookup is left to QNetworkAccessManager.
            const QHostInfo info = QHostInfo::fromName(host);
            if (info.error() == QHostInfo::NoError) {
                for (const QHostAddress &address : info.addresses()) {
                    if (isPrivateOrSpecialAddress(address) &&
                        !(allowLoopbackForTests && address.isLoopback())) {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    bool NetworkRequestHelper::IsSafePublicUrl(const QUrl &url) {
        if (!url.isValid()) return false;
        QString scheme = url.scheme().toLower();
        if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) return false;

        QString host = url.host().trimmed().toLower();
        if (host.isEmpty()) return false;

        // In test environments (e.g. QPA offscreen or test runner), allow loopback mock test servers
        // Test builds must opt in explicitly. A headless UI environment is not
        // a security boundary and must never make localhost requests acceptable.
        bool isTestEnv = qEnvironmentVariableIsSet("BEAXTY_ALLOW_LOCAL_TEST_REQUESTS");
        return !isUnsafeHost(host, isTestEnv);
    }

    HTTPResponse NetworkRequestHelper::HttpGet(const QString &url, bool sendHwid, bool useProxy, qint64 maxBytes) {
        QEventLoop loop;
        HTTPResponse result;
        bool finished = false;
        HttpGetAsync(&loop, url, sendHwid, useProxy, maxBytes, [&](HTTPResponse response) {
            result = std::move(response);
            finished = true;
            loop.quit();
        });
        if (!finished) loop.exec();
        return result;
    }

    void NetworkRequestHelper::HttpGetAsync(QObject *context, const QString &url, bool sendHwid,
                                            bool useProxy, qint64 maxBytes,
                                            std::function<void(HTTPResponse)> done) {
        if (!Configs::dataManager || !Configs::dataManager->settingsRepo) {
            QTimer::singleShot(0, context, [done = std::move(done)]() {
                done(HTTPResponse{QObject::tr("Database is not ready.")});
            });
            return;
        }

        QUrl parsedUrl(url);
        if (!IsSafePublicUrl(parsedUrl)) {
            QString blockedMsg = QObject::tr("Blocked request to prohibited host/IP: %1").arg(url);
            QTimer::singleShot(0, context, [done = std::move(done), blockedMsg]() {
                done(HTTPResponse{blockedMsg});
            });
            return;
        }

        QNetworkRequest request;
        auto accessManager = new QNetworkAccessManager(context);
        accessManager->setTransferTimeout(10000);
        request.setUrl(parsedUrl);
        if (Configs::dataManager->settingsRepo->net_use_proxy || Configs::dataManager->settingsRepo->spmode_system_proxy || useProxy) {
            if (Configs::dataManager->settingsRepo->started_id < 0) {
                accessManager->deleteLater();
                done(HTTPResponse{QObject::tr("Request with proxy but no profile started.")});
                return;
            }
            QNetworkProxy p;
            p.setType(QNetworkProxy::HttpProxy);
            p.setHostName(Configs::dataManager->settingsRepo->inbound_address == "::" ? "127.0.0.1" : Configs::dataManager->settingsRepo->inbound_address);
            p.setPort(Configs::dataManager->settingsRepo->inbound_socks_port);
            if (Configs::dataManager->settingsRepo->inbound_auth) {
                p.setUser(Configs::dataManager->settingsRepo->inbound_user);
                p.setPassword(Configs::dataManager->settingsRepo->inbound_pass);
            }
            accessManager->setProxy(p);
        }
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, Configs::dataManager->settingsRepo->GetUserAgent());
        if (Configs::dataManager->settingsRepo->net_insecure) {
            QSslConfiguration c;
            c.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
            request.setSslConfiguration(c);
        }
        if (sendHwid) {
            auto details = GetDeviceDetails();

            QMap<QString, QString> customParams;
            if (!Configs::dataManager->settingsRepo->sub_custom_hwid_params.isEmpty()) {
                QStringList pairs = Configs::dataManager->settingsRepo->sub_custom_hwid_params.split(',');
                for (const QString &pair : pairs) {
                    QString trimmed = pair.trimmed();
                    int eqPos = trimmed.indexOf('=');
                    if (eqPos > 0) {
                        QString key = trimmed.left(eqPos).trimmed();
                        QString value = trimmed.mid(eqPos + 1).trimmed();
                        if (!key.isEmpty() && !value.isEmpty() &&
                            !value.contains('\n') && !value.contains('\r') &&
                            value.length() < 1000) {
                            QString lowerKey = key.toLower();
                            if (lowerKey == "hwid" || lowerKey == "os" ||
                                lowerKey == "osversion" || lowerKey == "model") {
                                customParams[lowerKey] = value;
                            }
                        }
                    }
                }
            }

            // Never send the raw machine identifier from DeviceDetailsHelper.
            // The facade stores the same stable, one-way representation in the
            // database, but hashing here also protects callers that use the
            // network helper before DeviceIdentity has been constructed.
            QString hwid = customParams.contains("hwid")
                               ? customParams["hwid"]
                               : QString::fromLatin1(QCryptographicHash::hash(
                                     details.hwid.toUtf8(), QCryptographicHash::Sha256).toHex());
            QString os = customParams.contains("os") ? customParams["os"] : details.os;
            QString osVersion = customParams.contains("osversion") ? customParams["osversion"] : details.osVersion;
            QString model = customParams.contains("model") ? customParams["model"] : details.model;

            if (!hwid.isEmpty()) request.setRawHeader("x-hwid", hwid.toUtf8());
            if (!os.isEmpty()) request.setRawHeader("x-device-os", os.toUtf8());
            if (!osVersion.isEmpty()) request.setRawHeader("x-ver-os", osVersion.toUtf8());
            if (!model.isEmpty()) request.setRawHeader("x-device-model", model.toUtf8());
        }
        auto reply = accessManager->get(request);
        connect(reply, &QNetworkReply::redirected, reply, [reply](const QUrl &redirectUrl) {
            if (!IsSafePublicUrl(redirectUrl)) {
                qWarning() << "[HTTPRequestHelper] Blocked SSRF redirect to:" << redirectUrl;
                reply->abort();
            }
        });
        auto body = std::make_shared<QByteArray>();
        auto tooLarge = std::make_shared<bool>(false);
        // Bound both memory consumption and total request duration, including slow streams.
        reply->setReadBufferSize(64 * 1024);
        auto deadline = new QTimer(reply);
        deadline->setSingleShot(true);
        connect(deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
        deadline->start(30000);
        connect(reply, &QNetworkReply::readyRead, reply, [reply, body, tooLarge, maxBytes] {
            *body += reply->readAll();
            if (maxBytes > 0 && body->size() > maxBytes) {
                *tooLarge = true;
                reply->abort();
            }
        });
        connect(reply, &QNetworkReply::finished, context,
                [reply, accessManager, body, tooLarge, maxBytes, done = std::move(done)]() {
            if (reply->isOpen()) *body += reply->readAll();
            HTTPResponse result;
            result.header = reply->rawHeaderPairs();
            if (*tooLarge || (maxBytes > 0 && body->size() > maxBytes)) {
                result.error = QObject::tr("Response larger than %1 MB").arg(maxBytes / (1024 * 1024));
            } else if (reply->error() != QNetworkReply::NoError) {
                result.error = reply->errorString();
            } else {
                result.data = std::move(*body);
            }
            accessManager->deleteLater();
            done(std::move(result));
        });
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        const QByteArray needle = name.toLatin1();
        for (const auto &p: header) {
            if (p.first.compare(needle, Qt::CaseInsensitive) == 0) return p.second;
        }
        return {};
    }

    QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName, bool useProxy) {
        QUrl parsedUrl(url);
        if (!IsSafePublicUrl(parsedUrl)) {
            return QObject::tr("Blocked download from prohibited host/IP: %1").arg(url);
        }

        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        request.setUrl(parsedUrl);
        if (Configs::dataManager->settingsRepo->net_use_proxy || Configs::dataManager->settingsRepo->spmode_system_proxy || useProxy) {
            if (Configs::dataManager->settingsRepo->started_id < 0) {
                return QObject::tr("Request with proxy but no profile started.");
            }
            QNetworkProxy p;
            p.setType(QNetworkProxy::HttpProxy);
            p.setHostName(Configs::dataManager->settingsRepo->inbound_address == "::" ? "127.0.0.1" : Configs::dataManager->settingsRepo->inbound_address);
            p.setPort(Configs::dataManager->settingsRepo->inbound_socks_port);
            if (Configs::dataManager->settingsRepo->inbound_auth) {
                p.setUser(Configs::dataManager->settingsRepo->inbound_user);
                p.setPassword(Configs::dataManager->settingsRepo->inbound_pass);
            }
            accessManager.setProxy(p);
        }
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        if (Configs::dataManager->settingsRepo->net_insecure) {
            QSslConfiguration c;
            c.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
            request.setSslConfiguration(c);
        }

        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::redirected, _reply, [_reply](const QUrl &redirectUrl) {
            if (!IsSafePublicUrl(redirectUrl)) {
                qWarning() << "[HTTPRequestHelper] Blocked SSRF redirect in DownloadAsset to:" << redirectUrl;
                _reply->abort();
            }
        });
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1 %2").arg(error_str.join(","), Configs::dataManager->settingsRepo->net_insecure ? "(Ignored)" : ""));
        });
        connect(_reply, &QNetworkReply::downloadProgress, _reply, [&](qint64 bytesReceived, qint64 bytesTotal)
        {
            runOnUiThread([=]{
                GetMainWindow()->setDownloadReport(DownloadProgressReport{fileName, bytesReceived, bytesTotal}, true);
                GetMainWindow()->UpdateDataView();
            });
        });
        QEventLoop loop;
        connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        runOnUiThread([=]
        {
            GetMainWindow()->setDownloadReport({}, false);
            GetMainWindow()->UpdateDataView(true);
        });
        auto netErr = _reply->error();
        const QString netErrStr = _reply->errorString();
        const int httpStatus = _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = _reply->readAll();
        _reply->deleteLater();

        if (netErr != QNetworkReply::NetworkError::NoError) {
            return netErrStr;
        }

        if (httpStatus != 0 && (httpStatus < 200 || httpStatus >= 300)) {
            return QObject::tr("Download failed: server returned HTTP status %1.").arg(httpStatus);
        }
        if (body.isEmpty()) {
            return QObject::tr("Download failed: the server returned an empty response.");
        }

        const auto filePath = Configs::GetBasePath() + "/" + fileName;
        const auto tmpPath = filePath + ".tmp";
        QFile tmp(tmpPath);
        if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return QObject::tr("Could not open file.");
        }
        if (tmp.write(body) != body.size() || !tmp.flush()) {
            tmp.close();
            tmp.remove();
            return QObject::tr("Could not write file.");
        }
        tmp.close();
        QFile::remove(filePath);
        if (!tmp.rename(filePath)) {
            tmp.remove();
            return QObject::tr("Could not save downloaded file.");
        }
        return "";
    }

} // namespace Configs_network
