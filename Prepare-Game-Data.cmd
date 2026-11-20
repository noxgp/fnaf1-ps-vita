@echo off
cd /d "%~dp0"
if "%~1"=="" (
  echo Drag your supported APK onto this file, or run prepare_assets.py --help.
  pause
  exit /b 1
)
py -3 prepare_assets.py --apk "%~1"
if errorlevel 1 (pause & exit /b 1)
pause
