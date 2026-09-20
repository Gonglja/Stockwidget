cmake_minimum_required(VERSION 3.21)

# ── 版本号：环境变量 SW_VERSION_OVERRIDE（CI 用 tag 注入，不缓存，避免污染 CMakeCache）
#            > git 当前 tag（v1.6.1 → 1.6.1）> 0.0.0
# 目的：让 project(VERSION ...)/可执行文件属性/托盘提示都跟着 tag 走，不再手改。
set(SW_VERSION "0.0.0")
if(DEFINED ENV{SW_VERSION_OVERRIDE} AND NOT "$ENV{SW_VERSION_OVERRIDE}" STREQUAL "")
    set(SW_VERSION "$ENV{SW_VERSION_OVERRIDE}")
else()
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0 --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE SW_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(SW_TAG MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
            set(SW_VERSION "${CMAKE_MATCH_1}")
        endif()
    endif()
endif()
string(REGEX REPLACE "^v" "" SW_VERSION "${SW_VERSION}")
string(STRIP "${SW_VERSION}" SW_VERSION)
if(NOT SW_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(WARNING "无法解析版本号 '${SW_VERSION}'（应为 x.y.z），回落 0.0.0")
    set(SW_VERSION "0.0.0")
endif()

project(StockWidget VERSION ${SW_VERSION} LANGUAGES CXX)

message(STATUS "StockWidget version = ${SW_VERSION}")
# 供 UI/测试显示与断言（SW_VERSION 展开为字符串字面量）
add_compile_definitions(SW_VERSION="${SW_VERSION}")
if(WIN32)
    enable_language(RC)  # 可执行文件的 VERSIONINFO（文件属性里能看到版本号）
    configure_file(resources/StockWidget.rc.in ${CMAKE_CURRENT_BINARY_DIR}/StockWidget.rc @ONLY)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network Core5Compat Test)
qt_standard_project_setup()

add_library(StockWidgetCore STATIC
    src/data/StockCode.cpp
    src/data/Schedule.cpp
    src/data/QuoteColumns.cpp
    src/data/QuoteParser.cpp
    src/data/QuoteSort.cpp
    src/data/NameAlias.cpp
    src/data/SinaQuoteSource.cpp
    src/data/StockSuggestSource.cpp
    src/app/ConfigStore.cpp
    src/ui/QuoteModel.cpp
)
target_include_directories(StockWidgetCore PUBLIC src)
target_link_libraries(StockWidgetCore PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Core5Compat)

qt_add_executable(StockWidget WIN32
    ${CMAKE_CURRENT_BINARY_DIR}/StockWidget.rc
    src/main.cpp
    src/app/Application.cpp
    src/ui/CustomConfigDialog.cpp
    src/ui/FloatWindow.cpp
    src/ui/KLineDelegate.cpp
    src/ui/SettingsDialog.cpp
    src/platform/GlobalHotkey.cpp
    src/platform/AutoStart.cpp
)
target_include_directories(StockWidget PRIVATE src)
target_link_libraries(StockWidget PRIVATE
    StockWidgetCore
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Core5Compat
    shell32
)

qt_add_resources(StockWidget "app_resources"
    PREFIX "/"
    BASE resources
    FILES resources/StockWidget.ico
)

enable_testing()
add_subdirectory(tests)
