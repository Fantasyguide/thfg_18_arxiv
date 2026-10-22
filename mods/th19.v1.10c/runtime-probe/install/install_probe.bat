@echo off
setlocal EnableExtensions
rem ===========================================================================
rem  th19_probe2 installer
rem
rem  The modkit launcher (the th19 launcher .bat) shows a CHECKBOX list and installs
rem  the ticked items. Per mods/README.md the launcher takes "mods/*.dll" and
rem  copies the ticked ones into <thcrap>\bin. So the right place to drop a
rem  plugin DLL is  <modkit>\mods\  - then you TICK it in the launcher.
rem
rem  This script:
rem    1) finds thcrap.dll  -> modkit root + <thcrap>\bin
rem    2) copies th19_probe2.dll into <modkit>\mods\   (checkbox path)
rem       - if <modkit>\mods\ does not exist, falls back to <thcrap>\bin
rem    3) writes th19_probe2.ini beside th19.exe
rem
rem  Deletes nothing, does not touch the game exe.  ASCII only + CRLF on
rem  purpose: cmd.exe renders .bat in the OEM codepage.
rem
rem  Usage:  install_probe.bat "D:\path\to\th19_modkit"
rem ===========================================================================

set "MK=%~1"
if "%MK%"=="" set /p MK=modkit dir (e.g. D:\...\th19_modkit):
if "%MK%"=="" goto :nopath
if not exist "%MK%\" goto :nopath
rem normalise trailing backslash
if "%MK:~-1%"=="\" set "MK=%MK:~0,-1%"
set "SRC=%~dp0..\native\th19_probe2.dll"
if not exist "%SRC%" set "SRC=%~dp0th19_probe2.dll"
if not exist "%SRC%" (
  echo [x] th19_probe2.dll not found - run make in native\ first.
  goto :end
)

echo.
echo [1/3] searching thcrap.dll under "%MK%" ...
set "THCRAP_DLL="
for /f "delims=" %%F in ('dir /s /b "%MK%\thcrap.dll" 2^>nul') do if not defined THCRAP_DLL set "THCRAP_DLL=%%F"
if not defined THCRAP_DLL (
  echo [x] thcrap.dll not found. This modkit may not be thcrap-based,
  echo     or the dir is wrong. A thcrap plugin can only live in bin\.
  goto :end
)
for %%D in ("%THCRAP_DLL%") do set "BIN=%%~dpD"
echo     found: %THCRAP_DLL%
echo     bin  = %BIN%

echo.
echo [2/3] choosing install target ...
set "DST=%MK%\mods"
if not exist "%DST%\" (
  echo     no "%MK%\mods" - falling back to bin ^(no checkbox entry^)
  set "DST=%BIN:~0,-1%"
) else (
  echo     using "%DST%" ^(the launcher's checkbox list reads mods\*.dll^)
)
set "TARGET=%DST%\th19_probe2.dll"
copy /y "%SRC%" "%TARGET%" >nul
if errorlevel 1 ( echo [x] copy failed ^(permissions?^) & goto :end )
if not exist "%TARGET%" ( echo [x] file missing after copy & goto :end )
echo     OK  %TARGET%

echo.
echo [3/3] locating th19.exe and writing ini ...
set "GAME="
for /f "delims=" %%F in ('dir /s /b "%MK%\th19.exe" 2^>nul') do if not defined GAME set "GAME=%%F"
if defined GAME (
  for %%D in ("%GAME%") do set "GDIR=%%~dpD"
  echo     th19.exe = %GAME%
  if not exist "%GDIR%th19_probe2.ini" (
    > "%GDIR%th19_probe2.ini" echo anchor_rva=0x0
    echo     wrote default ini: %GDIR%th19_probe2.ini  ^(heartbeat only^)
  ) else (
    echo     ini already exists, left alone: %GDIR%th19_probe2.ini
  )
  echo     log will be: %GDIR%th19_probe2.log
) else (
  echo     [!] th19.exe not found - put th19_probe2.ini in the game dir yourself.
)

echo.
echo NEXT:
echo   1^) start TH19 with your launcher
echo   2^) in the checkbox list TICK  th19_probe2   ^(if you see it^)
echo   3^) play Sanae C3 once, then open ^<game dir^>\th19_probe2.log
echo Expected lines:
echo     === th19_probe2 start ... ===
echo     [guard] 609 handler signature matched, th19.v1.10c confirmed.
if /i not "%DST%"=="%BIN:~0,-1%" (
  echo.
  echo [note] If the launcher does NOT show a th19_probe2 checkbox,
  echo        copy "%TARGET%"  into  %BIN%
  echo        i.e. straight into bin\ - that always works, just no checkbox.
)
goto :end

:nopath
echo [x] modkit dir does not exist: %MK%
echo     e.g. install_probe.bat "D:\...\th19_modkit"
goto :end

:end
echo.
pause
endlocal
