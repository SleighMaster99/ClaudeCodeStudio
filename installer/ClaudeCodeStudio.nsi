; ClaudeCodeStudio installer (NSIS)
; Per-user install (no admin): exe + 3 module DLLs + web assets + StatusLine.ps1 런타임.
; 앱 파일은 ..\bin\Release\ 에서, 런타임 파일은 repo 루트에서 가져온다.
; 전제: 사용자 PC 에 Microsoft Edge WebView2 런타임 설치됨.

Unicode true

!define APPKEY     "ClaudeCodeStudio"
!define APPDISPLAY "ClaudeCodeStudio"
!define COMPANY    "SleighMaster99"
!define VERSION    "0.1.0"

Name "${APPDISPLAY}"
OutFile "ClaudeCodeStudio Setup.exe"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\${APPKEY}"
InstallDirRegKey HKCU "Software\${APPKEY}" "InstallDir"
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPKEY}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "지금 실행"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "English"

Section "Install"
  ; ----- 앱 (exe + 모듈 DLL) -----
  SetOutPath "$INSTDIR"
  File "..\bin\Release\ClaudeCodeStudio.exe"
  File "..\bin\Release\Core.dll"
  File "..\bin\Release\SyncClaudeCodeSetting.dll"
  File "..\bin\Release\ClaudeCodeStatusBar.dll"

  ; ----- 런타임 (StatusLine.ps1 + 기본 config) -----
  File "..\StatusLine.ps1"
  File "..\config.json"
  File "..\usage_config.json"

  ; ----- web (Core 셸 + 모듈별 web/sync, web/statusbar) -----
  SetOutPath "$INSTDIR\web"
  File /r "..\bin\Release\web\*.*"

  ; ----- 바로가기 -----
  CreateDirectory "$SMPROGRAMS\${APPDISPLAY}"
  CreateShortcut "$SMPROGRAMS\${APPDISPLAY}\${APPDISPLAY}.lnk" "$INSTDIR\${APPKEY}.exe"
  CreateShortcut "$DESKTOP\${APPDISPLAY}.lnk" "$INSTDIR\${APPKEY}.exe"

  ; ----- 등록 + uninstaller -----
  WriteRegStr HKCU "Software\${APPKEY}" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  !define UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPKEY}"
  WriteRegStr   HKCU "${UNINST}" "DisplayName"     "${APPDISPLAY}"
  WriteRegStr   HKCU "${UNINST}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKCU "${UNINST}" "Publisher"       "${COMPANY}"
  WriteRegStr   HKCU "${UNINST}" "DisplayIcon"     "$INSTDIR\${APPKEY}.exe"
  WriteRegStr   HKCU "${UNINST}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegDWORD HKCU "${UNINST}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\${APPKEY}.exe"
  Delete "$INSTDIR\Core.dll"
  Delete "$INSTDIR\SyncClaudeCodeSetting.dll"
  Delete "$INSTDIR\ClaudeCodeStatusBar.dll"
  Delete "$INSTDIR\StatusLine.ps1"
  Delete "$INSTDIR\config.json"
  Delete "$INSTDIR\usage_config.json"
  RMDir /r "$INSTDIR\web"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${APPDISPLAY}\${APPDISPLAY}.lnk"
  RMDir  "$SMPROGRAMS\${APPDISPLAY}"
  Delete "$DESKTOP\${APPDISPLAY}.lnk"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPKEY}"
  DeleteRegKey HKCU "Software\${APPKEY}"
SectionEnd
