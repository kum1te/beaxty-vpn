// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QLocalServer>
#include <QHash>
#include <QByteArray>
#include <QString>

class QLocalSocket;

// Local, same-user-only channel used to raise the already running GUI and
// optionally deliver one validated application deep link.
class ActivationChannel final : public QObject {
    Q_OBJECT

public:
    explicit ActivationChannel(QObject *parent = nullptr);

    static bool preparePrivateDirectory(const QString &privateDirectory);
    static QString serverNameForDirectory(const QString &privateDirectory);
    bool listenInDirectory(const QString &privateDirectory);
    bool hasRestrictedSocketAccess() const;

signals:
    void showRequested();
    void deepLinkRequested(const QString &url);

private:
    void acceptConnections();
    void readClient(QLocalSocket *socket);
    void finishClient(QLocalSocket *socket);
    void rejectClient(QLocalSocket *socket);

    QLocalServer m_server;
    QHash<QLocalSocket *, QByteArray> m_clientData;
};
