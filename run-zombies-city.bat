@echo off
setlocal
set "ZOMBIES_LEVEL=%~dp0build-win64\bin\Assets\zombies\hollywood_city_preview\level.json"
call "%~dp0run-zombies.bat"
exit /b %errorlevel%
