cmake_minimum_required(VERSION 3.16)

project(IBKRHotkeyTrader VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Charts Network Sql)
find_package(Protobuf REQUIRED)

# Try to find abseil-cpp, or use Homebrew path if not found
find_package(absl CONFIG)
if(NOT absl_FOUND)
    message(STATUS "absl not found via find_package, trying Homebrew")
    set(ABSL_ROOT "/opt/homebrew/opt/abseil")
    if(EXISTS "${ABSL_ROOT}")
        include_directories("${ABSL_ROOT}/include")
        link_directories("${ABSL_ROOT}/lib")
        message(STATUS "Using abseil from: ${ABSL_ROOT}")
    endif()
endif()

# IBKR TWS API
# The TWS API C++ client files should be in external/twsapi
# Run ./download_tws_api.sh to download and extract automatically
set(TWS_API_DIR "${CMAKE_SOURCE_DIR}/external/twsapi")

if(NOT EXISTS "${TWS_API_DIR}/source/cppclient/client")
    message(FATAL_ERROR
        "\n"
        "═══════════════════════════════════════════════════════════════\n"
        " TWS API not found!\n"
        "═══════════════════════════════════════════════════════════════\n"
        "\n"
        " Please run the download script:\n"
        "   ./download_tws_api.sh\n"
        "\n"
        " Or manually download from:\n"
        "   https://interactivebrokers.github.io/\n"
        "\n"
        " Extract to: ${TWS_API_DIR}\n"
        "═══════════════════════════════════════════════════════════════\n"
    )
endif()

message(STATUS "Using TWS API from: ${TWS_API_DIR}")

# Generate protobuf files from TWS API proto definitions
file(GLOB TWS_PROTO_FILES "${TWS_API_DIR}/source/proto/*.proto")
set(PROTO_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_OUTPUT_DIR})

foreach(PROTO_FILE ${TWS_PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    list(APPEND PROTO_SRCS "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.cc")
    list(APPEND PROTO_HDRS "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.h")

    add_custom_command(
        OUTPUT "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.cc" "${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.h"
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${PROTO_OUTPUT_DIR} -I${TWS_API_DIR}/source/proto ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ code from ${PROTO_NAME}.proto"
    )
endforeach()

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${TWS_API_DIR}/source/cppclient/client
    ${PROTO_OUTPUT_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/bid_stub.cpp
    # UI
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/toastnotification.cpp
    src/ui/toastnotification.h
    # Dialogs
    src/dialogs/settingsdialog.cpp
    src/dialogs/settingsdialog.h
    src/dialogs/symbolsearchdialog.cpp
    src/dialogs/symbolsearchdialog.h
    src/dialogs/debuglogdialog.cpp
    src/dialogs/debuglogdialog.h
    # Widgets
    src/widgets/chartwidget.cpp
    src/widgets/chartwidget.h
    src/widgets/tickerlistwidget.cpp
    src/widgets/tickerlistwidget.h
    src/widgets/tickeritemdelegate.cpp
    src/widgets/tickeritemdelegate.h
    src/widgets/orderhistorywidget.cpp
    src/widgets/orderhistorywidget.h
    # Client
    src/client/ibkrclient.cpp
    src/client/ibkrclient.h
    src/client/ibkrwrapper.cpp
    src/client/ibkrwrapper.h
    # Trading
    src/trading/tradingmanager.cpp
    src/trading/tradingmanager.h
    # Models
    src/models/settings.cpp
    src/models/settings.h
    src/models/uistate.cpp
    src/models/uistate.h
    src/models/order.cpp
    src/models/order.h
    # Utils
    src/utils/logger.cpp
    src/utils/logger.h
)

# TWS API source files
file(GLOB TWS_API_SOURCES
    ${TWS_API_DIR}/source/cppclient/client/*.cpp
)

# Remove main.cpp from TWS API sources if exists
list(FILTER TWS_API_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${TWS_API_SOURCES}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

if(absl_FOUND)
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Charts
        Qt6::Network
        Qt6::Sql
        ${Protobuf_LIBRARIES}
        absl::base
        absl::hash
        absl::log_internal_check_op
        absl::log_internal_message
        absl::strings
    )
else()
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Charts
        Qt6::Network
        Qt6::Sql
        ${Protobuf_LIBRARIES}
        absl_base
        absl_hash
        absl_log_internal_check_op
        absl_log_internal_message
        absl_strings
    )
endif()

# Platform-specific settings
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "IBKR Hotkey Trader"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.kinect-pro.ibkr-hotkey-trader"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE icon.icns
    )

    # Copy icon to bundle resources
    set_source_files_properties(${CMAKE_SOURCE_DIR}/resources/icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/resources/icon.icns)
endif()

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # Windows icon resource
    if(EXISTS ${CMAKE_SOURCE_DIR}/resources/icon.ico)
        configure_file(
            ${CMAKE_SOURCE_DIR}/resources/icon.rc.in
            ${CMAKE_BINARY_DIR}/icon.rc
        )
        target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/icon.rc)
    endif()
endif()

# Install
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
