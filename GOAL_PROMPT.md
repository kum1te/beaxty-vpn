# GOAL PROMPT: Autonomous Development of INCY-Style Monochrome VPN Client (Throne Wrapper)

## 1. Executive Summary & Objective

You are tasked with building a modern, high-performance, minimalist desktop VPN client for Linux (with clean cross-platform architecture for future Windows/macOS targets).

The project is **a clean wrapper around the Throne application core** (https://github.com/throneproj/Throne), replacing Throne's cluttered and complex desktop UI with an intuitive, mobile-inspired, card-based interface styled after **INCY** (https://github.com/INCY-DEV/incy-platforms), rendered in a **pure monochrome / black-and-white (ч/б) aesthetic**.

### Key Imperatives:
1. **Do NOT reinvent the wheel**: Throne's backend (built on `sing-box`, SQLite storage, subscription reconciler, and Linux network/TUN managers) is robust and battle-tested. Preserve and reuse this backend as a modular engine.
2. **Upstream Maintainability**: Architect the wrapper so that when Throne releases updates upstream, your application can be rebased or synced with minimal friction (clean abstraction layer, no spaghetti hacks directly inside Throne's core files).
3. **License Compliance**: Throne is licensed under **GPL-3.0**. The entire resulting wrapper and codebase MUST strictly comply with GPL-3.0. Incy is used strictly as UX/UI design inspiration (clean-room visual design, zero proprietary code copied).
4. **Default Behaviors**:
   - **TUN Mode**: Enabled by default (out of the box).
   - **HWID Transmission**: Enabled by default (using Throne's hardware ID calculation for subscription/server verification).
   - **Simplified Routing**: User-friendly preset-based routing ("Route All", "Bypass Domestic/RU", "Split-Tunneling / Specific Apps & Domains") without complex JSON editing.
5. **Quality & Verification Gates**: Before completing the task, you MUST run a comprehensive verification suite:
   - Functional test suite (TUN interface lifecycle, subscription import, node switching, HWID reporting).
   - Optimization & performance audit (instant UI response, native feel, memory < 80MB, idle CPU < 0.5%).
   - Security audit (safe Linux privilege escalation via `cap_net_admin` / Polkit, no plain-text secret leaking in logs, DNS leak prevention).

---

## 2. Architectural Blueprint & Technology Stack

### 2.1 Recommended Technology Stack
* **Core Engine**: C++20, CMake, Throne upstream (`throne-core`), powered by `sing-box`.
* **Application Framework**: Qt 6 (Qt6 Core, Network, DBus, QML / Qt Quick).
  - *Rationale*: Throne already relies on Qt 6 and C++20. Using **Qt Quick / QML** for the frontend allows 100% native C++ integration with zero IPC serialization penalty, GPU-accelerated rendering, and silky-smooth mobile-like cards/animations identical to INCY's UX.
* **Storage**: SQLite (reusing Throne's existing `DatabaseManager`, `GroupsRepo`, and `RoutesRepo`).
* **Privilege Helper**: Linux capability configuration (`setcap cap_net_admin,cap_net_bind_service+ep`) or Polkit action for headless TUN management.

### 2.2 Project Directory Architecture
Organize the repository to ensure strict isolation between upstream Throne and the custom wrapper:

```text
beaxtyvpnapp/
├── 3rdparty/
│   └── throne/                # Git submodule or clean vendor checkout of throneproj/Throne
├── patches/                   # Minimal patches applied to throne core (if strictly required)
├── src/
│   ├── core/                  # Core Adapter & Facade Layer
│   │   ├── ThroneEngine.hpp/cpp     # Lifecycle, sing-box runner, state machine
│   │   ├── ConfigAdapter.hpp/cpp    # Facade over DatabaseManager, GroupsRepo, RoutesRepo
│   │   ├── SubscriptionBridge.hpp/cpp # GroupUpdater / SubscriptionParser integration
│   │   ├── DeviceIdentity.hpp/cpp   # DeviceDetailsHelper integration (HWID generator)
│   │   ├── RoutingManager.hpp/cpp   # Simple routing presets -> Throne RouteProfiles
│   │   └── TrafficMonitor.hpp/cpp   # Live speeds, ping test, traffic statistics
│   ├── ui/                    # INCY-Inspired Monochrome UI
│   │   ├── qml/
│   │   │   ├── main.qml             # Window frame, dark/monochrome theme root
│   │   │   ├── components/          # Reusable cards, switches, buttons, ping pills
│   │   │   │   ├── MonochromeButton.qml
│   │   │   │   ├── Card.qml
│   │   │   │   ├── PingBadge.qml
│   │   │   │   ├── TrafficGauge.qml
│   │   │   │   └── ToggleRow.qml
│   │   │   ├── views/
│   │   │   │   ├── DashboardView.qml   # Main connect button, active node, quick stats
│   │   │   │   ├── NodesView.qml       # Server list, ping latencies, search filter
│   │   │   │   ├── RoutingView.qml     # Simple routing switches & custom rule editor
│   │   │   │   └── SettingsView.qml    # TUN, HWID toggle, auto-start, kill-switch
│   │   └── assets/                  # Minimalist monochrome SVG icons & typography
│   └── main.cpp               # Application entry point, initializes Throne backend & QML engine
├── tests/                     # Automated and verification scripts
│   ├── test_hwid.cpp
│   ├── test_tun_routing.sh
│   └── test_performance.sh
├── CMakeLists.txt             # Root CMake coordinating 3rdparty/throne and custom targets
├── LICENSE                    # GNU General Public License v3.0
└── README.md
```

---

## 3. Detailed Feature Specifications

### 3.1 INCY-Inspired Monochrome UI (ч/б)
The design must evoke high-end minimalism (reminiscent of Nothing OS, Leica, or Teenage Engineering aesthetics):

* **Color Palette**:
  - Background (Dark/OLED): `#0A0A0A` / `#121212`
  - Cards & Containers: `#181818` with subtle 1px border `#282828`
  - Primary Text / Active Elements: Pure White `#FFFFFF`
  - Secondary Text / Inactive Elements: Neutral Gray `#8E8E93`
  - Border / Separators: `#242424`
  - Accent / Status: High contrast monochrome (White filled circle = Connected; Hollow ring = Disconnected; Pulsing ring = Connecting).
* **Navigation**:
  - Bottom navigation bar (or compact sidebar): **Dashboard**, **Servers**, **Routing**, **Settings**.
* **Dashboard View**:
  - Large, prominent circular or pill-shaped Connect/Disconnect button in the center.
  - Connection status badge (`DISCONNECTED`, `CONNECTING`, `PROTECTED`).
  - Active server card displaying server name, flag/code, and live ping (ms). Clicking opens server selection.
  - Real-time download/upload speed counters (`0.0 KB/s`, `1.4 MB/s`) and session data usage.
* **Servers / Nodes View**:
  - Search bar at the top to filter servers.
  - "Ping All" test button with latency indicators: fast (`< 100ms`), medium (`100-250ms`), slow (`> 250ms`) rendered in monochrome styling (e.g. solid dots / gauge bars).
  - Grouping by subscription or region.
  - Quick action to add subscription URL or paste configuration link from clipboard.
* **Routing View (Simplified & Intuitive)**:
  - Eliminate complex JSON routing tables for end-users. Provide 3 primary selectable presets:
    1. **Route Everything (All Traffic)**: All traffic flows through the VPN tunnel.
    2. **Bypass Local & Domestic (RU / Local Networks)**: Direct connection for local addresses (RFC 1918) and designated national domains/IPs (`geosite:ru`, `geoip:ru`), routing international traffic via VPN.
    3. **Custom Split-Tunneling**: Simple toggleable list where users can input custom domains (e.g. `instagram.com`, `notion.so`) or select desktop applications to route or bypass.
* **Settings View**:
  - **TUN Mode**: Switch present, **default = ON**. (Includes badge: "Recommended for seamless VPN").
  - **Send Device HWID**: Switch present, **default = ON**. Displays the sanitized current HWID preview.
  - **Auto-connect on launch**: Toggle (default = OFF).
  - **Kill Switch**: Block non-tunneled internet if VPN drops (default = OFF / optional).
  - **System Tray Integration**: Minimize to tray on close, quick connect/disconnect from tray menu.

### 3.2 Backend Integration with Throne Core
* **Throne Submodule Integration**:
  - Consume Throne's C++ components (`src/global/Configs.cpp`, `src/global/DeviceDetailsHelper.cpp`, `src/configs/sub/SubscriptionParser.cpp`, `src/sys/Process.cpp`, `src/stats/traffic/TrafficStatsManager.cpp`).
  - Isolate or bypass Throne's existing `src/ui/mainwindow.cpp` and legacy QWidget dialogs. The new executable should launch our modern QML interface while calling Throne's backend management functions directly.
* **Hardware ID (HWID)**:
  - Leverage `DeviceDetailsHelper` from Throne.
  - Ensure Linux generation relies on `/etc/machine-id` or `/var/lib/dbus/machine-id` combined with motherboard UUID / CPU ID, SHA-256 hashed.
  - Wire HWID into subscription update requests (HTTP headers e.g. `X-HWID` or query parameter `?hwid=...`) and user-agent string as expected by the service backend.
* **TUN Mode & Privileges**:
  - Sing-box TUN mode requires `cap_net_admin` permissions.
  - Implement a clean privilege mechanism: provide a helper script or Polkit rule during installation so that regular users can run the client without starting the entire GUI application as `root`.
  - Ensure proper DNS leak prevention inside the sing-box configuration generator (`dns` rules using remote DNS over HTTPS/QUIC with fallback).

### 3.3 GPL-3.0 License Compliance
* Maintain the `LICENSE` file containing the full GNU General Public License v3.0 text.
* Include standard GPL-3.0 header comments in all newly created source files.
* Ensure all third-party dependencies are GPL-3.0 compatible.
* Provide clean build instructions so users can inspect and recompile the binary.

---

## 4. Step-by-Step Autonomous Execution Plan

Follow these phases sequentially:

### Phase 1: Environment & Dependency Discovery
1. Check Linux toolchain: `gcc`/`g++` (>= 11 supporting C++20), `cmake` (>= 3.20), `ninja-build` or `make`, `qt6-base-dev`, `qt6-declarative-dev` (QML), `libdbus-1-dev`.
2. Inspect or clone Throne repository: `git submodule add https://github.com/throneproj/Throne.git 3rdparty/throne` (or clone as reference vendor).
3. Analyze Throne's `CMakeLists.txt` to identify core backend libraries vs UI components.

### Phase 2: Core Adapter & Headless Engine
1. Create a static library or component target `throne_backend` that compiles Throne's core modules without its `src/ui/` widgets.
2. Build the C++ Adapter classes:
   - `ThroneEngine`: Wraps connection start/stop, sing-box daemon spawn, and TUN interface lifecycle.
   - `ConfigAdapter`: Manages SQLite database, profiles, and nodes.
   - `DeviceIdentity`: Implements HWID generation with default ON state.
   - `RoutingManager`: Translates user-friendly presets into sing-box routing rules / Throne `RouteProfile`.
3. Verify that `throne_backend` links and runs headless tests cleanly.

### Phase 3: INCY-Style Monochrome QML UI
1. Implement the monochrome design system in QML (Dark palette, typography, custom styled controls).
2. Build `DashboardView`, `NodesView`, `RoutingView`, and `SettingsView`.
3. Connect QML properties and signals to C++ adapters via `QQmlApplicationEngine` / QObject context properties.
4. Implement live traffic rate smoothing and real-time ping measurements.

### Phase 4: System Integration & Packaging
1. Create Linux desktop entry (`beaxty-vpn.desktop`) and application icon.
2. Implement system tray icon with status-dependent monochrome icon (connected/disconnected).
3. Implement permissions setup (`scripts/setup-cap.sh`) using `setcap` or Polkit action.

### Phase 5: Verification & Quality Assurance Suite
The agent must execute and log the following mandatory tests:
1. **Functional Tests**:
   - Test config/subscription parsing (VLESS, Reality, Shadowsocks, Hysteria2).
   - Test TUN interface creation (`ip link show`) and verify that default routing is active.
   - Test HWID generation consistency (verifying the ID remains stable across restarts).
   - Test connect / disconnect / reconnect cycle 10 times consecutively without deadlocks or zombie sing-box processes.
2. **Performance & Native Feel**:
   - Verify UI startup time (< 1.2s cold start).
   - Verify CPU usage when idle (< 0.5% CPU) and under load (minimal overhead over raw sing-box).
   - Verify memory consumption (< 80 MB RSS in steady state).
   - Check for memory leaks using Valgrind or AddressSanitizer on core modules.
3. **Security Audit**:
   - Verify that GUI does NOT run as root (TUN managed via capability or daemon helper).
   - Verify no private keys, passwords, or UUIDs are logged in plain text in stdout/stderr or debug logs.
   - Test DNS leaks via `systemd-resolved` / `resolv.conf` verification when TUN is engaged.

---

## 5. Definition of Done (DoD)

The task is considered **COMPLETE** only when:
- [ ] The application compiles cleanly with CMake on Linux.
- [ ] The UI faithfully implements the INCY-inspired monochrome dark design.
- [ ] TUN mode is enabled by default and functional out-of-the-box.
- [ ] HWID is generated and transmitted by default with subscription requests.
- [ ] Simplified routing (All / Bypass Domestic / Custom) is operational and intuitive.
- [ ] The codebase cleanly wraps Throne with an isolated architecture suitable for future upstream merges.
- [ ] GPL-3.0 licensing notices are intact.
- [ ] All verification tests (functional, performance, security) pass and are documented in `walkthrough.md`.
