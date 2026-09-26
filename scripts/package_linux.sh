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

QT_LIBS_DIR="$("${QMAKE}" -query QT_INSTALL_LIBS)"
QT_PLUGINS_DIR="$("${QMAKE}" -query QT_INSTALL_PLUGINS)"
QT_QML_DIR="$("${QMAKE}" -query QT_INSTALL_QML)"

echo "==> Deploying all Qt6 shared libraries from ${QT_LIBS_DIR}..."
mkdir -p "${APPDIR}/usr/lib"
cp -a "${QT_LIBS_DIR}"/libQt6*.so* "${APPDIR}/usr/lib/" 2>/dev/null || true

"${TOOLS_DIR}/linuxdeploy" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/beaxty-vpn" \
    --desktop-file "${APPDIR}/beaxty-vpn.desktop" \
    "${ICON_OPT[@]}" \
    --plugin qt || true

# Explicitly deploy Qt Plugins and QML modules to guarantee complete autonomous runtime
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

echo "==> Resolving deep dependencies for plugins and QML modules..."
for plugin in "${APPDIR}/usr/plugins/platforms/"*.so; do
    if [ -f "${plugin}" ]; then
        "${TOOLS_DIR}/linuxdeploy" --appdir "${APPDIR}" --deploy-deps-only "${plugin}" 2>/dev/null || true
    fi
done
"${TOOLS_DIR}/linuxdeploy" --appdir "${APPDIR}" \
    --deploy-deps-only "${APPDIR}/usr/plugins" \
    --deploy-deps-only "${APPDIR}/usr/qml" \
    --deploy-deps-only "${APPDIR}/usr/lib" 2>/dev/null || true
# Remove any accidental .so copied into usr/bin
rm -f "${APPDIR}/usr/bin/"*.so* 2>/dev/null || true

# QtWebEngine's Chromium runtime loads NSS modules with dlopen(), so linuxdeploy
# cannot discover these dependencies by inspecting ELF DT_NEEDED entries.
# Bundle the NSS modules together with their integrity-check files and expose
# this directory through LD_LIBRARY_PATH at runtime.
NSS_MULTIARCH=""
if command -v dpkg-architecture &>/dev/null; then
    NSS_MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)"
fi

NSS_SEARCH_DIRS=()
if [ -n "${NSS_MULTIARCH}" ]; then
    NSS_SEARCH_DIRS+=(
        "/usr/lib/${NSS_MULTIARCH}/nss"
        "/lib/${NSS_MULTIARCH}/nss"
        "/usr/lib/${NSS_MULTIARCH}"
        "/lib/${NSS_MULTIARCH}"
    )
fi
NSS_SEARCH_DIRS+=(/usr/lib/nss /usr/lib64/nss /lib/nss /usr/lib /lib)

NSS_MODULES_DIR=""
for candidate in "${NSS_SEARCH_DIRS[@]}"; do
    if [ -s "${candidate}/libsoftokn3.so" ]; then
        NSS_MODULES_DIR="${candidate}"
        break
    fi
done

if [ -z "${NSS_MODULES_DIR}" ]; then
    echo "ERROR: Could not find the NSS runtime module libsoftokn3.so." >&2
    echo "       Install the system NSS runtime package before packaging." >&2
    exit 1
fi

