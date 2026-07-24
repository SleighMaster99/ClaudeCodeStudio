# ClaudeCodeStudio

Claude Code 설정 관리 Windows 통합 앱 (C++ + WebView2). 좌측 탭 2개:
- **설정 동기화** — 글로벌 `~/.claude` 를 git 으로 서버 동기화 (`SyncClaudeCodeSetting` 모듈)
- **상태바 설정** — statusLine 레이아웃(`config.json`) 편집 (`ClaudeCodeStatusBar` 모듈)

statusLine 렌더는 PowerShell 런타임 `StatusLine.ps1` 이 담당(유지). 앱은 번들 + 설정만 관리.

> 통합 계획·결정 로그·단계(P0~P5) SSOT: `docs/ClaudeCodeStudio-Integration-Plan.md`.

## 프로젝트 구조

| 경로 | 종류 | 역할 |
| --- | --- | --- |
| `ClaudeCodeStudio/` | exe | 최소 부트스트래퍼 (`WinMain → Core_Run`) |
| `Core/` | dll | 창 + WebView2 호스트 + 탭 셸 + 모듈 로더(`modules.cpp`)/라우터(`router.cpp`)/호스트(`host.cpp`) |
| `SyncClaudeCodeSetting/` | dll | 동기화 모듈 (`sync.cpp`: git status/log/pull/push + 초기 설정) |
| `ClaudeCodeStatusBar/` | dll | 상태바 모듈 (`statusbar.cpp`: config.json I/O + settings.json apply) |
| `shared/module_api.h` | 헤더 | 모듈 C 계약 (`Module_Info/Init/Handle`, `PostToUiFn`) |
| `StatusLine.ps1`, `config.json`, `usage_config.json` | 런타임 | statusLine 렌더러 + 레이아웃 + 한도 |

각 모듈 dll 은 자기 `web/` UI(HTML/JS)를 소유하고, 빌드 시 `bin/Release/web/<모듈>/` 로 복사된다.

## 빌드

- 요구: VS 2022 **v143**, WebView2 SDK(NuGet `microsoft.web.webview2`, 정적 `WebView2LoaderStatic.lib`), WebView2 런타임
- `MSBuild ClaudeCodeStudio.sln /p:Configuration=Release /p:Platform=x64`
- MSVC 한글 리터럴 → **`/utf-8` 필수**. 산출물: `bin/Release/`
- 실행 검증: 격리 데스크톱(`CreateDesktop` + `STARTUPINFO.lpDesktop`) + `PrintWindow`(flag 2) 캡처 — 활성 세션/포그라운드 무간섭

## 모듈 아키텍처

- **로딩**: `Core/src/modules.cpp` 의 `kModuleDlls[]` 에 dll 명을 하드코딩 → 시작 시 `LoadLibrary` + `GetProcAddress` 로 `Module_*` 바인딩 (D11 "암시적" = 목록 하드코딩).
- **메시지 흐름**:
  - iframe(모듈 web) → `window.parent.postMessage({module,cmd,arg})` → Core 셸이 `chrome.webview.postMessage(JSON)` → C++
  - C++ `WebMessageReceived` → `Router_Handle` → `module` 로 모듈 조회 → `Module_Handle(json)` (cmd/arg 해석은 모듈 몫)
  - 모듈이 `post` 콜백 호출 → Core 가 `{module,payload}` **봉투**로 씌워 회신 → Core 셸이 `env.module` iframe 에 payload 중계
- 새 모듈 추가: dll 프로젝트(`MODULE_EXPORTS` + `$(SolutionDir)shared` include) + `Module_*` 구현 + `kModuleDlls` 등록 + Core 셸 `index.html` 에 탭/iframe(`frames` 맵).

## StatusLine.ps1 런타임

1. Claude Code 가 매 stdin 마다 `StatusLine.ps1` 실행, 세션 JSON 전달.
2. stdin 파싱 → `$ctx` → `config.json` 의 `rows` 순회 렌더링. 줄 단위 `Write-Output` → 각 라인이 statusLine 한 줄.

**stdin JSON 주요 필드** (`.last_input.json` 참고): `model.id`, `model.display_name`, `version`, `session_id`, `workspace.current_dir/project_dir`, `context_window.*`, `cost.total_cost_usd/total_duration_ms`, `effort.level`, `rate_limits.five_hour/seven_day.used_percentage`, `transcript_path`. 접근은 `Get-Field`(문자열, 기본값)/`Get-FieldRaw`(raw, null 가능).

## 항목 추가/수정 절차

새 항목 타입은 **두 곳** 동시 수정:
1. **`StatusLine.ps1`** — `# ----- Render -----` 의 `switch ($it.type)` 에 case 추가 (런타임 렌더링).
2. **`ClaudeCodeStatusBar/web/app.js`** — `CATALOG` 배열에 `{key,name,ex,cat}` 추가 (편집기 팔레트).

