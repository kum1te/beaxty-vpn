// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

Item {
    id: root
    anchors.fill: parent
    property bool animationsEnabled: true

    readonly property int vpnState: (typeof throneEngine !== "undefined") ? throneEngine.state : 0
    // 0: Disconnected, 1: Connecting, 2: Protected

    // Base background fill
    Rectangle {
        anchors.fill: parent
        color: Theme.bgDark
    }

    // Time uniform driven at display refresh rate
    property real animTime: 0.0

    NumberAnimation on animTime {
        from: 0.0
        to: 628.31853 // 100 * 2*PI to keep float precision accurate
        duration: 300000 // 5 minutes continuous smooth cycle
        loops: Animation.Infinite
        running: true
        paused: running && !root.animationsEnabled
    }

    // Procedural Halftone Wave ShaderEffect
    ShaderEffect {
        id: shader
        anchors.fill: parent

        property real u_time: root.animTime
        property vector2d u_resolution: Qt.vector2d(Math.max(1, root.width), Math.max(1, root.height))

        property color u_bgColor: Theme.bgDark
        property color u_dotColor: (Theme.themeMode === 1) // Light mode
            ? Qt.rgba(0.2, 0.25, 0.35, 0.18)
            : Qt.rgba(0.4, 0.45, 0.55, 0.16)

        property color u_accentColor: (root.vpnState === 2)
            ? Qt.rgba(0.2, 0.85, 0.6, 0.40)  // Protected: Emerald-cyan crest
            : ((root.vpnState === 1)
                ? Qt.rgba(0.3, 0.6, 1.0, 0.45) // Connecting: Electric blue crest
                : Qt.rgba(0.7, 0.75, 0.9, 0.22)) // Disconnected: Silver obsidian

        fragmentShader: "qrc:/shaders/halftone_wave.frag.qsb"

        Behavior on u_accentColor {
            ColorAnimation { duration: 600 }
        }
    }
}
