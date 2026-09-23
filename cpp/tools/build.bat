@echo off
setlocal
title Build GEDI Process Desktop C++
set "APP_DIR=%~dp0.."
rem Override these for your machine, or set QT_DIR / HDF5_DLL beforehand
if not defined QT_DIR set "QT_DIR=F:\software\QT\6.10.1\mingw_64"
if not defined MINGW_DIR set "MINGW_DIR=F:\software\QT\Tools\mingw1310_64"
if not defined CMAKE_EXE set "CMAKE_EXE=F:\software\QT\Tools\CMake_64\bin\cmake.exe"
if not defined NINJA_EXE set "NINJA_EXE=F:\software\QT\Tools\Ninja\ninja.exe"
if not defined HDF5_DLL set "HDF5_DLL=D:\software\Anaconda\Library\bin\hdf5.dll"
if not defined HDF5_ZLIB set "HDF5_ZLIB=D:\software\Anaconda\Library\bin\zlib.dll"

if not exist "%CMAKE_EXE%" (
  echo [ERROR] cmake not found:
  echo   %CMAKE_EXE%
  pause
  exit /b 1
)
if not exist "%MINGW_DIR%\bin\g++.exe" (
  echo [ERROR] g++ not found:
  echo   %MINGW_DIR%\bin\g++.exe
  pause
  exit /b 1
)

set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin%;F:\software\QT\Tools\CMake_64\bin;F:\software\QT\Tools\Ninja;%PATH%"

cd /d "%APP_DIR%"
echo Configure with CMake...
"%CMAKE_EXE%" -S . -B build -G Ninja ^
  -DCMAKE_PREFIX_PATH=%QT_DIR% ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"
if errorlevel 1 (
  echo [ERROR] Configure failed.
  pause
  exit /b 1
)

echo Build...
"%CMAKE_EXE%" --build build -j
if errorlevel 1 (
  echo [ERROR] Build failed.
  pause
  exit /b 1
)

echo Deploy Qt DLLs (windeployqt)...
if exist "%QT_DIR%\bin\windeployqt.exe" (
  "%QT_DIR%\bin\windeployqt.exe" --release --no-translations "build\GEDIProcessDesktopCpp.exe"
) else (
  echo [WARN] windeployqt not found, copy Qt DLLs manually if exe will not start.
)

echo Copy HDF5...
if exist "%HDF5_DLL%" copy /Y "%HDF5_DLL%" "build\" >nul
if exist "%HDF5_ZLIB%" copy /Y "%HDF5_ZLIB%" "build\" >nul
if exist "%MINGW_DIR%\bin\libgcc_s_seh-1.dll" copy /Y "%MINGW_DIR%\bin\libgcc_s_seh-1.dll" "build\" >nul
if exist "%MINGW_DIR%\bin\libstdc++-6.dll" copy /Y "%MINGW_DIR%\bin\libstdc++-6.dll" "build\" >nul
if exist "%MINGW_DIR%\bin\libwinpthread-1.dll" copy /Y "%MINGW_DIR%\bin\libwinpthread-1.dll" "build\" >nul

echo.
echo [OK] Run: build\GEDIProcessDesktopCpp.exe
pause
endlocal
