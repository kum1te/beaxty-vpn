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

    readonly property bool isTesting: (typeof trafficMonitor !== "undefined") && trafficMonitor.isTestingPing

    property real wavePhase: 0.0
    NumberAnimation on wavePhase {
        running: root.isTesting
        loops: Animation.Infinite
        from: 0.0
        to: Math.PI * 2
        duration: 1100
    }

    implicitWidth: pingRow.implicitWidth + 16
    implicitHeight: 24
    width: implicitWidth
    height: implicitHeight
    radius: height / 2
    color: Theme.cardBg
    border.color: root.isTesting
        ? Qt.rgba(
            Theme.cardBorder.r + (Theme.textSecondary.r - Theme.cardBorder.r) * (0.35 + 0.35 * Math.sin(root.wavePhase)),
            Theme.cardBorder.g + (Theme.textSecondary.g - Theme.cardBorder.g) * (0.35 + 0.35 * Math.sin(root.wavePhase)),
            Theme.cardBorder.b + (Theme.textSecondary.b - Theme.cardBorder.b) * (0.35 + 0.35 * Math.sin(root.wavePhase)),
            1.0)
        : Theme.cardBorder
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
                    // Traveling wave ripple across bars during testing
                    readonly property real waveIntensity: root.isTesting ? Math.max(0.2, (Math.sin(root.wavePhase - index * 0.9) + 1) / 2) : 1.0
                    opacity: root.isTesting ? (0.25 + 0.75 * waveIntensity) : 1.0
                    color: root.isTesting ? Theme.textPrimary : ((index < root.filledBars) ? root.valueColor : Theme.textMuted)
                    Behavior on color { ColorAnimation { duration: Theme.durationNormal } }
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.measured ? (root.ping + " ms") : (root.ping < 0 ? qsTr("Timeout") : (root.unreachable ? "n/a" : "--"))
            color: root.isTesting ? Theme.textPrimary : root.valueColor
            opacity: root.isTesting ? (0.55 + 0.45 * Math.sin(root.wavePhase)) : 1.0
            font.pixelSize: 11
            font.family: Theme.fontSans
            font.letterSpacing: 0
            font.bold: true
            Behavior on color { ColorAnimation { duration: Theme.durationNormal } }
        }
    }
}
