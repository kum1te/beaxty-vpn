// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>

class AutostartManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autostartEnabled READ autostartEnabled WRITE setAutostartEnabled NOTIFY autostartEnabledChanged)

public:
    explicit AutostartManager(QObject *parent = nullptr);
    ~AutostartManager() override = default;

    bool autostartEnabled() const;
    void setAutostartEnabled(bool enabled);

signals:
    void autostartEnabledChanged();

private:
#if defined(Q_OS_LINUX)
    QString desktopFilePath() const;
#endif
};
