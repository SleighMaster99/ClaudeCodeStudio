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
!define COMPANY    "SleighMaster"

; 설치 폴더 · 시작 메뉴 · 레지스트리를 모두 {회사}\{프로그램} 으로 묶는다.
; 회사 폴더는 프로그램이 늘어나도 자리가 흩어지지 않게 하는 칸막이다.
!define REGKEY     "Software\${COMPANY}\${APPKEY}"
!define OLDREGKEY  "Software\${APPKEY}"            ; 회사 폴더가 없던 시절의 자리

Name "${APPDISPLAY} ${VERSION}"
OutFile "${OUT_FILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\${COMPANY}\${APPKEY}"
InstallDirRegKey HKCU "${REGKEY}" "InstallDir"
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
!include "FileFunc.nsh"
!insertmacro un.GetParent      ; 언인스톨에서 회사 폴더(설치 폴더의 부모)를 구할 때 쓴다
!define MUI_ABORTWARNING
; Setup / 언인스톨러 아이콘 — 저장소 로고 자산을 컴파일 타임에 임베드 (경로는 이 스크립트 기준)
!define MUI_ICON   "${__FILEDIR__}\..\assets\logo\logo.ico"
!define MUI_UNICON "${__FILEDIR__}\..\assets\logo\logo.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPKEY}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "지금 실행"
; 바탕화면 바로가기는 설치가 끝난 뒤 마지막 화면에서 고르게 한다.
; SHOWREADME 는 원래 파일을 여는 용도지만, 경로를 비우면 체크박스와 콜백만 남는다.
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "바탕화면에 바로가기 만들기"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortcut

; ----- 설치 옵션 페이지 (nsDialogs) -----
; 서버 저장소 주소를 미리 받아 두면 앱 첫 실행 때 초기 설정 화면에 채워진다.
; 비워 두고 넘어가도 된다 — 그때는 앱에서 직접 입력하거나 GitHub 계정에 새로 만든다.
Var Dialog
Var RepoUrlText
Var RepoUrl        ; 입력값 (빈 값 = 건너뜀)

Function OptionsPageCreate
  !insertmacro MUI_HEADER_TEXT "설치 옵션" "설정을 보관할 서버 저장소를 지정합니다."
  nsDialogs::Create 1018
  Pop $Dialog
  ${If} $Dialog == error
    Abort
  ${EndIf}

  ${NSD_CreateLabel} 0 0 100% 26u "Claude Code 설정을 보관할 서버 저장소 주소입니다. 이미 만들어 둔 저장소여야 합니다.$\r$\n지금 비워 두면 앱을 처음 실행할 때 입력하거나 GitHub 계정에 새로 만들 수 있습니다."
  Pop $0

  ${NSD_CreateLabel} 0 32u 100% 11u "저장소 URL (선택)"
  Pop $0

  ${NSD_CreateText} 0 44u 100% 13u "$RepoUrl"
  Pop $RepoUrlText
  ${NSD_SetTextLimit} $RepoUrlText 500

  ${NSD_CreateLabel} 0 60u 100% 11u "예: https://github.com/OWNER/REPO.git"
  Pop $0

  nsDialogs::Show
FunctionEnd

Function OptionsPageLeave
  ${NSD_GetText} $RepoUrlText $RepoUrl
FunctionEnd

; 재설치라면 이전에 넣은 주소를 다시 보여준다.
; 회사 폴더가 없던 설치본이 남아 있으면 여기서 함께 정리한다 — 두면 시작 메뉴와
; 프로그램 제거 목록에 같은 앱이 둘로 보인다.
Function .onInit
  ReadRegStr $RepoUrl HKCU "${REGKEY}" "RepoUrl"
  ${If} $RepoUrl == ""
    ReadRegStr $RepoUrl HKCU "${OLDREGKEY}" "RepoUrl"   ; 옛 설치본이 남긴 값을 이어받는다
  ${EndIf}

  ReadRegStr $0 HKCU "${OLDREGKEY}" "InstallDir"
  ${If} $0 != ""
  ${AndIf} ${FileExists} "$0\uninstall.exe"
    ; /S 는 조용히, _?= 는 언인스톨러를 임시 폴더로 복사하지 않게 한다
    ; (복사하면 ExecWait 가 곧바로 돌아와 뒤이은 삭제가 어긋난다).
    ExecWait '"$0\uninstall.exe" /S _?=$0'
    Delete "$0\uninstall.exe"
    RMDir "$0"
  ${EndIf}
