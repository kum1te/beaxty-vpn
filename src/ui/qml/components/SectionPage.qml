// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick

Item {
    id: root
    default property alias content: body.data
    property real entranceProgress: 1
    clip: true

    function updateEntrance() {
        entrance.stop();
        if (visible) {
            entranceProgress = 0;
            entrance.start();
        } else {
            entranceProgress = 1;
        }
    }
    onVisibleChanged: updateEntrance()
    Component.onCompleted: updateEntrance()

    NumberAnimation {
        id: entrance
        target: root
        property: "entranceProgress"
        to: 1
        duration: 160
        easing.type: Easing.OutCubic
    }

    Item {
        id: body
        anchors.fill: parent
        opacity: root.entranceProgress
        transform: Translate { y: 6 * (1 - root.entranceProgress) }
    }
}
