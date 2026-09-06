// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QRegularExpression>
#include "src/core/DeviceIdentity.hpp"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "[TEST] Starting HWID Determinism & Validation Test..." << std::endl;

    QString hwid1 = DeviceIdentity::computeSha256Hwid();
    QString hwid2 = DeviceIdentity::computeSha256Hwid();

    std::cout << "  Calculated HWID: " << hwid1.toStdString() << std::endl;

    // 1. Check length
    if (hwid1.length() != 64) {
        std::cerr << "FAIL: HWID length is not 64 hex characters (got " << hwid1.length() << ")" << std::endl;
        return 1;
    }

    // 2. Check format (hexadecimal)
    QRegularExpression hexRe("^[0-9a-f]{64}$");
    if (!hexRe.match(hwid1).hasMatch()) {
        std::cerr << "FAIL: HWID is not a valid 64-character lowercase hexadecimal hash" << std::endl;
        return 1;
    }

    // 3. Check stability / determinism across multiple calls
    if (hwid1 != hwid2) {
        std::cerr << "FAIL: HWID is not deterministic across multiple calculations!" << std::endl;
        return 1;
    }

    // 4. Test instance properties
    DeviceIdentity identity;
    if (!identity.isHwidEnabled()) {
        std::cerr << "FAIL: HWID must be enabled by default according to specifications" << std::endl;
        return 1;
    }

    QString sanitized = identity.sanitizedHwid();
    std::cout << "  Sanitized HWID: " << sanitized.toStdString() << std::endl;
    if (!sanitized.contains("...")) {
        std::cerr << "FAIL: Sanitized HWID must contain ellipsis" << std::endl;
        return 1;
    }

    std::cout << "[PASS] All HWID tests passed successfully!" << std::endl;
    return 0;
}
