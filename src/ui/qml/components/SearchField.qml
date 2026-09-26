// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

Rectangle {
    id: root

    property alias text: textInput.text
    property string placeholder: "Search nodes..."

    implicitHeight: 40
    height: implicitHeight
    radius: 12
    color: Theme.cardBg
    border.color: textInput.activeFocus ? Theme.textSecondary : Theme.cardBorder
    border.width: textInput.activeFocus ? 2 : 1

    Behavior on border.color { ColorAnimation { duration: 150 } }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            source: Theme.icon("qrc:/icons/search.svg", Theme.isDark)
            opacity: 0.5
        }

        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, parent.width - 52)
            height: parent.height

            TextInput {
                id: textInput
                anchors.fill: parent
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.textPrimary
                font.pixelSize: 13
                selectByMouse: true
                clip: true
                activeFocusOnTab: true
                Accessible.name: root.placeholder
                Accessible.searchEdit: true
                Keys.onEscapePressed: textInput.text = ""

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    text: root.placeholder
                    color: Theme.textMuted
                    font.pixelSize: 13
                    visible: !textInput.text && !textInput.activeFocus
                }
            }
        }

        // Clear button
        Image {
            id: clearButton
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            source: Theme.icon("qrc:/icons/close.svg", Theme.isDark)
            opacity: 0.6
            visible: textInput.text.length > 0
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Очистить поиск")
            Accessible.onPressAction: {
                textInput.text = ""
                textInput.forceActiveFocus()
            }
            activeFocusOnTab: visible
            Keys.onReturnPressed: {
                textInput.text = ""
                textInput.forceActiveFocus()
            }
            Keys.onSpacePressed: {
                textInput.text = ""
                textInput.forceActiveFocus()
            }

            MouseArea {
                id: clearMouseArea
                anchors.fill: parent
                anchors.margins: -5
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: {
                    textInput.text = "";
                    textInput.forceActiveFocus();
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: 5
                color: "transparent"
                border.color: Theme.textSecondary
                border.width: 2
                visible: clearButton.activeFocus
            }
        }
    }
}
