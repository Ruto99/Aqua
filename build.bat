@echo off
rem Build Aqua (links the RutoEngine SDK)
rem   build.bat [debug^|release] [build^|install^|run^|clean]
rem     install : release build, then copy to bin\Aqua.exe (RutoEditor / RutoLauncher launch it from there)
rem   RutoEngine location: %RUTO_ENGINE_DIR%, otherwise ..\RutoEngine
setlocal EnableExtensions
cd /d "%~dp0"
set "CONFIG=%~1"
set "ACTION=%~2"
if "%CONFIG%"=="" set "CONFIG=release"
if "%ACTION%"=="" set "ACTION=install"
if /i "%CONFIG%"=="install" (set "CONFIG=release" & set "ACTION=install")

set "ENGINE_DIR=%RUTO_ENGINE_DIR%"
if "%ENGINE_DIR%"=="" for %%i in ("%~dp0..\RutoEngine") do set "ENGINE_DIR=%%~fi"
if not exist "%ENGINE_DIR%\Tools\vsenv.bat" (
    echo [Aqua] RutoEngine was not found: %ENGINE_DIR%
    echo [Aqua] Set the RUTO_ENGINE_DIR environment variable to the engine folder.
    exit /b 1
)
call "%ENGINE_DIR%\Tools\vsenv.bat"
if errorlevel 1 exit /b 1

if /i "%ACTION%"=="clean" (
    if exist "build\%CONFIG%" rmdir /s /q "build\%CONFIG%"
    echo [Aqua] Cleaned build\%CONFIG%.
    exit /b 0
)

echo [Aqua] Configuring [%CONFIG%] with RutoEngine: %ENGINE_DIR%
cmake --preset %CONFIG% "-DRUTO_ENGINE_DIR=%ENGINE_DIR%"
if errorlevel 1 goto :failed
echo [Aqua] Building [%CONFIG%]
cmake --build --preset %CONFIG%
if errorlevel 1 goto :failed

if /i "%ACTION%"=="install" (
    rem A running Aqua.exe cannot be overwritten, so move the old one aside first
    if exist "bin\Aqua.exe" (
        del /q "bin\Aqua.exe.old" >nul 2>nul
        move /y "bin\Aqua.exe" "bin\Aqua.exe.old" >nul 2>nul
    )
    cmake --install "build\%CONFIG%"
    if errorlevel 1 goto :failed
    rem Remove the old exe now unless it is still running
    del /q "bin\Aqua.exe.old" >nul 2>nul
    echo [Aqua] Installed: %~dp0bin\Aqua.exe
)
if /i "%ACTION%"=="run" start "" "build\%CONFIG%\bin\Aqua.exe"
echo [Aqua] Build succeeded.
exit /b 0

:failed
echo [Aqua] Build failed.
exit /b 1
