#include "src/core/FailoverPolicy.hpp"

#include <cassert>

int main() {
    FailoverPolicy::FailureDetector detector;
    assert(!detector.recordProbe(false));
    assert(detector.consecutiveFailures() == 1);
    assert(!detector.recordProbe(true));
    assert(detector.consecutiveFailures() == 0);
    assert(!detector.recordProbe(false));
    assert(detector.recordProbe(false));

    const QList<int> pool{10, 30, 20, 20, -1};
    const QSet<int> available{10, 20, 30, 40};
    const QMap<int, int> pings{{10, 85}, {20, 28}, {30, 999}};
    const QSet<int> excluded{40, 10};

    const auto byLatency = FailoverPolicy::orderCandidates(
        pool, available, pings, excluded, FailoverPolicy::Strategy::LowestLatency);
    assert((byLatency == QList<int>{20, 30}));

    const auto byOrder = FailoverPolicy::orderCandidates(
        pool, available, pings, excluded, FailoverPolicy::Strategy::ConfiguredOrder);
    assert((byOrder == QList<int>{30, 20}));
}
