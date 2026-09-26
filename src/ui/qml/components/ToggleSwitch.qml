// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

Item {
    id: root

    property bool checked: false

    signal toggled(bool value)

    implicitWidth: 48
    implicitHeight: 28
    width: implicitWidth
    height: implicitHeight

    // `enabled` is Item's own property; redeclaring it shadows the base member.
    opacity: root.enabled ? 1.0 : 0.4
    Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

    Accessible.role: Accessible.CheckBox
    Accessible.checked: root.checked
    Accessible.onToggleAction: root.activate()
    Accessible.onPressAction: root.activate()
    activeFocusOnTab: root.enabled
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.activate();
            event.accepted = true;
        }
    }

    // `checked` is not flipped here: the owner drives it from the backing property,
    // so a rejected or reverted change cannot leave the switch lying about state.
    function activate() {
        if (!root.enabled) return;
        root.toggled(!root.checked);
    }

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.textPrimary
                            : (mouseArea.containsMouse ? Theme.controlBgHover : Theme.cardHover)
        border.color: root.checked ? Theme.textPrimary
                                   : (mouseArea.containsMouse ? Theme.controlBorderHover : Theme.cardBorder)
        border.width: 1

        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 180 } }

        Rectangle {
            id: thumb
            width: parent.height - 6
            height: width
            radius: width / 2
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: root.checked ? Theme.bgDark : Theme.textSecondary
            scale: mouseArea.pressed ? 0.9 : 1.0

            Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }
            Behavior on color { ColorAnimation { duration: 180 } }
            Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.enabled
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.activate()
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: height / 2
        color: "transparent"
        border.color: Theme.textSecondary
        border.width: 2
        visible: root.activeFocus
    }
}
