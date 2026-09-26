// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTimer>

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#endif

#include "src/core/ActivationChannel.hpp"

static bool sendMessage(ActivationChannel &channel, const QString &serverName,
                        const QByteArray &message, int waitMs,
                        QString *receivedDeepLink = nullptr) {
    QLocalSocket client;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool received = false;

    QObject::connect(&client, &QLocalSocket::connected, &loop, [&]() {
        assert(client.write(message) == message.size());
        client.disconnectFromServer();
    });
    QObject::connect(&channel, &ActivationChannel::showRequested, &loop, [&]() {
        received = true;
        loop.quit();
    });
    QObject::connect(&channel, &ActivationChannel::deepLinkRequested, &loop,
                     [&](const QString &url) {
        received = true;
        if (receivedDeepLink) *receivedDeepLink = url;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    client.connectToServer(serverName);
    timeout.start(waitMs);
    loop.exec();
    return received;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    assert(temporary.isValid());

    const QString runtimeDirectory = temporary.path() + QStringLiteral("/beaxty-vpn");
    assert(ActivationChannel::preparePrivateDirectory(runtimeDirectory));
#if defined(Q_OS_UNIX)
    struct stat status{};
    assert(::stat(QFile::encodeName(runtimeDirectory).constData(), &status) == 0);
    assert((status.st_mode & 0777) == 0700);
#endif

    ActivationChannel channel;
    assert(channel.hasRestrictedSocketAccess());
    assert(channel.listenInDirectory(runtimeDirectory));
    const QString serverName = ActivationChannel::serverNameForDirectory(runtimeDirectory);

    assert(sendMessage(channel, serverName, QByteArrayLiteral("show\n"), 1000));
    QString receivedDeepLink;
    const QByteArray deepLink = QByteArrayLiteral("DEEPLINK beaxty://import?url=https%3A%2F%2Fexample.com%2Fsub\n");
    assert(sendMessage(channel, serverName, deepLink, 1000, &receivedDeepLink));
    assert(receivedDeepLink == QStringLiteral("beaxty://import?url=https%3A%2F%2Fexample.com%2Fsub"));

    assert(!sendMessage(channel, serverName, QByteArrayLiteral("show\nshow\n"), 100));
    assert(!sendMessage(channel, serverName, QByteArray(8193, 'x') + '\n', 100));
    assert(!sendMessage(channel, serverName, QByteArray::fromHex("73686f77ff0a"), 100));

    const QString symlinkPath = temporary.path() + QStringLiteral("/beaxty-link");
    assert(QFile::link(runtimeDirectory, symlinkPath));
    assert(!ActivationChannel::preparePrivateDirectory(symlinkPath));

    std::cout << "Activation channel tests passed.\n";
    return 0;
}
