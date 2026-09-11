@echo off
setlocal
set "ROOT=%~dp0.."
set "QT=C:/1/Qt/6.11.1/msvc2022_64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%QT%\bin;%PATH%"
cmake --build "%ROOT%\build" --config Release || exit /b 1
if exist "%ROOT%\dist" rmdir /s /q "%ROOT%\dist"
mkdir "%ROOT%\dist"
copy "%ROOT%\build\StockWidget.exe" "%ROOT%\dist\" >nul
"%QT%\bin\windeployqt.exe" --release --no-translations "%ROOT%\dist\StockWidget.exe" || exit /b 1
echo Packaged to %ROOT%\dist
