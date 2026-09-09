// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

Rectangle {
    id: root

    property string text: ""
    property bool primary: false
    // Shows an indeterminate sweep and blocks clicks while an action is in flight.
    property bool busy: false

    signal clicked()

    // `enabled` is Item's own property; shadowing it made every instance log a
    // property-cache override warning.
    readonly property bool interactive: root.enabled && !root.busy

    implicitWidth: contentRow.implicitWidth + 28
    implicitHeight: 38
    width: implicitWidth
    height: implicitHeight
    radius: height / 2

    opacity: root.enabled ? 1.0 : 0.45
    Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

    color: root.primary ? Theme.accentWhite
                        : (mouseArea.containsMouse && root.interactive ? Theme.cardHover : Theme.cardBg)
    property real pulseVal: 0.0
    SequentialAnimation on pulseVal {
        running: root.busy
        loops: Animation.Infinite
        NumberAnimation { from: 0.0; to: 1.0; duration: 750; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.0; to: 0.0; duration: 750; easing.type: Easing.InOutSine }
    }

    readonly property color idleBorderColor: root.primary ? Theme.accentWhite
                                : (mouseArea.containsMouse && root.interactive ? Theme.textSecondary : Theme.cardBorder)
    readonly property color activePulseColor: root.primary ? Theme.accentDim : Theme.textPrimary

    border.color: root.busy ? Qt.rgba(
        idleBorderColor.r + (activePulseColor.r - idleBorderColor.r) * pulseVal,
        idleBorderColor.g + (activePulseColor.g - idleBorderColor.g) * pulseVal,
        idleBorderColor.b + (activePulseColor.b - idleBorderColor.b) * pulseVal,
        1.0
    ) : idleBorderColor
    border.width: 1

    scale: !root.interactive ? 1.0
                             : (mouseArea.pressed ? 0.96 : (mouseArea.containsMouse ? 1.02 : 1.0))
    Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { enabled: !root.busy; ColorAnimation { duration: 150 } }

    Accessible.role: Accessible.Button
    Accessible.name: root.text
    Accessible.onPressAction: if (root.interactive) root.clicked()
    activeFocusOnTab: root.interactive
    Keys.onPressed: function(event) {
        if (!root.interactive) return;
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.clicked();
            event.accepted = true;
        }
    }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 8

        // Busy spinner: 16x16 circular arc (270°) with round caps and guide ring
        Item {
            id: spinnerContainer
            anchors.verticalCenter: parent.verticalCenter
            width: root.busy ? 16 : 0
            height: 16
            visible: root.busy
            clip: true
            Behavior on width { NumberAnimation { duration: Theme.durationFast } }

            Item {
                anchors.centerIn: parent
                width: 16
                height: 16

                // Guide ring (opacity 0.2)
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: "transparent"
                    border.color: root.primary ? Theme.textInverted : Theme.textPrimary
                    border.width: 2
                    opacity: 0.2
                }

                Canvas {
                    id: spinnerCanvas
                    anchors.fill: parent
                    renderTarget: Canvas.Image

                    Connections {
                        target: Theme
                        function onThemeChanged() { spinnerCanvas.requestPaint(); }
                    }
                    Connections {
                        target: root
                        function onPrimaryChanged() { spinnerCanvas.requestPaint(); }
                    }

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.clearRect(0, 0, width, height);
                        var strokeCol = root.primary ? Theme.textInverted : Theme.textPrimary;
                        ctx.strokeStyle = strokeCol.toString();
                        ctx.lineWidth = 2;
                        ctx.lineCap = "round";
                        ctx.beginPath();
                        var cx = width / 2;
                        var cy = height / 2;
                        var r = (width - 2) / 2;
                        // 270 degree arc from top (-PI/2) to left (PI)
                        ctx.arc(cx, cy, r, -Math.PI / 2, Math.PI, false);
                        ctx.stroke();
                    }

                    NumberAnimation on rotation {
                        running: root.busy
                        loops: Animation.Infinite
                        from: 0
                        to: 360
                        duration: 800
                    }
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.primary ? Theme.textInverted : Theme.textPrimary
            font.pixelSize: 13
            font.bold: true
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.interactive
        enabled: root.interactive
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.color: Theme.textSecondary
        border.width: 1
        visible: root.activeFocus
    }
}
