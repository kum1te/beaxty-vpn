// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    objectName: "routingView"

    readonly property bool hasRouting: typeof routingManager !== "undefined"
    readonly property int activePreset: hasRouting ? routingManager.activePreset : 0
    readonly property int appMode: hasRouting ? routingManager.appRoutingMode : 0
    readonly property var domains: hasRouting ? routingManager.customDomains : []
    readonly property var apps: hasRouting ? routingManager.selectedApps : []
    readonly property var advancedRulesList: hasRouting ? routingManager.advancedRules : []

    property var runningAppsList: []
    property string appFilterText: ""

    // Advanced rule dialog states
    property string newRuleType: "Domain"
    property string newRuleAction: "Proxy"

    function refreshRunningApps() {
        if (root.hasRouting) {
            runningAppsList = routingManager.getRunningApplications();
        }
    }

    function pickPreset(preset) {
        if (root.hasRouting) routingManager.setActivePreset(preset);
    }

    ScrollView {
        id: routingScrollView
        objectName: "routingScrollView"
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Item {
            width: routingScrollView.width
            implicitHeight: contentCol.implicitHeight + 64

            ColumnLayout {
                id: contentCol
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                anchors.bottomMargin: 32
                width: Math.min(parent.width - 48, 920)
                spacing: 12

                Text {
                    text: qsTr("Режим маршрутизации")
                    color: Theme.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Выберите, как направлять трафик приложений и сайтов через защищенный VPN-туннель:")
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                // ----------------------------------------------------
                // Preset 0: Full Tunnel
                // ----------------------------------------------------
                PresetCard {
                    title: qsTr("Весь трафик (Full Tunnel)")
                    description: qsTr("Абсолютно весь системный трафик и приложения направляются через зашифрованный туннель.")
                    selected: root.activePreset === 0
                    onPicked: root.pickPreset(0)
                }

                // ----------------------------------------------------
                // Preset 1: Unified Split Tunneling
                // ----------------------------------------------------
                PresetCard {
                    title: qsTr("Раздельный туннель (Split Tunneling)")
                    description: qsTr("Через VPN направляются только указанные сайты и выбранные приложения. Весь остальной трафик идет напрямую.")
                    selected: root.activePreset === 1
                    onPicked: root.pickPreset(1)
                }

                // Unified Split Tunneling Configuration Card
                Card {
                    Layout.fillWidth: true
                    visible: root.activePreset === 1

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 16

                        // Mode selector (Proxy vs Bypass)
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Repeater {
                                model: [
                                    { mode: 0, label: qsTr("Только выбранные через VPN") },
                                    { mode: 1, label: qsTr("Все, кроме выбранных") }
                                ]

                                Rectangle {
                                    required property var modelData

                                    readonly property bool active: root.appMode === modelData.mode

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    radius: 8
                                    color: active ? Theme.accentWhite
                                                  : (chipMouse.containsMouse ? Theme.controlBgHover : Theme.cardHover)
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
                                        id: chipMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!root.hasRouting) return;
                                            routingManager.setAppRoutingMode(modelData.mode);
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.separator
                        }

                        // Section 1: Tunnelled Domains
                        Text {
                            text: qsTr("Туннелируемые домены")
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                radius: 8
                                color: Theme.cardHover
                                border.color: domainInput.activeFocus ? Theme.textSecondary : Theme.cardBorder
                                border.width: 1
                                Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                                TextInput {
                                    id: domainInput
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    font.family: Theme.fontMono
                                    selectByMouse: true
                                    onAccepted: addDomainButton.clicked()

                                    Text {
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: qsTr("Добавить домен (например, instagram.com)...")
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                        visible: !domainInput.text && !domainInput.activeFocus
                                    }
                                }
                            }

                            ActionButton {
                                id: addDomainButton
                                text: qsTr("Добавить")
                                primary: true
                                enabled: domainInput.text.trim().length > 0
                                onClicked: {
                                    const value = domainInput.text.trim();
                                    if (value.length === 0 || !root.hasRouting) return;
                                    routingManager.addCustomDomain(value);
                                    domainInput.text = "";
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Список пуст — добавьте сайты, которые должны открываться через VPN.")
                            color: Theme.textMuted
                            font.pixelSize: 11
                            visible: root.domains.length === 0
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: root.domains.length > 0

                            Repeater {
                                model: root.domains

                                Rectangle {
                                    required property string modelData
                                    required property int index

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    radius: 6
                                    color: Theme.cardHover
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.family: Theme.fontMono
                                            elide: Text.ElideRight
                                        }

                                        Item {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24

                                            Image {
                                                anchors.centerIn: parent
                                                width: 12
                                                height: 12
                                                sourceSize.width: 12
                                                sourceSize.height: 12
                                                source: Theme.icon("qrc:/icons/trash.svg", Theme.isDark)
                                                opacity: deleteDomainArea.containsMouse ? 0.9 : 0.45
                                                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
                                            }

                                            MouseArea {
                                                id: deleteDomainArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: if (root.hasRouting) routingManager.removeCustomDomain(index)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.separator
                        }

                        // Section 2: Tunnelled Applications
                        Text {
                            text: qsTr("Туннелируемые приложения")
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                radius: 8
                                color: Theme.cardHover
                                border.color: appInput.activeFocus ? Theme.textSecondary : Theme.cardBorder
                                border.width: 1
                                Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                                TextInput {
                                    id: appInput
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    font.family: Theme.fontMono
                                    selectByMouse: true
                                    onAccepted: addAppButton.clicked()

                                    Text {
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: qsTr("Имя процесса (например, telegram-desktop)...")
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                        visible: !appInput.text && !appInput.activeFocus
                                    }
                                }
                            }

                            ActionButton {
                                id: addAppButton
                                text: qsTr("Добавить")
                                primary: true
                                enabled: appInput.text.trim().length > 0
                                onClicked: {
                                    const value = appInput.text.trim();
                                    if (value.length === 0 || !root.hasRouting) return;
                                    routingManager.addApp(value);
                                    appInput.text = "";
                                }
                            }

                            ActionButton {
                                text: qsTr("Выбрать из списка")
                                onClicked: {
                                    root.refreshRunningApps();
                                    appSelectorDialog.visible = true;
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.apps.length === 0
                            text: qsTr("Список приложений пуст. Введите имя процесса или выберите из списка.")
                            color: Theme.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: root.apps.length > 0

                            Repeater {
                                model: root.apps

                                Rectangle {
                                    required property string modelData
                                    required property int index

                                    height: 30
                                    width: tagRow.implicitWidth + 20
                                    radius: 15
                                    color: Theme.cardHover
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    RowLayout {
                                        id: tagRow
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Rectangle {
                                            width: 6
                                            height: 6
                                            radius: 3
                                            color: Theme.accentWhite
                                        }

                                        Text {
                                            text: modelData
                                            color: Theme.textPrimary
                                            font.pixelSize: 11
                                            font.family: Theme.fontMono
                                            font.bold: true
                                        }

                                        Text {
                                            text: "✕"
                                            color: deleteAppArea.containsMouse ? Theme.accentWhite : Theme.textMuted
                                            font.pixelSize: 11
                                            font.bold: true

                                            MouseArea {
                                                id: deleteAppArea
                                                anchors.fill: parent
                                                anchors.margins: -4
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: if (root.hasRouting) routingManager.removeApp(index)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Изменения применяются автоматически.")
                            color: Theme.textMuted
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // ----------------------------------------------------
                // Preset 2: Advanced Routing
                // ----------------------------------------------------
                PresetCard {
                    title: qsTr("Продвинутая маршрутизация (Advanced Routing)")
                    description: qsTr("Пользовательские правила фильтрации по доменам, IP/CIDR, процессам, портам и GeoIP с точечным выбором действия.")
                    selected: root.activePreset === 2
                    onPicked: root.pickPreset(2)
                }

                // Advanced Routing Rules Management Card
                Card {
                    Layout.fillWidth: true
                    visible: root.activePreset === 2

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Пользовательские правила (%1):").arg(root.advancedRulesList.length)
                                color: Theme.textPrimary
                                font.pixelSize: 13
                                font.bold: true
                            }

                            ActionButton {
                                text: qsTr("Добавить правило")
                                primary: true
                                onClicked: advancedRulesDialog.visible = true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.advancedRulesList.length === 0
                            text: qsTr("Правила отсутствуют. Нажмите «Добавить правило».")
                            color: Theme.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: root.advancedRulesList.length > 0

                            Repeater {
                                model: root.advancedRulesList

                                Rectangle {
                                    required property var modelData
                                    required property int index

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 38
                                    radius: 8
                                    color: Theme.cardHover
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 10

                                        // Type badge
                                        Rectangle {
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: typeText.implicitWidth + 12
                                            radius: 4
                                            color: Theme.cardBg
                                            border.color: Theme.cardBorder
                                            border.width: 1

                                            Text {
                                                id: typeText
                                                anchors.centerIn: parent
                                                text: modelData.type || "Rule"
                                                color: Theme.textSecondary
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }

                                        // Value
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.value || ""
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.family: Theme.fontMono
                                            elide: Text.ElideRight
                                        }

                                        // Action badge
                                        Rectangle {
                                            readonly property string act: (modelData.action || "Proxy").toLowerCase()
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: actText.implicitWidth + 14
                                            radius: 10
                                            color: act === "proxy" ? Theme.accentWhite : (act === "block" ? "#33EF4444" : Theme.cardBg)
                                            border.color: act === "block" ? "#EF4444" : Theme.cardBorder
                                            border.width: 1

                                            Text {
                                                id: actText
                                                anchors.centerIn: parent
                                                text: (modelData.action || "Proxy").toUpperCase()
                                                color: parent.act === "proxy" ? Theme.textInverted : (parent.act === "block" ? "#EF4444" : Theme.textSecondary)
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }

                                        // Delete button
                                        Item {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24

                                            Image {
                                                anchors.centerIn: parent
                                                width: 12
                                                height: 12
                                                sourceSize.width: 12
                                                sourceSize.height: 12
                                                source: Theme.icon("qrc:/icons/trash.svg", Theme.isDark)
                                                opacity: delAdvArea.containsMouse ? 0.9 : 0.45
                                                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
                                            }

                                            MouseArea {
                                                id: delAdvArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: if (root.hasRouting) routingManager.deleteAdvancedRule(index)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 32 }
            }
        }
    }

    // ==========================================
    // Advanced Rules Modal Dialog
    // ==========================================
    Rectangle {
        id: advancedRulesDialog
        objectName: "advancedRulesDialog"
        anchors.fill: parent
        visible: false
        color: "#B3080808"

        MouseArea {
            anchors.fill: parent
            onClicked: advancedRulesDialog.visible = false
        }

        Keys.onEscapePressed: advancedRulesDialog.visible = false
        focus: visible

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(520, parent.width - 48)
            height: 380
            radius: 12
            color: Theme.bgDark
            border.color: Theme.cardBorder
            border.width: 1

            MouseArea {
                anchors.fill: parent
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Создание пользовательского правила")
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Text {
                        text: "✕"
                        color: Theme.textSecondary
                        font.pixelSize: 16
                        font.bold: true

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.PointingHandCursor
                            onClicked: advancedRulesDialog.visible = false
                        }
                    }
                }

                // Type selector
                Text {
                    text: qsTr("Тип правила")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: ["Domain", "IP-CIDR", "Process", "Port", "GeoIP"]

                        Rectangle {
                            required property string modelData
                            readonly property bool active: root.newRuleType === modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            radius: 6
                            color: active ? Theme.accentWhite : Theme.cardBg
                            border.color: active ? Theme.accentWhite : Theme.cardBorder
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: parent.active ? Theme.textInverted : Theme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.newRuleType = modelData
                            }
                        }
                    }
                }

                // Value input
                Text {
                    text: qsTr("Значение (домен, CIDR, процесс, порт, страна)")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 8
                    color: Theme.cardBg
                    border.color: ruleValueInput.activeFocus ? Theme.textSecondary : Theme.cardBorder
                    border.width: 1

                    TextInput {
                        id: ruleValueInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        font.family: Theme.fontMono
                        selectByMouse: true

                        Text {
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            text: {
                                if (root.newRuleType === "Domain") return "example.com";
                                if (root.newRuleType === "IP-CIDR") return "192.168.1.0/24";
                                if (root.newRuleType === "Process") return "telegram-desktop";
                                if (root.newRuleType === "Port") return "8080";
                                if (root.newRuleType === "GeoIP") return "ru";
                                return "";
                            }
                            color: Theme.textMuted
                            font.pixelSize: 12
                            visible: !ruleValueInput.text && !ruleValueInput.activeFocus
                        }
                    }
                }

                // Action selector
                Text {
                    text: qsTr("Действие")
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: [
                            { act: "Proxy", label: qsTr("Через VPN (Proxy)") },
                            { act: "Direct", label: qsTr("Прямое подключение (Direct)") },
                            { act: "Block", label: qsTr("Блокировать (Block)") }
                        ]

                        Rectangle {
                            required property var modelData
                            readonly property bool active: root.newRuleAction === modelData.act

                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            radius: 6
                            color: active ? Theme.accentWhite : Theme.cardBg
                            border.color: active ? Theme.accentWhite : Theme.cardBorder
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: parent.active ? Theme.textInverted : Theme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.newRuleAction = modelData.act
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    ActionButton {
                        text: qsTr("Отмена")
                        onClicked: advancedRulesDialog.visible = false
                    }

                    ActionButton {
                        text: qsTr("Сохранить правило")
                        primary: true
                        enabled: ruleValueInput.text.trim().length > 0
                        onClicked: {
                            if (!root.hasRouting) return;
                            routingManager.addAdvancedRule(root.newRuleType, ruleValueInput.text.trim(), root.newRuleAction);
                            ruleValueInput.text = "";
                            advancedRulesDialog.visible = false;
                        }
                    }
                }
            }
        }
    }

    // ==========================================
    // Running applications selector modal
    // ==========================================
    Rectangle {
        id: appSelectorDialog
        objectName: "appSelectorDialog"
        anchors.fill: parent
        visible: false
        color: "#B3080808"

        MouseArea {
            anchors.fill: parent
            onClicked: appSelectorDialog.visible = false
        }

        Keys.onEscapePressed: appSelectorDialog.visible = false
        focus: visible

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(540, parent.width - 48)
            height: Math.min(480, parent.height - 48)
            radius: 12
            color: Theme.bgDark
            border.color: Theme.cardBorder
            border.width: 1

            MouseArea {
                anchors.fill: parent
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: qsTr("Выбор приложения для роутинга")
                            color: Theme.textPrimary
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Выберите программы для включения или исключения из туннеля")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }

                    Text {
                        text: "✕"
                        color: Theme.textSecondary
                        font.pixelSize: 16
                        font.bold: true

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Close")

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appSelectorDialog.visible = false
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 8
                    color: Theme.cardBg
                    border.color: processFilterInput.activeFocus ? Theme.textSecondary : Theme.cardBorder
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Image {
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                            sourceSize.width: 14
                            sourceSize.height: 14
                            source: Theme.icon("qrc:/icons/search.svg", Theme.isDark)
                            opacity: 0.5
                        }

                        TextInput {
                            id: processFilterInput
                            Layout.fillWidth: true
                            verticalAlignment: TextInput.AlignVCenter
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            selectByMouse: true
                            onTextChanged: root.appFilterText = text.toLowerCase().trim()

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                text: qsTr("Поиск процесса или приложения...")
                                color: Theme.textMuted
                                font.pixelSize: 12
                                visible: !processFilterInput.text && !processFilterInput.activeFocus
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Приложения не найдены")
                        color: Theme.textMuted
                        font.pixelSize: 12
                        visible: processListView.count === 0
                    }

                    ListView {
                        id: processListView
                        anchors.fill: parent
                        clip: true
                        spacing: 6
                        model: {
                            if (!root.runningAppsList) return [];
                            if (!root.appFilterText) return root.runningAppsList;
                            return root.runningAppsList.filter(function(it) {
                                return (it.displayName || "").toLowerCase().includes(root.appFilterText)
                                    || (it.processName || "").toLowerCase().includes(root.appFilterText);
                            });
                        }

                        delegate: Rectangle {
                            required property var modelData

                            readonly property bool picked: root.apps.indexOf(modelData.processName) !== -1

                            width: processListView.width
                            height: 44
                            radius: 8
                            color: procMouse.containsMouse ? Theme.cardHover : Theme.cardBg
                            border.color: picked ? Theme.textSecondary : Theme.cardBorder
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                            Accessible.role: Accessible.CheckBox
                            Accessible.name: modelData.displayName || modelData.processName
                            Accessible.checked: picked

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Text {
                                            text: modelData.displayName || modelData.processName
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.bold: true
                                            elide: Text.ElideRight
                                            Layout.maximumWidth: processListView.width - 140
                                        }

                                        Rectangle {
                                            visible: modelData.isRunning === true
                                            Layout.preferredWidth: 60
                                            Layout.preferredHeight: 16
                                            radius: 8
                                            color: Theme.cardHover
                                            border.color: Theme.cardBorder
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: qsTr("АКТИВНО")
                                                color: Theme.accentWhite
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                        }

                                        Item { Layout.fillWidth: true }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.processName
                                        color: Theme.textMuted
                                        font.pixelSize: 10
                                        font.family: Theme.fontMono
                                        elide: Text.ElideRight
                                    }
                                }

                                Rectangle {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    radius: 11
                                    color: parent.parent.picked ? Theme.accentWhite : "transparent"
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: parent.parent.parent.picked ? "✓" : "+"
                                        color: parent.parent.parent.picked ? Theme.textInverted : Theme.textSecondary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }
                            }

                            MouseArea {
                                id: procMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!root.hasRouting) return;
                                    const idx = root.apps.indexOf(modelData.processName);
                                    if (idx !== -1) {
                                        routingManager.removeApp(idx);
                                    } else {
                                        routingManager.addApp(modelData.processName);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
