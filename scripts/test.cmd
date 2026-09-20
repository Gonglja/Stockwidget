@echo off
setlocal
if not defined QT_DIR set "QT_DIR=C:/1/Qt/6.11.1/msvc2022_64"

call "%~dp0_vcvars.cmd" || exit /b 1
set "PATH=%QT_DIR%\bin;%PATH%"

rem 额外参数直接透传给 ctest，例如：test.cmd -E test_ui --timeout 180
ctest --test-dir "%~dp0..\build" %* --output-on-failure || exit /b 1
