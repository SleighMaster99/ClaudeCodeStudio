; ClaudeCodeStudio installer (NSIS)
; Per-user install (no admin): exe + 3 module DLLs + web assets + StatusLine.ps1 런타임 + VC++ CRT.
; 설치 파일은 Build.bat 이 만든 스테이징 폴더에서, CRT 는 installer\redist\ 에서 가져온다.
; 전제: 사용자 PC 에 Microsoft Edge WebView2 런타임 설치됨.
;
; Build.bat 이 /DVERSION /DSTAGE_DIR /DREDIST_DIR /DOUT_FILE 을 넘긴다.
; 인자 없이 makensis 를 직접 부르면 아래 !error 로 중단된다 (버전이 박힌 설치본이 실수로 나오지 않게).

Unicode true

!ifndef VERSION
  !error "VERSION not defined. Build with: Build.bat <version>"
!endif
!ifndef STAGE_DIR
  !error "STAGE_DIR not defined. Build with: Build.bat <version>"
!endif
!ifndef REDIST_DIR
  !error "REDIST_DIR not defined. Build with: Build.bat <version>"
!endif
!ifndef OUT_FILE
  !error "OUT_FILE not defined. Build with: Build.bat <version>"
!endif

!define APPKEY     "ClaudeCodeStudio"
!define APPDISPLAY "ClaudeCodeStudio"
!define COMPANY    "SleighMaster99"

Name "${APPDISPLAY} ${VERSION}"
OutFile "${OUT_FILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\${APPKEY}"
InstallDirRegKey HKCU "Software\${APPKEY}" "InstallDir"
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName"     "${APPDISPLAY}"
VIAddVersionKey "ProductVersion"  "${VERSION}"
VIAddVersionKey "FileVersion"     "${VERSION}.0"
VIAddVersionKey "CompanyName"     "${COMPANY}"
VIAddVersionKey "LegalCopyright"  "Copyright (C) ${COMPANY}"
VIAddVersionKey "FileDescription" "${APPDISPLAY} Setup"

!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "WinMessages.nsh"
!define MUI_ABORTWARNING
; Setup / 언인스톨러 아이콘 — 저장소 로고 자산을 컴파일 타임에 임베드 (경로는 이 스크립트 기준)
!define MUI_ICON   "${__FILEDIR__}\..\assets\logo\logo.ico"
!define MUI_UNICON "${__FILEDIR__}\..\assets\logo\logo.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPKEY}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "지금 실행"

; ----- 설치 옵션 페이지 (nsDialogs) -----
; 서버 저장소 주소를 미리 받아 두면 앱 첫 실행 때 초기 설정 화면에 채워진다.
; 비워 두고 넘어가도 된다 — 그때는 앱에서 직접 입력하거나 GitHub 계정에 새로 만든다.
Var Dialog
Var RepoUrlText
Var DesktopCheck
Var RepoUrl        ; 입력값 (빈 값 = 건너뜀)
Var MakeDesktop    ; 바탕화면 바로가기 생성 여부

Function OptionsPageCreate
  !insertmacro MUI_HEADER_TEXT "설치 옵션" "서버 저장소와 바로가기를 설정합니다."
  nsDialogs::Create 1018
  Pop $Dialog
  ${If} $Dialog == error
    Abort
  ${EndIf}

  ${NSD_CreateLabel} 0 0 100% 26u "Claude Code 설정을 보관할 서버 저장소 주소입니다.$\r$\n지금 비워 두면 앱을 처음 실행할 때 입력하거나 GitHub 계정에 새로 만들 수 있습니다."
  Pop $0

  ${NSD_CreateLabel} 0 32u 100% 11u "저장소 URL (선택)"
  Pop $0

  ${NSD_CreateText} 0 44u 100% 13u "$RepoUrl"
  Pop $RepoUrlText
  ${NSD_SetTextLimit} $RepoUrlText 500

  ${NSD_CreateLabel} 0 60u 100% 11u "예: https://github.com/OWNER/REPO.git"
  Pop $0

  ${NSD_CreateCheckbox} 0 84u 100% 12u "바탕화면에 바로가기 만들기"
  Pop $DesktopCheck
  ${If} $MakeDesktop == ${BST_UNCHECKED}
    ${NSD_Uncheck} $DesktopCheck
  ${Else}
    ${NSD_Check} $DesktopCheck
  ${EndIf}

  nsDialogs::Show
FunctionEnd

Function OptionsPageLeave
  ${NSD_GetText} $RepoUrlText $RepoUrl
  ${NSD_GetState} $DesktopCheck $MakeDesktop
FunctionEnd

; 재설치라면 이전에 넣은 주소를 다시 보여준다. 바로가기는 기본 체크.
Function .onInit
  ReadRegStr $RepoUrl HKCU "Software\${APPKEY}" "RepoUrl"
  StrCpy $MakeDesktop ${BST_CHECKED}
FunctionEnd

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom OptionsPageCreate OptionsPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "English"

Section "Install"
  ; ----- 앱 (exe + 모듈 DLL) -----
  SetOutPath "$INSTDIR"
  File "${STAGE_DIR}\ClaudeCodeStudio.exe"
  File "${STAGE_DIR}\Core.dll"
  File "${STAGE_DIR}\SyncClaudeCodeSetting.dll"
  File "${STAGE_DIR}\ClaudeCodeStatusBar.dll"

  ; ----- VC++ CRT -----
  ; Release 가 /MD 링크라 이 셋이 없으면 대상 PC 에서 실행 자체가 되지 않는다.
  ; per-user 설치(관리자 권한 없음)라 재배포 패키지를 깔 수 없어 앱 폴더에 동봉한다.
  File "${REDIST_DIR}\vcruntime140.dll"
  File "${REDIST_DIR}\vcruntime140_1.dll"
  File "${REDIST_DIR}\msvcp140.dll"

  ; ----- 런타임 (StatusLine.ps1 + 기본 config) -----
  File "${STAGE_DIR}\StatusLine.ps1"
  File "${STAGE_DIR}\config.json"
  File "${STAGE_DIR}\usage_config.json"

  ; ----- web (Core 셸 + 모듈별 web/sync, web/statusbar) -----
  SetOutPath "$INSTDIR\web"
  File /r "${STAGE_DIR}\web\*.*"

  ; ----- 바로가기 -----
  CreateDirectory "$SMPROGRAMS\${APPDISPLAY}"
  CreateShortcut "$SMPROGRAMS\${APPDISPLAY}\${APPDISPLAY}.lnk" "$INSTDIR\${APPKEY}.exe"
  ${If} $MakeDesktop == ${BST_CHECKED}
    CreateShortcut "$DESKTOP\${APPDISPLAY}.lnk" "$INSTDIR\${APPKEY}.exe"
  ${EndIf}

  ; ----- 등록 + uninstaller -----
  WriteRegStr HKCU "Software\${APPKEY}" "InstallDir" "$INSTDIR"
  ; 앱의 초기 설정 화면이 이 값을 저장소 URL 로 미리 채운다 (비었으면 기록하지 않는다).
  ${If} $RepoUrl != ""
    WriteRegStr HKCU "Software\${APPKEY}" "RepoUrl" "$RepoUrl"
  ${EndIf}
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
  Delete "$INSTDIR\vcruntime140.dll"
  Delete "$INSTDIR\vcruntime140_1.dll"
  Delete "$INSTDIR\msvcp140.dll"
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