`cat`(카테고리): `Claude`, `워크스페이스`, `Git`, `시간`, `시스템`, `사용률`, `아이콘`, `구분자/포맷`.
값 null 시 `?` 같은 placeholder 대신 `0`/빈 문자열 (기존 패턴, 예: `ctx_pct`, `ctx_bar`).

## 아이콘 시스템

`icon_*` 는 같은 카테고리 항목을 한 줄에 묶을 때 쓰는 독립 항목 (예: `ctx_bar`+`ctx_pct` 줄에 `icon_ctx` 하나).
- `StatusLine.ps1` 의 `$script:Icons` 해시테이블에 `<key> → 글리프` 매핑.
- `switch` 의 `{ $_ -like 'icon_*' }` 케이스가 키 잘라 조회 후 출력.
- 새 아이콘: `$script:Icons` 추가 + `app.js` `CATALOG` 에 `icon_<key>`(cat="아이콘").
- 기본 글리프는 이모지 (폰트 의존 없음). Nerd Font 로 바꾸려면 `$script:Icons` 만 교체.

### VS16 (Variation Selector-16, U+FE0F)

- **emoji_presentation = Yes**: 그냥 2칸 컬러 이모지 (🤖 🧠 📁).
- **= No**: 1칸 흑백 글리프 → 뒤에 `U+FE0F` 붙여 2칸 강제 (`'⏱'` → `'⏱️'`).
- 새 아이콘 추가 시 Unicode 공식 `Emoji_Presentation` 확인, `No` 면 VS16 부착. 현재 적용: `version`, `duration`, `host`, `h5`, `week`.

## statusLine 적용

**상태바 설정** 탭 **"저장 & 적용"**:
1. `config.json` 저장 (`statusbar.cpp` `CmdApply`).
2. `~/.claude/settings.json` 의 `statusLine` 을 `powershell -NoProfile -ExecutionPolicy Bypass -File <StatusLine.ps1>` 로 갱신 — **다른 필드 보존**(미니 JSON 파서로 statusLine 객체만 교체).

수동 적용 시에도 동일한 `statusLine` 객체를 넣으면 된다.

## 디자인 제약

- **Windows 전용**: PowerShell 5+ 런타임, C++/WebView2 앱.
- **모듈 경계 = C 문자열(JSON)**: STL 을 DLL 경계로 넘기지 않음 (ABI 안전 + 플러그인 친화).
- **에러는 조용히**: `StatusLine.ps1` 은 `$ErrorActionPreference='SilentlyContinue'` + try/catch 로 statusLine 이 깨지지 않게.
- **출력 인코딩 UTF-8** 고정 (한글/이모지 깨짐 방지).
- **config.json 포맷 호환**: `rows` → `[{type, value?}]` 유지.

## Git 규칙

- **main 직접 커밋 금지**: 모든 변경은 feature 브랜치(`feature/<주제>`)에서 작업.
- 반영 흐름: feature 브랜치 커밋 → push → PR → main **스쿼시 머지**(`gh pr merge --squash`). merge commit / rebase 머지 비활성 (스쿼시만).

## GUI 자동 테스트 (E2E)

`Tests/E2E.Net/` — WebView2 앱을 CDP(`--remote-debugging-port`)로 구동하는 .NET/Selenium E2E. 하네스 `CcsApp` 가 앱을 env 포트로 spawn → EdgeDriver attach → top-frame ↔ 모듈 iframe(`SwitchToModule`) 조작.

- **반드시 격리 러너로 실행**한다. `dotnet test` 를 사용자 데스크톱에서 직접 실행 금지 — 앱 창이 떠 포커스를 뺏는다.
  - 표준 명령(전체): `pwsh Tests\격리데스크톱러너.ps1 -Command 'dotnet test Tests\E2E.Net\ClaudeCodeStudio.E2E.csproj -c Debug'`
  - 필터 단위: `... -c Debug --filter <테스트명>'`
  - `--no-build` 사용 시 사전 `dotnet build Tests\E2E.Net\ClaudeCodeStudio.E2E.csproj -c Debug` 필요.
- 러너(`격리데스크톱러너.ps1`)가 `CreateDesktop` 격리 데스크톱에서 테스트 트리를 실행 → 창이 사용자 데스크톱에 뜨지 않는다. 입력은 CDP 합성이라 실제 마우스/키보드도 안 건드린다.
- **SendInput / SetForegroundWindow 등 실제 OS 입력을 쓰는 테스트 코드 추가 금지** — 격리 데스크톱에 닿지 않고, 사용자 데스크톱으로 새면 간섭이 된다.
- 사전 요구: 앱 **Debug 빌드**(`bin\Debug\ClaudeCodeStudio.exe`) — VS F5 또는 `MSBuild ClaudeCodeStudio.sln /t:ClaudeCodeStudio /p:Configuration=Debug /p:Platform=x64`. msedgedriver 는 Selenium Manager 가 자동 해석(버전 핀 필요 시 `CCS_MSEDGEDRIVER_DIR`).
