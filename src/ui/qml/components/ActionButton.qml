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
    border.color: root.primary ? Theme.accentWhite
                               : (mouseArea.containsMouse && root.interactive ? Theme.textSecondary : Theme.cardBorder)
    border.width: 1

    scale: !root.interactive ? 1.0
                             : (mouseArea.pressed ? 0.96 : (mouseArea.containsMouse ? 1.02 : 1.0))
    Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

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

        // Busy spinner: a rotating arc in the same ink as the label.
        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: root.busy ? 12 : 0
            height: 12
            visible: root.busy
            Behavior on width { NumberAnimation { duration: Theme.durationFast } }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.color: root.primary ? Theme.textInverted : Theme.textPrimary
                border.width: 1.5
                opacity: 0.28
            }

            Rectangle {
                width: 3
                height: 3
                radius: 1.5
                color: root.primary ? Theme.textInverted : Theme.textPrimary
                x: parent.width / 2 - 1.5
                y: -1

                transform: Rotation {
                    origin.x: 1.5
                    origin.y: parent ? parent.height / 2 + 1 : 6
                    angle: spin.angle
                }
            }

            QtObject {
                id: spin
                property real angle: 0
            }

            NumberAnimation {
                target: spin
                property: "angle"
                from: 0
                to: 360
                duration: 900
                loops: Animation.Infinite
                running: root.busy
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
