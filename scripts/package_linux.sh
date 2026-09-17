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
if command -v ninja &>/dev/null; then
    cmake -B "${BUILD_DIR}" -S "${REPO_ROOT}" -G "Ninja" -DCMAKE_BUILD_TYPE=Release
else
    cmake -B "${BUILD_DIR}" -S "${REPO_ROOT}" -DCMAKE_BUILD_TYPE=Release
fi
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
            curl -fL --retry 3 --retry-delay 2 -o "${target}" "${url}"
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

if [ -n "${QT_ROOT_DIR:-}" ] && [ -x "${QT_ROOT_DIR}/bin/qmake" ]; then
    export QMAKE="${QT_ROOT_DIR}/bin/qmake"
elif command -v qmake6 &>/dev/null; then
    export QMAKE="$(command -v qmake6)"
elif command -v qmake &>/dev/null; then
    export QMAKE="$(command -v qmake)"
fi

"${TOOLS_DIR}/linuxdeploy" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/beaxty-vpn" \
    --desktop-file "${APPDIR}/beaxty-vpn.desktop" \
    "${ICON_OPT[@]}" \
    --plugin qt || true

# Explicitly deploy Qt Plugins and QML modules to guarantee complete autonomous runtime
QT_PLUGINS_DIR="$("${QMAKE}" -query QT_INSTALL_PLUGINS)"
QT_QML_DIR="$("${QMAKE}" -query QT_INSTALL_QML)"

echo "==> Deploying Qt plugins from ${QT_PLUGINS_DIR}..."
mkdir -p "${APPDIR}/usr/plugins"
cp -r "${QT_PLUGINS_DIR}/platforms" "${APPDIR}/usr/plugins/"
cp -r "${QT_PLUGINS_DIR}/tls" "${APPDIR}/usr/plugins/" 2>/dev/null || true
cp -r "${QT_PLUGINS_DIR}/imageformats" "${APPDIR}/usr/plugins/" 2>/dev/null || true
cp -r "${QT_PLUGINS_DIR}/networkinformation" "${APPDIR}/usr/plugins/" 2>/dev/null || true
if ls "${QT_PLUGINS_DIR}"/wayland-* 1> /dev/null 2>&1; then
    cp -r "${QT_PLUGINS_DIR}"/wayland-* "${APPDIR}/usr/plugins/" 2>/dev/null || true
fi

echo "==> Deploying QML modules from ${QT_QML_DIR}..."
mkdir -p "${APPDIR}/usr/qml"
cp -r "${QT_QML_DIR}/"* "${APPDIR}/usr/qml/"

echo "==> Collecting dependencies for platform plugins..."
for plugin in "${APPDIR}/usr/plugins/platforms/"*.so; do
    [ -f "${plugin}" ] && "${TOOLS_DIR}/linuxdeploy" --appdir "${APPDIR}" -e "${plugin}" || true
done

echo "==> Creating qt.conf..."
cat << 'QTCONF' > "${APPDIR}/usr/bin/qt.conf"
[Paths]
Prefix = ..
Plugins = plugins
Imports = qml
Qml2Imports = qml
QTCONF
cp -p "${APPDIR}/usr/bin/qt.conf" "${APPDIR}/qt.conf"

# Ensure QtWebEngineProcess and resources are copied if built with WebEngine
QT_LIB_DIR="$(dirname "$("${QMAKE}" -query QT_INSTALL_LIBS)")/lib"
QT_LIBEXEC_DIR="$("${QMAKE}" -query QT_INSTALL_LIBEXECS)"
QT_DATA_DIR="$("${QMAKE}" -query QT_INSTALL_DATA)"
QT_TRANS_DIR="$("${QMAKE}" -query QT_INSTALL_TRANSLATIONS)"

if [ -f "${QT_LIBEXEC_DIR}/QtWebEngineProcess" ]; then
    echo "==> Deploying QtWebEngineProcess..."
    mkdir -p "${APPDIR}/usr/libexec"
    cp -p "${QT_LIBEXEC_DIR}/QtWebEngineProcess" "${APPDIR}/usr/libexec/QtWebEngineProcess"
    chmod +x "${APPDIR}/usr/libexec/QtWebEngineProcess"
fi

if [ -d "${QT_DATA_DIR}/resources" ]; then
    echo "==> Deploying QtWebEngine resources..."
    mkdir -p "${APPDIR}/usr/resources"
    cp -p "${QT_DATA_DIR}/resources/"*.pak "${APPDIR}/usr/resources/" 2>/dev/null || true
    cp -p "${QT_DATA_DIR}/resources/"*.dat "${APPDIR}/usr/resources/" 2>/dev/null || true
fi

if [ -d "${QT_TRANS_DIR}/qtwebengine_locales" ]; then
    echo "==> Deploying QtWebEngine translations..."
    mkdir -p "${APPDIR}/usr/translations/qtwebengine_locales"
    cp -p "${QT_TRANS_DIR}/qtwebengine_locales/"*.pak "${APPDIR}/usr/translations/qtwebengine_locales/" 2>/dev/null || true
fi

# 6. Create custom AppRun and run.sh launcher with fontconfig and WebEngine support
echo "==> Creating custom AppRun and run.sh with system font & WebEngine environment..."
cat << 'LAUNCHER' > "${APPDIR}/AppRun"
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export APPDIR="${HERE}"

# System Fontconfig fallback so AppImage uses host fonts (Noto Color Emoji, system sans, etc.)
if [ -z "${FONTCONFIG_PATH:-}" ]; then
    if [ -d "/etc/fonts" ]; then
        export FONTCONFIG_PATH="/etc/fonts"
    fi
fi

# Chromium sandbox does not work unprivileged without SUID helper inside AppImage
if [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi

# Libraries and Qt paths
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/plugins/platforms"
export QML_IMPORT_PATH="${HERE}/usr/qml"
export QML2_IMPORT_PATH="${HERE}/usr/qml"
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM="wayland;xcb"
fi
export QTWEBENGINEPROCESS_PATH="${HERE}/usr/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="${HERE}/usr/resources"

exec "${HERE}/usr/bin/beaxty-vpn" "$@"
LAUNCHER
chmod +x "${APPDIR}/AppRun"

cat << 'LAUNCHER' > "${APPDIR}/run.sh"
#!/usr/bin/env bash
# BeaxtyVPN Portable Launcher
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${FONTCONFIG_PATH:-}" ] && [ -d "/etc/fonts" ]; then
    export FONTCONFIG_PATH="/etc/fonts"
fi

if [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi

export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/plugins/platforms"
export QML_IMPORT_PATH="${HERE}/usr/qml"
export QML2_IMPORT_PATH="${HERE}/usr/qml"
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM="wayland;xcb"
fi
export QTWEBENGINEPROCESS_PATH="${HERE}/usr/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="${HERE}/usr/resources"

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
ARCH=x86_64 "${TOOLS_DIR}/appimagetool" --no-appstream "${APPDIR}" "${DIST_DIR}/BeaxtyVPN-Linux-x86_64.AppImage"

# 9. Summary & Checksums
echo "============================================================"
echo "  Packaging Succeeded!"
echo "============================================================"
ls -lh "${DIST_DIR}/BeaxtyVPN-Linux-x86_64"*
echo ""
echo "Checksums (SHA-256):"
sha256sum "${DIST_DIR}/BeaxtyVPN-Linux-x86_64"*
echo "============================================================"
