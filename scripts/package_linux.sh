#!/usr/bin/env bash
# ====================================================================
# Beaxty VPN - Linux Packaging Script (AppImage & Portable tar.gz)
# ====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
DIST_DIR="${REPO_ROOT}/dist"
APPDIR="${BUILD_DIR}/AppDir"
TOOLS_DIR="${BUILD_DIR}/tools"

echo "============================================================"
echo "  Beaxty VPN - Linux Packaging Workflow"
echo "============================================================"
echo "Repo root : ${REPO_ROOT}"
echo "Build dir : ${BUILD_DIR}"
echo "Dist dir  : ${DIST_DIR}"
echo "AppDir    : ${APPDIR}"
echo "Tools dir : ${TOOLS_DIR}"
echo "============================================================"

mkdir -p "${DIST_DIR}"
mkdir -p "${TOOLS_DIR}"

# 1. Ensure core daemon (beaxty-core) is present
if [ ! -f "${REPO_ROOT}/bin/beaxty-core" ]; then
    echo "==> beaxty-core not found in bin/. Building core daemon..."
    "${REPO_ROOT}/scripts/build_core.sh"
else
    echo "==> Found existing core daemon: ${REPO_ROOT}/bin/beaxty-core"
fi

# 2. Build Release beaxty-vpn
echo "==> Configuring and building BeaxtyVPN (Release)..."
cmake -B "${BUILD_DIR}" -S "${REPO_ROOT}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target beaxty-vpn -j"$(nproc)"

# 3. Setup packaging tools
echo "==> Preparing packaging tools..."

download_tool() {
    local target="$1"
    local url="$2"
    if [ ! -f "${target}" ]; then
        if [ -f "/tmp/tools/$(basename "${target}")" ]; then
            echo "  Copying $(basename "${target}") from /tmp/tools..."
            cp "/tmp/tools/$(basename "${target}")" "${target}"
        else
            echo "  Downloading $(basename "${target}")..."
            curl -fsSL -o "${target}" "${url}"
        fi
        chmod +x "${target}"
    fi
}

download_tool "${TOOLS_DIR}/linuxdeploy" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"

download_tool "${TOOLS_DIR}/linuxdeploy-plugin-qt" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

download_tool "${TOOLS_DIR}/appimagetool" \
    "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

# 4. Assemble AppDir skeleton
echo "==> Assembling AppDir structure..."
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/512x512/apps"

# Copy binaries
cp "${BUILD_DIR}/beaxty-vpn" "${APPDIR}/usr/bin/beaxty-vpn"
cp "${REPO_ROOT}/bin/beaxty-core" "${APPDIR}/usr/bin/beaxty-core"
if [ -f "${REPO_ROOT}/scripts/setup-cap.sh" ]; then
    cp "${REPO_ROOT}/scripts/setup-cap.sh" "${APPDIR}/usr/bin/setup-cap.sh"
fi
chmod +x "${APPDIR}/usr/bin/"*

# Copy desktop and icon metadata
cp "${REPO_ROOT}/res/beaxty-vpn.desktop" "${APPDIR}/usr/share/applications/beaxty-vpn.desktop"
cp "${REPO_ROOT}/res/beaxty-vpn.desktop" "${APPDIR}/beaxty-vpn.desktop"

ICON_OPT=()
if [ -f "${REPO_ROOT}/res/icons/app_icon.svg" ]; then
    mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
    cp "${REPO_ROOT}/res/icons/app_icon.svg" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/beaxty-vpn.svg"
    cp "${REPO_ROOT}/res/icons/app_icon.svg" "${APPDIR}/beaxty-vpn.svg"
    ICON_OPT=(--icon-file "${APPDIR}/beaxty-vpn.svg")

    if command -v rsvg-convert &>/dev/null; then
        mkdir -p "${APPDIR}/usr/share/icons/hicolor/512x512/apps"
        rsvg-convert -w 512 -h 512 "${APPDIR}/beaxty-vpn.svg" -o "${APPDIR}/beaxty-vpn.png"
        cp "${APPDIR}/beaxty-vpn.png" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/beaxty-vpn.png"
    fi
elif [ -f "${REPO_ROOT}/3rdparty/throne/res/public/Throne.png" ]; then
    cp "${REPO_ROOT}/3rdparty/throne/res/public/Throne.png" "${APPDIR}/beaxty-vpn.png"
    ICON_OPT=(--icon-file "${APPDIR}/beaxty-vpn.png")
fi

# 5. Run linuxdeploy with Qt plugin
echo "==> Bundling Qt libraries and runtime via linuxdeploy..."
export NO_STRIP=1
export APPIMAGE_EXTRACT_AND_RUN=1
export QML_SOURCES_PATHS="${REPO_ROOT}/src/ui"
export PATH="${TOOLS_DIR}:${PATH}"

if command -v qmake6 &>/dev/null; then
    export QMAKE="$(command -v qmake6)"
elif command -v qmake &>/dev/null; then
    export QMAKE="$(command -v qmake)"
fi

"${TOOLS_DIR}/linuxdeploy" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/beaxty-vpn" \
    --desktop-file "${APPDIR}/beaxty-vpn.desktop" \
    "${ICON_OPT[@]}" \
    --plugin qt

# 6. Create run.sh launcher for standalone portable usage
echo "==> Creating standalone portable launcher (run.sh)..."
cat << 'LAUNCHER' > "${APPDIR}/run.sh"
#!/usr/bin/env bash
# BeaxtyVPN Portable Launcher
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
export QML_IMPORT_PATH="${HERE}/usr/qml"
export QML2_IMPORT_PATH="${HERE}/usr/qml"
exec "${HERE}/usr/bin/beaxty-vpn" "$@"
LAUNCHER
chmod +x "${APPDIR}/run.sh"

# 7. Package Linux Portable Tarball
echo "==> Creating BeaxtyVPN-Linux-x86_64-Portable.tar.gz..."
PORTABLE_TMP="${BUILD_DIR}/BeaxtyVPN-Linux-x86_64-Portable"
rm -rf "${PORTABLE_TMP}"
cp -a "${APPDIR}" "${PORTABLE_TMP}"
tar -czf "${DIST_DIR}/BeaxtyVPN-Linux-x86_64-Portable.tar.gz" -C "${BUILD_DIR}" "BeaxtyVPN-Linux-x86_64-Portable"
rm -rf "${PORTABLE_TMP}"

# 8. Package Linux AppImage
echo "==> Creating BeaxtyVPN-Linux-x86_64.AppImage..."
ARCH=x86_64 "${TOOLS_DIR}/appimagetool" "${APPDIR}" "${DIST_DIR}/BeaxtyVPN-Linux-x86_64.AppImage"

# 9. Summary & Checksums
echo "============================================================"
echo "  Packaging Succeeded!"
echo "============================================================"
ls -lh "${DIST_DIR}/BeaxtyVPN-Linux-x86_64"*
echo ""
echo "Checksums (SHA-256):"
sha256sum "${DIST_DIR}/BeaxtyVPN-Linux-x86_64"*
echo "============================================================"
