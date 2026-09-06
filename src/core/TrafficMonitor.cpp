// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "TrafficMonitor.hpp"
#include "ConfigAdapter.hpp"
#include "3rdparty/throne/include/global/Configs.hpp"
#include "3rdparty/throne/include/database/DatabaseManager.h"
#include "3rdparty/throne/include/database/ProfilesRepo.h"

#include "3rdparty/throne/include/api/RPC.h"
#include "3rdparty/throne/include/configs/generate.h"
#include "3rdparty/throne/include/database/SettingsRepo.h"
#include "3rdparty/throne/include/global/Utils.hpp"

#include <QTcpSocket>
#include <QElapsedTimer>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QThreadPool>
#include <QPointer>
#include <QCoreApplication>
#include <memory>

TrafficMonitor *TrafficMonitor::s_instance = nullptr;

TrafficMonitor::TrafficMonitor(QObject *parent) : QObject(parent) {
    s_instance = this;
}

TrafficMonitor *TrafficMonitor::instance() {
    return s_instance;
}

QString TrafficMonitor::downloadSpeed() const {
    return formatSpeed(m_downRate);
}

QString TrafficMonitor::uploadSpeed() const {
    return formatSpeed(m_upRate);
}

QString TrafficMonitor::totalTraffic() const {
    return formatBytes(m_sessionTotalBytes);
}

int TrafficMonitor::currentPing() const {
    return m_currentPing;
}

bool TrafficMonitor::isTestingPing() const {
    return m_isTestingPing;
}

void TrafficMonitor::updateTraffic(int proxyDl, int proxyUp, int directDl, int directUp) {
    m_downRate = static_cast<quint64>(qMax(0, proxyDl) + qMax(0, directDl));
    m_upRate = static_cast<quint64>(qMax(0, proxyUp) + qMax(0, directUp));

    // The looper hands us bytes-per-tick, so the session total is the integral of
    // the rate over the real elapsed time rather than a bare sum (which silently
    // assumed an exactly 1 Hz tick).
    if (m_tickTimer.isValid()) {
        double seconds = static_cast<double>(m_tickTimer.restart()) / 1000.0;
        // Clamp against a stalled or resumed-from-suspend looper.
        seconds = qBound(0.0, seconds, 5.0);
        m_sessionTotalBytes += static_cast<quint64>((m_downRate + m_upRate) * seconds);
    } else {
        m_tickTimer.start();
    }

    emit speedUpdated();
    emit trafficUpdated();
}

void TrafficMonitor::setCurrentPing(int pingMs) {
    if (m_currentPing != pingMs) {
        m_currentPing = pingMs;
        emit pingUpdated(pingMs);
    }
}

void TrafficMonitor::resetSessionStats() {
    m_downRate = 0;
    m_upRate = 0;
    m_sessionTotalBytes = 0;
    m_tickTimer.invalidate();
    emit speedUpdated();
    emit trafficUpdated();
}

