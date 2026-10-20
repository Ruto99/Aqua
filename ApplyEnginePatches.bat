@echo off
rem Apply the Aqua engine patches (EnginePatches\*.patch) to RutoEngine with git am,
rem then rebuild the RutoEngine SDK (Setup.bat) so that Aqua can be built.
rem   ApplyEnginePatches.bat [--no-setup] [--no-pause]
rem     --no-setup : only apply the patches (do not run Setup.bat)
rem     --no-pause : do not wait for a key at the end
rem   Patches that are already applied are skipped, so running this again is safe.
rem   RutoEngine location: %RUTO_ENGINE_DIR%, otherwise ..\RutoEngine
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "RUN_SETUP=1"
set "PAUSE_AT_END=1"
for %%a in (%*) do (
    if /i "%%~a"=="--no-setup" set "RUN_SETUP=0"
    if /i "%%~a"=="--no-pause" set "PAUSE_AT_END=0"
)

set "PATCH_DIR=%~dp0EnginePatches"
set "ENGINE_DIR=%RUTO_ENGINE_DIR%"
if "%ENGINE_DIR%"=="" for %%i in ("%~dp0..\RutoEngine") do set "ENGINE_DIR=%%~fi"
if not exist "%ENGINE_DIR%\Setup.bat" (
    echo [Aqua] RutoEngine was not found: %ENGINE_DIR%
    echo [Aqua] Set the RUTO_ENGINE_DIR environment variable to the engine folder.
    goto :failed
)
where git >nul 2>nul
if errorlevel 1 (
    echo [Aqua] git was not found. Install Git for Windows and try again.
    goto :failed
)
git -C "%ENGINE_DIR%" rev-parse --git-dir >nul 2>nul
if errorlevel 1 (
    echo [Aqua] RutoEngine is not a git repository: %ENGINE_DIR%
    goto :failed
)
if exist "%ENGINE_DIR%\.git\rebase-apply" (
    echo [Aqua] A previous git am is still in progress in RutoEngine.
    echo [Aqua] Finish it with "git am --continue" or cancel it with "git am --abort", then run this again.
    goto :failed
)

echo [Aqua] RutoEngine: %ENGINE_DIR%
rem patch-ids of the recent RutoEngine commits (a patch is already applied when its patch-id is among them)
set "APPLIED_IDS=%TEMP%\aqua_engine_patch_ids_%RANDOM%.txt"
git -C "%ENGINE_DIR%" log -p -n 300 | git patch-id --stable > "%APPLIED_IDS%"
set "APPLIED=0"
set "DIRTY_CHECKED=0"
for /f "delims=" %%p in ('dir /b /on "%PATCH_DIR%\*.patch"') do (
    set "PID="
    for /f "tokens=1" %%i in ('git patch-id --stable ^< "%PATCH_DIR%\%%p"') do set "PID=%%i"
    set "DONE=0"
    if defined PID (
        findstr /b /c:"!PID! " "%APPLIED_IDS%" >nul 2>nul
        if not errorlevel 1 set "DONE=1"
    )
    rem Also treat it as applied when the patch can be reversed cleanly (applied by hand, not committed)
    if "!DONE!"=="0" (
        git -C "%ENGINE_DIR%" apply --check --reverse "%PATCH_DIR%\%%p" >nul 2>nul
        if not errorlevel 1 set "DONE=1"
    )
    if "!DONE!"=="1" (
        echo [Aqua] Already applied: %%p
    ) else (
        if "!DIRTY_CHECKED!"=="0" (
            set "DIRTY_CHECKED=1"
            git -C "%ENGINE_DIR%" diff --quiet HEAD >nul 2>nul
            if errorlevel 1 (
                echo [Aqua] RutoEngine has uncommitted changes. Commit or stash them first, then run this again.
                goto :failed
            )
        )
        echo [Aqua] Applying: %%p
        git -C "%ENGINE_DIR%" am --3way "%PATCH_DIR%\%%p"
        if errorlevel 1 (
            echo [Aqua] Could not apply %%p. Undoing this patch ^(git am --abort^).
            git -C "%ENGINE_DIR%" am --abort >nul 2>nul
            goto :failed
        )
        set /a APPLIED+=1
    )
)
del /q "%APPLIED_IDS%" >nul 2>nul
echo [Aqua] Patches applied this time: %APPLIED%

if "%RUN_SETUP%"=="0" (
    echo [Aqua] Skipped Setup.bat. Run it in RutoEngine to rebuild the SDK.
    goto :done
)
echo [Aqua] Rebuilding the RutoEngine SDK ^(Setup.bat^)...
call "%ENGINE_DIR%\Setup.bat" --no-pause
if errorlevel 1 goto :failed

:done
echo [Aqua] Done. Build Aqua with build.bat or start it with Aqua.bat.
if "%PAUSE_AT_END%"=="1" pause
exit /b 0

:failed
if defined APPLIED_IDS del /q "%APPLIED_IDS%" >nul 2>nul
echo [Aqua] Failed.
if "%PAUSE_AT_END%"=="1" pause
exit /b 1
