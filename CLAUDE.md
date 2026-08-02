# ClaudeCodeStudio

Claude Code 설정 관리 Windows 통합 앱 (C++ + WebView2). 좌측 탭 3개:
- **설정 동기화** — 글로벌 `~/.claude` 를 git 으로 서버 동기화 (`SyncClaudeCodeSetting` 모듈)
- **상태바 설정** — statusLine 레이아웃(`config.json`) 편집 (`ClaudeCodeStatusBar` 모듈)
- **⚙ 설정** — 셸이 직접 소유(모듈 아님). 카테고리 탭(일반/화면/동기화) + 전 카테고리 검색.
  일반=시작 탭·창 크기, 화면=테마·글자 크기·글꼴, 동기화=자동 새로고침·시작 시 원격 확인·이력 개수·커밋 메시지 형식·기본 저장소 URL.
  값은 웹 `localStorage`(`ccs.ui.settings.v1` / `ccs.sync.settings.v1`)에 저장되고,
  창 크기만 C++ 이 `%LOCALAPPDATA%\ClaudeCodeStudio\window_size.txt` 에 기록한다(`CCSTUDIO_STATE_DIR` 로 오버라이드).

statusLine 렌더는 PowerShell 런타임 `StatusLine.ps1` 이 담당(유지). 앱은 번들 + 설정만 관리.

> 통합 계획·결정 로그·단계(P0~P5) SSOT: `docs/ClaudeCodeStudio-Integration-Plan.md`.

## 프로젝트 구조

| 경로 | 종류 | 역할 |
| --- | --- | --- |
| `ClaudeCodeStudio/` | exe | 최소 부트스트래퍼 (`WinMain → Core_Run`) |
| `Core/` | dll | 창 + WebView2 호스트 + 탭 셸 + 모듈 로더(`modules.cpp`)/라우터(`router.cpp`)/호스트(`host.cpp`) |
| `SyncClaudeCodeSetting/` | dll | 동기화 모듈 (`sync.cpp`: git status/log/pull/push + 초기 설정) |
| `ClaudeCodeStatusBar/` | dll | 상태바 모듈 (`statusbar.cpp`: config.json I/O + settings.json apply) |
| `ReleaseTool/` | exe | 릴리스 도구 — 별도 창(창+WebView2 자체 보유). 버전 검증 후 `Build.bat` 구동. **설치본 미포함** |
| `shared/module_api.h` | 헤더 | 모듈 C 계약 (`Module_Info/Init/Handle`, `PostToUiFn`) |
| `Build.bat` | 스크립트 | 릴리스 파이프라인 (버전 검증 → 빌드 → 스테이징 → NSIS → 이력 갱신) |
| `installer/` | 인스톨러 | `ClaudeCodeStudio.nsi` + `VERSION`(발행 이력) + `redist/`(VC++ CRT 3종) |
| `StatusLine.ps1`, `config.json` | 런타임 | statusLine 렌더러 + 레이아웃(`rows`)/표시 옵션(`options`) |
| `usage_config.json` | (미사용) | 플랜 한도 오버라이드용 파일. `Get-PlanLimits` 가 정의만 되어 있고 호출되지 않아 **현재 렌더에 영향 없음** |

각 모듈 dll 은 자기 `web/` UI(HTML/JS)를 소유하고, 빌드 시 `bin/Release/web/<모듈>/` 로 복사된다.
복사는 vcxproj 의 **`CustomBuild` 항목**이 한다 — **web 파일을 추가하면 vcxproj 의 `Include` 목록에도 넣어야** 배포된다.
`<None>` + `PostBuildEvent` 로 두면 HTML/JS 만 고쳤을 때 VS 의 최신 여부 검사에 걸리지 않아
빌드가 통째로 스킵되고 복사도 일어나지 않는다(F5 가 옛 화면을 띄운다 — 실측).
`ReleaseTool` 은 모듈이 아니라 독립 exe 라 자기 web 을 `bin/Release/releasetool/` 에 둔다 —
인스톨러가 `web/` 만 담으므로 이 위치면 설치본에 딸려 들어가지 않는다.

## 빌드

- 요구: VS 2022 **v143**, WebView2 SDK(NuGet `microsoft.web.webview2`, 정적 `WebView2LoaderStatic.lib`), WebView2 런타임
- `MSBuild ClaudeCodeStudio.sln /p:Configuration=Release /p:Platform=x64`
- MSVC 한글 리터럴 → **`/utf-8` 필수**. 산출물: `bin/Release/`
- 실행 검증: 격리 데스크톱(`CreateDesktop` + `STARTUPINFO.lpDesktop`) + `PrintWindow`(flag 2) 캡처 — 활성 세션/포그라운드 무간섭

## 릴리스 (인스톨러 생성)

`Build.bat` 이 전 과정을 수행하고, `ReleaseTool.exe` 는 그것을 띄우는 창일 뿐이다.

```
bin\Release\ReleaseTool.exe        창에서 버전 입력 → [인스톨러 생성]
Build.bat 1.0.1                    콘솔에서 직접 (CI 용)
Build.bat 1.0.1 --skip-build       기존 bin\Release 재사용
```