QString TrafficMonitor::formatBytes(quint64 bytes) {
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

QString TrafficMonitor::formatSpeed(quint64 bytesPerSec) {
    if (bytesPerSec < 1024) {
        return QString("%1 B/s").arg(bytesPerSec);
    } else if (bytesPerSec < 1024 * 1024) {
        return QString("%1 KB/s").arg(bytesPerSec / 1024.0, 0, 'f', 1);
    } else {
        return QString("%1 MB/s").arg(bytesPerSec / (1024.0 * 1024.0), 0, 'f', 2);
    }
}

void TrafficMonitor::applyPingResult(int profileId, int pingMs) {
    if (auto *cfg = ConfigAdapter::instance()) {
        cfg->updateServerPing(profileId, pingMs);
        if (profileId == cfg->selectedServerId()) {
            setCurrentPing(pingMs);
        }
    }

    if (Configs::dataManager && Configs::dataManager->profilesRepo) {
        if (auto p = Configs::dataManager->profilesRepo->GetProfile(profileId)) {
            p->SetLatency(pingMs);
            Configs::dataManager->profilesRepo->Save(p);
        }
    }
    emit serverPingUpdated(profileId, pingMs);

    if (m_pendingPingCount > 0 && --m_pendingPingCount == 0 && m_isTestingPing) {
        m_isTestingPing = false;
        emit pingTestingChanged(false);
    }
}

static int probeSingleProfileLatency(int profileId) {
    if (!Configs::dataManager || !Configs::dataManager->profilesRepo) return -1;
    auto prof = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (!prof || !prof->outbound) return -1;

    if (!API::defaultClient) return -1;

    auto buildObject = Configs::BuildTestConfig({prof});
    if (!buildObject || !buildObject->error.isEmpty()) {
        return -1;
    }

    QString testUrl = (Configs::dataManager->settingsRepo && !Configs::dataManager->settingsRepo->test_latency_url.isEmpty())
                      ? Configs::dataManager->settingsRepo->test_latency_url
                      : QStringLiteral("http://cp.cloudflare.com/generate_204");

    if (!buildObject->outboundTags.empty()) {
        libcore::TestReq req;
        for (const auto& tag : buildObject->outboundTags) req.outbound_tags.push_back(tag.toStdString());
        req.config = QJsonObject2QString(buildObject->coreConfig, false).toStdString();
        req.use_default_outbound = false;
        req.xray_config = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, false).toStdString() : "";
        req.need_xray = buildObject->isXrayNeeded;
        req.xray_outbound_dns_strategy = buildObject->xrayDnsStrategy.toStdString();
        for (const auto& xc : buildObject->xrayFullConfigs) req.xray_full_configs.push_back(xc.toStdString());

        req.url = testUrl.toStdString();
        req.max_concurrency = 1;
        req.test_timeout_ms = 3500;

        bool rpcOK = false;
        QString coreError;
        auto resp = API::defaultClient->Test(&rpcOK, req, &coreError);
        if (rpcOK && !resp.results.empty()) {
            for (const auto &r : resp.results) {
                if (r.error.value().empty() && r.latency_ms.value() > 0) {
                    return r.latency_ms.value();
                }
            }
        }
        return -1;
    }

    if (!buildObject->fullConfigs.empty()) {
        for (auto it = buildObject->fullConfigs.cbegin(); it != buildObject->fullConfigs.cend(); ++it) {
            libcore::TestReq req;
            req.config = it.value().toStdString();
            req.use_default_outbound = true;
            req.url = testUrl.toStdString();
            req.test_timeout_ms = 3500;
            bool rpcOK = false;
            QString coreError;
            auto resp = API::defaultClient->Test(&rpcOK, req, &coreError);
            if (rpcOK && !resp.results.empty() && resp.results[0].error.value().empty() && resp.results[0].latency_ms.value() > 0) {
                return resp.results[0].latency_ms.value();
            }
        }
    }

    return -1;
}

