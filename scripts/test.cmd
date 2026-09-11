@echo off
setlocal
if not defined QT_DIR set "QT_DIR=C:/1/Qt/6.11.1/msvc2022_64"

call "%~dp0_vcvars.cmd" || exit /b 1
set "PATH=%QT_DIR%\bin;%PATH%"

ctest --test-dir "%~dp0..\build" --output-on-failure || exit /b 1
