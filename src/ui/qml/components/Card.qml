// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

Rectangle {
    id: root

    default property alias content: innerContainer.children
    property bool interactive: false
    property alias hovered: mouseArea.containsMouse
    // Draws a focus ring around the whole card. Subclasses cannot add one as a
    // child of their own, because `content` reparents children into innerContainer.
    property bool showFocusRing: false

    signal clicked()

    implicitWidth: innerContainer.implicitWidth + 32
    implicitHeight: innerContainer.implicitHeight + 32

    color: (interactive && mouseArea.containsMouse) ? Theme.cardHover : Theme.cardBg
    border.color: (interactive && mouseArea.containsMouse) ? Theme.textSecondary : Theme.cardBorder
    border.width: 1
    radius: Theme.radiusMedium

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    // Subtle top highlight rim for dark glass depth
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 1
        anchors.leftMargin: root.radius
        anchors.rightMargin: root.radius
        height: 1
        color: (root.interactive && mouseArea.containsMouse) ? Theme.rimHighlightHover : Theme.rimHighlight
        opacity: 0.6
        z: 2
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.interactive
        hoverEnabled: root.interactive
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }

    Item {
        id: innerContainer
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
        height: childrenRect.height
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.color: Theme.textSecondary
        border.width: 1
        visible: root.showFocusRing
        z: 3
    }
}