FunctionEnd

; 다른 위치를 골라도 {회사}\{프로그램} 구조를 지킨다.
; NSIS 는 browse 로 고른 폴더에 마지막 조각(ClaudeCodeStudio)만 되붙이므로
; 그대로 두면 회사 폴더가 빠진 자리에 설치된다.
Function DirectoryLeave
  Push $0
  Push $1
  StrLen $0 "\${COMPANY}\${APPKEY}"
  StrCpy $1 "$INSTDIR" "" -$0
  ${If} $1 != "\${COMPANY}\${APPKEY}"          ; 이미 맞는 모양이면(기본값·재설치) 손대지 않는다
    StrLen $0 "\${APPKEY}"
    StrCpy $1 "$INSTDIR" "" -$0
    ${If} $1 == "\${APPKEY}"
      StrCpy $INSTDIR "$INSTDIR" -$0           ; NSIS 가 붙인 \ClaudeCodeStudio 를 뗀다
    ${EndIf}
    ; 회사 폴더를 이미 가리키고 있으면 프로그램 이름만 붙인다 — 회사명이 두 번 들어가지 않게.
    StrLen $0 "\${COMPANY}"
    StrCpy $1 "$INSTDIR" "" -$0
    ${If} $1 == "\${COMPANY}"
      StrCpy $INSTDIR "$INSTDIR\${APPKEY}"
    ${Else}
      StrCpy $INSTDIR "$INSTDIR\${COMPANY}\${APPKEY}"
    ${EndIf}
  ${EndIf}
  Pop $1
  Pop $0
FunctionEnd

; 마지막 화면의 '바탕화면에 바로가기 만들기' 체크박스가 부른다.
Function CreateDesktopShortcut
  CreateShortcut "$DESKTOP\${APPDISPLAY}.lnk" "$INSTDIR\${APPKEY}.exe"
FunctionEnd

!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
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
  ; 시작 메뉴도 회사 폴더로 묶는다 — 프로그램이 늘어도 한자리에 모인다.
  CreateDirectory "$SMPROGRAMS\${COMPANY}"
  CreateShortcut "$SMPROGRAMS\${COMPANY}\${APPDISPLAY}.lnk" "$INSTDIR\${APPKEY}.exe"
  ; 바탕화면 바로가기는 마지막 화면의 체크박스가 만든다 (CreateDesktopShortcut).

  ; ----- 등록 + uninstaller -----
  WriteRegStr HKCU "${REGKEY}" "InstallDir" "$INSTDIR"
  ; 앱의 초기 설정 화면이 이 값을 저장소 URL 로 미리 채운다 (비었으면 기록하지 않는다).
  ${If} $RepoUrl != ""
    WriteRegStr HKCU "${REGKEY}" "RepoUrl" "$RepoUrl"
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

  ; 회사 폴더·시작 메뉴 폴더는 /r 없이 지운다 — 비어 있을 때만 사라진다.
  ; 같은 회사의 다른 프로그램이 들어 있으면 그대로 둔다.
  ${un.GetParent} "$INSTDIR" $0
  RMDir "$0"

  Delete "$SMPROGRAMS\${COMPANY}\${APPDISPLAY}.lnk"
  RMDir  "$SMPROGRAMS\${COMPANY}"
  Delete "$DESKTOP\${APPDISPLAY}.lnk"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPKEY}"
  DeleteRegKey HKCU "${REGKEY}"
  ; 회사 키도 같은 규칙 — 하위 키나 값이 남아 있으면 지우지 않는다.
  DeleteRegKey /ifempty HKCU "Software\${COMPANY}"
SectionEnd
