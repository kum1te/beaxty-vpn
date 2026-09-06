// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Layouts
import ".."

// Placeholder card for "nothing here yet" states.
Rectangle {
    id: root

    property url iconSource: "qrc:/icons/nodes_list.svg"
    property string title: ""
    property string subtitle: ""
    property string actionText: ""

    signal actionTriggered()

    implicitHeight: 180
    height: implicitHeight
    radius: Theme.radiusMedium
    color: Theme.cardBg
    border.color: Theme.cardBorder
    border.width: 1

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        spacing: 10

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            sourceSize.width: 36
            sourceSize.height: 36
            source: root.iconSource
            opacity: 0.3
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: root.title
            color: Theme.textSecondary
            font.pixelSize: 14
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: root.subtitle
            color: Theme.textMuted
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: text.length > 0
        }

        Item { Layout.preferredHeight: 4 }

        ActionButton {
            Layout.alignment: Qt.AlignHCenter
            text: root.actionText
            primary: true
            visible: root.actionText.length > 0
            onClicked: root.actionTriggered()
        }
    }
}
