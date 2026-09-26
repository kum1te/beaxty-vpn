// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QLocalServer>
#include <QTimer>
#include <QThread>
#include <QThreadPool>
#include <QSet>
#include <QVariantList>
#include <QList>
#include <atomic>

#include "FailoverPolicy.hpp"

class ThroneEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateString READ stateString NOTIFY stateChanged)
    Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY stateChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY stateChanged)
    Q_PROPERTY(bool tunModeEnabled READ isTunModeEnabled WRITE setTunModeEnabled NOTIFY tunModeChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY autoConnectChanged)
    Q_PROPERTY(bool killSwitch READ killSwitch WRITE setKillSwitch NOTIFY killSwitchChanged)
    Q_PROPERTY(bool failoverEnabled READ failoverEnabled WRITE setFailoverEnabled NOTIFY failoverEnabledChanged)
    Q_PROPERTY(QVariantList failoverServerIds READ failoverServerIds NOTIFY failoverServerIdsChanged)
    Q_PROPERTY(int failoverStrategy READ failoverStrategy WRITE setFailoverStrategy NOTIFY failoverStrategyChanged)
    Q_PROPERTY(int activeServerId READ activeServerId NOTIFY activeServerChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString localizedStatusMessage READ localizedStatusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString connectionModeLabel READ connectionModeLabel NOTIFY tunModeChanged)
    // Read straight out of the live settings so the dashboard cannot advertise
    // protection the running configuration does not have.
    Q_PROPERTY(QString networkStackLabel READ networkStackLabel NOTIFY tunModeChanged)
    Q_PROPERTY(QString remoteDnsLabel READ remoteDnsLabel NOTIFY remoteDnsChanged)
    Q_PROPERTY(QString routingIsolationLabel READ routingIsolationLabel NOTIFY tunModeChanged)
    Q_PROPERTY(QString remoteDns READ remoteDns WRITE setRemoteDns NOTIFY remoteDnsChanged)
    Q_PROPERTY(int dnsPreset READ dnsPreset WRITE setDnsPreset NOTIFY dnsPresetChanged)
    Q_PROPERTY(QString customDns READ customDns WRITE setCustomDns NOTIFY customDnsChanged)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY closeToTrayChanged)

public:
    enum State {
        Disconnected = 0,
        Connecting = 1,
        Protected = 2
    };
    Q_ENUM(State)

    explicit ThroneEngine(QObject *parent = nullptr);
    ~ThroneEngine() override;
    static ThroneEngine *instance();

    int state() const;
    QString stateString() const;
    QString stateLabel() const;
    bool isConnected() const;
    bool isTunModeEnabled() const;
    void setTunModeEnabled(bool enabled);
    bool autoConnect() const;
    void setAutoConnect(bool enabled);
    bool killSwitch() const;
    void setKillSwitch(bool enabled);
    bool failoverEnabled() const;
    void setFailoverEnabled(bool enabled);
    QVariantList failoverServerIds() const;
    int failoverStrategy() const;
    void setFailoverStrategy(int strategy);
    int activeServerId() const;
    QString statusMessage() const;
    QString localizedStatusMessage() const;
    QString connectionModeLabel() const;
    QString networkStackLabel() const;
    QString remoteDnsLabel() const;
    QString routingIsolationLabel() const;

    QString remoteDns() const;
    void setRemoteDns(const QString &dns);
    int dnsPreset() const;
    void setDnsPreset(int preset);
    QString customDns() const;
    void setCustomDns(const QString &dns);
    bool closeToTray() const;
    void setCloseToTray(bool enabled);

    Q_INVOKABLE void exportSupportReport();
    Q_INVOKABLE bool isFailoverServer(int profileId) const;
    Q_INVOKABLE void setFailoverServer(int profileId, bool enabled);
    Q_INVOKABLE void moveFailoverServer(int profileId, int offset);

    Q_INVOKABLE void toggleConnect();
    Q_INVOKABLE void startConnection();
    Q_INVOKABLE void stopConnection();
    Q_INVOKABLE void restartConnection();
    Q_INVOKABLE void requestElevateCapabilities();
    Q_INVOKABLE void connectAfterTunPermissionConsent();
    // Hooked up in main(); QML calls these so the window/tray policy stays in one place.
    Q_INVOKABLE void requestQuit();
    Q_INVOKABLE void notifyMinimizedToTray();

    bool initialize(const QString &dbPath = QString(), const QString &coreBinaryPath = QString());
    void cleanup();
    void setStateForTesting(int state);

    // Called once from main() after the QML engine is up: honours "auto-connect on launch".
    void runPostStartupTasks();

    void onCoreExited(int exitCode);
    void onProfileStopped();
    bool triggerFailover();

