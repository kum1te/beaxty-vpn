# Release hardening in the independent working copy

This directory is the working copy of `antigravity/beaxtyvpnapp`. Use `build-local`; the copied `build` and `dist` directories contain older artifacts.

## Changes

- Subscription downloads are asynchronous, owned by the requesting object, limited to 16 MiB, and have a 30-second total deadline. Closing the owner cancels its requests. Concurrent refreshes of the same group and duplicate import submissions are coalesced.
- Invalid or empty subscription responses preserve saved servers and the last successful update timestamp. Imports validate content before creating a group. Batch insert failures roll back and propagate to the caller, preventing refresh from deleting working nodes after a failed insertion.
- Unchanged servers retain IDs and history. Matching uses indexed content lookups. Refreshing another subscription no longer changes the selected server. A profile used by an active connection remains available until a later refresh after disconnection.
- Server lists load through batched repository reads; ping-driven list notifications are coalesced over 50 ms. Saved latency appears correctly on the dashboard, stale selections and ping entries are cleared, and invalid selections are rejected.
- Saved subscription-update and sorting preferences load after database initialization. Disabling automatic updates also cancels the scheduled startup refresh.
- The import sheet supports resizing, long text, visible download progress, disabled duplicate submissions, keyboard actions, Escape dismissal, and Ctrl+Enter submission. The underlying interface is disabled while the sheet is open.
- Background animation pause bindings avoid pausing stopped animations. Hidden/minimized windows retain the earlier animation suspension behavior.
- The executable always loads its bundled UI, so its behavior does not depend on the working directory.
- Demo server names now survive persistence. Real users still start without sample nodes.

## Build and run

```sh
cmake -S . -B build-local -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-local -j 4
./build-local/beaxty-vpn
```

## Verification

```sh
ctest --test-dir build-local --output-on-failure
QT_QUICK_BACKEND=software ./build-local/test_capture_ui
```

The seven CTest tests cover hardware identity, configuration generation, subscription import/safety, routing persistence, tunnel configuration wiring, and per-app rules. Assertions remain enabled in Release builds. Two tests reserve loopback sockets, so a sandbox that prohibits listening requires an exception. The default suite does not establish a live VPN connection; the older routing integration scenario is opt-in with `test_routing_presets --integration`.

The subscription regression uses a delayed local HTTP server and an injected SQLite insertion failure. It checks invalid import cleanup, responsive downloads, duplicate suppression, unchanged ID preservation, unrelated selection preservation, changing subscription contents, deletion while downloading, write-failure preservation, response size limits, owner destruction, stored settings, saved latency, and coalesced model notifications.

The UI harness fails on QML warnings or failed screenshots. It exercises dark/light themes, Russian/English, navigation, import/deletion dialogs, window sizes from 840×560 to 1920×1080, import focus, long input, and Escape dismissal. Images are in `build-local/ui-captures/` (override with `BEAXTY_CAPTURE_DIR`).

## Measured locally, September 13, 2026

With an empty database, Linux Qt offscreen platform and software rendering:

- Launch through exit: 214 ms, **including an intentional 100 ms exit timer**. This is not a direct first-frame measurement.
- Resident memory: 109.84 MiB; proportional memory: 52.76 MiB.
- CPU during a two-second steady-state sample: 0.5% of one core.
- Clean exit: code 0.

Raw measurements: `build-local/benchmark.json`. These are environment-specific measurements, not a hardware-rendering or live-tunnel benchmark.

## Remaining release validation

This pass does not certify production readiness. Live provider subscriptions, privileged TUN setup, DNS leak behavior, reconnect/failover under real network loss, prolonged connected sessions, GPU rendering, and Windows/macOS packaging still need integration testing. Subscription refresh is staged to preserve data on insertion failures; replacing profiles and saving all group metadata are not yet a single database-wide transaction. Large valid documents are parsed on the UI thread after download, so very large imports may still cause a short pause. Existing legacy audit scripts are not evidence that these integration checks passed.
