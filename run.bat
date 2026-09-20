@echo off
setlocal
set "CONDA_PY=D:\software\Anaconda\envs\gedi_desktop\python.exe"
if not exist "%CONDA_PY%" (
  echo [ERROR] Conda env python not found: %CONDA_PY%
  echo Please create env: conda create -n gedi_desktop python=3.11 -y
  echo Then: "%CONDA_PY%" -m pip install -r requirements.txt
  pause
  exit /b 1
)
cd /d "%~dp0"
"%CONDA_PY%" main.py %*
endlocal
