@echo off
setlocal EnableDelayedExpansion
rem ClaudeCodeStudio - Release build + NSIS installer.
rem
rem   Build.bat 1.0.1                 build, stage, make the installer
rem   Build.bat 1.0.1 --skip-build    reuse whatever is already in bin\Release
rem   Build.bat                       prompt for the version
rem
rem The new version must be HIGHER than installer\VERSION (the last published one).
rem ReleaseTool.exe drives this script and streams the output below into its log pane,
rem so keep the "[n/5]" step markers at the start of the line.
rem
rem ASCII only: Korean text in a .bat desyncs the cmd batch parser.

set "ROOT=%~dp0"
set "OUT=%ROOT%bin\Release"
set "VERFILE=%ROOT%installer\VERSION"
set "REDIST=%ROOT%installer\redist"
set "NSI=%ROOT%installer\ClaudeCodeStudio.nsi"
set "WV2=%USERPROFILE%\.nuget\packages\microsoft.web.webview2\1.0.3967.48\build\native"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "NSISFALLBACK=%ProgramFiles(x86)%\NSIS\makensis.exe"

set "SKIPBUILD="
if /i "%~2"=="--skip-build" set "SKIPBUILD=1"

rem ===========================================================================
echo [1/5] Version check
rem ===========================================================================

rem --- last published version -------------------------------------------------
set "LATEST="
if exist "%VERFILE%" (
  for /f "usebackq tokens=1 delims= " %%v in ("%VERFILE%") do if not defined LATEST set "LATEST=%%v"
)
if not defined LATEST set "LATEST=0.0.0"

for /f "tokens=1-3 delims=." %%a in ("!LATEST!") do (
  set "LMAJ=%%a"
  set "LMIN=%%b"
  set "LPAT=%%c"
)
if not defined LPAT goto :badlatest

rem --- new version -------------------------------------------------------------
set "VER=%~1"
if not defined VER (
  echo       Last published: !LATEST!
  set /p "VER=      New version (e.g. 1.0.1): "
)
if not defined VER goto :badformat

rem Strip every legal character; anything left over is illegal. This runs before the
rem value reaches a pipe: "echo <ver>| findstr" spawns a child cmd that would re-parse
rem a "&" in the value as a command separator, so "1.0.0&whoami" would run whoami.
set "SCAN=!VER!"
for %%c in (0 1 2 3 4 5 6 7 8 9 .) do set "SCAN=!SCAN:%%c=!"
if defined SCAN goto :badformat

echo !VER!| findstr /r /x "[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*" >nul
if errorlevel 1 goto :badformat

for /f "tokens=1-3 delims=." %%a in ("!VER!") do (
  set "MAJ=%%a"
  set "MIN=%%b"
  set "PAT=%%c"
)

rem Reject leading zeros: v01.02.03 would become a non-canonical installer name.
if not "!MAJ!"=="0" if "!MAJ:~0,1!"=="0" goto :badzero
if not "!MIN!"=="0" if "!MIN:~0,1!"=="0" goto :badzero
if not "!PAT!"=="0" if "!PAT:~0,1!"=="0" goto :badzero

rem --- must be newer than the last published version ----------------------------
if !MAJ! GTR !LMAJ! goto :vercmp_ok
if !MAJ! LSS !LMAJ! goto :notnewer
if !MIN! GTR !LMIN! goto :vercmp_ok
if !MIN! LSS !LMIN! goto :notnewer
if !PAT! GTR !LPAT! goto :vercmp_ok
goto :notnewer

:vercmp_ok
echo       !LATEST! -^> !VER!

rem ===========================================================================
echo [2/5] MSBuild  Release^|x64
rem ===========================================================================
if defined SKIPBUILD goto :build_skipped

if not exist "%WV2%\include\WebView2.h" goto :nowv2
if not exist "%VSWHERE%" goto :novswhere

set "MSBUILD="
for /f "usebackq delims=" %%p in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%p"
if not defined MSBUILD goto :nomsbuild

rem -t:ClaudeCodeStudio builds the app and (through its ProjectReferences) Core + both
rem module DLLs, but NOT ReleaseTool - which is the process running this script and
rem cannot overwrite its own .exe while it is loaded.
"!MSBUILD!" "%ROOT%ClaudeCodeStudio.sln" -t:ClaudeCodeStudio -p:Configuration=Release -p:Platform=x64 -m -v:minimal -nologo
if errorlevel 1 goto :buildfail
goto :build_done

:build_skipped
echo       skipped - reusing "%OUT%"

:build_done
for %%f in (ClaudeCodeStudio.exe Core.dll SyncClaudeCodeSetting.dll ClaudeCodeStatusBar.dll) do (
  if not exist "%OUT%\%%f" set "MISSING=%%f"
)
if defined MISSING goto :missingoutput

