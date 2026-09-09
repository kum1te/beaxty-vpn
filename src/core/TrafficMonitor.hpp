// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <QTimer>

class TrafficMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString downloadSpeed READ downloadSpeed NOTIFY speedUpdated)
    Q_PROPERTY(QString uploadSpeed READ uploadSpeed NOTIFY speedUpdated)
    Q_PROPERTY(QString totalTraffic READ totalTraffic NOTIFY trafficUpdated)
    Q_PROPERTY(int currentPing READ currentPing NOTIFY pingUpdated)
    Q_PROPERTY(bool isTestingPing READ isTestingPing NOTIFY pingTestingChanged)

public:
    // Reported latency for a node we could not reach at all.
    static constexpr int PingUnreachable = 999;
    static constexpr int PingTimeoutMs = 2500;

    explicit TrafficMonitor(QObject *parent = nullptr);
    static TrafficMonitor *instance();

    QString downloadSpeed() const;
    QString uploadSpeed() const;
    QString totalTraffic() const;
    int currentPing() const;
    bool isTestingPing() const;
    void setTestingPing(bool testing);

    void updateTraffic(int proxyDl, int proxyUp, int directDl, int directUp);
    void setCurrentPing(int pingMs);

    Q_INVOKABLE void testAllPings();
    Q_INVOKABLE void testServerPing(int profileId);
    Q_INVOKABLE void resetSessionStats();
    void applyPingResult(int profileId, int pingMs);

    static QString formatBytes(quint64 bytes);
    static QString formatSpeed(quint64 bytesPerSec);

signals:
    void speedUpdated();
    void trafficUpdated();
    void pingUpdated(int pingMs);
    void pingTestingChanged(bool testing);
    void serverPingUpdated(int profileId, int pingMs);

private:
    static TrafficMonitor *s_instance;
    quint64 m_downRate = 0;
    quint64 m_upRate = 0;
    quint64 m_sessionTotalBytes = 0;
    int m_currentPing = 0;
    bool m_isTestingPing = false;
    int m_pendingPingCount = 0;
    QElapsedTimer m_tickTimer;
};
