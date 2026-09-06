// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Layouts
import ".."

// One row of the sidebar navigation. Extracted so the four entries share a single
// definition instead of four copy-pasted blocks.
Rectangle {
    id: root

    property string label: ""
    property url iconSource: ""
    property bool active: false

    signal clicked()

    implicitHeight: 44
    radius: 8
    color: active ? Theme.cardBg : (mouseArea.containsMouse ? Theme.cardHover : "transparent")
    border.color: active ? Theme.cardBorder : "transparent"
    border.width: 1

    Behavior on color { ColorAnimation { duration: 180 } }
    Behavior on border.color { ColorAnimation { duration: 180 } }

    // Active marker: a short vertical bar on the leading edge.
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 1
        width: 2
        height: root.active ? 18 : 0
        radius: 1
        color: Theme.accentWhite
        opacity: root.active ? 0.9 : 0.0
        Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 12

        Image {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            sourceSize.width: 18
            sourceSize.height: 18
            source: root.iconSource
            opacity: root.active ? 1.0 : (mouseArea.containsMouse ? 0.75 : 0.5)
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        Text {
            Layout.fillWidth: true
            text: root.label
            elide: Text.ElideRight
            color: root.active ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: 13
            font.bold: root.active
            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    // Keyboard reachability: the row is focusable and activates on Space/Enter.
    Accessible.role: Accessible.Button
    Accessible.name: root.label
    Accessible.onPressAction: root.clicked()
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.clicked();
            event.accepted = true;
        }
    }

    // Visible focus ring, so tabbing through the sidebar is not invisible.
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.color: Theme.textSecondary
        border.width: 1
        visible: root.activeFocus
    }
}
