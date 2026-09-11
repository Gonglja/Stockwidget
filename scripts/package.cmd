@echo off
setlocal
set "ROOT=%~dp0.."
if not defined QT_DIR set "QT_DIR=C:/1/Qt/6.11.1/msvc2022_64"

call "%~dp0_vcvars.cmd" || exit /b 1
set "PATH=%QT_DIR%\bin;%PATH%"

rem Refuse to package while an instance is running (dist files would be locked).
"%SystemRoot%\System32\tasklist.exe" /FI "IMAGENAME eq StockWidget.exe" 2>nul | "%SystemRoot%\System32\findstr.exe" /I /C:"StockWidget.exe" >nul
if not errorlevel 1 (
    echo [ERROR] StockWidget.exe is still running.
    echo         Exit it first ^(tray icon -^> Exit^), otherwise dist files are locked.
    exit /b 1
)

cmake --build "%ROOT%\build" --config Release || exit /b 1
if exist "%ROOT%\dist" rmdir /s /q "%ROOT%\dist" || exit /b 1
mkdir "%ROOT%\dist"
copy "%ROOT%\build\StockWidget.exe" "%ROOT%\dist\" >nul || exit /b 1
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations "%ROOT%\dist\StockWidget.exe" || exit /b 1
echo Packaged to %ROOT%\dist
