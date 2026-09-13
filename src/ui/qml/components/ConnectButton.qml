// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

// The main connect dial. State is carried entirely by fill, rim and motion:
// hollow ring = disconnected, pulsing = connecting, solid white = protected.
Item {
    id: root

    property int connectionState: 0 // 0: Disconnected, 1: Connecting, 2: Protected
    property bool busy: connectionState === 1
    signal clicked()

    readonly property bool isProtected: connectionState === 2
    property bool animationsEnabled: (Window.window ? (Window.window.visible && Window.window.visibility !== Window.Minimized) : true)

    width: 180
    height: 180

    Accessible.role: Accessible.Button
    Accessible.name: root.isProtected ? qsTr("Disconnect") : qsTr("Connect")
    Accessible.onPressAction: root.clicked()
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.clicked();
            event.accepted = true;
        }
    }

    // ====================================================
    // Layer 0: Concentric vector aura rings when protected.
    // Pure vector QML, zero Canvas / FBO blur or bleed-through.
    // ====================================================
    Item {
        id: ambientAura
        anchors.centerIn: parent
        width: parent.width + 80
        height: parent.height + 80
        visible: root.isProtected

        // Ring 1 (innermost aura ring)
        Rectangle {
            anchors.centerIn: parent
            width: root.width + 24
            height: width
            radius: width / 2
            color: "transparent"
            border.color: Theme.accentWhite
            border.width: 1.0
            opacity: root.isProtected ? 0.32 : 0.0
        }

        // Ring 2 (middle aura ring)
        Rectangle {
            anchors.centerIn: parent
            width: root.width + 48
            height: width
            radius: width / 2
            color: "transparent"
            border.color: Theme.accentWhite
            border.width: 1.0
            opacity: root.isProtected ? 0.18 : 0.0

            SequentialAnimation on scale {
                running: root.isProtected
                paused: running && !root.animationsEnabled
                loops: Animation.Infinite
                NumberAnimation { from: 1.01; to: 1.06; duration: 2600; easing.type: Easing.InOutSine }
                NumberAnimation { from: 1.06; to: 1.01; duration: 2600; easing.type: Easing.InOutSine }
            }

            SequentialAnimation on opacity {
                running: root.isProtected
                paused: running && !root.animationsEnabled
                loops: Animation.Infinite
                NumberAnimation { from: 0.18; to: 0.06; duration: 2600; easing.type: Easing.InOutSine }
                NumberAnimation { from: 0.06; to: 0.18; duration: 2600; easing.type: Easing.InOutSine }
            }
        }

        // Ring 3 (outer aura ring)
        Rectangle {
            anchors.centerIn: parent
            width: root.width + 72
            height: width
            radius: width / 2
            color: "transparent"
            border.color: Theme.accentWhite
            border.width: 1.0
            opacity: root.isProtected ? 0.08 : 0.0

            SequentialAnimation on scale {
                running: root.isProtected
                paused: running && !root.animationsEnabled
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 1.05; duration: 3000; easing.type: Easing.InOutSine }
                NumberAnimation { from: 1.05; to: 1.0; duration: 3000; easing.type: Easing.InOutSine }
            }

            SequentialAnimation on opacity {
                running: root.isProtected
                paused: running && !root.animationsEnabled
                loops: Animation.Infinite
                NumberAnimation { from: 0.08; to: 0.02; duration: 3000; easing.type: Easing.InOutSine }
                NumberAnimation { from: 0.02; to: 0.08; duration: 3000; easing.type: Easing.InOutSine }
            }
        }
    }

    // ====================================================
    // Layer 1: Connecting ripple. Two offset waves read as a heartbeat
    // rather than a single flicker.
    // ====================================================
    Repeater {
        model: 2

        Rectangle {
            required property int index

            anchors.centerIn: parent
            width: root.width + 20
            height: root.height + 20
            radius: width / 2
            color: "transparent"
            border.color: Theme.accentWhite
            border.width: 1.5
            visible: root.connectionState === 1
            opacity: 0

            SequentialAnimation {
                running: root.connectionState === 1
                paused: running && !root.animationsEnabled
                loops: Animation.Infinite
                PauseAnimation { duration: index * 600 }
                ParallelAnimation {
                    NumberAnimation { target: parent; property: "scale"; from: 0.95; to: 1.25; duration: 1200; easing.type: Easing.OutQuad }
                    NumberAnimation { target: parent; property: "opacity"; from: 0.5; to: 0.0; duration: 1200; easing.type: Easing.OutQuad }
                }
            }
        }
    }

    // ====================================================
    // Layer 2: Outer breathing rim when protected.
    // ====================================================
    Rectangle {
        anchors.centerIn: parent
        width: parent.width + 12
        height: parent.height + 12
        radius: width / 2
        color: "transparent"
        border.color: Theme.accentWhite
        border.width: 1.5
        opacity: root.isProtected ? 0.45 : 0.0
        visible: opacity > 0.001

        SequentialAnimation on scale {
            running: root.isProtected
            paused: running && !root.animationsEnabled
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 1.06; duration: 1800; easing.type: Easing.InOutSine }
            NumberAnimation { from: 1.06; to: 1.0; duration: 1800; easing.type: Easing.InOutSine }
        }

        SequentialAnimation on opacity {
            running: root.isProtected
            paused: running && !root.animationsEnabled
            loops: Animation.Infinite
            NumberAnimation { from: 0.45; to: 0.18; duration: 1800; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.18; to: 0.45; duration: 1800; easing.type: Easing.InOutSine }
        }

        Behavior on opacity { NumberAnimation { duration: 300 } }
    }

    // ====================================================
    // Layer 3: Rotating progress arc while connecting.
    // ====================================================
    Item {
        anchors.centerIn: parent
        width: parent.width - 6
        height: parent.height - 6
        visible: root.connectionState === 1

        Canvas {
            id: connectingArcCanvas
            anchors.fill: parent
            Connections {
                target: Theme
                function onThemeChanged() {
                    connectingArcCanvas.requestPaint();
                }
            }
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var cx = width / 2;
                var cy = height / 2;
                var r = (width - 6) / 2;

                ctx.beginPath();
                ctx.arc(cx, cy, r, -Math.PI / 2, Math.PI / 4, false);
                ctx.strokeStyle = Theme.textPrimary.toString();
                ctx.lineWidth = 3.5;
                ctx.lineCap = "round";
                ctx.stroke();
            }
        }

        NumberAnimation on rotation {
            running: root.connectionState === 1
            paused: running && !root.animationsEnabled
            loops: Animation.Infinite
            from: 0
            to: 360
            duration: 900
        }
    }

    // ====================================================
    // Layer 3.5: Solid opaque backing disk.
    // Guarantees zero bleed-through from background shaders or halftone dots.
    // ====================================================
    Rectangle {
        id: baseDisc
        anchors.centerIn: parent
        width: parent.width - 12
        height: width
        radius: width / 2
        color: root.isProtected ? Theme.accentWhite : Theme.controlBg
        Behavior on color { ColorAnimation { duration: 250 } }
    }

    // ====================================================
    // Layer 4: Chamfered rim and core.
    // ====================================================
    Rectangle {
        id: outerRim
        anchors.centerIn: parent
        width: parent.width - 12
        height: width
        radius: width / 2

        color: root.isProtected ? Theme.accentWhite
                                : (mouseArea.containsMouse ? Theme.controlBgHover : Theme.controlBg)
        border.color: root.isProtected ? Theme.accentWhite
                                       : (mouseArea.containsMouse ? Theme.controlBorderHover : Theme.controlBorder)
        border.width: 1.5

        scale: mouseArea.pressed ? 0.97 : (mouseArea.containsMouse ? 1.02 : 1.0)
        Behavior on scale { NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutQuad } }
        Behavior on color { ColorAnimation { duration: 250 } }
        Behavior on border.color { ColorAnimation { duration: Theme.durationNormal } }

        // Inner core body
        Rectangle {
            id: buttonBody
            anchors.centerIn: parent
            width: parent.width - 12
            height: width
            radius: width / 2

            color: root.isProtected ? Theme.accentWhite
                                    : (mouseArea.containsMouse ? Theme.controlBgHover : Theme.controlBg)
            border.color: root.isProtected ? Theme.accentWhite
                                           : (mouseArea.containsMouse ? Theme.controlBorderHover : Theme.controlBorder)
            border.width: root.isProtected ? 0 : 1

            Behavior on color { ColorAnimation { duration: 250 } }
            Behavior on border.color { ColorAnimation { duration: Theme.durationNormal } }

            // Vector power glyph
            Item {
                id: powerIcon
                anchors.centerIn: parent
                width: 48
                height: 48

                property color iconColor: root.isProtected ? Theme.textInverted : Theme.textPrimary
                Behavior on iconColor { ColorAnimation { duration: 250 } }

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.color: powerIcon.iconColor
                    border.width: 3.5
                }

                // Masks the top of the ring so the glyph reads as a power symbol.
                Rectangle {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 14
                    height: 10
                    color: buttonBody.color
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.topMargin: -2
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 3.5
                    height: 20
                    radius: 2
                    color: powerIcon.iconColor
                }
            }
        }

        // Focus ring for keyboard users.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -5
            radius: width / 2
            color: "transparent"
            border.color: Theme.textSecondary
            border.width: 1
            visible: root.activeFocus
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
