// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Item {
    id: root
    z: 99999
    width: Math.min(parent ? parent.width - 48 : 360, 400)
    implicitHeight: card.implicitHeight
    height: card.implicitHeight
    visible: opacity > 0
    opacity: 0

    property string message: ""
    property string toastType: "info" // "info", "error", "success"

    transform: Translate {
        y: root.opacity > 0 ? 0 : -20
        Behavior on y {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    Timer {
        id: hideTimer
        interval: 3500
        repeat: false
        onTriggered: root.dismiss()
    }

    function show(msg, type, duration) {
        root.message = msg;
        root.toastType = type || "info";
        root.opacity = 1.0;
        hideTimer.interval = (duration && duration > 0) ? duration : 3500;
        hideTimer.restart();
    }

    function dismiss() {
        root.opacity = 0.0;
        hideTimer.stop();
    }

    Connections {
        target: (typeof toastManager !== "undefined") ? toastManager : null
        function onToastRequested(msg, type, duration) {
            root.show(msg, type, duration);
        }
    }

    Rectangle {
        id: card
        width: parent.width
        implicitHeight: Math.max(48, toastRow.implicitHeight + 24)
        height: implicitHeight
        color: "#18181B"
        border.color: root.toastType === "error" ? "#552222" : (root.toastType === "success" ? "#225533" : "#2E2E33")
        border.width: 1
        radius: 12

        RowLayout {
            id: toastRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 4
                Layout.preferredHeight: Math.max(20, msgText.implicitHeight)
                radius: 2
                color: root.toastType === "error" ? "#FF5555" : (root.toastType === "success" ? "#FFFFFF" : "#8E8E93")
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                id: msgText
                Layout.fillWidth: true
                text: root.message
                color: "#FFFFFF"
                font.pixelSize: 12
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: "✕"
                color: Theme.textMuted
                font.pixelSize: 12
                font.bold: true
                Layout.alignment: Qt.AlignVCenter

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.dismiss()
                }
            }
        }
    }
}
