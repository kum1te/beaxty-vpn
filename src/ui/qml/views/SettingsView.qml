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
    readonly property bool hasConfigAdapter: typeof configAdapter !== "undefined"
    readonly property bool hasIdentity: typeof deviceIdentity !== "undefined"
    readonly property bool hasLoc: typeof locManager !== "undefined"
    readonly property bool hasWebEngineSupport: (typeof hasWebEngine !== "undefined") ? Boolean(hasWebEngine) : false
    property bool cabinetExternalBrowser: (typeof appPrefs !== "undefined") ? appPrefs.getBool("cabinet_external_browser", false) : false
    property bool cabinetMemorySaver: (typeof appPrefs !== "undefined") ? appPrefs.getBool("cabinet_memory_saver", true) : true

    Connections {
        target: (typeof appPrefs !== "undefined") ? appPrefs : null
        function onPrefChanged(key) {
            if (key === "cabinet_external_browser") {
                root.cabinetExternalBrowser = appPrefs.getBool(key, false);
            } else if (key === "cabinet_memory_saver") {
                root.cabinetMemorySaver = appPrefs.getBool(key, true);
            }
        }
    }

    function failoverPoolContains(id) {
        if (!root.hasEngine) return false
        var ids = throneEngine.failoverServerIds
        // Read the notifyable property so this binding re-evaluates when the
        // pool changes; the C++ method performs the integer ID comparison.
        return ids.length > 0 && throneEngine.isFailoverServer(Number(id))
    }

    function failoverPoolIndex(id) {
        if (!root.hasEngine) return -1
        var ids = throneEngine.failoverServerIds
        for (var i = 0; i < ids.length; ++i) {
            if (Number(ids[i]) === Number(id)) return i
        }
        return -1
    }

    function failoverServersInPool(query) {
        if (!root.hasEngine || !root.hasConfigAdapter) return []
        var servers = configAdapter.servers
        var ids = throneEngine.failoverServerIds
        var normalizedQuery = String(query || "").trim().toLocaleLowerCase()
        var byId = {}
        for (var i = 0; i < servers.length; ++i) {
            var server = servers[i]
            byId[Number(server.id)] = server
        }

        var result = []
        for (var j = 0; j < ids.length; ++j) {
            var item = byId[Number(ids[j])]
            if (!item) continue
            var searchable = [item.name, item.type, item.address, item.country].join(" ").toLocaleLowerCase()
            if (!normalizedQuery || searchable.indexOf(normalizedQuery) >= 0) result.push(item)
        }
        return result
    }

    function failoverServersAvailable(query) {
        if (!root.hasEngine || !root.hasConfigAdapter) return []
        var servers = configAdapter.servers
        var normalizedQuery = String(query || "").trim().toLocaleLowerCase()
        var result = []
        for (var i = 0; i < servers.length; ++i) {
            var item = servers[i]
            if (root.failoverPoolContains(item.id)) continue
            var searchable = [item.name, item.type, item.address, item.country].join(" ").toLocaleLowerCase()
            if (!normalizedQuery || searchable.indexOf(normalizedQuery) >= 0) result.push(item)
        }
        return result
    }

    function hasEligibleFailoverServer() {
        if (!root.hasEngine || !root.hasConfigAdapter) return false
        var currentId = (throneEngine.state === 2 && throneEngine.activeServerId >= 0)
                      ? throneEngine.activeServerId : configAdapter.selectedServerId
        var ids = throneEngine.failoverServerIds
        for (var i = 0; i < ids.length; ++i) {
            if (Number(ids[i]) !== Number(currentId)) return true
        }
        return false
    }

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
                    description: qsTr("При разрыве приложение запросит блокировку у работающего ядра. Системный firewall не устанавливается, поэтому блокировка всего трафика после сбоя ядра не гарантируется.")
                    checked: root.hasEngine && throneEngine.killSwitch
                    onToggled: function(value) {
                        if (root.hasEngine) throneEngine.killSwitch = value
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("После сбоя ядра системный firewall не включается, поэтому блокировка всего трафика не гарантируется.")
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.pixelSize: 10
                    }
                }

                ToggleRow {
                    objectName: "failoverToggleRow"
                    title: qsTr("Автоматическое переключение на резервный сервер")
                    description: qsTr("После двух неудачных проверок маршрута приложение попробует выбранные вами серверы. Смена IP может оборвать игру и другие активные соединения. Пока туннель переподключается, трафик может пойти через обычную сеть; Kill Switch приложения не гарантирует системную блокировку.")
                    checked: root.hasEngine && throneEngine.failoverEnabled
                    onToggled: function(value) {
                        if (root.hasEngine) throneEngine.failoverEnabled = value
                    }

                    ColumnLayout {
                        objectName: "failoverControls"
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.hasEngine && throneEngine.failoverEnabled

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("РЕЗЕРВНЫЙ ПУЛ · %1 выбрано").arg(root.hasEngine ? throneEngine.failoverServerIds.length : 0)
                            color: Theme.textMuted
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 0.8
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Текущий маршрут проверяется по нескольким адресам примерно раз в 3 секунды. Один сбой не запускает переключение.")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: !root.hasEligibleFailoverServer()
                            text: qsTr("Выберите хотя бы один резервный сервер, отличный от текущего.")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: [
                                    { label: qsTr("По последнему пингу"), value: 0 },
                                    { label: qsTr("По порядку"), value: 1 }
                                ]

                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    height: 34
                                    radius: 8
                                    color: (root.hasEngine && throneEngine.failoverStrategy === modelData.value)
                                           ? Theme.cardHover : Theme.bgDark
                                    border.color: (root.hasEngine && throneEngine.failoverStrategy === modelData.value)
                                                  ? Theme.textSecondary : Theme.cardBorder
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: Theme.textPrimary
                                        font.pixelSize: 11
                                        font.bold: root.hasEngine && throneEngine.failoverStrategy === modelData.value
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: if (root.hasEngine) throneEngine.failoverStrategy = modelData.value
                                    }
                                }
                            }
                        }

                        GridLayout {
                            id: fallbackLists
                            objectName: "fallbackLists"
                            Layout.fillWidth: true
                            columns: contentCol.width >= 620 ? 2 : 1
                            columnSpacing: 12
                            rowSpacing: 10
                            visible: root.hasConfigAdapter && configAdapter.serverCount > 0

                            Rectangle {
                                objectName: "fallbackAvailablePane"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 300
                                Layout.minimumWidth: 0
                                radius: 12
                                color: Theme.bgDark
                                border.color: Theme.cardBorder
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("Доступные серверы")
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                        Text {
                                            text: String(root.failoverServersAvailable(availableSearch.text).length)
                                            color: Theme.textMuted
                                            font.pixelSize: 10
                                            font.family: Theme.fontMono
                                        }
                                    }

                                    SearchField {
                                        id: availableSearch
                                        objectName: "fallbackAvailableSearch"
                                        Layout.fillWidth: true
                                        height: 34
                                        placeholder: qsTr("Поиск сервера")
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true

                                        ListView {
                                            id: availableFallbackList
                                            objectName: "availableFallbackList"
                                            anchors.fill: parent
                                            clip: true
                                            spacing: 5
                                            model: root.failoverServersAvailable(availableSearch.text)
                                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                            delegate: Rectangle {
                                                required property var modelData
                                                width: availableFallbackList.width
                                                height: 48
                                                radius: 8
                                                color: Theme.cardBg
                                                border.color: Theme.cardBorder
                                                border.width: 1

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 9
                                                    anchors.rightMargin: 8
                                                    spacing: 8

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 1
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: modelData.name || qsTr("Сервер")
                                                            color: Theme.textPrimary
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                            elide: Text.ElideRight
                                                        }
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: [modelData.type, modelData.address,
                                                                   modelData.ping > 0 && modelData.ping < 999 ? modelData.ping + " ms" : "—"]
                                                                  .filter(function(part) { return Boolean(part) }).join(" · ")
                                                            color: Theme.textMuted
                                                            font.pixelSize: 9
                                                            font.family: Theme.fontMono
                                                            elide: Text.ElideRight
                                                        }
                                                    }

                                                    ActionButton {
                                                        objectName: "fallbackAddButton"
                                                        text: qsTr("Добавить")
                                                        height: 30
                                                        enabled: root.hasEngine
                                                        onClicked: throneEngine.setFailoverServer(modelData.id, true)
                                                    }
                                                }
                                            }

                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            width: parent.width - 24
                                            text: availableSearch.text.length > 0
                                                  ? qsTr("Серверы не найдены")
                                                  : qsTr("Все серверы уже в резервном пуле")
                                            color: Theme.textMuted
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignHCenter
                                            wrapMode: Text.WordWrap
                                            visible: availableFallbackList.count === 0
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                objectName: "fallbackPoolPane"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 300
                                Layout.minimumWidth: 0
                                radius: 12
                                color: Theme.bgDark
                                border.color: Theme.cardBorder
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("В резервном пуле")
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                        Text {
                                            text: String(root.failoverServersInPool(poolSearch.text).length)
                                            color: Theme.textMuted
                                            font.pixelSize: 10
                                            font.family: Theme.fontMono
                                        }
                                    }

                                    SearchField {
                                        id: poolSearch
                                        objectName: "fallbackPoolSearch"
                                        Layout.fillWidth: true
                                        height: 34
                                        placeholder: qsTr("Поиск сервера")
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true

                                        ListView {
                                            id: selectedFallbackList
                                            objectName: "selectedFallbackList"
                                            anchors.fill: parent
                                            clip: true
                                            spacing: 5
                                            model: root.failoverServersInPool(poolSearch.text)
                                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                            delegate: Rectangle {
                                                required property var modelData
                                                readonly property int profileId: Number(modelData.id)
                                                readonly property int poolOrderIndex: root.failoverPoolIndex(profileId)
                                                width: selectedFallbackList.width
                                                height: 52
                                                radius: 8
                                                color: Theme.cardBg
                                                border.color: Theme.cardBorder
                                                border.width: 1

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 9
                                                    anchors.rightMargin: 8
                                                    spacing: 5

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 1
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: modelData.name || qsTr("Сервер")
                                                            color: Theme.textPrimary
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                            elide: Text.ElideRight
                                                        }
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: [modelData.type, modelData.address,
                                                                   modelData.ping > 0 && modelData.ping < 999 ? modelData.ping + " ms" : "—"]
                                                                  .filter(function(part) { return Boolean(part) }).join(" · ")
                                                            color: Theme.textMuted
                                                            font.pixelSize: 9
                                                            font.family: Theme.fontMono
                                                            elide: Text.ElideRight
                                                        }
                                                    }

                                                    Column {
                                                        spacing: 2
                                                        Repeater {
                                                            model: [
                                                                { glyph: "↑", label: qsTr("Переместить выше"), delta: -1 },
                                                                { glyph: "↓", label: qsTr("Переместить ниже"), delta: 1 }
                                                            ]
                                                            delegate: Rectangle {
                                                                required property var modelData
                                                                width: 26
                                                                height: 24
                                                                radius: 6
                                                                color: moveButtonMouse.containsMouse ? Theme.cardHover : Theme.bgDark
                                                                border.color: activeFocus ? Theme.textSecondary : Theme.cardBorder
                                                                opacity: moveButtonMouse.enabled ? 1 : 0.4
                                                                Accessible.role: Accessible.Button
                                                                Accessible.name: modelData.label
                                                                Accessible.onPressAction: {
                                                                    var targetIndex = poolOrderIndex + modelData.delta
                                                                    if (root.hasEngine && targetIndex >= 0 && targetIndex < throneEngine.failoverServerIds.length)
                                                                        throneEngine.moveFailoverServer(profileId, modelData.delta)
                                                                }
                                                                activeFocusOnTab: moveButtonMouse.enabled
                                                                Keys.onReturnPressed: function(event) {
                                                                    if (!moveButtonMouse.enabled) return
                                                                    throneEngine.moveFailoverServer(profileId, modelData.delta)
                                                                    event.accepted = true
                                                                }
                                                                Keys.onSpacePressed: function(event) {
                                                                    if (!moveButtonMouse.enabled) return
                                                                    throneEngine.moveFailoverServer(profileId, modelData.delta)
                                                                    event.accepted = true
                                                                }

                                                                Text {
                                                                    anchors.centerIn: parent
                                                                    text: modelData.glyph
                                                                    color: Theme.textPrimary
                                                                    font.pixelSize: 12
                                                                    font.bold: true
                                                                }
                                                                MouseArea {
                                                                    id: moveButtonMouse
                                                                    anchors.fill: parent
                                                                    hoverEnabled: true
                                                                    enabled: root.hasEngine &&
                                                                             poolOrderIndex + modelData.delta >= 0 &&
                                                                             poolOrderIndex + modelData.delta < throneEngine.failoverServerIds.length
                                                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                                    onClicked: throneEngine.moveFailoverServer(profileId, modelData.delta)
                                                                }
                                                            }
                                                        }
                                                    }

                                                    ActionButton {
                                                        objectName: "fallbackRemoveButton"
                                                        text: qsTr("Убрать")
                                                        height: 30
                                                        enabled: root.hasEngine
                                                        onClicked: throneEngine.setFailoverServer(modelData.id, false)
                                                    }
                                                }
                                            }

                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            width: parent.width - 24
                                            text: poolSearch.text.length > 0
                                                  ? qsTr("Серверы не найдены")
                                                  : qsTr("Добавьте серверы из списка слева")
                                            color: Theme.textMuted
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignHCenter
                                            wrapMode: Text.WordWrap
                                            visible: selectedFallbackList.count === 0
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.hasConfigAdapter && configAdapter.serverCount === 0
                            text: qsTr("Сначала добавьте серверы во вкладке «Серверы».")
                            color: Theme.textMuted
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
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
                    objectName: "cabinetExternalBrowserToggle"
                    title: qsTr("Открывать кабинет во внешнем браузере")
                    description: !root.hasWebEngineSupport ?
                                 qsTr("В текущей сборке встроенный веб-движок не установлен. Кабинет всегда открывается в системном браузере.") :
                                 qsTr("При переходе во вкладку «Кабинет» отображать карточку для перехода в системный браузер вместо встроенного веб-движка.")
                    checked: !root.hasWebEngineSupport || root.cabinetExternalBrowser
                    switchEnabled: root.hasWebEngineSupport
                    opacity: root.hasWebEngineSupport ? 1.0 : 0.55
                    onToggled: function(value) {
                        if (typeof appPrefs !== "undefined" && root.hasWebEngineSupport) {
                            root.cabinetExternalBrowser = value;
                            appPrefs.setBool("cabinet_external_browser", value);
                        }
                    }
                }

                ToggleRow {
                    objectName: "cabinetMemorySaverToggle"
                    title: qsTr("Режим экономии памяти для кабинета")
                    badge: qsTr("РЕКОМЕНДУЕТСЯ")
                    description: qsTr("Автоматически выгружать веб-движок Chromium при неактивности или сворачивании приложения для полного освобождения RAM.")
                    checked: root.cabinetMemorySaver
                    switchEnabled: root.hasWebEngineSupport
                    opacity: root.hasWebEngineSupport ? 1.0 : 0.55
                    onToggled: function(value) {
                        if (typeof appPrefs !== "undefined" && root.hasWebEngineSupport) {
                            root.cabinetMemorySaver = value;
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
                                text: qsTr("Для TUN приложение установит отдельную root-owned копию ядра в каталоге текущего пользователя и назначит ей только cap_net_admin. Доступ к копии ограничен ACL; действие требует подтверждения Polkit.")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }

                        ActionButton {
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Установить права TUN")
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
                                text: "beaxty VPN v1.1.0"
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