5단계: 버전 검증 → MSBuild → `Shipping\<ver>\` 스테이징 → makensis → `installer\VERSION` 갱신.
산출물 `Shipping\ClaudeCodeStudio-Setup-<ver>.exe` (`Shipping/` 은 .gitignore).

**버전 규칙**: `MAJOR.MINOR.PATCH`, 선행 0 금지, **마지막 발행분보다 높아야** 통과.
기준선은 `installer\VERSION` 과 **원격 태그(`v<ver>`) 중 높은 쪽**이다 —
파일만 보면 VERSION 커밋을 빠뜨린 clone 이 이미 배포한 버전을 다시 만든다.
오프라인이거나 `--sort` 를 모르는 구버전 git 이면 조용히 파일 값만 쓴다. `0.0.0` = 아직 발행 없음.

**`VERSION` 커밋은 ReleaseTool 이 한다.** 배포(`gh release create`)가 성공하면
`chore/version-<ver>` 브랜치를 만들어 `installer\VERSION` 만 커밋·push 하고 원래 브랜치로 돌아온다.
전용 브랜치를 쓰는 이유는 이 저장소가 main 직접 커밋을 금지하기 때문 — 머지는 사람이 PR 로 한다.
`Build.bat` 을 콘솔에서 직접 부른 경우에는 파일만 갱신되므로 직접 커밋해야 한다.

**주의할 점 세 가지**:
- `Build.bat` 은 솔루션 전체가 아니라 **`-t:ClaudeCodeStudio`** 로 빌드한다. ProjectReference 로 Core+모듈 2개는
  함께 빌드되지만 `ReleaseTool` 은 빠진다 — 스크립트를 구동 중인 프로세스가 자기 exe 를 덮어쓸 수 없기 때문.
- `ClaudeCodeStudio.nsi` 는 `/DVERSION /DSTAGE_DIR /DREDIST_DIR /DOUT_FILE` 을 **요구**한다.
  인자 없이 makensis 를 직접 부르면 `!error` 로 중단된다.
- `installer/redist/` 의 **VC++ CRT 3종은 필수**다. Release 가 `/MD` 링크라 이것이 없으면 재배포 패키지가 없는
  PC 에서 실행 자체가 안 된다. per-user 설치라 재배포 패키지를 깔 수 없어(관리자 권한) 앱 폴더에 동봉한다.
  툴셋을 올리면 `VC\Redist\MSVC\<ver>\x64\Microsoft.VC143.CRT\` 에서 3개를 다시 복사한다.

**설치 옵션 페이지**: Directory 다음에 nsDialogs 커스텀 페이지가 하나 있다 — 서버 저장소 URL(선택) 하나만 받는다.
넣으면 `HKCU\Software\ClaudeCodeStudio` 의 `RepoUrl` 값에 기록되고, 비우면 기록하지 않는다(건너뛰기).

**바탕화면 바로가기는 마지막(Finish) 화면의 체크박스**가 만든다. MUI 의 `SHOWREADME` 슬롯에 경로 대신 빈
문자열을 주고 `CreateDesktopShortcut` 을 콜백으로 걸었다 — 체크박스와 콜백만 쓰는 관용적 활용이다.
시작 메뉴 바로가기는 옵션과 무관하게 항상 만든다.

## 첫 실행 (초기 설정)

`~/.claude` 가 git 워킹트리가 아니면 sync 모듈이 `configured:false` 를 올려 **초기 설정 화면**만 노출한다.
저장소 URL 은 **어떤 기본값도 채우지 않는다** — 특정 저장소가 박혀 있으면 남의 저장소로 부트스트랩될 수 있다.
화면은 두 갈래다.

- **서버 주소를 직접 입력** — `bootstrap` 명령. 입력칸은 인스톨러가 남긴 `RepoUrl`(있으면)로만 채워진다.
  **이미 있는 저장소를 연결하는 갈래다** — 없는 URL 을 넣으면 `fetch` 가 실패하고 롤백된다(저장소를 만들지 않는다).
- **GitHub 계정에 새로 만들기** — `createRepo` 명령. `gh` CLI 에 위임한다
  (`gh api user --jq .login` 으로 계정 확인 → `gh repo view` 로 존재 확인 → 없으면 `gh repo create`).
  gh 미설치면 이 갈래는 잠긴다.

**gh 로그인은 앱 안에서 한다** — 터미널을 열 일이 없다. 설치돼 있는데 로그인 전이면 **[GitHub 로그인]** 버튼이 뜨고,
`ghLogin` 이 `gh auth login --hostname github.com --git-protocol https --skip-ssh-key` 를 파이프로 띄운다.
플래그로 프롬프트를 미리 채워 대화형 입력이 없고, 일회용 코드가 나오는 즉시 브라우저를 열고 코드를 화면에 띄운다.
gh 프로세스는 남겨 둔다(승인되면 스스로 끝남) — 종료를 기다리면 창이 멈추고, 워커 스레드에서는 WebView2 로 회신할 수 없다.
완료 여부는 웹이 3초마다 `ghInfo` 를 물어 감지한다.

`CmdBootstrap` 은 fetch 후 `origin/main` 유무로 방향을 정한다 — 있으면 서버 것으로 정렬(기존 설정은
`.sync-backup-<시각>/` 에 백업), 없으면(빈 저장소) 화이트리스트 `.gitignore` 를 만들고 이 PC 설정을 첫 커밋으로 push.
중간 실패 시 이 실행에서 만든 `.git` 을 지운다 — 롤백하지 않으면 워킹트리로 남아 초기 설정 화면이 다시 뜨지 않는다.

## 모듈 아키텍처

- **로딩**: `Core/src/modules.cpp` 의 `kModuleDlls[]` 에 dll 명을 하드코딩 → 시작 시 `LoadLibrary` + `GetProcAddress` 로 `Module_*` 바인딩 (D11 "암시적" = 목록 하드코딩).
- **메시지 흐름**:
  - iframe(모듈 web) → `window.parent.postMessage({module,cmd,arg})` → Core 셸이 `chrome.webview.postMessage(JSON)` → C++
  - C++ `WebMessageReceived` → `Router_Handle` → `module` 로 모듈 조회 → `Module_Handle(json)` (cmd/arg 해석은 모듈 몫)
  - 모듈이 `post` 콜백 호출 → Core 가 `{module,payload}` **봉투**로 씌워 회신 → Core 셸이 `env.module` iframe 에 payload 중계
- 새 모듈 추가: dll 프로젝트(`MODULE_EXPORTS` + `$(SolutionDir)shared` include) + `Module_*` 구현 + `kModuleDlls` 등록 + Core 셸 `index.html` 에 탭/iframe(`frames` 맵) + vcxproj 에 web 파일 `CustomBuild` 등록.

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
- `StatusLine.ps1` 에 세트 **두 개**: `$script:Icons`(이모지, 기본) / `$script:IconsNerd`(Nerd Font).
  `config.json` 의 `options.icon_set` 이 `nerd` 면 시작 시 `$script:Icons = $script:IconsNerd` 로 교체된다.
- `switch` 의 `{ $_ -like 'icon_*' }` 케이스가 키 잘라 조회 후 출력.
- 새 아이콘: **두 세트 모두** 추가 + `app.js` `CATALOG` 에 `icon_<key>`(cat="아이콘").
  한쪽만 넣으면 그 세트를 쓰는 사용자에게만 빈칸으로 보인다.
- 이모지는 폰트 의존이 없고, Nerd 글리프는 터미널 폰트가 Nerd Font 일 때만 표시된다.
  Nerd 코드포인트는 공식 `glyphnames.json` 으로 확인 후 추가한다(현재 세트는 `fa-*`/`cod-*` 계열).

### VS16 (Variation Selector-16, U+FE0F)

- **emoji_presentation = Yes**: 그냥 2칸 컬러 이모지 (🤖 🧠 📁).
- **= No**: 1칸 흑백 글리프 → 뒤에 `U+FE0F` 붙여 2칸 강제 (`'⏱'` → `'⏱️'`).
- 새 아이콘 추가 시 Unicode 공식 `Emoji_Presentation` 확인, `No` 면 VS16 부착. 현재 적용: `version`, `duration`, `host`, `h5`, `week`.

## statusLine 적용

**상태바 설정** 탭 **"저장 & 적용"**:
1. `config.json` 저장 (`statusbar.cpp` `CmdApply`) — `PrettyJson` 으로 들여써서 기록(설치본 배포 + 수동 편집 대상).
2. `~/.claude/settings.json` 의 `statusLine` 을 `<셸> -NoProfile -ExecutionPolicy Bypass -File <StatusLine.ps1>` 로 갱신 — **다른 필드 보존**(미니 JSON 파서로 statusLine 객체만 교체).

`<셸>` 은 `options.shell` 로 정해진다. **기본 `pwsh`**(PowerShell 7), 명시 선택 시 `powershell`.
pwsh 가 PATH 에 없으면 `powershell` 로 폴백하고 결과 메시지로 알린다 — 미설치 PC 에서 statusLine 이 비지 않게.

수동 적용 시에도 동일한 `statusLine` 객체를 넣으면 된다.

## 디자인 제약

- **Windows 전용**: PowerShell 런타임(기본 pwsh 7, 미설치 시 내장 powershell 5), C++/WebView2 앱.
- **모듈 경계 = C 문자열(JSON)**: STL 을 DLL 경계로 넘기지 않음 (ABI 안전 + 플러그인 친화).
- **에러는 조용히**: `StatusLine.ps1` 은 `$ErrorActionPreference='SilentlyContinue'` + try/catch 로 statusLine 이 깨지지 않게.
- **출력 인코딩 UTF-8** 고정 (한글/이모지 깨짐 방지).
- **config.json 포맷 호환**: `rows` → `[{type, value?}]` 유지. `options`(막대 폭·색 임계값·아이콘 세트·실행 셸)는
  **선택 키** — 없으면 `StatusLine.ps1` 과 편집기가 각자 기본값을 쓴다(기존 config 그대로 동작).

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
