qt_add_executable(test_codes test_codes.cpp)
target_link_libraries(test_codes PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_codes COMMAND test_codes)

qt_add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_parser COMMAND test_parser)

qt_add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_config COMMAND test_config)

qt_add_executable(test_model test_model.cpp)
target_link_libraries(test_model PRIVATE Qt6::Test StockWidgetCore)
add_test(NAME test_model COMMAND test_model)

# UI 交互测试（复用 app 的 UI 源文件）
qt_add_executable(test_ui test_ui.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/FloatWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/KLineDelegate.cpp)
target_include_directories(test_ui PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ui PRIVATE Qt6::Test Qt6::Widgets StockWidgetCore)
add_test(NAME test_ui COMMAND test_ui)
