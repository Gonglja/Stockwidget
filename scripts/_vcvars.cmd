@echo off
rem Locate and invoke MSVC vcvars64.bat. Skips if already in a VS dev prompt.
rem NOTE: no setlocal here, otherwise PATH/cl changes do not propagate to the caller.
where cl >nul 2>nul && exit /b 0

set "VSPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=C:\Program Files\Microsoft Visual Studio\2022\Community"

if not defined VSPATH (
    echo [ERROR] Visual Studio ^(MSVC x64^) not found. Install VS2022 with the C++ workload.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
exit /b 0