rem ===========================================================================
echo [3/5] Staging  Shipping\!VER!
rem ===========================================================================
set "STAGE=%ROOT%Shipping\!VER!"
if exist "!STAGE!" rmdir "!STAGE!" /s /q
mkdir "!STAGE!"
if errorlevel 1 goto :stagefail

set "CPFAIL="
for %%f in (ClaudeCodeStudio.exe Core.dll SyncClaudeCodeSetting.dll ClaudeCodeStatusBar.dll) do (
  copy /y "%OUT%\%%f" "!STAGE!\" >nul
  if errorlevel 1 set "CPFAIL=%%f"
)
for %%f in (StatusLine.ps1 config.json usage_config.json) do (
  copy /y "%ROOT%%%f" "!STAGE!\" >nul
  if errorlevel 1 set "CPFAIL=%%f"
)
if defined CPFAIL goto :copyfail

robocopy "%OUT%\web" "!STAGE!\web" /E /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 goto :webfail

for %%f in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll) do (
  if not exist "%REDIST%\%%f" set "CPFAIL=%%f"
)
if defined CPFAIL goto :noredist

rem ===========================================================================
echo [4/5] NSIS  makensis /DVERSION=!VER!
rem ===========================================================================
set "MAKENSIS="
for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\NSIS" /ve 2^>nul ^| findstr /r "REG_SZ"') do set "MAKENSIS=%%b\makensis.exe"
if not exist "!MAKENSIS!" set "MAKENSIS=%NSISFALLBACK%"
if not exist "!MAKENSIS!" goto :nonsis

set "SETUP=%ROOT%Shipping\ClaudeCodeStudio-Setup-!VER!.exe"
if exist "!SETUP!" del /q "!SETUP!"

"!MAKENSIS!" /V2 "/DVERSION=!VER!" "/DSTAGE_DIR=!STAGE!" "/DREDIST_DIR=%REDIST%" "/DOUT_FILE=!SETUP!" "%NSI%"
if errorlevel 1 goto :nsisfail
if not exist "!SETUP!" goto :nsismissing

rem ===========================================================================
echo [5/5] Update VERSION  !LATEST! -^> !VER!
rem ===========================================================================
> "%VERFILE%" echo !VER!
if errorlevel 1 goto :verwritefail

echo.
echo [OK] Installer  "!SETUP!"
echo [OK] Staging    "!STAGE!"
exit /b 0

rem ===========================================================================
rem  failures
rem ===========================================================================
:badformat
echo [ERROR] Invalid version "!VER!". Expected MAJOR.MINOR.PATCH, e.g. 1.0.1
exit /b 1

:badzero
echo [ERROR] Invalid version "!VER!". Leading zeros are not allowed - use 1.2.0, not 1.02.0
exit /b 1

:notnewer
set /a NEXTPAT=!LPAT!+1
echo [ERROR] Version "!VER!" is not newer than the last published version "!LATEST!".
echo         Use !LMAJ!.!LMIN!.!NEXTPAT! or higher.
exit /b 1

:badlatest
echo [ERROR] "%VERFILE%" does not hold a MAJOR.MINOR.PATCH value: "!LATEST!"
exit /b 1

:nowv2
echo [ERROR] WebView2 SDK not found:
echo         "%WV2%"
echo         The vcxproj files reference this exact path. Restore it with:
echo         nuget install Microsoft.Web.WebView2 -Version 1.0.3967.48
exit /b 1

:novswhere
echo [ERROR] vswhere.exe not found: "%VSWHERE%"
exit /b 1

:nomsbuild
echo [ERROR] MSBuild.exe not found. Check the Visual Studio 2022 installation.
exit /b 1

:buildfail
echo [ERROR] Build failed.
exit /b 1

:missingoutput
echo [ERROR] Build output is missing: "%OUT%\!MISSING!"
if defined SKIPBUILD echo         Run without --skip-build to produce it.
exit /b 1

:stagefail
echo [ERROR] Could not create the staging folder "!STAGE!"
exit /b 1

:copyfail
echo [ERROR] Failed to stage "!CPFAIL!"
exit /b 1

:webfail
echo [ERROR] Failed to copy "%OUT%\web" into the staging folder.
exit /b 1

:noredist
echo [ERROR] VC++ CRT redistributable missing: "%REDIST%\!CPFAIL!"
echo         Copy vcruntime140.dll, vcruntime140_1.dll and msvcp140.dll from
echo         the Visual Studio VC\Redist\MSVC\^<ver^>\x64\Microsoft.VC143.CRT folder.
exit /b 1

:nonsis
echo [ERROR] makensis.exe not found. Install NSIS 3.x from https://nsis.sourceforge.io/
exit /b 1

:nsisfail
echo [ERROR] makensis failed.
exit /b 1

:nsismissing
echo [ERROR] makensis reported success but "!SETUP!" was not created.
exit /b 1

:verwritefail
echo [ERROR] Installer was built but "%VERFILE%" could not be updated.
exit /b 1
