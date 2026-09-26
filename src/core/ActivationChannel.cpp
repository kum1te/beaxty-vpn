// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "ActivationChannel.hpp"
#include "LocalPeerCredentials.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QTimer>
#include <QDebug>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr qsizetype kMaximumMessageBytes = 8192;
constexpr int kClientTimeoutMs = 2000;
constexpr int kMaximumPendingClients = 4;

bool isPrivateOwnedDirectory(const QString &path) {
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir() || info.isSymLink()) return false;

#if defined(Q_OS_UNIX)
    struct stat status{};
    const QByteArray encodedPath = QFile::encodeName(info.absoluteFilePath());
    if (::lstat(encodedPath.constData(), &status) != 0 || !S_ISDIR(status.st_mode) ||
        status.st_uid != ::geteuid() || (status.st_mode & 0077) != 0) {
        return false;
    }
#endif
    return true;
}

bool ensurePrivateOwnedDirectory(const QString &path) {
    if (path.isEmpty()) return false;
#if defined(Q_OS_UNIX)
    const QByteArray encodedPath = QFile::encodeName(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    if (::mkdir(encodedPath.constData(), 0700) != 0) {
        if (errno == ENOENT) {
            QDir dir;
            if (!dir.mkpath(QFileInfo(path).absolutePath()) || ::mkdir(encodedPath.constData(), 0700) != 0) {
                return false;
            }
        } else if (errno != EEXIST || !isPrivateOwnedDirectory(path)) {
            return false;
        }
    }
#else
    QDir dir;
    if (!dir.mkpath(path)) return false;
#endif
    return isPrivateOwnedDirectory(path);
}
}

ActivationChannel::ActivationChannel(QObject *parent)
    : QObject(parent) {
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.setMaxPendingConnections(kMaximumPendingClients);
    connect(&m_server, &QLocalServer::newConnection, this, &ActivationChannel::acceptConnections);
}

QString ActivationChannel::serverNameForDirectory(const QString &privateDirectory) {
#if defined(Q_OS_WIN)
    const QString canonical = QFileInfo(privateDirectory).absoluteFilePath().toCaseFolded();
    const QByteArray digest = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("beaxty-vpn-activation-") + QString::fromLatin1(digest.left(32));
#else
    return QDir(privateDirectory).filePath(QStringLiteral("activation.sock"));
#endif
}

bool ActivationChannel::preparePrivateDirectory(const QString &privateDirectory) {
    return ensurePrivateOwnedDirectory(privateDirectory);
}

bool ActivationChannel::listenInDirectory(const QString &privateDirectory) {
    if (!isPrivateOwnedDirectory(privateDirectory)) return false;
    const QString endpoint = serverNameForDirectory(privateDirectory);
    if (endpoint.isEmpty()) return false;
#if defined(Q_OS_UNIX)
    // sockaddr_un.sun_path is commonly 108 bytes including the trailing NUL.
    if (QFile::encodeName(endpoint).size() >= 100) return false;
#endif
    QLocalServer::removeServer(endpoint);
    return m_server.listen(endpoint);
}

bool ActivationChannel::hasRestrictedSocketAccess() const {
    return m_server.socketOptions().testFlag(QLocalServer::UserAccessOption);
}

void ActivationChannel::acceptConnections() {
    while (m_server.hasPendingConnections()) {
        QLocalSocket *socket = m_server.nextPendingConnection();
        if (!socket) continue;

        if (m_clientData.size() >= kMaximumPendingClients) {
            socket->abort();
            socket->deleteLater();
            continue;
        }

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD)
        if (!LocalPeerCredentials::isSameUserPeer(socket->socketDescriptor())) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
#endif

        socket->setReadBufferSize(kMaximumMessageBytes + 1);
        m_clientData.insert(socket, QByteArray());
        auto *timer = new QTimer(socket);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, socket, [this, socket]() { rejectClient(socket); });
        connect(socket, &QLocalSocket::readyRead, socket, [this, socket]() { readClient(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() { finishClient(socket); });
        timer->start(kClientTimeoutMs);
    }
}

void ActivationChannel::readClient(QLocalSocket *socket) {
    auto it = m_clientData.find(socket);
    if (it == m_clientData.end()) return;

    it.value().append(socket->readAll());
    if (it.value().size() > kMaximumMessageBytes) rejectClient(socket);
}

void ActivationChannel::finishClient(QLocalSocket *socket) {
    auto it = m_clientData.find(socket);
    if (it == m_clientData.end()) {
        socket->deleteLater();
        return;
    }

    it.value().append(socket->readAll());
    const QByteArray data = it.value();
    m_clientData.erase(it);
    socket->deleteLater();

    if (data.isEmpty() || data.size() > kMaximumMessageBytes || data.endsWith('\n') == false ||
        data.count('\n') != 1 || data.contains('\r') || data.contains('\0')) {
        return;
    }

    const QByteArray body = data.left(data.size() - 1);
    const QString message = QString::fromUtf8(body.constData(), body.size());
    if (message.toUtf8() != body) return;
    if (message == QStringLiteral("show")) {
        emit showRequested();
        return;
    }
    if (message.startsWith(QStringLiteral("DEEPLINK ")) && message.size() > 9) {
        emit deepLinkRequested(message.mid(9));
    }
}

void ActivationChannel::rejectClient(QLocalSocket *socket) {
    m_clientData.remove(socket);
    socket->abort();
    socket->deleteLater();
}
