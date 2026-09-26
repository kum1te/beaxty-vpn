// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Layouts
import ".."

// One node in the server list. Shared by the subscription groups and the custom
// nodes section, which were previously two near-identical delegate blocks.
Rectangle {
    id: root

    property var server: ({})
    property int currentPing: (root.server && root.server.ping !== undefined) ? root.server.ping : 0
    onServerChanged: {
        if (root.server && root.server.ping !== undefined) {
            currentPing = root.server.ping;
        }
    }

    Connections {
        target: (typeof trafficMonitor !== "undefined") ? trafficMonitor : null
        function onServerPingUpdated(profileId, pingMs) {
            if (root.server && root.server.id === profileId) {
                root.currentPing = pingMs;
            }
        }
    }

    readonly property bool current: (typeof configAdapter !== "undefined") && (configAdapter.selectedServerId === (server ? server.id : -1))

    signal selected()
    signal removeRequested()

    implicitHeight: 54
    height: implicitHeight
    radius: 10
    color: mouseArea.containsMouse ? Theme.cardHover : Theme.bgElevated
    border.color: current ? Theme.accentWhite
                          : (mouseArea.containsMouse ? Theme.controlBorderHover : Theme.cardBorder)
    border.width: current ? 2 : 1

    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    Behavior on border.width { NumberAnimation { duration: Theme.durationFast } }

    Accessible.role: Accessible.RadioButton
    Accessible.name: (server.name || qsTr("Server")) + (root.current ? qsTr(", selected") : "")
    Accessible.checked: root.current
    Accessible.onPressAction: root.selected()
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.selected();
            event.accepted = true;
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.selected()
    }

    function parseServerFlagAndName(rawName, countryCode) {
        if (!rawName) return { flag: "🌐", name: qsTr("Server") };
        // Флаговые региональные символы Юникода (эмодзи-флаги стран)
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
        // Если в начале названия флага нет, но есть ISO-код страны (например, PL, DE, NL, FI)
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

    readonly property var parsedServer: parseServerFlagAndName(server ? server.name : "", server ? server.country : "")

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        // Selection radio
        Rectangle {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            radius: 10
            color: "transparent"
            border.color: root.current ? Theme.accentWhite : (mouseArea.containsMouse ? Theme.textSecondary : Theme.textMuted)
            border.width: root.current ? 2 : 1

            Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
            Behavior on border.width { NumberAnimation { duration: Theme.durationFast } }

            Rectangle {
                anchors.centerIn: parent
                width: root.current ? 10 : 0
                height: width
                radius: width / 2
                color: Theme.accentWhite
                opacity: root.current ? 1.0 : 0.0

                Behavior on width { NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutQuad } }
                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
            }
        }

        // Country Flag Badge (36x36 with 8px radius and dark background)
        Rectangle {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: 8
            color: Theme.cardHover
            border.color: Theme.cardBorder
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: root.parsedServer.flag
                font.pixelSize: 20
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.parsedServer.name
                color: root.current ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: 13
                font.weight: root.current ? Font.Bold : Font.DemiBold
                elide: Text.ElideRight
            }

            Row {
                spacing: 8

                Text {
                    text: root.server.type || ""
                    color: Theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                    visible: text.length > 0
                }

                Text {
                    text: (root.server.address || "") + (root.server.port ? (":" + root.server.port) : "")
                    color: Theme.textMuted
                    font.pixelSize: 10
                    font.family: Theme.fontMono
                }
            }
        }

        PingBadge {
            ping: root.currentPing

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof trafficMonitor !== "undefined" && root.server && root.server.id) {
                        trafficMonitor.testServerPing(root.server.id);
                    }
                }
            }
        }

        // Remove node
        Item {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Delete node")
            Accessible.onPressAction: root.removeRequested()

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                sourceSize.width: 14
                sourceSize.height: 14
                source: Theme.icon("qrc:/icons/trash.svg", Theme.isDark)
                opacity: deleteArea.containsMouse ? 0.9 : 0.4
                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
            }

            MouseArea {
                id: deleteArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.removeRequested()
            }
        }
    }

    // Focus ring
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.color: Theme.textSecondary
        border.width: 2
        visible: root.activeFocus
    }
}
