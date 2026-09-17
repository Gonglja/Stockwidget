cmake_minimum_required(VERSION 3.21)
project(StockWidget VERSION 1.0.0 LANGUAGES CXX)

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
    src/main.cpp
    src/app/Application.cpp
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
