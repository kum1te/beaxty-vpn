// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    objectName: "settingsView"

    readonly property bool hasEngine: typeof throneEngine !== "undefined"
    readonly property bool hasIdentity: typeof deviceIdentity !== "undefined"
    readonly property bool hasLoc: typeof locManager !== "undefined"
    readonly property bool hasWebEngineSupport: (typeof hasWebEngine !== "undefined") ? Boolean(hasWebEngine) : false

    ScrollView {
        id: settingsScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Item {
            width: settingsScroll.width
            implicitHeight: contentCol.implicitHeight + 64

            ColumnLayout {
                id: contentCol
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                anchors.bottomMargin: 32
                width: Math.min(parent.width - 48, 920)
                spacing: 16

                Text {
                    text: qsTr("Настройки")
                    color: Theme.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                }

                // ==========================================
                // Сетевой туннель
                // ==========================================
                Text {
                    text: qsTr("СЕТЕВОЙ ТУННЕЛЬ")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 4
                }

                ToggleRow {
                    title: qsTr("TUN Режим (Виртуальный адаптер)")
                    badge: qsTr("РЕКОМЕНДУЕТСЯ")
                    description: qsTr("Создает системный сетевой интерфейс. Прямое туннелирование без ручной настройки SOCKS/HTTP прокси.")
                    checked: root.hasEngine && throneEngine.tunModeEnabled
                    onToggled: function(value) {
                        if (root.hasEngine) throneEngine.tunModeEnabled = value
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.hasEngine ? qsTr("Активно: %1").arg(throneEngine.networkStackLabel) : ""
                        color: Theme.textMuted
                        font.pixelSize: 11
                        font.family: Theme.fontMono
                        elide: Text.ElideRight
                    }
                }

                ToggleRow {
                    title: qsTr("Аварийная блокировка (Kill Switch)")
                    description: qsTr("Если VPN-туннель неожиданно оборвётся, соединение будет остановлено, а не переключено на незащищённый канал.")
                    checked: root.hasEngine && throneEngine.killSwitch
                    onToggled: function(value) {
                        if (root.hasEngine) throneEngine.killSwitch = value
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Работает на уровне приложения: туннель не будет заменён прямым подключением. Блокировка трафика на уровне firewall требует прав root и не выполняется.")
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.pixelSize: 10
                    }
                }

                ToggleRow {
                    title: qsTr("Автопереключение при обрыве (Failover)")
                    description: qsTr("Автоматически переподключаться к следующему доступному серверу с минимальным пингом при неожиданном разрыве связи.")
                    checked: root.hasEngine && throneEngine.failoverEnabled
                    onToggled: function(value) {
                        if (root.hasEngine) throneEngine.failoverEnabled = value
                    }
                }

                // ==========================================
                // DNS Сервер
                // ==========================================
                Text {
                    text: qsTr("DNS СЕРВЕР")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                Card {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 12

                        Text {
                            text: qsTr("DNS СЕРВЕР")
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Выберите DNS-резолвер для защищенных запросов или укажите свой собственный DoH/IP адрес:")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        // Presets
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: [
                                    { id: 0, label: "Cloudflare (1.1.1.1)" },
                                    { id: 1, label: "Google (8.8.8.8)" },
                                    { id: 2, label: "Quad9 (9.9.9.9)" },
                                    { id: 3, label: "OpenDNS" },
                                    { id: 4, label: qsTr("Пользовательский") }
                                ]

                                Rectangle {
                                    required property var modelData
                                    readonly property bool active: root.hasEngine && throneEngine.dnsPreset === modelData.id

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    radius: 6
                                    color: active ? Theme.accentWhite : Theme.cardHover
                                    border.color: active ? Theme.accentWhite : Theme.cardBorder
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: parent.active ? Theme.textInverted : Theme.textSecondary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (root.hasEngine) throneEngine.setDnsPreset(modelData.id);
                                        }
                                    }
                                }
                            }
                        }

                        // Custom DNS input (visible only when custom is selected)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            visible: root.hasEngine && throneEngine.dnsPreset === 4
                            radius: 8
                            color: Theme.cardHover
                            border.color: customDnsInput.activeFocus ? Theme.textSecondary : Theme.cardBorder
                            border.width: 1

                            TextInput {
                                id: customDnsInput
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: TextInput.AlignVCenter
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                font.family: Theme.fontMono
                                selectByMouse: true
                                text: root.hasEngine ? throneEngine.customDns : ""
                                onEditingFinished: {
                                    if (root.hasEngine) throneEngine.setCustomDns(text.trim());
                                }

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Пользовательский DNS (URL DoH или IP)...")
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                    visible: !customDnsInput.text && !customDnsInput.activeFocus
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // Интерфейс и поведение
                // ==========================================
                Text {
                    text: qsTr("ИНТЕРФЕЙС И ПОВЕДЕНИЕ")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                ToggleRow {
                    title: qsTr("Сворачивать в трей при закрытии")
                    description: qsTr("При закрытии окна приложение останется активным в области уведомлений.")
                    checked: root.hasEngine && throneEngine.closeToTray
                    onToggled: function(value) {
                        if (root.hasEngine) throneEngine.closeToTray = value;
                    }
                }

                // Theme Mode Selector
                Card {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 10

                        Text {
                            text: qsTr("Тема оформления")
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: [
                                    { id: 0, label: qsTr("Темная") },
                                    { id: 1, label: qsTr("Светлая") },
                                    { id: 2, label: qsTr("Системная") }
                                ]

                                Rectangle {
                                    required property var modelData
                                    readonly property bool active: Theme.themeMode === modelData.id

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    radius: 6
                                    color: active ? Theme.accentWhite : Theme.cardHover
                                    border.color: active ? Theme.accentWhite : Theme.cardBorder
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: parent.active ? Theme.textInverted : Theme.textSecondary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Theme.setThemeMode(modelData.id)
                                    }
                                }
                            }
                        }
                    }
                }

                // Language Selector
                Card {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 10

                        Text {
                            text: qsTr("Язык интерфейса")
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: [
                                    { code: "ru", label: "Русский" },
                                    { code: "en", label: "English" }
                                ]

                                Rectangle {
                                    required property var modelData
                                    readonly property bool active: root.hasLoc && locManager.language === modelData.code

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    radius: 6
                                    color: active ? Theme.accentWhite : Theme.cardHover
                                    border.color: active ? Theme.accentWhite : Theme.cardBorder
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: parent.active ? Theme.textInverted : Theme.textSecondary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (root.hasLoc) locManager.setLanguage(modelData.code);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // Личный кабинет
                // ==========================================
                Text {
                    text: qsTr("ЛИЧНЫЙ КАБИНЕТ")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                ToggleRow {
                    title: qsTr("Открывать кабинет во внешнем браузере")
                    description: !root.hasWebEngineSupport ?
                                 qsTr("В текущей сборке встроенный веб-движок не установлен. Кабинет всегда открывается в системном браузере.") :
                                 qsTr("При переходе во вкладку «Кабинет» отображать карточку для перехода в системный браузер вместо встроенного веб-движка.")
                    checked: !root.hasWebEngineSupport || ((typeof appPrefs !== "undefined") ? appPrefs.getBool("cabinet_external_browser", false) : false)
                    switchEnabled: root.hasWebEngineSupport
                    opacity: root.hasWebEngineSupport ? 1.0 : 0.55
                    onToggled: function(value) {
                        if (typeof appPrefs !== "undefined" && root.hasWebEngineSupport) {
                            appPrefs.setBool("cabinet_external_browser", value);
                        }
                    }
                }

                ToggleRow {
                    title: qsTr("Режим экономии памяти для кабинета")
                    badge: qsTr("РЕКОМЕНДУЕТСЯ")
                    description: qsTr("Автоматически выгружать веб-движок Chromium при неактивности или сворачивании приложения для полного освобождения RAM.")
                    checked: (typeof appPrefs !== "undefined") ? appPrefs.getBool("cabinet_memory_saver", true) : true
                    switchEnabled: root.hasWebEngineSupport
                    opacity: root.hasWebEngineSupport ? 1.0 : 0.55
                    onToggled: function(value) {
                        if (typeof appPrefs !== "undefined" && root.hasWebEngineSupport) {
                            appPrefs.setBool("cabinet_memory_saver", value);
                        }
                    }
                }

                // ==========================================
                // Безопасность и идентификация
                // ==========================================
                Text {
                    text: qsTr("БЕЗОПАСНОСТЬ И ИДЕНТИФИКАЦИЯ")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                ToggleRow {
                    title: qsTr("Передача идентификатора устройства (HWID)")
                    description: qsTr("Передает анонимный хэш конфигурации устройства для авторизации подписки.")
                    checked: root.hasIdentity && deviceIdentity.hwidEnabled
                    onToggled: function(value) {
                        if (root.hasIdentity) deviceIdentity.hwidEnabled = value
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: hwidText.implicitWidth + 16
                            radius: 6
                            color: Theme.cardHover
                            border.color: Theme.cardBorder
                            border.width: 1

                            Text {
                                id: hwidText
                                anchors.centerIn: parent
                                text: "ID: " + (root.hasIdentity ? deviceIdentity.sanitizedHwid : "—")
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.family: Theme.fontMono
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: copyRow.implicitWidth + 16
                            radius: 6
                            color: copyMouse.containsMouse ? Theme.controlBgHover : Theme.cardBg
                            border.color: copyMouse.containsMouse ? Theme.textSecondary : Theme.cardBorder
                            border.width: 1
                            scale: copyMouse.pressed ? 0.96 : 1.0

                            Behavior on scale { NumberAnimation { duration: 100 } }
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Copy HWID")

                            Row {
                                id: copyRow
                                anchors.centerIn: parent
                                spacing: 6

                                Image {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 12
                                    height: 12
                                    sourceSize.width: 12
                                    sourceSize.height: 12
                                    source: Theme.icon("qrc:/icons/clipboard.svg", Theme.isDark)
                                    opacity: 0.8
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("Копировать")
                                    color: Theme.textPrimary
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            MouseArea {
                                id: copyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!root.hasIdentity) return;
                                    deviceIdentity.copyHwidToClipboard();
                                    if (typeof toastManager !== "undefined") {
                                        toastManager.showSuccess(qsTr("HWID скопирован в буфер обмена"));
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }

                // ==========================================
                // Автоматизация
                // ==========================================
                Text {
                    text: qsTr("АВТОМАТИЗАЦИЯ")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                ToggleRow {
                    title: qsTr("Автоподключение при старте")
                    description: qsTr("Автоматически подключаться к последнему выбранному серверу при запуске приложения.")
                    checked: root.hasEngine && throneEngine.autoConnect
                    onToggled: function(value) {
                        if (root.hasEngine) throneEngine.autoConnect = value
                    }
                }

                ToggleRow {
                    title: qsTr("Запускать при старте системы")
                    description: qsTr("Автоматически запускать beaxty VPN при входе в операционную систему.")
                    checked: typeof autostartManager !== "undefined" && autostartManager.autostartEnabled
                    onToggled: function(value) {
                        if (typeof autostartManager !== "undefined") autostartManager.autostartEnabled = value
                    }
                }

                Card {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 10

                        Text {
                            text: qsTr("Автообновление подписок")
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Режим периодического обновления списков серверов и данных профиля:")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: [
                                    { id: 0, label: qsTr("Никогда") },
                                    { id: 1, label: qsTr("При запуске") },
                                    { id: 2, label: qsTr("По расписанию") }
                                ]

                                Rectangle {
                                    required property var modelData
                                    readonly property bool active: typeof configAdapter !== "undefined" && configAdapter.autoUpdateSubsMode === modelData.id

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    radius: 6
                                    color: active ? Theme.accentWhite : Theme.cardHover
                                    border.color: active ? Theme.accentWhite : Theme.cardBorder
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: parent.active ? Theme.textInverted : Theme.textSecondary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof configAdapter !== "undefined") {
                                                configAdapter.autoUpdateSubsMode = modelData.id
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // Права доступа
                // ==========================================
                Text {
                    text: qsTr("ПРАВА ДОСТУПА")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                Card {
                    Layout.fillWidth: true

                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: qsTr("Права на создание TUN-интерфейса")
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("TUN-режим требует cap_net_admin для сетевого демона. Приложение запросит права через Polkit — сам интерфейс не работает от root.")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }

                        ActionButton {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Настроить")
                            onClicked: if (root.hasEngine) throneEngine.requestElevateCapabilities()
                        }
                    }
                }

                // ==========================================
                // Поддержка и диагностика
                // ==========================================
                Text {
                    text: qsTr("ПОДДЕРЖКА И ДИАГНОСТИКА")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                Card {
                    Layout.fillWidth: true

                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: qsTr("Сохранить отчет для поддержки")
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Экспорт обезличенного диагностического лога, сетевых маршрутов и состояния системы для службы поддержки.")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }

                        ActionButton {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Сохранить отчет")
                            onClicked: if (root.hasEngine) throneEngine.exportSupportReport()
                        }
                    }
                }

                // ==========================================
                // О приложении
                // ==========================================
                Text {
                    text: qsTr("О ПРИЛОЖЕНИИ")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                    Layout.topMargin: 8
                }

                Card {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 40

                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: "beaxty VPN v1.0.10"
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Minimalist Desktop VPN • Powered by Throne, sing-box & Xray-core • GPL-3.0")
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: 10
                            color: Theme.isDark ? Theme.cardBg : Theme.cardHover
                            border.color: Theme.cardBorder
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                width: 22
                                height: 22
                                sourceSize.width: 22
                                sourceSize.height: 22
                                source: Theme.icon("qrc:/icons/app_icon.svg", Theme.isDark)
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                    }
                }

                Item {
                    Layout.preferredHeight: 40
                }
            }
        }
    }
}
