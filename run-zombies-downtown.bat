@echo off
setlocal
set "ZOMBIES_LEVEL=%~dp0build-win64\bin\Assets\zombies\downtown_survival_01\level.json"
if not exist "%ZOMBIES_LEVEL%" (
    echo No existe el nivel Downtown. Generalo con:
    echo   python tools\zombies_convert\build_downtown.py
    pause
    exit /b 1
)
call "%~dp0run-zombies.bat"
exit /b %errorlevel%
