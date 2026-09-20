@echo off
setlocal
set "ROOT=%~dp0.."
if not defined QT_DIR set "QT_DIR=C:/1/Qt/6.11.1/msvc2022_64"

call "%~dp0_vcvars.cmd" || exit /b 1
set "PATH=%QT_DIR%\bin;%PATH%"

rem 额外参数透传给 cmake；版本号可用环境变量注入：set SW_VERSION_OVERRIDE=v1.6.2
cmake -S "%ROOT%" -B "%ROOT%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT_DIR% %* || exit /b 1
cmake --build "%ROOT%\build" || exit /b 1
