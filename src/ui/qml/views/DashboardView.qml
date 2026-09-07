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

            ColumnLayout {
                id: dashboardContent
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 24
                width: Math.min(parent.width - 48, 460)
                spacing: 16

                // 1. Status Pill Header
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

                // 2. Centerpiece Connect Button
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180

                    ConnectButton {
                        anchors.centerIn: parent
                        connectionState: root.vpnState
                        onClicked: {
                            if (!root.hasServer) {
                                root.requestNodesView();
                                return;
                            }
                            if (typeof throneEngine !== "undefined") throneEngine.toggleConnect();
                        }
                    }
                }

                // 3. Status Subtitle
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

                // 4. Compact Network Stats Pill (Speed & Volume)
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredHeight: 34
                    width: statsRow.implicitWidth + 28
                    radius: 10
                    color: Theme.cardBg
                    border.color: Theme.cardBorder
                    border.width: 1
                    opacity: root.vpnState === 2 ? 1.0 : (root.vpnState === 1 ? 0.85 : 0.6)
                    Behavior on opacity { NumberAnimation { duration: Theme.durationNormal } }

                    RowLayout {
                        id: statsRow
                        anchors.centerIn: parent
                        spacing: 12

                        // Download speed
                        Row {
                            spacing: 5
                            Layout.alignment: Qt.AlignVCenter
                            Text {
                                text: "↓"
                                color: root.vpnState === 2 ? Theme.accentWhite : Theme.textSecondary
                                font.pixelSize: 12
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: (typeof trafficMonitor !== "undefined") ? trafficMonitor.downloadSpeed : "0.0 KB/s"
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.family: Theme.fontMono
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Separator
                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 14
                            Layout.alignment: Qt.AlignVCenter
                            color: Theme.separator
                        }

                        // Upload speed
                        Row {
                            spacing: 5
                            Layout.alignment: Qt.AlignVCenter
                            Text {
                                text: "↑"
                                color: root.vpnState === 2 ? Theme.accentWhite : Theme.textSecondary
                                font.pixelSize: 12
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: (typeof trafficMonitor !== "undefined") ? trafficMonitor.uploadSpeed : "0.0 KB/s"
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.family: Theme.fontMono
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Separator
                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 14
                            Layout.alignment: Qt.AlignVCenter
                            color: Theme.separator
                        }

                        // Total volume
                        Row {
                            spacing: 5
                            Layout.alignment: Qt.AlignVCenter
                            Text {
                                text: qsTr("Объем:")
                                color: Theme.textMuted
                                font.pixelSize: 11
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: (typeof trafficMonitor !== "undefined") ? trafficMonitor.totalTraffic : "0 B"
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                font.family: Theme.fontMono
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 4 }

                // 5. Active Server Card
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

                // 6. Current Routing Mode Card
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
                                    source: Theme.icon("qrc:/icons/routing_fork.svg", Theme.isDark)
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

                // Bottom breathing spacer
                Item {
                    Layout.preferredHeight: 16
                }
            }
        }
    }
}
