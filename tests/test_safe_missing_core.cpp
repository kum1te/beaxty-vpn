// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <iostream>

#include "src/core/ThroneEngine.hpp"
#include "3rdparty/throne/include/api/RPC.h"

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    QTemporaryDir tempDir(QStringLiteral("beaxty-missing-core-XXXXXX"));
    if (!tempDir.isValid()) {
        std::cerr << "Could not create isolated test directory\n";
        return 1;
    }

    ThroneEngine engine;
    const QString missingCore = tempDir.filePath(QStringLiteral("beaxty-core-does-not-exist"));
    if (!engine.initialize(tempDir.filePath(QStringLiteral("throne.db")), missingCore)) {
        std::cerr << "Engine initialization failed\n";
        return 1;
    }

    bool errorReported = false;
    QObject::connect(&engine, &ThroneEngine::errorOccurred, &app,
                     [&errorReported](const QString &) { errorReported = true; });

    QElapsedTimer timer;
    timer.start();
    engine.startConnection();
    const qint64 elapsedMs = timer.elapsed();

    const bool passed = engine.state() == ThroneEngine::Disconnected &&
                        errorReported && elapsedMs < 1000;
    engine.cleanup();
    delete API::defaultClient;
    API::defaultClient = nullptr;

    if (!passed) {
        std::cerr << "Missing-core failure was not reported promptly (" << elapsedMs << " ms)\n";
        return 1;
    }

    std::cout << "Safe missing-core test passed in " << elapsedMs
              << " ms; no daemon or tunnel was available to start.\n";
    return 0;
}
