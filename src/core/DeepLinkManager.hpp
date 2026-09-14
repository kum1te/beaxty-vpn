// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class DeepLinkManager : public QObject {
    Q_OBJECT

public:
    explicit DeepLinkManager(QObject *parent = nullptr);
    ~DeepLinkManager() override;
    static DeepLinkManager *instance();

    // Validates and processes a deep link string. If valid, emits deepLinkReceived().
    // Returns true if successfully handled.
    Q_INVOKABLE bool handleDeepLink(const QString &rawUrl);

    // Static helper to validate and parse deeplink without side effects
    static bool parseDeepLink(const QString &rawUrl, QString &targetUrl, QString &groupName, QString *outError = nullptr);

    // Register/Unregister the beaxty:// scheme in the operating system
    Q_INVOKABLE static void registerScheme();
    Q_INVOKABLE static void unregisterScheme();
    Q_INVOKABLE static bool isSchemeRegistered();

signals:
    void deepLinkReceived(const QString &targetUrl, const QString &groupName);

private:
    static DeepLinkManager *s_instance;
};
