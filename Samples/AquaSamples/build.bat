@echo off
rem Build or run this project with RutoEngine.
rem   build.bat [debug^|release] [build^|run^|package^|clean^|editor-build]
rem   example: build.bat debug run
rem   RutoEngine location: %RUTO_ENGINE_DIR%, otherwise MyProgram\RutoEngine (next to the Aqua folder)
setlocal
set "ENGINE_DIR=%RUTO_ENGINE_DIR%"
if "%ENGINE_DIR%"=="" for %%i in ("%~dp0..\..\..\RutoEngine") do set "ENGINE_DIR=%%~fi"
call "%ENGINE_DIR%\Tools\ruto_build.bat" "%~dp0." %*
