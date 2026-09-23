@echo off
setlocal
title Pack GEDI Process Desktop C++ (Inno Setup)
set "ROOT=%~dp0.."
set "ISS=%~dp0gedi_cpp_setup.iss"
set "OUT=%~dp0installer_output"

rem 默认打包仓库内 cpp\build；也可 set BUILD_DIR=... 指向其它构建目录
if not defined BUILD_DIR set "BUILD_DIR=%ROOT%\cpp\build"
if not exist "%BUILD_DIR%\GEDIProcessDesktopCpp.exe" (
  echo [ERROR] Not built: %BUILD_DIR%\GEDIProcessDesktopCpp.exe
  echo Run: cpp\tools\build.bat  (includes windeployqt^)
  echo Or:  set BUILD_DIR=...\gedi_desktop_cpp\build
  pause
  exit /b 1
)

set "ISCC="
if defined ISCC_EXE set "ISCC=%ISCC_EXE%"
if not defined ISCC if exist "D:\software\Inno Setup 7\ISCC.exe" set "ISCC=D:\software\Inno Setup 7\ISCC.exe"
if not defined ISCC if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"

if not defined ISCC (
  echo [ERROR] ISCC.exe not found. Install Inno Setup 6/7 or set ISCC_EXE.
  pause
  exit /b 1
)

echo Using: %ISCC%
echo Build: %BUILD_DIR%
"%ISCC%" "/DBuildDir=%BUILD_DIR%" "%ISS%"
if errorlevel 1 (
  echo [ERROR] Inno Setup compile failed.
  pause
  exit /b 1
)

echo.
echo [OK] Installer:
dir /b "%OUT%\*.exe"
pause
endlocal

