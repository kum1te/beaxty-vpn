cmake_minimum_required(VERSION 3.20)

project(BeaxtyVPN VERSION 1.0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (UNIX AND NOT APPLE)
    set(QT_DBUS_COMPONENT DBus)
    set(QT_DBUS_LIBRARY Qt6::DBus)
endif ()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Network ${QT_DBUS_COMPONENT})
find_package(Qt6 OPTIONAL_COMPONENTS WebEngineQuick)
find_package(Threads REQUIRED)

set(THRONE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/throne)

# Platform specific configuration
if (WIN32)
    set(PLATFORM_SOURCES
        ${THRONE_DIR}/3rdparty/WinCommander.cpp
        ${THRONE_DIR}/src/sys/windows/guihelper.cpp
        ${THRONE_DIR}/src/sys/windows/MiniDump.cpp
        ${THRONE_DIR}/src/sys/windows/eventHandler.cpp
        ${THRONE_DIR}/src/sys/windows/WinVersion.cpp
        ${THRONE_DIR}/src/sys/windows/AutoRun.cpp
        ${THRONE_DIR}/src/sys/windows/UrlScheme.cpp
    )
    set(PLATFORM_LIBRARIES wininet wsock32 ws2_32 user32 rasapi32 iphlpapi ntdll wbemuuid psapi shell32)
    add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
    set(GUI_TYPE WIN32)
    if (MSVC)
        add_compile_options("/utf-8")
        add_definitions(-D_WIN32_WINNT=0x600 -D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
    endif ()
elseif (APPLE)
    find_library(SECURITY_FRAMEWORK Security)
    set(PLATFORM_SOURCES
        ${THRONE_DIR}/src/sys/macos/MacOS.cpp
        ${THRONE_DIR}/src/sys/macos/AutoRun.cpp
        ${THRONE_DIR}/src/sys/macos/UrlScheme.cpp
    )
    set(PLATFORM_LIBRARIES ${SECURITY_FRAMEWORK})
    set(GUI_TYPE MACOSX_BUNDLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME "BeaxtyVPN")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.beaxty.vpn")
    set(MACOSX_BUNDLE_ICON_FILE "Throne.icns")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.5")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "1.0.5")
else ()
    set(PLATFORM_SOURCES
        ${THRONE_DIR}/src/sys/linux/LinuxCap.cpp
        ${THRONE_DIR}/src/sys/linux/coreDump.cpp
        ${THRONE_DIR}/src/sys/linux/AutoRun.cpp
        ${THRONE_DIR}/src/sys/linux/UrlScheme.cpp
        ${THRONE_DIR}/src/sys/linux/systemChecks.cpp
    )
    set(PLATFORM_LIBRARIES dl ${QT_DBUS_LIBRARY})
endif ()

# Generate NkrVersion.h required by Throne headers
set(NKR_VERSION "1.0.5.0")
configure_file("${THRONE_DIR}/cmake/NkrVersion.h.in" "${CMAKE_CURRENT_BINARY_DIR}/NkrVersion.h" @ONLY)

# Protobuf support via Throne's simple-protobuf
add_subdirectory(${THRONE_DIR}/3rdparty/simple-protobuf simple-protobuf-build)
spb_protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${THRONE_DIR}/core/server/gen/libcore.proto)

