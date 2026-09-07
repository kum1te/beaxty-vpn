// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: root
    objectName: "nodesView"

    function openDeleteDialog(groupId, groupName) {
        deleteDialog.targetGroupId = groupId;
        deleteDialog.targetGroupName = groupName;
        deleteDialog.open();
    }

    function closeDeleteDialog() {
        deleteDialog.close();
    }

    property string filterText: searchField.text.toLowerCase().trim()
    signal requestImport()

    // Shared filter, so the group list and the custom list cannot drift apart.
    function matchesFilter(s) {
        if (!root.filterText) return true;
        const needle = root.filterText;
        return (s.name || "").toLowerCase().includes(needle)
            || (s.type || "").toLowerCase().includes(needle)
            || (s.country || "").toLowerCase().includes(needle)
            || (s.address || "").toLowerCase().includes(needle);
    }

    function applySort(list) {
        if (!list || list.length <= 1) return list;
        var mode = (typeof configAdapter !== "undefined") ? configAdapter.serverSortMode : 0;
        if (mode === 0) return list;
        var copy = list.slice();
        if (mode === 1) {
            // By ping: lowest positive ping first (> 0), then 0 / untested (--), then negative / unreachable (-1)
            copy.sort(function(a, b) {
                var pa = (typeof a.ping === "number") ? a.ping : 0;
                var pb = (typeof b.ping === "number") ? b.ping : 0;
                var catA = (pa > 0) ? 1 : (pa === 0 ? 2 : 3);
                var catB = (pb > 0) ? 1 : (pb === 0 ? 2 : 3);
                if (catA !== catB) return catA - catB;
                if (catA === 1) return pa - pb;
                return (a.name || "").localeCompare(b.name || "");
            });
        } else if (mode === 2) {
            // Alphabetical / Country (A-Z)
            copy.sort(function(a, b) {
                var cA = a.country || "";
                var cB = b.country || "";
                if (cA !== cB) return cA.localeCompare(cB);
                return (a.name || "").localeCompare(b.name || "");
            });
        }
        return copy;
    }

    // Cached once per servers change instead of re-querying C++ inside bindings.
    property var customNodesAll: []
    readonly property var customNodes: {
        var _sort = (typeof configAdapter !== "undefined") ? configAdapter.serverSortMode : 0;
        return root.applySort(customNodesAll.filter(root.matchesFilter));
    }

    function refreshCustomNodes() {
        customNodesAll = (typeof configAdapter !== "undefined") ? configAdapter.customServers() : [];
    }

    Component.onCompleted: root.refreshCustomNodes()

    Connections {
        target: (typeof configAdapter !== "undefined") ? configAdapter : null
        function onServersChanged() { root.refreshCustomNodes(); }
    }

    Item {
        id: container
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(parent.width - 48, 920)

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 20
            anchors.bottomMargin: 24
            spacing: 16

            // Top action bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                SearchField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholder: qsTr("Поиск узлов и подписок...")
                }

                ActionButton {
                    text: qsTr("Обновить все")
                    onClicked: {
                        if (typeof configAdapter !== "undefined") configAdapter.refreshSubscriptions()
                    }
                }

                ActionButton {
                    busy: (typeof trafficMonitor !== "undefined") && trafficMonitor.isTestingPing
                    text: busy ? qsTr("Тест...") : qsTr("Пинг")
                    onClicked: {
                        if (typeof trafficMonitor !== "undefined") trafficMonitor.testAllPings()
                    }
                }

                ActionButton {
                    id: sortBtn
                    property int sortMode: (typeof configAdapter !== "undefined") ? configAdapter.serverSortMode : 0
                    text: sortMode === 1 ? qsTr("Пинг ⚡") : (sortMode === 2 ? qsTr("А-Я 🔤") : qsTr("Сортировка"))
                    onClicked: {
                        if (typeof configAdapter !== "undefined") {
                            configAdapter.serverSortMode = (configAdapter.serverSortMode + 1) % 3;
                        }
                    }
                }

                ActionButton {
                    text: qsTr("+ Добавить")
                    primary: true
                    onClicked: root.requestImport()
                }
            }

            ScrollView {
                id: nodesScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                Column {
                    width: nodesScrollView.width
                    spacing: 18

                    // ============================================================
                    // Subscription groups
                    // ============================================================
                    Repeater {
                        model: (typeof configAdapter !== "undefined") ? configAdapter.groups : []

                        delegate: Rectangle {
                            id: groupCard
                            required property var modelData
                            required property int index

                            width: parent.width
                            implicitHeight: groupCardCol.implicitHeight + 36
                            height: implicitHeight
                            radius: Theme.radiusMedium
                            color: Theme.cardBg
                            border.color: Theme.cardBorder
                            border.width: 1

                            // Top highlight rim for dark-glass depth
                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.topMargin: 1
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                height: 1
                                color: Theme.rimHighlight
                                opacity: 0.6
                            }

                            readonly property var groupServers: {
                                if (typeof configAdapter === "undefined") return []
                                var _dummy = configAdapter.servers
                                var _sort = configAdapter.serverSortMode
                                return root.applySort(configAdapter.serversForGroup(modelData.id).filter(root.matchesFilter))
                            }

                            visible: !root.filterText
                                     || groupServers.length > 0
                                     || (modelData.name || "").toLowerCase().includes(root.filterText)

                            Column {
                                id: groupCardCol
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 18
                                spacing: 14

                                // ---- Subscription header ----
                                RowLayout {
                                    width: parent.width
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: 34
                                        Layout.preferredHeight: 34
                                        radius: 8
                                        color: Theme.bgElevated
                                        border.color: Theme.cardBorder
                                        border.width: 1

                                        Image {
                                            anchors.centerIn: parent
                                            width: 17
                                            height: 17
                                            sourceSize.width: 17
                                            sourceSize.height: 17
                                            source: Theme.icon("qrc:/icons/nodes_list.svg", Theme.isDark)
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            Text {
                                                text: modelData.name || qsTr("Подписка")
                                                color: Theme.textPrimary
                                                font.pixelSize: 16
                                                font.bold: true
                                                elide: Text.ElideRight
                                                Layout.maximumWidth: groupCard.width - 290
                                            }

                                            // Edit/Rename subscription button
                                            Rectangle {
                                                Layout.preferredWidth: 22
                                                Layout.preferredHeight: 22
                                                radius: 6
                                                color: editNameArea.containsMouse ? Theme.cardHover : "transparent"
                                                border.color: editNameArea.containsMouse ? Theme.cardBorder : "transparent"
                                                border.width: 1

                                                Accessible.role: Accessible.Button
                                                Accessible.name: qsTr("Переименовать подписку")

                                                Image {
                                                    anchors.centerIn: parent
                                                    width: 11
                                                    height: 11
                                                    sourceSize.width: 11
                                                    sourceSize.height: 11
                                                    source: Theme.icon("qrc:/icons/edit.svg", Theme.isDark)
                                                    opacity: editNameArea.containsMouse ? 1.0 : 0.65
                                                }

                                                MouseArea {
                                                    id: editNameArea
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        renameDialog.targetGroupId = modelData.id;
                                                        renameDialog.initialName = modelData.name || "";
                                                        renameDialog.open();
                                                    }
                                                }
                                            }

                                            // Active badge: a filled dot, no colour coding.
                                            Rectangle {
                                                Layout.preferredHeight: 22
                                                Layout.preferredWidth: statusRow.implicitWidth + 14
                                                radius: 11
                                                color: Theme.cardHover
                                                border.color: Theme.cardBorder
                                                border.width: 1

                                                Row {
                                                    id: statusRow
                                                    anchors.centerIn: parent
                                                    spacing: 5

                                                    Rectangle {
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        width: 6
                                                        height: 6
                                                        radius: 3
                                                        color: Theme.accentWhite
                                                    }

                                                    Text {
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        text: modelData.status || qsTr("Активна")
                                                        color: Theme.textPrimary
                                                        font.pixelSize: 10
                                                        font.bold: true
                                                    }
                                                }
                                            }

                                            Rectangle {
                                                Layout.preferredHeight: 22
                                                Layout.preferredWidth: countText.implicitWidth + 14
                                                radius: 11
                                                color: Theme.bgElevated
                                                border.color: Theme.cardBorder
                                                border.width: 1

                                                Text {
                                                    id: countText
                                                    anchors.centerIn: parent
                                                    text: qsTr("%n узлов", "", modelData.count !== undefined
                                                                                ? modelData.count
                                                                                : groupCard.groupServers.length)
                                                    color: Theme.textSecondary
                                                    font.pixelSize: 10
                                                    font.weight: Font.DemiBold
                                                }
                                            }

                                            Item { Layout.fillWidth: true }
                                        }

                                        Text {
                                            text: modelData.url || ""
                                            color: Theme.textMuted
                                            font.pixelSize: 11
                                            font.family: Theme.fontMono
                                            elide: Text.ElideMiddle
                                            visible: text.length > 0 && text.startsWith("http")
                                            Layout.maximumWidth: groupCard.width - 150
                                        }
                                    }

                                    // Refresh this subscription
                                    Rectangle {
                                        Layout.preferredWidth: 32
                                        Layout.preferredHeight: 32
                                        radius: 8
                                        color: refreshArea.containsMouse ? Theme.controlBgHover : Theme.bgElevated
                                        border.color: refreshArea.containsMouse ? Theme.controlBorderHover : Theme.cardBorder
                                        border.width: 1

                                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                                        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                                        Accessible.role: Accessible.Button
                                        Accessible.name: qsTr("Обновить подписку")

                                        Image {
                                            id: refreshIcon
                                            anchors.centerIn: parent
                                            width: 14
                                            height: 14
                                            sourceSize.width: 14
                                            sourceSize.height: 14
                                            source: Theme.icon("qrc:/icons/refresh.svg", Theme.isDark)
                                            opacity: refreshArea.containsMouse ? 1.0 : 0.7

                                            RotationAnimation {
                                                id: rotateAnim
                                                target: refreshIcon
                                                from: 0
                                                to: 360
                                                duration: 700
                                                running: false
                                                loops: 1
                                            }
                                        }

                                        MouseArea {
                                            id: refreshArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                rotateAnim.restart()
                                                if (typeof configAdapter !== "undefined") {
                                                    configAdapter.updateGroup(modelData.id)
                                                }
                                            }
                                        }
                                    }

                                    // Delete this subscription
                                    Rectangle {
                                        Layout.preferredWidth: 32
                                        Layout.preferredHeight: 32
                                        radius: 8
                                        color: delGroupArea.containsMouse ? Theme.controlBgHover : Theme.bgElevated
                                        // Destructive intent reads as a bright rim, not a red one.
                                        border.color: delGroupArea.containsMouse ? Theme.accentWhite : Theme.cardBorder
                                        border.width: 1

                                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                                        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                                        Accessible.role: Accessible.Button
                                        Accessible.name: qsTr("Удалить подписку")

                                        Image {
                                            anchors.centerIn: parent
                                            width: 14
                                            height: 14
                                            sourceSize.width: 14
                                            sourceSize.height: 14
                                            source: Theme.icon("qrc:/icons/trash.svg", Theme.isDark)
                                            opacity: delGroupArea.containsMouse ? 1.0 : 0.6
                                        }

                                        MouseArea {
                                            id: delGroupArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                deleteDialog.targetGroupId = groupCard.modelData.id;
                                                deleteDialog.targetGroupName = groupCard.modelData.name || "";
                                                deleteDialog.open();
                                            }
                                        }
                                    }
                                }

                                // ---- Announce & traffic telemetry ----
                                Rectangle {
                                    width: parent.width
                                    implicitHeight: announceCol.implicitHeight + 24
                                    height: implicitHeight
                                    radius: 12
                                    color: Theme.bgElevated
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    Column {
                                        id: announceCol
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.margins: 14
                                        spacing: 10

                                        RowLayout {
                                            width: parent.width
                                            spacing: 10
                                            visible: Boolean((modelData.announcement && modelData.announcement.trim().length > 0) ||
                                                            (modelData.supportUrl && modelData.supportUrl.length > 0) ||
                                                            (modelData.webUrl && modelData.webUrl.length > 0))
                                            height: visible ? implicitHeight : 0

                                            Rectangle {
                                                Layout.preferredWidth: 3
                                                Layout.fillHeight: true
                                                Layout.minimumHeight: 18
                                                radius: 1.5
                                                color: Theme.accentWhite
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 8

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.announcement ? modelData.announcement.trim() : ""
                                                    color: Theme.textPrimary
                                                    font.pixelSize: 12
                                                    font.weight: Font.Medium
                                                    lineHeight: 1.3
                                                    wrapMode: Text.Wrap
                                                    textFormat: Text.PlainText
                                                    visible: Boolean(modelData.announcement && modelData.announcement.trim().length > 0)
                                                }

                                                // Quick action link buttons (Support / Website)
                                                RowLayout {
                                                    spacing: 8
                                                    visible: Boolean((modelData.supportUrl && modelData.supportUrl.length > 0) ||
                                                                     (modelData.webUrl && modelData.webUrl.length > 0))

                                                    Rectangle {
                                                        visible: Boolean(modelData.supportUrl && modelData.supportUrl.length > 0)
                                                        implicitHeight: 26
                                                        implicitWidth: supRow.implicitWidth + 20
                                                        radius: 6
                                                        color: supMa.pressed ? Theme.controlBgHover : (supMa.containsMouse ? Theme.cardHover : Theme.bgDark)
                                                        border.color: supMa.containsMouse ? Theme.accentWhite : Theme.cardBorder
                                                        border.width: 1

                                                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                                                        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                                                        Row {
                                                            id: supRow
                                                            anchors.centerIn: parent
                                                            spacing: 5
                                                            Text {
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                text: "💬"
                                                                font.pixelSize: 11
                                                            }
                                                            Text {
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                text: qsTr("Поддержка")
                                                                color: Theme.textPrimary
                                                                font.pixelSize: 11
                                                                font.weight: Font.Medium
                                                            }
                                                        }

                                                        MouseArea {
                                                            id: supMa
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (modelData.supportUrl) Qt.openUrlExternally(modelData.supportUrl)
                                                            }
                                                        }
                                                    }

                                                    Rectangle {
                                                        visible: Boolean(modelData.webUrl && modelData.webUrl.length > 0)
                                                        implicitHeight: 26
                                                        implicitWidth: webRow.implicitWidth + 20
                                                        radius: 6
                                                        color: webMa.pressed ? Theme.controlBgHover : (webMa.containsMouse ? Theme.cardHover : Theme.bgDark)
                                                        border.color: webMa.containsMouse ? Theme.accentWhite : Theme.cardBorder
                                                        border.width: 1

                                                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                                                        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

                                                        Row {
                                                            id: webRow
                                                            anchors.centerIn: parent
                                                            spacing: 5
                                                            Text {
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                text: "🌐"
                                                                font.pixelSize: 11
                                                            }
                                                            Text {
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                text: qsTr("Сайт")
                                                                color: Theme.textPrimary
                                                                font.pixelSize: 11
                                                                font.weight: Font.Medium
                                                            }
                                                        }

                                                        MouseArea {
                                                            id: webMa
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (modelData.webUrl) Qt.openUrlExternally(modelData.webUrl)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        Column {
                                            width: parent.width
                                            spacing: 6

                                            readonly property bool unlimited: modelData.isUnlimited === true
                                                                              || !modelData.totalBytes
                                                                              || modelData.totalBytes === 0
                                            readonly property int usedPercent: Math.round((modelData.trafficPercent || 0) * 100)

                                            RowLayout {
                                                width: parent.width

                                                Text {
                                                    color: Theme.textSecondary
                                                    font.pixelSize: 12
                                                    text: {
                                                        const used = modelData.trafficUsedStr || "0 B";
                                                        if (parent.parent.unlimited) {
                                                            return qsTr("Использовано: %1  •  Безлимитный трафик").arg(used);
                                                        }
                                                        return qsTr("Использовано: %1 из %2 (%3%)")
                                                            .arg(used)
                                                            .arg(modelData.trafficTotalStr || "0 B")
                                                            .arg(parent.parent.usedPercent);
                                                    }
                                                }

                                                Item { Layout.fillWidth: true }

                                                Rectangle {
                                                    Layout.preferredHeight: 18
                                                    Layout.preferredWidth: trafficBadgeText.implicitWidth + 10
                                                    radius: 4
                                                    color: Theme.cardHover
                                                    border.color: Theme.cardBorder
                                                    border.width: 1

                                                    Text {
                                                        id: trafficBadgeText
                                                        anchors.centerIn: parent
                                                        text: parent.parent.parent.unlimited
                                                              ? qsTr("БЕЗЛИМИТ")
                                                              : (parent.parent.parent.usedPercent + "%")
                                                        color: Theme.textPrimary
                                                        font.pixelSize: 10
                                                        font.bold: true
                                                        font.family: Theme.fontMono
                                                    }
                                                }
                                            }

                                            // Usage bar. Near-quota is signalled by brightness, not colour.
                                            Rectangle {
                                                width: parent.width
                                                height: 5
                                                radius: 2.5
                                                color: Theme.accentDim

                                                Rectangle {
                                                    width: parent.parent.unlimited
                                                           ? parent.width
                                                           : Math.max(6, Math.min(parent.width, parent.width * (modelData.trafficPercent || 0)))
                                                    height: parent.height
                                                    radius: 2.5
                                                    color: parent.parent.unlimited ? Theme.statusMedium
                                                         : ((modelData.trafficPercent || 0) > 0.9 ? Theme.accentWhite
                                                                                                  : Theme.statusMedium)
                                                    Behavior on width { NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.OutCubic } }
                                                }
                                            }
                                        }

                                        Text {
                                            width: parent.width
                                            text: qsTr("Срок: %1  •  %2")
                                                  .arg(modelData.isPerpetual || !modelData.expireDateStr
                                                       ? qsTr("Бессрочно") : modelData.expireDateStr)
                                                  .arg(modelData.lastUpdateStr || qsTr("Не обновлялось"))
                                            color: Theme.textMuted
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                // ---- Nodes in this subscription ----
                                Column {
                                    width: parent.width
                                    spacing: 8

                                    Repeater {
                                        model: groupCard.groupServers

                                        delegate: ServerRow {
                                            required property var modelData
                                            width: parent.width
                                            server: modelData
                                            onSelected: {
                                                if (typeof configAdapter !== "undefined") configAdapter.selectServer(modelData.id)
                                            }
                                            onRemoveRequested: {
                                                if (typeof configAdapter !== "undefined") configAdapter.deleteServer(modelData.id)
                                            }
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        text: qsTr("Узлы не найдены по запросу")
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        visible: groupCard.groupServers.length === 0 && root.filterText.length > 0
                                        topPadding: 4
                                        bottomPadding: 4
                                    }
                                }
                            }

                        }
                    }

                    // ============================================================
                    // Custom (unassigned) nodes
                    // ============================================================
                    Rectangle {
                        width: parent.width
                        implicitHeight: customCol.implicitHeight + 36
                        height: implicitHeight
                        radius: Theme.radiusMedium
                        color: Theme.cardBg
                        border.color: Theme.cardBorder
                        border.width: 1
                        visible: root.customNodes.length > 0

                        Column {
                            id: customCol
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 18
                            spacing: 14

                            RowLayout {
                                width: parent.width
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                                    radius: 8
                                    color: Theme.bgElevated
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    Image {
                                        anchors.centerIn: parent
                                        width: 17
                                        height: 17
                                        sourceSize.width: 17
                                        sourceSize.height: 17
                                        source: Theme.icon("qrc:/icons/nodes_list.svg", Theme.isDark)
                                    }
                                }

                                Text {
                                    text: qsTr("Пользовательские серверы")
                                    color: Theme.textPrimary
                                    font.pixelSize: 16
                                    font.bold: true
                                }

                                Rectangle {
                                    Layout.preferredHeight: 22
                                    Layout.preferredWidth: customCountText.implicitWidth + 14
                                    radius: 11
                                    color: Theme.bgElevated
                                    border.color: Theme.cardBorder
                                    border.width: 1

                                    Text {
                                        id: customCountText
                                        anchors.centerIn: parent
                                        text: qsTr("%n узлов", "", root.customNodes.length)
                                        color: Theme.textSecondary
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }

                            Column {
                                width: parent.width
                                spacing: 8

                                Repeater {
                                    model: root.customNodes

                                    delegate: ServerRow {
                                        required property var modelData
                                        width: parent.width
                                        server: modelData
                                        onSelected: {
                                            if (typeof configAdapter !== "undefined") configAdapter.selectServer(modelData.id)
                                        }
                                        onRemoveRequested: {
                                            if (typeof configAdapter !== "undefined") configAdapter.deleteServer(modelData.id)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ============================================================
                    // Empty state
                    // ============================================================
                    EmptyState {
                        width: parent.width
                        visible: (typeof configAdapter !== "undefined") && configAdapter.serverCount === 0
                        title: qsTr("Нет добавленных узлов или подписок")
                        subtitle: qsTr("Нажмите «+ Добавить», чтобы импортировать конфигурацию или ссылку на подписку.")
                        actionText: qsTr("+ Добавить узел")
                        onActionTriggered: root.requestImport()
                    }

                    // Beaxty VPN Promo / Free Trial Card
                    Rectangle {
                        width: parent.width
                        visible: (typeof configAdapter !== "undefined") && configAdapter.serverCount === 0
                        implicitHeight: promoCol.implicitHeight + 40
                        height: implicitHeight
                        radius: Theme.radiusMedium
                        color: Theme.cardBg
                        border.color: Theme.cardBorder
                        border.width: 1

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

                        ColumnLayout {
                            id: promoCol
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 12

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Нет подписки? Тебе в beaxty VPN!")
                                color: Theme.textPrimary
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Попробуй 3 дня бесплатно! Высокая скорость, обход блокировок и доступ ко всем локациям.")
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                spacing: 10

                                // Telegram Bot Button
                                Rectangle {
                                    id: tgBtn
                                    Layout.preferredHeight: 36
                                    Layout.preferredWidth: tgBtnContent.implicitWidth + 28
                                    radius: 8
                                    color: tgArea.pressed ? Theme.controlBgHover
                                         : (tgArea.containsMouse ? Theme.controlBgHover : Theme.controlBg)
                                    border.color: tgArea.containsMouse ? Theme.textSecondary : Theme.controlBorder
                                    border.width: 1

                                    RowLayout {
                                        id: tgBtnContent
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Text {
                                            text: "💬"
                                            font.pixelSize: 13
                                        }

                                        Text {
                                            text: qsTr("Telegram-бот (@beaxtyvpnbot)")
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    MouseArea {
                                        id: tgArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Qt.openUrlExternally("https://t.me/beaxtyvpnbot")
                                    }
                                }

                                // Personal Cabinet Button
                                Rectangle {
                                    id: cabBtn
                                    Layout.preferredHeight: 36
                                    Layout.preferredWidth: cabBtnContent.implicitWidth + 28
                                    radius: 8
                                    color: cabArea.pressed ? Theme.controlBgHover
                                         : (cabArea.containsMouse ? Theme.controlBgHover : Theme.controlBg)
                                    border.color: cabArea.containsMouse ? Theme.textSecondary : Theme.controlBorder
                                    border.width: 1

                                    RowLayout {
                                        id: cabBtnContent
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Text {
                                            text: "🌐"
                                            font.pixelSize: 13
                                        }

                                        Text {
                                            text: qsTr("Личный кабинет (cabinet.beaxty.com)")
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    MouseArea {
                                        id: cabArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Qt.openUrlExternally("https://cabinet.beaxty.com")
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }

                    // No results for the current search, but nodes do exist.
                    EmptyState {
                        width: parent.width
                        iconSource: "qrc:/icons/search.svg"
                        visible: (typeof configAdapter !== "undefined")
                                 && configAdapter.serverCount > 0
                                 && root.filterText.length > 0
                                 && root.customNodes.length === 0
                                 && configAdapter.servers.filter(root.matchesFilter).length === 0
                        title: qsTr("Ничего не найдено")
                        subtitle: qsTr("Попробуйте изменить поисковый запрос.")
                    }

                    Item {
                        width: parent.width
                        height: 24
                    }
                }
            }
        }
    }

    Dialog {
        id: renameDialog
        objectName: "renameDialog"
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 20
        dim: true

        property int targetGroupId: -1
        property string initialName: ""

        background: Rectangle {
            color: Theme.cardBg
            border.color: Theme.cardBorder
            border.width: 1
            radius: 12
        }

        header: Item {
            height: 48
            width: parent.width

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 16

                Text {
                    text: qsTr("Переименование подписки")
                    color: Theme.textPrimary
                    font.pixelSize: 15
                    font.bold: true
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 26
                    height: 26
                    radius: 13
                    color: closeArea.containsMouse ? Theme.cardHover : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        sourceSize.width: 12
                        sourceSize.height: 12
                        source: Theme.icon("qrc:/icons/close.svg", Theme.isDark)
                        opacity: closeArea.containsMouse ? 1.0 : 0.6
                    }

                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: renameDialog.reject()
                    }
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 16
            width: 360

            Text {
                text: qsTr("Введите новое название для подписки:")
                color: Theme.textSecondary
                font.pixelSize: 12
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: 8
                color: Theme.bgElevated
                border.color: newNameInput.activeFocus ? Theme.accentWhite : Theme.cardBorder
                border.width: 1

                TextInput {
                    id: newNameInput
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    selectByMouse: true
                    onAccepted: renameDialog.applyRename()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 10

                Item { Layout.fillWidth: true }

                ActionButton {
                    text: qsTr("Отмена")
                    onClicked: renameDialog.reject()
                }

                Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 100
                    radius: 8
                    color: saveArea.pressed ? "#E0E0E0" : (saveArea.containsMouse ? "#FFFFFF" : Theme.accentWhite)

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Сохранить")
                        color: "#000000"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        id: saveArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: renameDialog.applyRename()
                    }
                }
            }
        }

        function applyRename() {
            const trimmed = newNameInput.text.trim();
            if (trimmed.length > 0 && targetGroupId !== -1 && typeof configAdapter !== "undefined") {
                configAdapter.renameGroup(targetGroupId, trimmed);
            }
            renameDialog.close();
        }

        onAboutToShow: {
            newNameInput.text = initialName;
            newNameInput.forceActiveFocus();
            newNameInput.selectAll();
        }
    }

    Dialog {
        id: deleteDialog
        objectName: "deleteDialog"
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 20
        dim: true

        property int targetGroupId: -1
        property string targetGroupName: ""

        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.7)
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }

        background: Rectangle {
            color: Theme.cardBg
            border.color: Theme.cardBorder
            border.width: 1
            radius: 14
        }

        header: Item {
            height: 48
            width: parent.width

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 16

                Text {
                    text: qsTr("Удалить подписку?")
                    color: Theme.textPrimary
                    font.pixelSize: 15
                    font.bold: true
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 26
                    height: 26
                    radius: 13
                    color: delCloseArea.containsMouse ? Theme.cardHover : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        sourceSize.width: 12
                        sourceSize.height: 12
                        source: Theme.icon("qrc:/icons/close.svg", Theme.isDark)
                        opacity: delCloseArea.containsMouse ? 1.0 : 0.6
                    }

                    MouseArea {
                        id: delCloseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: deleteDialog.reject()
                    }
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 16
            width: 380

            Text {
                text: qsTr("Вы действительно хотите удалить подписку «%1»? Все связанные с ней серверы будут удалены.").arg(deleteDialog.targetGroupName)
                color: Theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 10

                Item { Layout.fillWidth: true }

                ActionButton {
                    text: qsTr("Отмена")
                    onClicked: deleteDialog.reject()
                }

                Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 100
                    radius: 8
                    color: delConfirmArea.pressed ? "#B71C1C"
                         : (delConfirmArea.containsMouse ? "#D32F2F" : "#E53935")

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Удалить")
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        id: delConfirmArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: deleteDialog.applyDelete()
                    }
                }
            }
        }

        function applyDelete() {
            if (targetGroupId !== -1 && typeof configAdapter !== "undefined") {
                configAdapter.deleteGroup(targetGroupId);
            }
            deleteDialog.close();
        }
    }
}
