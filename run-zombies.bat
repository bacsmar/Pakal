@echo off
setlocal
pushd "%~dp0build-win64\bin" 2>nul
if errorlevel 1 (
    echo No existe build-win64\bin. Compila primero el proyecto.
    pause
    exit /b 1
)
if not exist PakalPlayer.exe goto :missing
if not exist Pakal.dll goto :missing
if not exist ZombiesModule.dll goto :missing
if not defined ZOMBIES_LEVEL if not exist Assets\zombies\hollywood_s01t01\level.json (
    echo No se encontro el nivel convertido de Zombies.
    echo Consulta docs\BUILDING.md para regenerarlo con ZombiesConvert.
    popd
    pause
    exit /b 1
)
PakalPlayer.exe ZombiesModule.dll
set "game_result=%errorlevel%"
popd
if not "%game_result%"=="0" (
    echo El juego termino con codigo %game_result%.
    pause
)
exit /b %game_result%

:missing
echo Faltan PakalPlayer.exe, Pakal.dll o ZombiesModule.dll. Compila primero el proyecto.
popd
pause
exit /b 1
