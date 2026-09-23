@echo off
setlocal
cd /d "%~dp0"
if defined GEDI_PYTHON (
  set "PY=%GEDI_PYTHON%"
) else (
  set "PY=python"
)
where "%PY%" >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Python not found. Set GEDI_PYTHON or add python to PATH.
  pause
  exit /b 1
)
"%PY%" main.py %*
endlocal
