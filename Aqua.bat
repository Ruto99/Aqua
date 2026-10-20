@echo off
rem Start Aqua (the UI building tool for RutoEngine) on its own.
rem Builds Aqua first (build.bat release install) when bin\Aqua.exe does not exist yet,
rem or when the Aqua source or the RutoEngine SDK is newer than bin\Aqua.exe.
rem Arguments are passed to Aqua, e.g.  Aqua.bat --project D:\Games\MyGame
setlocal EnableExtensions
cd /d "%~dp0"
set "AQUA_EXE=%~dp0bin\Aqua.exe"

set "ENGINE_DIR=%RUTO_ENGINE_DIR%"
if "%ENGINE_DIR%"=="" for %%i in ("%~dp0..\RutoEngine") do set "ENGINE_DIR=%%~fi"

if not exist "%AQUA_EXE%" (
    echo [Aqua] Aqua has not been built yet. Building...
    goto :build
)
rem Rebuild when something is newer than the exe (source, CMake files, the engine SDK)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$exe = (Get-Item -LiteralPath $env:AQUA_EXE).LastWriteTime;" ^
  "$files = @(Get-ChildItem -LiteralPath 'Source','Resources' -Recurse -File) + @(Get-Item 'CMakeLists.txt','CMakePresets.json');" ^
  "$sdk = Join-Path $env:ENGINE_DIR 'SDK\Release\lib\RutoEngine.lib'; if (Test-Path -LiteralPath $sdk) { $files += Get-Item -LiteralPath $sdk };" ^
  "if ($files | Where-Object { $_.LastWriteTime -gt $exe }) { exit 1 } else { exit 0 }"
if not errorlevel 1 goto :start
echo [Aqua] Aqua or the RutoEngine SDK was updated. Rebuilding...

:build
call "%~dp0build.bat" release install
if errorlevel 1 (
    if exist "%AQUA_EXE%" (
        echo [Aqua] Build failed. Starting the previous Aqua.exe.
        timeout /t 3 >nul
        goto :start
    )
    echo [Aqua] Build failed.
    pause
    exit /b 1
)

:start
start "" "%AQUA_EXE%" %*
