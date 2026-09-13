// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import ".."

FocusScope {
    id: root
    objectName: "importSheet"
    anchors.fill: parent
    z: 1000
    visible: opacity > 0
    opacity: 0
    enabled: visible
    readonly property bool busy: typeof configAdapter !== "undefined" && configAdapter.importing
    property var previousFocus: null
    Keys.onEscapePressed: close()
    onVisibleChanged: if (!visible && previousFocus) previousFocus.forceActiveFocus()

    function submit() {
        if (!busy && inputField.text.trim().length > 0 && typeof configAdapter !== "undefined")
            configAdapter.importSubscription(inputField.text.trim(), groupNameField.text.trim());
    }

    Shortcut {
        sequences: ["Ctrl+Return", "Ctrl+Enter"]
        enabled: root.visible && !root.busy
        onActivated: root.submit()
    }

    Behavior on opacity {
        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
    }

    function open() {
        if (busy) return;
        previousFocus = root.Window.window ? root.Window.window.activeFocusItem : null;
        inputField.text = "";
        groupNameField.text = "";
        root.opacity = 1.0;
        sheetScroll.contentItem.contentY = 0;
        inputField.forceActiveFocus();
    }

    function close() {
        if (busy) return;
        root.opacity = 0.0;
    }

    readonly property string detectedProtocol: {
        var txt = inputField.text.trim().toLowerCase();
        if (!txt) return "";
        if (txt.startsWith("vless://")) return "VLESS";
        if (txt.startsWith("vmess://")) return "VMESS";
        if (txt.startsWith("ss://")) return "SHADOWSOCKS";
        if (txt.startsWith("trojan://")) return "TROJAN";
        if (txt.startsWith("hy2://") || txt.startsWith("hysteria2://")) return "HYSTERIA2";
        if (txt.startsWith("tuic://")) return "TUIC";
        if (txt.startsWith("wireguard://") || txt.startsWith("wg://")) return "WIREGUARD";
        if (txt.startsWith("http://") || txt.startsWith("https://")) return "HTTP SUBSCRIPTION";
        if (txt.length > 30 && !txt.includes(" ") && !txt.includes("\n")) return "BASE64 SUBSCRIPTION";
        return "CUSTOM CONFIG";
    }

    Connections {
        target: (typeof configAdapter !== "undefined") ? configAdapter : null
        function onImportFinished(success, count, message) {
            if (success) {
                root.opacity = 0.0;
            }
        }
    }

    // Dimmed Backdrop
    Rectangle {
        anchors.fill: parent
        color: Theme.isDark ? "#D9000000" : "#66000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    // Bottom Sheet Container
    Rectangle {
        id: sheet
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 48, 640)
        height: Math.min(480, parent.height * 0.85)
        y: root.opacity > 0 ? (root.height - height - (parent.width > 688 ? 24 : 0)) : root.height
        color: Theme.cardBg
        border.color: Theme.cardBorder
        border.width: 1
        radius: 20

        Behavior on y {
            NumberAnimation { duration: 280; easing.type: Easing.OutCubic }
        }

        // Prevent click propagation through the sheet
        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

        ScrollView {
            id: sheetScroll
            anchors.fill: parent
            anchors.margins: 24
            clip: true
            contentWidth: availableWidth

            Column {
                width: sheetScroll.availableWidth
                spacing: 16

                // Grabber Handle
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 38
                    height: 4
                    radius: 2
                    color: Theme.controlBorder
                }

                // Header row
                Row {
                    width: parent.width
                    spacing: 12

                    Text {
                        text: qsTr("Импорт конфигурации")
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 40
                    }

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: closeMa.containsMouse ? Theme.cardHover : "transparent"
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: Theme.icon("qrc:/icons/close.svg", Theme.isDark)
                            opacity: closeMa.containsMouse ? 0.9 : 0.5
                        }

                        MouseArea {
                            id: closeMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: root.close()
                        }
                    }
                }

                // Group Name input (optional)
                Column {
                    width: parent.width
                    spacing: 6

                    Text {
                        text: qsTr("НАЗВАНИЕ ГРУППЫ (ОПЦИОНАЛЬНО)")
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: Theme.textMuted
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        color: Theme.isDark ? "#0D0D0E" : Theme.bgDark
                        border.color: groupNameField.activeFocus ? Theme.accentWhite : Theme.cardBorder
                        border.width: 1
                        radius: 8

                        TextInput {
                            id: groupNameField
                            activeFocusOnTab: true
                            KeyNavigation.tab: inputField
                            KeyNavigation.backtab: importButton
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            verticalAlignment: TextInput.AlignVCenter
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            enabled: !root.busy
                            selectByMouse: true
                            clip: true

                            Text {
                                text: qsTr("Автоопределение или свое название")
                                color: Theme.textMuted
                                font.pixelSize: 13
                                visible: !groupNameField.text && !groupNameField.activeFocus
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }

                // URL or Raw Config input
                Column {
                    width: parent.width
                    spacing: 6

                    Item {
                        width: parent.width
                        height: 26

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("ССЫЛКА НА ПОДПИСКУ ИЛИ КЛЮЧ")
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: Theme.textMuted
                        }

                        // Fast Clipboard Paste Button
                        Rectangle {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: pasteRow.implicitWidth + 20
                            height: 26
                            color: pasteArea.containsMouse ? Theme.cardHover : Theme.bgElevated
                            border.color: Theme.cardBorder
                            border.width: 1
                            radius: 6

                            Row {
                                id: pasteRow
                                anchors.centerIn: parent
                                spacing: 6

                                Image {
                                    width: 12
                                    height: 12
                                    source: Theme.icon("qrc:/icons/clipboard.svg", Theme.isDark)
                                    anchors.verticalCenter: parent.verticalCenter
                                    opacity: pasteArea.containsMouse ? 0.9 : 0.6
                                }

                                Text {
                                    text: qsTr("Вставить")
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                id: pasteArea
                                enabled: !root.busy
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof configAdapter !== "undefined") {
                                        var clip = configAdapter.getClipboardText();
                                        if (clip && clip.length > 0) {
                                            inputField.text = clip;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 110
                        color: Theme.isDark ? "#0D0D0E" : Theme.bgDark
                        border.color: inputField.activeFocus ? Theme.accentWhite : Theme.cardBorder
                        border.width: 1
                        radius: 8

                        ScrollView {
                            id: inputScroll
                            anchors.fill: parent
                            anchors.margins: 8
                            clip: true

                            TextArea {
                                id: inputField
                                KeyNavigation.tab: cancelButton
                                KeyNavigation.backtab: groupNameField
                                objectName: "inputField"
                                width: inputScroll.availableWidth
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                font.family: Theme.fontMono
                                wrapMode: TextEdit.WrapAnywhere
                                enabled: !root.busy
                            selectByMouse: true
                                placeholderText: qsTr("Вставьте ссылку https://... или vless://, vmess://, ss://, trojan://...")
                                placeholderTextColor: Theme.textMuted
                                background: null
                            }
                        }
                    }
                }

                // Protocol Detection Badge
                Row {
                    width: parent.width
                    spacing: 8
                    visible: root.detectedProtocol !== ""

                    Text {
                        text: qsTr("Определено:")
                        font.pixelSize: 11
                        color: Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        height: 22
                        width: protoBadgeText.implicitWidth + 14
                        radius: 4
                        color: Theme.cardHover
                        border.color: Theme.cardBorder
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            id: protoBadgeText
                            anchors.centerIn: parent
                            text: root.detectedProtocol
                            color: Theme.textPrimary
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }

                Item { height: 6; width: 1 }

                // Action Buttons
                Row {
                    width: parent.width
                    spacing: 12

                    ActionButton {
                        id: cancelButton
                        width: (parent.width - 12) * 0.35
                        height: 44
                        text: qsTr("Отмена")
                        enabled: !root.busy
                        KeyNavigation.tab: importButton
                        onClicked: root.close()
                    }

                    ActionButton {
                        id: importButton
                        width: (parent.width - 12) * 0.65
                        height: 44
                        primary: true
                        busy: root.busy
                        enabled: inputField.text.trim().length > 0
                        text: root.busy ? qsTr("Загрузка...") : qsTr("Импортировать")
                        KeyNavigation.tab: groupNameField
                        onClicked: root.submit()
                    }
                }
            }
        }
    }
}