echo "==> Bundling NSS runtime modules from ${NSS_MODULES_DIR}..."
mkdir -p "${APPDIR}/usr/lib/nss"
shopt -s nullglob
NSS_RUNTIME_FILES=(
    "${NSS_MODULES_DIR}"/*.so
    "${NSS_MODULES_DIR}"/*.so.*
    "${NSS_MODULES_DIR}"/*.chk
)
shopt -u nullglob
if [ "${#NSS_RUNTIME_FILES[@]}" -eq 0 ]; then
    echo "ERROR: No NSS runtime files found in ${NSS_MODULES_DIR}." >&2
    exit 1
fi
cp -pL "${NSS_RUNTIME_FILES[@]}" "${APPDIR}/usr/lib/nss/"
if [ ! -s "${APPDIR}/usr/lib/nss/libsoftokn3.so" ]; then
    echo "ERROR: Failed to bundle libsoftokn3.so into the AppDir." >&2
    exit 1
fi

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
    # Chromium's V8 startup snapshot is a .bin file, not a .dat resource.
    # Without it QtWebEngine aborts during startup with
    # "Error loading V8 startup snapshot file".
    cp -p "${QT_DATA_DIR}/resources/"*.bin "${APPDIR}/usr/resources/" 2>/dev/null || true
fi

if [ ! -f "${APPDIR}/usr/resources/v8_context_snapshot.bin" ]; then
    echo "ERROR: QtWebEngine V8 snapshot is missing from the AppDir." >&2
    echo "       Expected: ${QT_DATA_DIR}/resources/v8_context_snapshot.bin" >&2
    exit 1
fi

if [ -d "${QT_TRANS_DIR}/qtwebengine_locales" ]; then
    echo "==> Deploying QtWebEngine translations..."
    mkdir -p "${APPDIR}/usr/translations/qtwebengine_locales"
    cp -p "${QT_TRANS_DIR}/qtwebengine_locales/"*.pak "${APPDIR}/usr/translations/qtwebengine_locales/" 2>/dev/null || true
fi

# 6. Ensure original ELF binary is intact and create custom AppRun and run.sh launchers
echo "==> Restoring original binary and creating custom AppRun and run.sh..."
cp -f "${BUILD_DIR}/beaxty-vpn" "${APPDIR}/usr/bin/beaxty-vpn"
chmod +x "${APPDIR}/usr/bin/beaxty-vpn"

rm -f "${APPDIR}/AppRun"
cat << 'LAUNCHER' > "${APPDIR}/AppRun"
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export APPDIR="${HERE}"
# System Fontconfig fallback
if [ -z "${FONTCONFIG_PATH:-}" ] && [ -d "/etc/fonts" ]; then
    export FONTCONFIG_PATH="/etc/fonts"
fi
# Chromium sandbox flags. Keep the sandbox enabled by default; the escape
# hatch is intentionally explicit for diagnostics on systems without a helper.
if [ "${BEAXTY_ALLOW_NO_SANDBOX:-}" = "1" ] && [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi
# Libraries and Qt paths
export LD_LIBRARY_PATH="${HERE}/usr/lib/nss:${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
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

if [ "${BEAXTY_ALLOW_NO_SANDBOX:-}" = "1" ] && [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi

export LD_LIBRARY_PATH="${HERE}/usr/lib/nss:${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
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

# Sanity checks before packaging
if [ -L "${APPDIR}/AppRun" ]; then
    echo "ERROR: AppRun is a symlink! Must be a script." >&2
    exit 1
fi
if ! file "${APPDIR}/usr/bin/beaxty-vpn" | grep -q "ELF"; then
    echo "ERROR: ${APPDIR}/usr/bin/beaxty-vpn is NOT an ELF binary!" >&2
    exit 1
fi

echo "============================================================"
echo "==> Verifying complete dependency closure in AppDir..."
echo "============================================================"
FAIL_VERIFY=0
ERRORS=()
while IFS= read -r elf_file; do
    if file "${elf_file}" | grep -q "ELF"; then
        ldd_out=$(LD_LIBRARY_PATH="${APPDIR}/usr/lib/nss:${APPDIR}/usr/lib:${APPDIR}/usr/lib/x86_64-linux-gnu" ldd "${elf_file}" 2>&1 || true)
        
        # 1. Check for missing dependencies
        if echo "${ldd_out}" | grep -q "not found"; then
            ERRORS+=("Missing dependency in ${elf_file}: $(echo "${ldd_out}" | grep "not found")")
            FAIL_VERIFY=1
        fi
        
        # 2. Check for host Qt leakage (must never resolve to host /usr/lib or /lib for libQt6)
        if echo "${ldd_out}" | grep -E "=> /(usr/)?lib/(x86_64-linux-gnu/)?libQt6" | grep -v "${APPDIR}"; then
            ERRORS+=("Host Qt library leak in ${elf_file}: $(echo "${ldd_out}" | grep -E "=> /(usr/)?lib/(x86_64-linux-gnu/)?libQt6" | grep -v "${APPDIR}")")
            FAIL_VERIFY=1
        fi
    fi
done < <(find "${APPDIR}/usr" -type f)
if [ "${FAIL_VERIFY}" -ne 0 ]; then
    echo "FATAL: Pre-package dependency closure verification FAILED!" >&2
    for err in "${ERRORS[@]}"; do
        echo "  - ${err}" >&2
    done
    exit 1
fi
echo "==> Dependency verification PASSED! Zero missing libraries and zero host Qt leaks."
echo "============================================================"

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
