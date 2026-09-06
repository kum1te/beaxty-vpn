// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Layouts
import ".."

// A radio-style routing preset card: indicator, title, optional badge, description.
Card {
    id: root

    property string title: ""
    property string description: ""
    property string badge: ""
    property bool selected: false

    signal picked()

    Layout.fillWidth: true
    interactive: true
    onClicked: root.picked()
    showFocusRing: root.activeFocus

    border.color: root.selected ? Theme.accentWhite
                                : (root.hovered ? Theme.textSecondary : Theme.cardBorder)

    Accessible.role: Accessible.RadioButton
    Accessible.name: root.title
    Accessible.checked: root.selected
    Accessible.onPressAction: root.picked()
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.picked();
            event.accepted = true;
        }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            Layout.alignment: Qt.AlignTop
            radius: 11
            color: "transparent"
            border.color: root.selected ? Theme.accentWhite : Theme.textMuted
            border.width: root.selected ? 2 : 1

            Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

            Rectangle {
                anchors.centerIn: parent
                width: root.selected ? 10 : 0
                height: width
                radius: width / 2
                color: Theme.accentWhite
                Behavior on width { NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic } }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: root.title
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.width - 160
                }

                Rectangle {
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: badgeLabel.implicitWidth + 14
                    radius: 9
                    color: Theme.cardHover
                    border.color: Theme.cardBorder
                    border.width: 1
                    visible: root.badge.length > 0

                    Text {
                        id: badgeLabel
                        anchors.centerIn: parent
                        text: root.badge
                        color: Theme.accentWhite
                        font.pixelSize: 8
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Text {
                Layout.fillWidth: true
                text: root.description
                color: Theme.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
