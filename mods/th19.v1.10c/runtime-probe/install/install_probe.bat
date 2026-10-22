@echo off
setlocal EnableExtensions
rem ===========================================================================
rem  th19_probe2 installer - put the read-only probe into <thcrap>\bin
rem
rem  Usage:
rem    1) double-click (it will ask for the modkit dir), or
rem    2) install_probe.bat "D:\path\to\th19_modkit"
rem
rem  It only: finds thcrap.dll / copies th19_probe2.dll next to it /
rem           writes th19_probe2.ini beside th19.exe
rem  It deletes nothing and does not touch the game exe.
rem
rem  NOTE: ASCII-only on purpose - cmd.exe renders .bat in the OEM codepage.
rem ===========================================================================

set "MK=%~1"
if "%MK%"=="" set /p MK=modkit dir (e.g. D:\...\th19_modkit):
if "%MK%"=="" goto :nopath
if not exist "%MK%\" goto :nopath
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
  echo [x] thcrap.dll not found. This modkit may not load via thcrap,
  echo     or the dir is wrong. This probe is a thcrap plugin: bin only.
  goto :end
)
for %%D in ("%THCRAP_DLL%") do set "BIN=%%~dpD"
echo     found: %THCRAP_DLL%
echo     bin  = %BIN%

echo.
echo [2/3] copying probe into bin ...
copy /y "%SRC%" "%BIN%th19_probe2.dll" >nul
if errorlevel 1 ( echo [x] copy failed (permissions?) & goto :end )
if exist "%BIN%th19_probe2.dll" (echo     OK  %BIN%th19_probe2.dll) else (echo [x] file missing after copy & goto :end)

echo.
echo [3/3] locating th19.exe and writing ini ...
set "GAME="
for /f "delims=" %%F in ('dir /s /b "%MK%\th19.exe" 2^>nul') do if not defined GAME set "GAME=%%F"
if defined GAME (
  for %%D in ("%GAME%") do set "GDIR=%%~dpD"
  echo     th19.exe = %GAME%
  if not exist "%GDIR%th19_probe2.ini" (
    > "%GDIR%th19_probe2.ini" echo anchor_rva=0x0
    echo     wrote default ini: %GDIR%th19_probe2.ini  (anchor_rva=0x0 = heartbeat only)
  ) else (
    echo     ini already exists, left alone: %GDIR%th19_probe2.ini
  )
  echo     log will be: %GDIR%th19_probe2.log
) else (
  echo     [!] th19.exe not found - put th19_probe2.ini in the game dir yourself.
)

echo.
echo Done. Start TH19 with your launcher, play Sanae C3 once, then open:
echo     ^<game dir^>\th19_probe2.log
echo Expected lines:
echo     === th19_probe2 start ... ===
echo     [guard] 609 handler signature matched, th19.v1.10c confirmed.
goto :end

:nopath
echo [x] modkit dir does not exist: %MK%
echo     e.g. install_probe.bat "D:\...\th19_modkit"
goto :end

:end
echo.
pause
endlocal