add_library(myproto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(myproto PUBLIC spb-proto)
target_include_directories(myproto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${THRONE_DIR}
)

# SQLite compilation definitions
set(SQLITE_DEFINITIONS
    SQLITE_DQS=0
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_MAX_EXPR_DEPTH=0
    SQLITE_UNTESTABLE
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_AUTHORIZATION
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_COMPLETE
    SQLITE_OMIT_GET_TABLE
    SQLITE_OMIT_TRACE
    SQLITE_OMIT_UTF16
    SQLITE_OMIT_INCRBLOB
    SQLITE_OMIT_DESERIALIZE
    SQLITE_OMIT_JSON
)

# Backend static library (Throne Core isolated from legacy QWidget UI)
set(BACKEND_SOURCES
    ${PLATFORM_SOURCES}

    # Global helpers & state
    ${THRONE_DIR}/src/global/Configs.cpp
    ${THRONE_DIR}/src/global/Utils.cpp
    ${THRONE_DIR}/src/global/Logger.cpp
    ${THRONE_DIR}/src/global/PeriodicRunner.cpp
    ${THRONE_DIR}/src/global/HTTPRequestHelper.cpp
    ${THRONE_DIR}/src/global/DeviceDetailsHelper.cpp
    ${THRONE_DIR}/src/global/CountryHelper.cpp
    ${THRONE_DIR}/src/global/LocalNetwork.cpp
    ${THRONE_DIR}/src/global/OTP.cpp
    ${THRONE_DIR}/src/global/OtpPlaceholder.cpp
    ${THRONE_DIR}/src/global/VpnCredentialOverride.cpp
    ${THRONE_DIR}/src/global/Common.cpp

    # 3rdparty libraries
    ${THRONE_DIR}/3rdparty/qrcodegen.cpp
    ${THRONE_DIR}/3rdparty/QrDecoder.cpp
    ${THRONE_DIR}/3rdparty/quirc/decode.c
    ${THRONE_DIR}/3rdparty/quirc/identify.c
    ${THRONE_DIR}/3rdparty/quirc/quirc.c
    ${THRONE_DIR}/3rdparty/quirc/version_db.c
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/Backup.cpp
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/Column.cpp
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/Database.cpp
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/Exception.cpp
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/Savepoint.cpp
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/sqlite3.c
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/Statement.cpp
    ${THRONE_DIR}/3rdparty/SQLiteCpp/src/Transaction.cpp

    # API & IPC
    ${THRONE_DIR}/src/api/RPC.cpp

    # Stats & loopers
    ${THRONE_DIR}/src/stats/traffic/TrafficLooper.cpp
    ${THRONE_DIR}/src/stats/traffic/TrafficStatsManager.cpp
    ${THRONE_DIR}/src/stats/connectionLister/connectionLister.cpp
    ${THRONE_DIR}/src/stats/autoselector/AutoSelectorMonitor.cpp

    # Subscription updater & parser
    ${THRONE_DIR}/src/configs/sub/GroupUpdater.cpp
    ${THRONE_DIR}/src/configs/sub/SubscriptionParser.cpp
    ${THRONE_DIR}/src/configs/sub/SubscriptionReconcile.cpp
    ${THRONE_DIR}/src/configs/sub/RouteUpdater.cpp
    ${THRONE_DIR}/src/configs/sub/clash.cpp
    ${THRONE_DIR}/src/configs/sub/vpnFileImport.cpp
    ${THRONE_DIR}/src/configs/sub/warp.cpp

    # Sys process
    ${THRONE_DIR}/src/sys/Process.cpp
    ${THRONE_DIR}/src/sys/ProcessMetrics.cpp
    ${THRONE_DIR}/src/sys/UrlScheme.cpp

    # Outbounds & Config generation
    ${THRONE_DIR}/src/configs/common/DialFields.cpp
    ${THRONE_DIR}/src/configs/common/QUICFields.cpp
    ${THRONE_DIR}/src/configs/common/TLS.cpp
    ${THRONE_DIR}/src/configs/common/multiplex.cpp
    ${THRONE_DIR}/src/configs/common/transport.cpp
    ${THRONE_DIR}/src/configs/common/Outbound.cpp
    ${THRONE_DIR}/src/configs/common/OutboundFactory.cpp
    ${THRONE_DIR}/src/configs/common/utils.cpp
    ${THRONE_DIR}/src/configs/common/xrayMultiplex.cpp
    ${THRONE_DIR}/src/configs/common/xrayStreamSetting.cpp
    ${THRONE_DIR}/src/configs/outbounds/anyTLS.cpp
    ${THRONE_DIR}/src/configs/outbounds/mieru.cpp
    ${THRONE_DIR}/src/configs/outbounds/snell.cpp
    ${THRONE_DIR}/src/configs/outbounds/http.cpp
    ${THRONE_DIR}/src/configs/outbounds/hysteria.cpp
    ${THRONE_DIR}/src/configs/outbounds/shadowsocks.cpp
    ${THRONE_DIR}/src/configs/outbounds/socks.cpp
    ${THRONE_DIR}/src/configs/outbounds/ssh.cpp
    ${THRONE_DIR}/src/configs/outbounds/tailscale.cpp
    ${THRONE_DIR}/src/configs/outbounds/trojan.cpp
    ${THRONE_DIR}/src/configs/outbounds/tuic.cpp
    ${THRONE_DIR}/src/configs/outbounds/juicity.cpp
    ${THRONE_DIR}/src/configs/outbounds/trusttunnel.cpp
    ${THRONE_DIR}/src/configs/outbounds/naive.cpp
    ${THRONE_DIR}/src/configs/outbounds/shadowtls.cpp
    ${THRONE_DIR}/src/configs/outbounds/vless.cpp
    ${THRONE_DIR}/src/configs/outbounds/vmess.cpp
    ${THRONE_DIR}/src/configs/outbounds/wireguard.cpp
    ${THRONE_DIR}/src/configs/outbounds/openvpn.cpp
    ${THRONE_DIR}/src/configs/outbounds/openconnect.cpp
    ${THRONE_DIR}/src/configs/outbounds/custom.cpp
    ${THRONE_DIR}/src/configs/outbounds/autoselector.cpp
    ${THRONE_DIR}/src/configs/outbounds/xrayVless.cpp
    ${THRONE_DIR}/src/configs/AutoSelectorPlan.cpp
    ${THRONE_DIR}/src/configs/generate.cpp

    # SQLite Database Repositories
    ${THRONE_DIR}/src/database/Database.cpp
    ${THRONE_DIR}/src/database/ProfilesRepo.cpp
    ${THRONE_DIR}/src/database/GroupsRepo.cpp
    ${THRONE_DIR}/src/database/RoutesRepo.cpp
    ${THRONE_DIR}/src/database/OtpProfilesRepo.cpp
    ${THRONE_DIR}/src/database/SettingsRepo.cpp
    ${THRONE_DIR}/src/database/TrafficStatsRepo.cpp
    ${THRONE_DIR}/src/database/DatabaseManager.cpp
    ${THRONE_DIR}/src/database/entities/Profile.cpp
    ${THRONE_DIR}/src/database/entities/Group.cpp
    ${THRONE_DIR}/src/database/entities/RouteProfile.cpp
    ${THRONE_DIR}/src/database/entities/RouteRule.cpp
    ${THRONE_DIR}/src/database/entities/OtpProfile.cpp

    # Headless MainWindow implementation
    ${THRONE_DIR}/include/ui/mainwindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge/MainWindowBridge.cpp

    ${THRONE_DIR}/res/Throne.qrc
)

add_library(throne_backend STATIC ${BACKEND_SOURCES})

target_compile_definitions(throne_backend PUBLIC
    MW_INTERFACE
    QT_MESSAGELOGCONTEXT
)
target_compile_definitions(throne_backend PRIVATE
    ${SQLITE_DEFINITIONS}
)

target_include_directories(throne_backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/throne
    ${THRONE_DIR}
    ${THRONE_DIR}/include
    ${THRONE_DIR}/3rdparty
    ${THRONE_DIR}/3rdparty/SQLiteCpp/include
    ${THRONE_DIR}/3rdparty/fkYAML/include
)

target_link_libraries(throne_backend PUBLIC
    Qt6::Widgets
    Qt6::Network
    Threads::Threads
    myproto
    spb-proto
    ${PLATFORM_LIBRARIES}
)

# Wrapper facade layer, shared by the app and every test target so the source list
# lives in exactly one place.
add_library(beaxty_core STATIC
    src/core/AppPrefs.hpp
    src/core/AppPrefs.cpp
    src/core/LocalizationManager.hpp
    src/core/LocalizationManager.cpp
    src/core/ThroneEngine.hpp
    src/core/ThroneEngine.cpp
    src/core/ConfigAdapter.hpp
    src/core/ConfigAdapter.cpp
    src/core/DeviceIdentity.hpp
    src/core/DeviceIdentity.cpp
    src/core/RoutingManager.hpp
    src/core/RoutingManager.cpp
    src/core/TrafficMonitor.hpp
    src/core/TrafficMonitor.cpp
    src/core/ToastManager.hpp
    src/core/ToastManager.cpp
    src/core/DeepLinkManager.hpp
    src/core/DeepLinkManager.cpp
    src/core/AutostartManager.hpp
    src/core/AutostartManager.cpp
    src/ui/Theme.hpp
    src/ui/Theme.cpp
)

target_compile_definitions(beaxty_core PUBLIC
    MW_INTERFACE
    QT_MESSAGELOGCONTEXT
)

target_include_directories(beaxty_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${THRONE_DIR}
    ${THRONE_DIR}/include
)

target_link_libraries(beaxty_core PUBLIC
    throne_backend
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Qml
    ${QT_DBUS_LIBRARY}
)

find_program(QSB_EXECUTABLE NAMES qsb PATHS /usr/lib/qt6/bin)
if(QSB_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/halftone_wave.frag.qsb
        COMMAND ${QSB_EXECUTABLE} --qt6 -o ${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/halftone_wave.frag.qsb ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/shaders/halftone_wave.frag
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/shaders/halftone_wave.frag
        COMMENT "Baking halftone_wave.frag into QSB bytecode"
    )
    add_custom_target(bake_shaders ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/halftone_wave.frag.qsb)
endif()

# QML and icons. This has to be compiled into each executable rather than into
# beaxty_core: a .qrc in a static library registers itself from a global
# initializer the linker drops when no symbol in that object is referenced, which
# silently leaves every qrc:/ lookup failing at runtime.
set(BEAXTY_RESOURCES res/resources.qrc)

# BeaxtyVPN Main Executable
add_executable(beaxty-vpn ${GUI_TYPE} src/main.cpp ${BEAXTY_RESOURCES})

if (APPLE)
    set_target_properties(beaxty-vpn PROPERTIES
        OUTPUT_NAME "BeaxtyVPN"
        MACOSX_BUNDLE_BUNDLE_NAME "BeaxtyVPN"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.beaxty.vpn"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.5"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.5"
    )
endif ()

if (WIN32)
    target_sources(beaxty-vpn PRIVATE res/windows.rc)
    if (MSVC)
        set_target_properties(beaxty-vpn PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
    endif()
endif()

target_link_libraries(beaxty-vpn PRIVATE
    beaxty_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
)
if (TARGET Qt6::WebEngineQuick)
    target_link_libraries(beaxty-vpn PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(beaxty-vpn PRIVATE BEAXTY_HAS_WEBENGINE=1)
endif()

# Test Targets
# Console-only tests: everything they need comes in through beaxty_core.
foreach(_beaxty_test
    test_hwid
    test_config_builder
    test_subscription_import
    test_subscription_safety
    test_connect_responsiveness
    test_traffic_looper_lifecycle
    test_routing_presets
    test_full_tunnel_wiring
    test_per_app_routing
    test_ping_diag
    test_security_audit
    test_deeplink
)
    add_executable(${_beaxty_test} tests/${_beaxty_test}.cpp ${BEAXTY_RESOURCES})
    target_link_libraries(${_beaxty_test} PRIVATE beaxty_core)
endforeach()

# Screenshot harness: additionally needs the QML runtime.
add_executable(test_capture_ui tests/test_capture_ui.cpp ${BEAXTY_RESOURCES})
target_link_libraries(test_capture_ui PRIVATE
    beaxty_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
)
if (TARGET Qt6::WebEngineQuick)
    target_link_libraries(test_capture_ui PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(test_capture_ui PRIVATE BEAXTY_HAS_WEBENGINE=1)
endif()





# Deterministic, local checks available through one standard command.
enable_testing()
foreach(_test test_hwid test_config_builder test_subscription_import test_subscription_safety
              test_routing_presets test_full_tunnel_wiring test_per_app_routing test_security_audit
              test_deeplink)
    add_test(NAME ${_test} COMMAND ${_test})
    set_tests_properties(${_test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    # Existing tests use assert(); keep their checks active in Release builds.
    if(MSVC)
        target_compile_options(${_test} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${_test} PRIVATE -UNDEBUG)
    endif()
endforeach()
