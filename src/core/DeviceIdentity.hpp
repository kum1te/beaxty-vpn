// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>

class DeviceIdentity : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hwidEnabled READ isHwidEnabled WRITE setHwidEnabled NOTIFY hwidEnabledChanged)
    Q_PROPERTY(QString rawHwid READ rawHwid CONSTANT)
    Q_PROPERTY(QString sanitizedHwid READ sanitizedHwid CONSTANT)

public:
    explicit DeviceIdentity(QObject *parent = nullptr);
    static DeviceIdentity *instance();

    bool isHwidEnabled() const;
    void setHwidEnabled(bool enabled);

    QString rawHwid() const;
    QString sanitizedHwid() const;

    Q_INVOKABLE void copyHwidToClipboard();

    // Returns the deterministic SHA-256 machine hash
    static QString computeSha256Hwid();

signals:
    void hwidEnabledChanged(bool enabled);

private:
    static DeviceIdentity *s_instance;
    QString m_hwidHash;
};