static QMap<int, int> probeBatchProfilesLatency(const QList<std::shared_ptr<Configs::Profile>> &profiles) {
    QMap<int, int> results;
    for (const auto &p : profiles) {
        if (p) results[p->id] = -1;
    }
    if (profiles.isEmpty() || !API::defaultClient) return results;

    auto buildObject = Configs::BuildTestConfig(profiles);
    if (!buildObject || !buildObject->error.isEmpty()) return results;

    QString testUrl = (Configs::dataManager->settingsRepo && !Configs::dataManager->settingsRepo->test_latency_url.isEmpty())
                      ? Configs::dataManager->settingsRepo->test_latency_url
                      : QStringLiteral("http://cp.cloudflare.com/generate_204");

    if (!buildObject->outboundTags.empty()) {
        libcore::TestReq req;
        for (const auto& tag : buildObject->outboundTags) req.outbound_tags.push_back(tag.toStdString());
        req.config = QJsonObject2QString(buildObject->coreConfig, false).toStdString();
        req.use_default_outbound = false;
        req.xray_config = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, false).toStdString() : "";
        req.need_xray = buildObject->isXrayNeeded;
        req.xray_outbound_dns_strategy = buildObject->xrayDnsStrategy.toStdString();
        for (const auto& xc : buildObject->xrayFullConfigs) req.xray_full_configs.push_back(xc.toStdString());

        req.url = testUrl.toStdString();
        req.max_concurrency = 5;
        req.test_timeout_ms = 4000;

        bool rpcOK = false;
        QString coreError;
        auto resp = API::defaultClient->Test(&rpcOK, req, &coreError);
        if (rpcOK) {
            for (const auto &r : resp.results) {
                QString tag = QString::fromStdString(r.outbound_tag.value());
                int entId = buildObject->tag2entID.value(tag, -1);
                if (entId > 0) {
                    if (r.error.value().empty() && r.latency_ms.value() > 0) {
                        results[entId] = r.latency_ms.value();
                    } else {
                        results[entId] = -1;
                    }
                }
            }
        }
    }

    if (!buildObject->fullConfigs.empty()) {
        for (auto it = buildObject->fullConfigs.cbegin(); it != buildObject->fullConfigs.cend(); ++it) {
            int entId = it.key();
            libcore::TestReq req;
            req.config = it.value().toStdString();
            req.use_default_outbound = true;
            req.url = testUrl.toStdString();
            req.test_timeout_ms = 4000;
            bool rpcOK = false;
            QString coreError;
            auto resp = API::defaultClient->Test(&rpcOK, req, &coreError);
            if (rpcOK && !resp.results.empty() && resp.results[0].error.value().empty() && resp.results[0].latency_ms.value() > 0) {
                results[entId] = resp.results[0].latency_ms.value();
            } else {
                results[entId] = -1;
            }
        }
    }

    return results;
}

void TrafficMonitor::testServerPing(int profileId) {
    if (!Configs::dataManager || !Configs::dataManager->profilesRepo) return;
    auto prof = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (!prof || !prof->outbound) return;

    m_pendingPingCount++;
    if (!m_isTestingPing) {
        m_isTestingPing = true;
        emit pingTestingChanged(true);
    }

    QPointer<TrafficMonitor> self(this);
    QThreadPool::globalInstance()->start([self, profileId]() {
        int pingMs = probeSingleProfileLatency(profileId);
        QMetaObject::invokeMethod(qApp, [self, profileId, pingMs]() {
            if (self) {
                self->applyPingResult(profileId, pingMs);
            }
        });
    });
}

void TrafficMonitor::testAllPings() {
    if (m_isTestingPing) return;

    QList<int> ids;
    if (Configs::dataManager && Configs::dataManager->profilesRepo) {
        ids = Configs::dataManager->profilesRepo->GetAllProfileIds();
    }
    if (ids.isEmpty()) return;

    m_isTestingPing = true;
    m_pendingPingCount = ids.size();
    emit pingTestingChanged(true);

    QPointer<TrafficMonitor> self(this);
    QThreadPool::globalInstance()->start([self, ids]() {
        constexpr int kBatchSize = 10;
        for (int i = 0; i < ids.size(); i += kBatchSize) {
            QList<int> batchIds = ids.mid(i, kBatchSize);
            QList<std::shared_ptr<Configs::Profile>> batchProfiles;
            if (Configs::dataManager && Configs::dataManager->profilesRepo) {
                batchProfiles = Configs::dataManager->profilesRepo->GetProfileBatch(batchIds);
            }
            auto results = probeBatchProfilesLatency(batchProfiles);
            QMetaObject::invokeMethod(qApp, [self, results]() {
                if (self) {
                    for (auto it = results.cbegin(); it != results.cend(); ++it) {
                        self->applyPingResult(it.key(), it.value());
                    }
                }
            });
        }
    });

    // Safety timeout
    QTimer::singleShot(10000, this, [this]() {
        if (m_isTestingPing) {
            m_pendingPingCount = 0;
            m_isTestingPing = false;
            emit pingTestingChanged(false);
        }
    });
}
