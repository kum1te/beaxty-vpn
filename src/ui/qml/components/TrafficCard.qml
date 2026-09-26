// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

Card {
    id: root

    property string downloadSpeed: "0.0 KB/s"
    property string uploadSpeed: "0.0 KB/s"
    property string totalTraffic: "0 B"

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 48
        spacing: 0

        // Download section
        Item {
            width: parent.width / 3
            height: parent.height

            Column {
                anchors.centerIn: parent
                spacing: 5

                Row {
                    spacing: 6
                    anchors.horizontalCenter: parent.horizontalCenter

                    Image {
                        width: 11
                        height: 11
                        source: Theme.icon("qrc:/icons/arrow_down.svg", Theme.isDark)
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: 0.7
                    }

                    Text {
                        text: "DOWN"
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.downloadSpeed
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: Theme.fontSans
                    font.letterSpacing: 0
                    font.bold: true
                }
            }
        }

        // Divider
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 36
            color: Theme.separator
        }

        // Upload section
        Item {
            width: parent.width / 3
            height: parent.height

            Column {
                anchors.centerIn: parent
                spacing: 5

                Row {
                    spacing: 6
                    anchors.horizontalCenter: parent.horizontalCenter

                    Image {
                        width: 11
                        height: 11
                        source: Theme.icon("qrc:/icons/arrow_up.svg", Theme.isDark)
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: 0.7
                    }

                    Text {
                        text: "UP"
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.uploadSpeed
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: Theme.fontSans
                    font.letterSpacing: 0
                    font.bold: true
                }
            }
        }

        // Divider
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 36
            color: Theme.separator
        }

        // Session Total
        Item {
            width: parent.width / 3
            height: parent.height

            Column {
                anchors.centerIn: parent
                spacing: 5

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "DATA"
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.totalTraffic
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: Theme.fontSans
                    font.letterSpacing: 0
                    font.bold: true
                }
            }
        }
    }
}
