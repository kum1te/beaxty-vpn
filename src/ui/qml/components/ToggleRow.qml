// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Layouts
import ".."

// A settings row: title, optional badge, description, and a switch on the right.
// `content` takes extra controls placed under the description.
Card {
    id: root

    property string title: ""
    property string description: ""
    property string badge: ""
    property bool checked: false
    property bool switchEnabled: true

    default property alias extraContent: extraColumn.children

    signal toggled(bool value)

    Layout.fillWidth: true

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: root.title
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.width - 140
                }

                Rectangle {
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: badgeText.implicitWidth + 14
                    radius: 9
                    color: Theme.cardHover
                    border.color: Theme.cardBorder
                    border.width: 1
                    visible: root.badge.length > 0

                    Text {
                        id: badgeText
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
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                font.pixelSize: 11
                visible: text.length > 0
            }

            ColumnLayout {
                id: extraColumn
                Layout.fillWidth: true
                spacing: 8
            }
        }

        ToggleSwitch {
            Layout.alignment: Qt.AlignTop
            checked: root.checked
            enabled: root.switchEnabled
            onToggled: function(value) { root.toggled(value) }
        }
    }
}