signals:
    void stateChanged(int state);
    void tunModeChanged(bool enabled);
    void autoConnectChanged(bool enabled);
    void killSwitchChanged(bool enabled);
    void failoverEnabledChanged(bool enabled);
    void failoverServerIdsChanged();
    void failoverStrategyChanged(int strategy);
    void activeServerChanged(int profileId);
    void statusMessageChanged(const QString &msg);
    void errorOccurred(const QString &error);
    void killSwitchTripped();
    void quitRequested();
    void minimizedToTray();
    void remoteDnsChanged(const QString &dns);
    void dnsPresetChanged(int preset);
    void customDnsChanged(const QString &dns);
    void closeToTrayChanged(bool enabled);
    void tunPermissionConsentRequested();

private:
    void setState(State s);
    void doStartConnection();
    bool spawnCoreDaemon();
    bool hasVerifiedTunCore() const;
    bool connectToCoreRpc();
    void stopTrafficLooper();
    void persistSettings();
    void applyKillSwitch(bool engaged);
    bool hasEligibleFallbackServers() const;
    void configureFailoverMonitoring();
    void probeActiveRoute();
    void handleRouteProbeResult(bool reachable, int latencyMs, uint64_t generation);
    void tryNextFailoverCandidate();
    void finishFailoverFailure(const QString &error);
    void beginFailoverRestart();

    static ThroneEngine *s_instance;
    State m_state = Disconnected;
    QString m_bundledCoreBinaryPath;
    QString m_coreBinaryPath;
    QString m_socketPath;
    QString m_socketFullPath;
    QLocalServer *m_localServer = nullptr;
    QProcess *m_coreProcess = nullptr;
    QProcess *m_capabilitySetupProcess = nullptr;
    // Serialize state-changing RPCs. In particular, a late Stop from an old
    // connection generation must not tear down a newer Start.
    QThreadPool m_workerPool;
    // Throne's RPC client takes ownership of accepted sockets and moves them to
    // its I/O thread. Keep only a cross-thread-safe connection flag here.
    std::atomic_bool m_rpcConnected{false};
    uint64_t m_rpcGeneration = 0;
    QThread *m_trafficThread = nullptr;
    QString m_statusMessage = QStringLiteral("Ready");
    bool m_autoConnect = false;
    bool m_killSwitch = false;
    bool m_killSwitchEngaged = false;
    bool m_failoverEnabled = false;
    int m_failoverStrategy = 0;
    int m_activeServerId = -1;
    int m_activeServerOverride = -1;
    bool m_failoverInProgress = false;
    QList<int> m_failoverServerIds;
    QList<int> m_failoverCandidateIds;
    int m_failoverCandidateIndex = 0;
    QTimer m_routeHealthTimer;
    QTimer m_routeConfirmTimer;
    QTimer m_failoverRetryTimer;
    int m_failoverRetryDelayMs = 30000;
    std::atomic_bool m_routeProbeInFlight{false};
    FailoverPolicy::FailureDetector m_routeFailureDetector;
    bool m_capabilitySetupInProgress = false;
    bool m_unsafeBundledCoreRejected = false;
    bool m_legacySystemCoreDetected = false;
    QSet<int> m_failedServerIds;
    bool m_initialized = false;
    bool m_cleanedUp = false;
    // Set while a stop is a deliberate user action, so onCoreExited() can tell a
    // crash apart from an orderly shutdown and only then trip the kill switch.
    std::atomic_bool m_intentionalStop{false};
    std::atomic_bool m_userWantsConnect{false};
    std::atomic<uint64_t> m_connectSeq{0};
};
