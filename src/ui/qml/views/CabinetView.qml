// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    objectName: "cabinetView"

    readonly property string cabinetUrl: "https://cabinet.beaxty.com"
    readonly property bool hasWebEngineSupport: (typeof hasWebEngine !== "undefined") ? Boolean(hasWebEngine) : false
    property bool userWantsExternal: (typeof appPrefs !== "undefined") ? appPrefs.getBool("cabinet_external_browser", false) : false
    readonly property bool useExternalBrowser: userWantsExternal || !hasWebEngineSupport
    property bool memorySaverMode: (typeof appPrefs !== "undefined") ? appPrefs.getBool("cabinet_memory_saver", true) : true
    readonly property bool isCurrentTab: root.visible

    // Check window visibility from QML Window
    readonly property bool isWindowActive: (Window.window ? (Window.window.visible && Window.window.visibility !== Window.Minimized) : true)

    // Memory saver timer: unload web engine after 3 minutes of inactivity when tab is switched away or window minimized
    Timer {
        id: discardTimer
        interval: 180000 // 3 minutes
        repeat: false
        running: root.hasWebEngineSupport && (!root.isCurrentTab || !root.isWindowActive) && root.memorySaverMode && webLoader.status === Loader.Ready
        onTriggered: {
            if (!root.isCurrentTab || !root.isWindowActive) {
                console.log("[CabinetView] Memory saver triggered: unloading WebEngineView to free RAM");
                webLoader.active = false;
            }
        }
    }

    onIsCurrentTabChanged: {
        if (isCurrentTab && root.hasWebEngineSupport && !useExternalBrowser) {
            discardTimer.stop();
            if (!webLoader.active) {
                webLoader.active = true;
            }
        }
    }

    Connections {
        target: (typeof appPrefs !== "undefined") ? appPrefs : null
        function onPrefChanged(key) {
            if (key === "cabinet_external_browser") {
                root.userWantsExternal = appPrefs.getBool("cabinet_external_browser", false);
                if (root.useExternalBrowser) {
                    webLoader.active = false;
                } else if (root.isCurrentTab && root.hasWebEngineSupport) {
                    webLoader.active = true;
                }
            } else if (key === "cabinet_memory_saver") {
                root.memorySaverMode = appPrefs.getBool("cabinet_memory_saver", true);
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. Native Secure Browser Navigation Header
        // ==========================================
        Rectangle {
            id: browserToolbar
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: Theme.isDark ? "#0E0E11" : "#F3F4F6"
            z: 10

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.separator
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10

                // Back Button
                Rectangle {
                    width: 32
                    height: 32
                    radius: 6
                    color: backMa.containsMouse ? Theme.cardHover : "transparent"
                    border.color: backMa.containsMouse ? Theme.cardBorder : "transparent"
                    border.width: 1
                    opacity: (webLoader.item && webLoader.item.canGoBack) ? 1.0 : 0.4

                    Image {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        rotation: 90
                        source: Theme.icon("qrc:/icons/arrow_down.svg", Theme.isDark)
                    }

                    MouseArea {
                        id: backMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: (webLoader.item && webLoader.item.canGoBack) ? Qt.PointingHandCursor : Qt.ArrowCursor
                        enabled: webLoader.item && webLoader.item.canGoBack
                        onClicked: if (webLoader.item) webLoader.item.goBack()
                    }
                }

                // Forward Button
                Rectangle {
                    width: 32
                    height: 32
                    radius: 6
                    color: fwdMa.containsMouse ? Theme.cardHover : "transparent"
                    border.color: fwdMa.containsMouse ? Theme.cardBorder : "transparent"
                    border.width: 1
                    opacity: (webLoader.item && webLoader.item.canGoForward) ? 1.0 : 0.4

                    Image {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        rotation: 270
                        source: Theme.icon("qrc:/icons/arrow_down.svg", Theme.isDark)
                    }

                    MouseArea {
                        id: fwdMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: (webLoader.item && webLoader.item.canGoForward) ? Qt.PointingHandCursor : Qt.ArrowCursor
                        enabled: webLoader.item && webLoader.item.canGoForward
                        onClicked: if (webLoader.item) webLoader.item.goForward()
                    }
                }

                // Reload / Stop Button
                Rectangle {
                    width: 32
                    height: 32
                    radius: 6
                    color: reloadMa.containsMouse ? Theme.cardHover : "transparent"
                    border.color: reloadMa.containsMouse ? Theme.cardBorder : "transparent"
                    border.width: 1

                    Image {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        source: (webLoader.item && webLoader.item.loading) ?
                                    Theme.icon("qrc:/icons/close.svg", Theme.isDark) :
                                    Theme.icon("qrc:/icons/refresh.svg", Theme.isDark)
                    }

                    MouseArea {
                        id: reloadMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!webLoader.active) {
                                webLoader.active = true;
                            } else if (webLoader.item) {
                                if (webLoader.item.loading) {
                                    webLoader.item.stop();
                                } else {
                                    webLoader.item.reload();
                                }
                            }
                        }
                    }
                }

                // URL & Security Badge Bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    radius: 8
                    color: Theme.cardBg
                    border.color: Theme.cardBorder
                    border.width: 1
                    clip: true

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        // SSL Security Lock Badge
                        Row {
                            spacing: 6
                            Layout.alignment: Qt.AlignVCenter

                            Image {
                                width: 12
                                height: 12
                                source: Theme.icon("qrc:/icons/lock.svg", Theme.isDark)
                                anchors.verticalCenter: parent.verticalCenter
                                opacity: 0.8
                            }

                            Text {
                                text: qsTr("Защищено")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Rectangle {
                            width: 1
                            height: 14
                            color: Theme.separator
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Domain display
                        Text {
                            Layout.fillWidth: true
                            text: (webLoader.item && webLoader.item.url) ? webLoader.item.url.toString() : root.cabinetUrl
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.family: Theme.fontMono
                            elide: Text.ElideRight
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                // Open in External Browser Button
                Rectangle {
                    id: extButton
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: extRow.implicitWidth + 20
                    radius: 8
                    color: extMa.containsMouse ? Theme.cardHover : Theme.cardBg
                    border.color: extMa.containsMouse ? Theme.textSecondary : Theme.cardBorder
                    border.width: 1

                    Row {
                        id: extRow
                        anchors.centerIn: parent
                        spacing: 6

                        Image {
                            width: 13
                            height: 13
                            source: Theme.icon("qrc:/icons/routing_fork.svg", Theme.isDark)
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: 0.8
                        }

                        Text {
                            text: qsTr("В браузере")
                            color: Theme.textPrimary
                            font.pixelSize: 11
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: extMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var current = (webLoader.item && webLoader.item.url) ? webLoader.item.url : root.cabinetUrl;
                            Qt.openUrlExternally(current);
                        }
                    }
                }
            }
        }

        // Loading Progress Bar
        Rectangle {
            Layout.fillWidth: true
            height: (webLoader.item && webLoader.item.loading) ? 2 : 0
            color: Theme.accentWhite
            visible: height > 0
            z: 11

            Rectangle {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: parent.width * ((webLoader.item && typeof webLoader.item.loadProgress !== "undefined") ? (webLoader.item.loadProgress / 100.0) : 0.1)
                color: Theme.accentWhite

                Behavior on width {
                    NumberAnimation { duration: 150 }
                }
            }
        }

        // ==========================================
        // 2. Main Web / Fallback Container
        // ==========================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Fallback / External Mode Card
            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 560)
                height: fallbackCol.implicitHeight + 48
                radius: Theme.radiusMedium
                color: Theme.cardBg
                border.color: Theme.cardBorder
                border.width: 1
                visible: root.useExternalBrowser || !webLoader.active || (webLoader.status === Loader.Error)
                z: 1

                // Subtle top highlight rim
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.topMargin: 1
                    anchors.leftMargin: parent.radius
                    anchors.rightMargin: parent.radius
                    height: 1
                    color: Theme.rimHighlight
                    opacity: 0.6
                }

                Column {
                    id: fallbackCol
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 16

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 56
                        height: 56
                        radius: 16
                        color: Theme.cardHover
                        border.color: Theme.cardBorder
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            source: Theme.icon("qrc:/icons/cabinet.svg", Theme.isDark)
                        }
                    }

                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        spacing: 6

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Личный кабинет beaxty VPN")
                            color: Theme.textPrimary
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: !root.hasWebEngineSupport ?
                                  qsTr("Встроенный веб-движок недоступен в данной сборке. Личный кабинет открывается в системном браузере.") :
                                  (root.userWantsExternal ?
                                      qsTr("Включен режим открытия во внешнем браузере.") :
                                      qsTr("Управляйте подпиской, продлевайте доступ и просматривайте статистику."))
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            width: Math.min(parent.width, 480)
                        }
                    }

                    ActionButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Открыть в браузере (cabinet.beaxty.com)")
                        primary: true
                        onClicked: Qt.openUrlExternally(root.cabinetUrl)
                    }

                    Item {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        height: 24
                        visible: root.hasWebEngineSupport && root.userWantsExternal

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("Вы можете включить встроенный браузер в Настройках приложения.")
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }
                    }
                }
            }

            // Lazy WebEngineView Loader (dynamically loaded via source string to prevent QML parse failures if QtWebEngine is missing)
            Loader {
                id: webLoader
                anchors.fill: parent
                active: false // Critical requirement: lazy loading, 0MB on launch
                asynchronous: true
                visible: root.hasWebEngineSupport && !root.useExternalBrowser && status === Loader.Ready
                source: (root.hasWebEngineSupport && !root.useExternalBrowser) ? "CabinetWebEngineComponent.qml" : ""
            }

            Connections {
                target: webLoader.item
                ignoreUnknownSignals: true
                function onDeepLinkTriggered(url) {
                    if (typeof deepLinkManager !== "undefined") {
                        deepLinkManager.handleDeepLink(url);
                    }
                }
                function onExternalUrlTriggered(url) {
                    Qt.openUrlExternally(url);
                }
            }

            // Offline / Error Overlay
            Rectangle {
                anchors.fill: parent
                color: Theme.bgDark
                visible: root.hasWebEngineSupport && !root.useExternalBrowser && webLoader.item && webLoader.item.hasError
                z: 5

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 16
                    width: Math.min(parent.width - 64, 440)

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 52
                        height: 52
                        radius: 26
                        color: Theme.cardBg
                        border.color: Theme.cardBorder
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            source: Theme.icon("qrc:/icons/close.svg", Theme.isDark)
                            opacity: 0.7
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Не удалось загрузить личный кабинет")
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Проверьте подключение к интернету или откройте кабинет через системный браузер.")
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12

                        ActionButton {
                            text: qsTr("Повторить")
                            onClicked: {
                                if (webLoader.item) {
                                    webLoader.item.hasError = false;
                                    webLoader.item.reload();
                                }
                            }
                        }

                        ActionButton {
                            text: qsTr("В браузере")
                            primary: true
                            onClicked: Qt.openUrlExternally(root.cabinetUrl)
                        }
                    }
                }
            }
        }
    }
}
