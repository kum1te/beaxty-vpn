// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

// Latency pill. Tiers come from Theme (fast / medium / slow) and are rendered as
// gauge bars plus lightness, never hue, to stay inside the monochrome palette.
Rectangle {
    id: root

    // 0 = never measured, TrafficMonitor.PingUnreachable (999) = could not connect.
    property int ping: 0

    readonly property bool measured: ping > 0 && ping < 999
    readonly property bool unreachable: ping >= 999 || ping < 0
    readonly property int filledBars: !measured ? 0
                                    : (ping < Theme.pingFastMs ? 3
                                    : (ping < Theme.pingMediumMs ? 2 : 1))

    readonly property color valueColor: !measured ? (ping < 0 ? Qt.rgba(0.95, 0.35, 0.35, 0.9) : Theme.textMuted)
                                      : (filledBars === 3 ? Theme.statusStrong
                                      : (filledBars === 2 ? Theme.textPrimary : Theme.statusMedium))

    implicitWidth: pingRow.implicitWidth + 16
    implicitHeight: 24
    width: implicitWidth
    height: implicitHeight
    radius: height / 2
    color: Theme.cardBg
    border.color: Theme.cardBorder
    border.width: 1

    Accessible.role: Accessible.StaticText
    Accessible.name: root.measured ? qsTr("Latency %1 milliseconds").arg(root.ping)
                                   : (root.unreachable ? qsTr("Node unreachable")
                                                       : qsTr("Latency not measured"))

    Row {
        id: pingRow
        anchors.centerIn: parent
        spacing: 6

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: [6, 9, 12]

                Rectangle {
                    required property int index
                    required property int modelData

                    width: 3
                    height: modelData
                    radius: 1
                    // Bars fill left-to-right: 3 filled = fast, 1 = slow.
                    color: (index < root.filledBars) ? root.valueColor : Theme.textMuted
                    Behavior on color { ColorAnimation { duration: Theme.durationNormal } }
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.measured ? (root.ping + " ms") : (root.ping < 0 ? qsTr("Timeout") : (root.unreachable ? "n/a" : "--"))
            color: root.valueColor
            font.pixelSize: 11
            font.family: Theme.fontMono
            font.bold: true
            Behavior on color { ColorAnimation { duration: Theme.durationNormal } }
        }
    }
}
