// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#pragma once

#include <QList>
#include <QMap>
#include <QSet>
#include <algorithm>

namespace FailoverPolicy {

enum class Strategy : int {
    LowestLatency = 0,
    ConfiguredOrder = 1
};

class FailureDetector {
public:
    static constexpr int RequiredConsecutiveFailures = 2;

    bool recordProbe(bool reachable) {
        if (reachable) {
            m_consecutiveFailures = 0;
            return false;
        }
        return ++m_consecutiveFailures >= RequiredConsecutiveFailures;
    }

    void reset() { m_consecutiveFailures = 0; }
    int consecutiveFailures() const { return m_consecutiveFailures; }

private:
    int m_consecutiveFailures = 0;
};

inline QList<int> orderCandidates(const QList<int> &configuredOrder,
                                 const QSet<int> &available,
                                 const QMap<int, int> &latencies,
                                 const QSet<int> &excluded,
                                 Strategy strategy) {
    QList<int> ordered;
    QSet<int> seen;
    for (int id : configuredOrder) {
        if (id < 0 || seen.contains(id) || excluded.contains(id) || !available.contains(id)) continue;
        seen.insert(id);
        ordered.append(id);
    }

    if (strategy == Strategy::LowestLatency) {
        std::stable_sort(ordered.begin(), ordered.end(), [&latencies](int left, int right) {
            const int leftPing = latencies.value(left, 0);
            const int rightPing = latencies.value(right, 0);
            const bool leftMeasured = leftPing > 0 && leftPing < 999;
            const bool rightMeasured = rightPing > 0 && rightPing < 999;
            if (leftMeasured != rightMeasured) return leftMeasured;
            if (leftMeasured && leftPing != rightPing) return leftPing < rightPing;
            return false; // stable_sort preserves the user's pool order for ties/unknown pings.
        });
    }

    return ordered;
}

} // namespace FailoverPolicy
