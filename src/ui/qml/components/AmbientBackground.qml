// GPL-3.0 License
// Copyright (C) 2026 BeaxtyVPN Authors

import QtQuick
import ".."

Item {
    id: root
    anchors.fill: parent

    // Dynamic state properties
    readonly property int vpnState: (typeof throneEngine !== "undefined") ? throneEngine.state : 0
    // 0: Disconnected, 1: Connecting, 2: Protected

    // Halftone Wave Matrix Background (Procedural shader matching bganim.mp4)
    HalftoneWaveBackground {
        anchors.fill: parent
    }

    // =========================================================================
    // Orb 1: Deep Indigo (Top-Right / Floating)
    // =========================================================================
    Item {
        id: orb1Container
        width: 600
        height: 600
        x: root.vpnState === 1 ? (root.width * 0.5 - 300) : defaultX
        y: root.vpnState === 1 ? (root.height * 0.45 - 300) : defaultY
        opacity: root.vpnState === 2 ? 0.38 : (root.vpnState === 1 ? 0.35 : 0.28)
        scale: root.vpnState === 1 ? 0.90 : 1.0

        Behavior on x { NumberAnimation { duration: 900; easing.type: Easing.InOutQuad } }
        Behavior on y { NumberAnimation { duration: 900; easing.type: Easing.InOutQuad } }
        Behavior on opacity { NumberAnimation { duration: 600 } }
        Behavior on scale { NumberAnimation { duration: 700; easing.type: Easing.InOutQuad } }

        property real defaultX: root.width * 0.52
        property real defaultY: -80

        SequentialAnimation on defaultX {
            running: root.vpnState !== 1
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.45; to: root.width * 0.62; duration: 9000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.45; duration: 9000; easing.type: Easing.InOutSine }
        }

        SequentialAnimation on defaultY {
            running: root.vpnState !== 1
            loops: Animation.Infinite
            NumberAnimation { from: -100; to: -20; duration: 11000; easing.type: Easing.InOutSine }
            NumberAnimation { to: -100; duration: 11000; easing.type: Easing.InOutSine }
        }

        // Pulse in connecting state
        SequentialAnimation on scale {
            running: root.vpnState === 1
            loops: Animation.Infinite
            NumberAnimation { from: 0.85; to: 1.04; duration: 1200; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0.85; duration: 1200; easing.type: Easing.InOutSine }
        }

        Canvas {
            anchors.fill: parent
            renderTarget: Canvas.Image
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var cx = width / 2;
                var cy = height / 2;
                var r = width / 2;
                var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);
                grad.addColorStop(0.0, "rgba(28, 38, 58, 0.45)");
                grad.addColorStop(0.35, "rgba(22, 30, 48, 0.30)");
                grad.addColorStop(0.70, "rgba(14, 18, 30, 0.12)");
                grad.addColorStop(1.0, "rgba(8, 8, 10, 0.0)");
                ctx.fillStyle = grad;
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, 2 * Math.PI, false);
                ctx.fill();
            }
        }
    }

    // =========================================================================
    // Orb 2: Cobalt Graphite (Bottom-Left / Floating)
    // =========================================================================
    Item {
        id: orb2Container
        width: 560
        height: 560
        x: root.vpnState === 1 ? (root.width * 0.5 - 280) : defaultX
        y: root.vpnState === 1 ? (root.height * 0.5 - 280) : defaultY
        opacity: root.vpnState === 2 ? 0.34 : (root.vpnState === 1 ? 0.32 : 0.25)
        scale: root.vpnState === 1 ? 0.88 : 1.0

        Behavior on x { NumberAnimation { duration: 900; easing.type: Easing.InOutQuad } }
        Behavior on y { NumberAnimation { duration: 900; easing.type: Easing.InOutQuad } }
        Behavior on opacity { NumberAnimation { duration: 600 } }
        Behavior on scale { NumberAnimation { duration: 700; easing.type: Easing.InOutQuad } }

        property real defaultX: root.width * 0.08
        property real defaultY: root.height * 0.40

        SequentialAnimation on defaultX {
            running: root.vpnState !== 1
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.05; to: root.width * 0.18; duration: 13000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.05; duration: 13000; easing.type: Easing.InOutSine }
        }

        SequentialAnimation on defaultY {
            running: root.vpnState !== 1
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.40; to: root.height * 0.58; duration: 10000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.40; duration: 10000; easing.type: Easing.InOutSine }
        }

        // Pulse in connecting state
        SequentialAnimation on scale {
            running: root.vpnState === 1
            loops: Animation.Infinite
            NumberAnimation { from: 0.84; to: 1.02; duration: 1300; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0.84; duration: 1300; easing.type: Easing.InOutSine }
        }

        Canvas {
            anchors.fill: parent
            renderTarget: Canvas.Image
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var cx = width / 2;
                var cy = height / 2;
                var r = width / 2;
                var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);
                grad.addColorStop(0.0, "rgba(36, 26, 52, 0.35)");
                grad.addColorStop(0.35, "rgba(26, 20, 40, 0.22)");
                grad.addColorStop(0.70, "rgba(16, 14, 26, 0.08)");
                grad.addColorStop(1.0, "rgba(8, 8, 10, 0.0)");
                ctx.fillStyle = grad;
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, 2 * Math.PI, false);
                ctx.fill();
            }
        }
    }

    // =========================================================================
    // Orb 3: Shield Center Pulse & Glow (Dynamic VPN State Reaction)
    // =========================================================================
    Item {
        id: centerShieldOrb
        width: 680
        height: 680
        anchors.centerIn: parent
        opacity: root.vpnState === 2 ? 0.38 : (root.vpnState === 1 ? 0.32 : 0.18)

        Behavior on opacity { NumberAnimation { duration: 600 } }

        // Pulse during connecting
        SequentialAnimation on scale {
            running: root.vpnState === 1
            loops: Animation.Infinite
            NumberAnimation { from: 0.90; to: 1.12; duration: 1100; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0.90; duration: 1100; easing.type: Easing.InOutSine }
        }

        // Gentle breathing when protected
        SequentialAnimation on scale {
            running: root.vpnState === 2
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 1.05; duration: 3200; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.0; duration: 3200; easing.type: Easing.InOutSine }
        }

        Canvas {
            id: shieldCanvas
            anchors.fill: parent
            renderTarget: Canvas.Image

            Connections {
                target: root
                function onVpnStateChanged() {
                    shieldCanvas.requestPaint();
                }
            }

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var cx = width / 2;
                var cy = height / 2;
                var r = width / 2;
                var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);

                if (root.vpnState === 2) {
                    // Protected: Emerald-Silver active shield glow
                    grad.addColorStop(0.0, "rgba(18, 68, 52, 0.45)");
                    grad.addColorStop(0.30, "rgba(14, 52, 42, 0.28)");
                    grad.addColorStop(0.65, "rgba(180, 220, 205, 0.08)");
                    grad.addColorStop(1.0, "rgba(8, 8, 10, 0.0)");
                } else if (root.vpnState === 1) {
                    // Connecting: Cyber Blue/Indigo sync
                    grad.addColorStop(0.0, "rgba(35, 55, 95, 0.50)");
                    grad.addColorStop(0.40, "rgba(22, 38, 70, 0.28)");
                    grad.addColorStop(0.75, "rgba(14, 24, 46, 0.10)");
                    grad.addColorStop(1.0, "rgba(8, 8, 10, 0.0)");
                } else {
                    // Disconnected: Deep silver-graphite core
                    grad.addColorStop(0.0, "rgba(50, 52, 64, 0.22)");
                    grad.addColorStop(0.45, "rgba(30, 32, 40, 0.10)");
                    grad.addColorStop(0.80, "rgba(18, 19, 24, 0.03)");
                    grad.addColorStop(1.0, "rgba(8, 8, 10, 0.0)");
                }

                ctx.fillStyle = grad;
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, 2 * Math.PI, false);
                ctx.fill();
            }
        }
    }
}
