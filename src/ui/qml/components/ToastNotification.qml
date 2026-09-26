// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

Item {
    id: root
    z: 99999
    width: Math.min(parent ? parent.width - 48 : 360,
                    Math.max(240, messageMetrics.advanceWidth + 76))
    implicitHeight: card.implicitHeight
    height: card.implicitHeight
    visible: true
    enabled: opacity > 0.01
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

    TextMetrics {
        id: messageMetrics
        text: root.message
        font: msgText.font
    }

    Rectangle {
        id: card
        width: parent.width
        implicitHeight: Math.max(48, toastRow.implicitHeight + 24)
        height: implicitHeight
        color: Theme.cardBg
        border.color: root.toastType === "error" ? (Theme.isDark ? "#7F1D1D" : "#FECACA")
                     : (root.toastType === "success" ? (Theme.isDark ? "#166534" : "#BBF7D0") : Theme.cardBorder)
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
                color: root.toastType === "error" ? (Theme.isDark ? "#F87171" : "#B91C1C")
                     : (root.toastType === "success" ? (Theme.isDark ? "#4ADE80" : "#15803D") : Theme.textSecondary)
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                id: msgText
                Layout.fillWidth: true
                text: root.message
                color: Theme.textPrimary
                font.pixelSize: 12
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                id: closeButton
                objectName: "toastCloseButton"
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "✕"
                color: closeArea.containsMouse ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: 14
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Закрыть уведомление")
                Accessible.onPressAction: root.dismiss()
                activeFocusOnTab: true
                Keys.onReturnPressed: root.dismiss()
                Keys.onSpacePressed: root.dismiss()

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.dismiss()
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: "transparent"
                    border.color: Theme.textSecondary
                    border.width: 2
                    visible: closeButton.activeFocus
                }
            }
        }
    }
}
