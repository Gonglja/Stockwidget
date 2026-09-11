@echo off
setlocal
set "QT=C:/1/Qt/6.11.1/msvc2022_64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%QT%\bin;%PATH%"
ctest --test-dir "%~dp0..\build" --output-on-failure || exit /b 1
