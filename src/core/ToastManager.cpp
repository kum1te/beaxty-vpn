// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

#include "ToastManager.hpp"
#include <QDebug>

ToastManager *ToastManager::s_instance = nullptr;

ToastManager::ToastManager(QObject *parent) : QObject(parent) {
    s_instance = this;
}

ToastManager::~ToastManager() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ToastManager *ToastManager::instance() {
    return s_instance;
}

void ToastManager::show(const QString &message, const QString &type, int durationMs) {
    qDebug().noquote() << QStringLiteral("[Toast %1]").arg(type.toUpper()) << message;
    emit toastRequested(message, type, durationMs);
}

void ToastManager::showError(const QString &message, int durationMs) {
    show(message, QStringLiteral("error"), durationMs);
}

void ToastManager::showSuccess(const QString &message, int durationMs) {
    show(message, QStringLiteral("success"), durationMs);
}

void ToastManager::showInfo(const QString &message, int durationMs) {
    show(message, QStringLiteral("info"), durationMs);
}
