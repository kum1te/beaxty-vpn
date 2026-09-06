// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root

    signal requestNodesView()
    signal requestRoutingView()

    readonly property int vpnState: (typeof throneEngine !== "undefined") ? throneEngine.state : 0
    readonly property bool tunEnabled: (typeof throneEngine !== "undefined") && throneEngine.tunModeEnabled
    readonly property bool hasServer: (typeof configAdapter !== "undefined") && configAdapter.serverCount > 0

    function parseServerFlagAndName(rawName, countryCode) {
        if (!rawName) return { flag: "🌐", name: qsTr("Select a server") };
        var flagMatch = rawName.match(/^([\uD83C][\uDDE6-\uDDFF][\uD83C][\uDDE6-\uDDFF])\s*(.*)$/);
        if (flagMatch) {
            return { flag: flagMatch[1], name: flagMatch[2] ? flagMatch[2] : rawName };
        }
        var lower = rawName.toLowerCase();
        var prefixMap = {
            "pl-": "🇵🇱", "de-": "🇩🇪", "nl-": "🇳🇱", "fi-": "🇫🇮",
            "ee-": "🇪🇪", "it-": "🇮🇹", "ru-": "🇷🇺", "us-": "🇺🇸",
            "fr-": "🇫🇷", "gb-": "🇬🇧", "uk-": "🇬🇧", "se-": "🇸🇪",
            "ch-": "🇨🇭", "kz-": "🇰🇿", "tr-": "🇹🇷", "es-": "🇪🇸"
        };
        for (var prefix in prefixMap) {
            if (lower.startsWith(prefix)) {
                return { flag: prefixMap[prefix], name: rawName };
            }
        }
        if (countryCode && countryCode.length === 2 && countryCode !== "NL") {
            var code = countryCode.toUpperCase();
            var f = String.fromCodePoint(0x1F1A5 + code.charCodeAt(0), 0x1F1A5 + code.charCodeAt(1));
            return { flag: f, name: rawName };
        }
        if (countryCode === "NL" && (lower.includes("nl") || lower.includes("ams") || lower.includes("netherland") || lower.includes("нидерланд"))) {
            return { flag: "🇳🇱", name: rawName };
        }
        if (countryCode && countryCode.length === 2) {
            var code2 = countryCode.toUpperCase();
            var f2 = String.fromCodePoint(0x1F1A5 + code2.charCodeAt(0), 0x1F1A5 + code2.charCodeAt(1));
            return { flag: f2, name: rawName };
        }
        return { flag: "🌐", name: rawName };
    }

    readonly property var activeServerParsed: parseServerFlagAndName(
        (typeof configAdapter !== "undefined") ? configAdapter.selectedServerName : "",
        (typeof configAdapter !== "undefined") ? configAdapter.selectedServerCountry : ""
    )

    ScrollView {
        id: dashboardScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Item {
            width: dashboardScroll.width
            implicitHeight: dashboardContent.implicitHeight + 48

            Item {
                id: container
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                width: Math.min(parent.width - 48, 920)
                height: dashboardContent.implicitHeight

                GridLayout {
                    id: dashboardContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    columns: container.width < 660 ? 1 : 2
                    columnSpacing: 24
                    rowSpacing: 20

                    // ==========================================
                    // Left Column: Connect Centerpiece & Active Server
                    // ==========================================
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: container.width < 660 ? container.width : Math.round((container.width - 24) * 0.46)
                        spacing: 16

                        // Status Pill Header
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32

                            Rectangle {
                                anchors.centerIn: parent
                                height: 28
                                width: statusRow.width + 24
                                radius: 14
                                color: Theme.cardBg
                                border.color: Theme.cardBorder
                                border.width: 1

                                Row {
                                    id: statusRow
                                    anchors.centerIn: parent
                                    spacing: 8

                                    // Status dot: hollow ring when off, filled when
                                    // protected, pulsing while connecting.
                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: root.vpnState === 2 ? Theme.accentWhite
                                             : (root.vpnState === 1 ? Theme.textSecondary : "transparent")
                                        border.color: Theme.textSecondary
                                        border.width: 1
                                        Behavior on color { ColorAnimation { duration: Theme.durationNormal } }

                                        SequentialAnimation on opacity {
                                            running: root.vpnState === 1
                                            loops: Animation.Infinite
                                            NumberAnimation { from: 1.0; to: 0.2; duration: 600 }
                                            NumberAnimation { from: 0.2; to: 1.0; duration: 600 }
                                        }
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: (typeof throneEngine !== "undefined") ? throneEngine.stateString : "DISCONNECTED"
                                        color: Theme.textPrimary
                                        font.pixelSize: 11
                                        font.bold: true
                                        font.letterSpacing: 1.5
                                    }
                                }
                            }
                        }

                        // Centerpiece Connect Button
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 180

                            ConnectButton {
                                anchors.centerIn: parent
                                connectionState: root.vpnState
                                onClicked: {
                                    // Connecting with no node selected only produces an
                                    // error toast; send the user where they can fix it.
                                    if (!root.hasServer) {
                                        root.requestNodesView();
                                        return;
                                    }
                                    if (typeof throneEngine !== "undefined") throneEngine.toggleConnect();
                                }
                            }
                        }

                        // Status Subtitle
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            text: !root.hasServer
                                  ? qsTr("Добавьте узел, чтобы подключиться")
                                  : ((typeof throneEngine !== "undefined") ? throneEngine.statusMessage : qsTr("Ready to connect"))
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }

                        Item { Layout.preferredHeight: 4 }

                        // Active Server Card
                        Card {
                            Layout.fillWidth: true
                            interactive: true
                            onClicked: root.requestNodesView()

                            RowLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                spacing: 12

                                // Country flag badge
                                Rectangle {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 8
                                    color: Theme.cardHover
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: root.activeServerParsed.flag
                                        font.pixelSize: 20
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 60
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.hasServer ? root.activeServerParsed.name : qsTr("Select a server")
                                        color: Theme.textPrimary
                                        font.pixelSize: 14
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: (typeof configAdapter !== "undefined" && configAdapter.selectedServerType.length > 0)
                                              ? (configAdapter.selectedServerType + " • " + (root.hasServer ? qsTr("Сменить сервер") : qsTr("Добавить узел")))
                                              : (root.hasServer ? qsTr("Сменить сервер") : qsTr("Добавить узел"))
                                        color: Theme.textMuted
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                }

                                PingBadge {
                                    Layout.alignment: Qt.AlignVCenter
                                    ping: (typeof configAdapter !== "undefined") ? configAdapter.selectedServerPing : 0
                                }

                                Text {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: 12
                                    text: "›"
                                    color: Theme.textSecondary
                                    font.pixelSize: 18
                                    font.bold: true
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }

                    // ==========================================
                    // Right Column: Network Stats & Routing Overview
                    // ==========================================
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: container.width < 660 ? container.width : Math.round((container.width - 24) * 0.54)
                        spacing: 16

                        Text {
                            text: qsTr("Сетевая статистика")
                            color: Theme.textPrimary
                            font.pixelSize: 16
                            font.bold: true
                        }

                        // Real-time Traffic Speeds Card
                        TrafficCard {
                            Layout.fillWidth: true
                            downloadSpeed: (typeof trafficMonitor !== "undefined") ? trafficMonitor.downloadSpeed : "0.0 KB/s"
                            uploadSpeed: (typeof trafficMonitor !== "undefined") ? trafficMonitor.uploadSpeed : "0.0 KB/s"
                            totalTraffic: (typeof trafficMonitor !== "undefined") ? trafficMonitor.totalTraffic : "0 B"
                        }

                        // Current Routing Mode Card
                        Card {
                            Layout.fillWidth: true
                            interactive: true
                            onClicked: root.requestRoutingView()

                            ColumnLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true

                                    Row {
                                        spacing: 8
                                        Layout.alignment: Qt.AlignVCenter

                                        Image {
                                            width: 16
                                            height: 16
                                            source: "qrc:/icons/routing_fork.svg"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: qsTr("Режим маршрутизации")
                                            color: Theme.textSecondary
                                            font.pixelSize: 12
                                            font.bold: true
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    Text {
                                        text: qsTr("Настроить ›")
                                        color: Theme.textPrimary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Theme.separator
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: Theme.accentWhite
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var p = (typeof routingManager !== "undefined") ? routingManager.activePreset : 0;
                                            if (p === 0) return qsTr("Весь трафик через VPN (Full Tunnel)");
                                            if (p === 1) return qsTr("Раздельный туннель (Split Tunneling)");
                                            if (p === 2) return qsTr("Продвинутая маршрутизация (Advanced Routing)");
                                            return qsTr("Маршрутизация по умолчанию");
                                        }
                                        color: Theme.textPrimary
                                        font.pixelSize: 13
                                        font.bold: true
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }

                        // Connection Parameters Card
                        Card {
                            Layout.fillWidth: true

                            ColumnLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                spacing: 8

                                Text {
                                    text: qsTr("Параметры соединения")
                                    color: Theme.textSecondary
                                    font.pixelSize: 12
                                    font.bold: true
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Theme.separator
                                }

                                // Every value here is read back from the engine rather
                                // than hardcoded, so the card cannot claim protection
                                // the running config does not actually have.
                                Repeater {
                                    model: [
                                        { label: qsTr("Сетевой стек:"),
                                          value: (typeof throneEngine !== "undefined") ? throneEngine.networkStackLabel : "—" },
                                        { label: qsTr("Удалённый DNS:"),
                                          value: (typeof throneEngine !== "undefined") ? throneEngine.remoteDnsLabel : "—" },
                                        { label: qsTr("Изоляция трафика:"),
                                          value: (typeof throneEngine !== "undefined") ? throneEngine.routingIsolationLabel : "—" }
                                    ]

                                    RowLayout {
                                        required property var modelData

                                        Layout.fillWidth: true

                                        Text {
                                            text: modelData.label
                                            color: Theme.textMuted
                                            font.pixelSize: 11
                                        }

                                        Item { Layout.fillWidth: true }

                                        Text {
                                            text: modelData.value
                                            color: Theme.textPrimary
                                            font.pixelSize: 11
                                            font.bold: true
                                            elide: Text.ElideRight
                                            Layout.maximumWidth: 200
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Bottom breathing spacer
            Item {
                anchors.top: container.bottom
                width: parent.width
                height: 32
            }
        }
    }
}
