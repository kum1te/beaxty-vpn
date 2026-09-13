// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import "views"
import "components"

Window {
    id: window
    objectName: "appWindow"
    width: 960
    height: 640
    minimumWidth: 840
    minimumHeight: 560
    visible: true
    title: "BeaxtyVPN"
    color: Theme.bgDark

    function openImportSheet() {
        importSheet.open();
    }

    function closeImportSheet() {
        importSheet.close();
    }

    function setView(idx) {
        viewStack.currentIndex = idx;
    }

    // Called from C++ when the tray icon or a second launch asks for the window.
    function showAndRaise() {
        window.show();
        window.raise();
        window.requestActivate();
    }

    // Closing hides to tray rather than quitting (GOAL_PROMPT: minimize to tray on
    // close). Quit stays available from the tray menu.
    property bool quitting: false
    onClosing: function(close) {
        var toTray = (typeof throneEngine !== "undefined") ? throneEngine.closeToTray : true
        if (toTray && !window.quitting) {
            close.accepted = false;
            window.hide();
            if (typeof throneEngine !== "undefined") {
                throneEngine.notifyMinimizedToTray();
            }
        } else {
            close.accepted = true;
            if (typeof throneEngine !== "undefined") {
                throneEngine.requestQuit();
            }
            Qt.quit();
        }
    }

    // Ctrl+Q quits outright; Escape closes the import sheet.
    Shortcut {
        sequences: [StandardKey.Quit]
        onActivated: if (typeof throneEngine !== "undefined") throneEngine.requestQuit()
    }

    // Atmospheric Living Background (Dark Glass / Ambient Mesh)
    AmbientBackground {
        animationsEnabled: window.visible && window.visibility !== Window.Minimized
        z: 0
    }

    property bool sidebarCollapsed: (typeof appPrefs !== "undefined") ? appPrefs.sidebarCollapsed : false
    // All sidebar geometry follows this single interruptible animation.
    property real sidebarProgress: sidebarCollapsed ? 1 : 0
    Behavior on sidebarProgress {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }

    function toggleSidebar() {
        sidebarCollapsed = !sidebarCollapsed;
        if (typeof appPrefs !== "undefined") {
            appPrefs.sidebarCollapsed = sidebarCollapsed;
        }
    }

    RowLayout {
        enabled: !importSheet.visible
        z: 1
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // Left Navigation Sidebar (Desktop Layout)
        // ==========================================
        Rectangle {
            id: sidebar
            objectName: "sidebar"
            Layout.preferredWidth: 220 - 152 * window.sidebarProgress
            Layout.fillHeight: true
            color: Theme.isDark ? "#E608080A" : "#FAFAFA"
            clip: true


            // Right border line
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.separator
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 1. Sidebar Brand Header
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 70

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16 + window.sidebarProgress
                        anchors.rightMargin: parent.width - anchors.leftMargin - 192
                        spacing: 10



                        Rectangle {
                            id: logoBadge
                            width: 34
                            height: 34
                            radius: 9
                            color: Theme.isDark ? Theme.cardBg : Theme.cardHover
                            border.color: Theme.cardBorder
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                source: Theme.icon("qrc:/icons/app_icon.svg", Theme.isDark)
                                fillMode: Image.PreserveAspectFit
                            }

                            MouseArea {
                                id: logoMouseArea
                                anchors.fill: parent
                                hoverEnabled: window.sidebarCollapsed
                                cursorShape: window.sidebarCollapsed ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    if (window.sidebarCollapsed) {
                                        window.toggleSidebar();
                                    }
                                }
                            }

                            ToolTip {
                                id: logoToolTip
                                visible: window.sidebarCollapsed && logoMouseArea.containsMouse
                                text: qsTr("Развернуть панель")
                                delay: 300
                                timeout: 2500
                                contentItem: Text {
                                    text: logoToolTip.text
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                background: Rectangle {
                                    color: Theme.cardBg
                                    border.color: Theme.cardBorder
                                    border.width: 1
                                    radius: 6
                                }
                            }
                        }

                        Row {
                            Layout.fillWidth: true
                            spacing: 5
                            Layout.alignment: Qt.AlignVCenter
                            opacity: 1 - window.sidebarProgress


                            Text {
                                text: "beaxty"
                                color: Theme.textPrimary
                                font.pixelSize: 15
                                font.bold: true
                                font.letterSpacing: 0.5
                            }

                            Text {
                                text: "VPN"
                                color: Theme.textSecondary
                                font.pixelSize: 13
                                font.bold: true
                                font.family: Theme.fontMono
                                font.letterSpacing: 1.2
                            }
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16 - 8 * window.sidebarProgress
                        anchors.rightMargin: 16 - 8 * window.sidebarProgress
                        height: 1
                        color: Theme.separator


                    }
                }

                Item { Layout.preferredHeight: 14 }

                // 2. Vertical Navigation Menu
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12 - 4 * window.sidebarProgress
                    Layout.rightMargin: 12 - 4 * window.sidebarProgress
                    spacing: 6



                    Repeater {
                        model: [
                            { label: qsTr("Подключение"),   icon: "qrc:/icons/vpn_shield.svg" },
                            { label: qsTr("Серверы"),       icon: "qrc:/icons/nodes_list.svg" },
                            { label: qsTr("Маршрутизация"), icon: "qrc:/icons/routing_fork.svg" },
                            { label: qsTr("Настройки"),     icon: "qrc:/icons/settings_gear.svg" }
                        ]

                        NavItem {
                            required property int index
                            required property var modelData

                            Layout.fillWidth: true
                            label: modelData.label
                            iconSource: modelData.icon
                            active: viewStack.currentIndex === index
                            collapsed: window.sidebarCollapsed
                            collapseProgress: window.sidebarProgress
                            onClicked: viewStack.currentIndex = index
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Sidebar Collapse Toggle Button (in bottom section)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    Layout.leftMargin: 12 - 4 * window.sidebarProgress
                    Layout.rightMargin: 12 - 4 * window.sidebarProgress
                    Layout.bottomMargin: 8
                    radius: 8
                    color: toggleRowMa.containsMouse ? Theme.cardHover : "transparent"
                    border.color: toggleRowMa.containsMouse ? Theme.cardBorder : "transparent"
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14 + 4 * window.sidebarProgress
                        anchors.rightMargin: parent.width - anchors.leftMargin - 168
                        spacing: 12

                        Image {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            Layout.alignment: Qt.AlignVCenter
                            source: Theme.icon("qrc:/icons/sidebar_toggle.svg", Theme.isDark)
                            rotation: 180 * window.sidebarProgress
                            opacity: toggleRowMa.containsMouse ? 1.0 : 0.6
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }

                        Text {
                            Layout.fillWidth: true
                            opacity: 1 - window.sidebarProgress
                            text: qsTr("Свернуть")
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            font.bold: false
                            elide: Text.ElideRight

                        }
                    }

                    MouseArea {
                        id: toggleRowMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: window.toggleSidebar()
                    }

                    ToolTip {
                        id: toggleRowTip
                        visible: window.sidebarCollapsed && toggleRowMa.containsMouse
                        text: qsTr("Развернуть панель")
                        delay: 250
                        timeout: 2500
                        contentItem: Text {
                            text: toggleRowTip.text
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        background: Rectangle {
                            color: Theme.cardBg
                            border.color: Theme.cardBorder
                            border.width: 1
                            radius: 6
                        }
                    }
                }

                // 3. Sidebar Bottom Status Footer
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.separator
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    color: Theme.bgDark

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16 + 13 * window.sidebarProgress
                        anchors.rightMargin: parent.width - anchors.leftMargin - 188
                        spacing: 10



                        // Status dot
                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            width: 10
                            height: 10
                            radius: 5
                            color: (typeof throneEngine !== "undefined" && throneEngine.state === 2) ? Theme.accentWhite : 
                                   ((typeof throneEngine !== "undefined" && throneEngine.state === 1) ? Theme.textSecondary : "transparent")
                            border.color: Theme.textSecondary
                            border.width: 1

                            SequentialAnimation on opacity {
                                running: (typeof throneEngine !== "undefined" && throneEngine.state === 1)
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.2; duration: 600 }
                                NumberAnimation { from: 0.2; to: 1.0; duration: 600 }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            opacity: 1 - window.sidebarProgress
                            spacing: 1


                            Text {
                                text: (typeof throneEngine !== "undefined") ? throneEngine.stateString : "DISCONNECTED"
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                                font.letterSpacing: 1
                            }

                            Text {
                                text: (typeof throneEngine !== "undefined" && throneEngine.tunModeEnabled) ? "TUN ACTIVE" : "PROXY ONLY"
                                color: Theme.textMuted
                                font.pixelSize: 9
                                font.letterSpacing: 0.8
                            }
                        }

                        // TUN Badge
                        Rectangle {
                            opacity: 1 - window.sidebarProgress
                            height: 20
                            width: 44
                            radius: 10
                            color: Theme.cardBg
                            border.color: Theme.cardBorder
                            border.width: 1


                            Text {
                                anchors.centerIn: parent
                                text: (typeof throneEngine !== "undefined" && throneEngine.tunModeEnabled) ? "TUN" : "PROXY"
                                color: Theme.textSecondary
                                font.pixelSize: 8
                                font.bold: true
                            }
                        }
                    }

                    MouseArea {
                        id: statusFooterMa
                        anchors.fill: parent
                        hoverEnabled: window.sidebarCollapsed
                        cursorShape: window.sidebarCollapsed ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (window.sidebarCollapsed) window.toggleSidebar()
                        }
                    }

                    ToolTip {
                        id: footerStatusTip
                        visible: window.sidebarCollapsed && statusFooterMa.containsMouse
                        text: (typeof throneEngine !== "undefined") ? (throneEngine.stateString + (throneEngine.tunModeEnabled ? " • TUN" : " • PROXY")) : "DISCONNECTED"
                        delay: 250
                        timeout: 3000
                        contentItem: Text {
                            text: footerStatusTip.text
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        background: Rectangle {
                            color: Theme.cardBg
                            border.color: Theme.cardBorder
                            border.width: 1
                            radius: 6
                        }
                    }
                }
            }
        }

        // ==========================================
        // Right Main Content Area (StackLayout)
        // ==========================================
        StackLayout {
            id: viewStack
            objectName: "viewStack"
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0

            SectionPage {
                DashboardView {
                    anchors.fill: parent
                    onRequestNodesView: viewStack.currentIndex = 1
                    onRequestRoutingView: viewStack.currentIndex = 2
                }
            }

            SectionPage {
                NodesView {
                    anchors.fill: parent
                    onRequestImport: window.openImportSheet()
                }
            }

            SectionPage {
                RoutingView { anchors.fill: parent }
            }

            SectionPage {
                SettingsView { anchors.fill: parent }
            }
        }
    }

    // Floating in-app Toast Notification Overlay
    ToastNotification {
        id: toast
        objectName: "toastNotification"
        z: 99999
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 24
        anchors.rightMargin: 28
    }

    // Bottom Sheet Import Modal Overlay
    ImportSheet {
        id: importSheet
    }
}
