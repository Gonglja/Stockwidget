@echo off
setlocal
set "ROOT=%~dp0.."
set "QT=C:/1/Qt/6.11.1/msvc2022_64"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=%QT%\bin;%PATH%"
cmake -S "%ROOT%" -B "%ROOT%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT% || exit /b 1
cmake --build "%ROOT%\build" || exit /b 1
