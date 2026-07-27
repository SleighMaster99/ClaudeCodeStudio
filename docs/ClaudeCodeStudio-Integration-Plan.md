# ClaudeCodeStudio 통합 계획

Claude Code 설정 관련 두 도구(**설정 동기화**, **상태바 설정**)를 **하나의 C++ + WebView2 앱**으로 통합하는 계획 문서. 이 문서가 SSOT.

> 작성 시점 기준: 이 repo(`ClaudeCodeStatusBar`)를 `ClaudeCodeStudio` 로 리네임해 통합 앱의 소스 저장소로 사용한다.

---

## 0. 새 세션 착수 가이드 (현재 상태 스냅샷)

> 새 세션은 이 문서만으로 착수 가능. 아래 현재 상태·전제·필독 파일을 먼저 확인하고 **P0(§13)** 부터 시작한다.

### 현재 상태 (실측 기준)
- 데이터 repo `SleighMaster99/claude-code-settings`: 최초 커밋 push 완료(`cbd4920`). **앱 아님, 동기화 대상.**
- `~/.claude` 미커밋 2건: `settings.json`, `hooks/inject-claude-md.sh` (이식성 수정분 — 커밋/푸시 대기).
- 동기화 프로토타입: `~/.claude/SyncClaudeCodeSetting/` (exe·src·installer 존재, 빌드·실행·격리데스크톱 렌더 검증됨) → 본 repo 로 이동 예정.
- `gh` 인증 = **SleighMaster99**.

### 검증된 전제 도구
- VS 2022 Community + **v143** 툴셋, MSBuild(`...\2022\Community\MSBuild\Current\Bin\MSBuild.exe`).
- WebView2 SDK: `~/.nuget/packages/microsoft.web.webview2/1.0.3967.48` (정적 `WebView2LoaderStatic.lib`).
- WebView2 런타임 설치됨. NSIS: `C:\Program Files (x86)\NSIS\makensis.exe`. git·gh.
- (.NET 9 은 C++ 전환으로 **불필요**.)

### 착수 전 필독 파일
- 프로토타입(이동 원본): `~/.claude/SyncClaudeCodeSetting/SyncClaudeCodeSetting/src/main.cpp`, `web/`.
- WPF 편집기(포팅 원본): `Editor/Services/{ConfigStore,SettingsApplier,ProjectPaths}.cs`, `Editor/Models/*`, `Editor/ViewModels/MainViewModel.cs`.
- 런타임: `StatusLine.ps1`, `config.json`, `usage_config.json`.
- 동기화 화이트리스트: `~/.claude/.gitignore`.

### 필수 준수 (전역 규칙 — 세션 자동 로드됨)
- **push/merge 는 사용자 명시 지시 시에만.** 이 repo 는 BMad 규칙상 커밋도 **feature 브랜치 + 스쿼시 머지** (main 직접 커밋 금지).
- MSVC 한글 리터럴 → **`/utf-8` 필수**. **네이티브 VS 솔루션** 구조(.sln 루트 + 동명 하위폴더).
- 실행 검증은 **격리 데스크톱 + PrintWindow**(§15).

---

## 1. 목표 & 범위

- **한 개의 앱(exe 1개)** 으로 두 기능을 좌측 탭으로 분리 제공: `[ 설정 동기화 | 상태바 설정 ]`.
- 스택: **C++ + WebView2**(HTML/JS UI). 기존 C# WPF 편집기는 이 스택으로 재구현.
- **exe 는 최소 부트스트래퍼**, 나머지는 전부 **DLL** 로 분리(호스트 프레임워크 포함). 추후 기능 확장 대비.
- `StatusLine.ps1`(렌더러 런타임)은 **PowerShell 그대로 유지** — 앱은 번들 + 설정만 관리.

### 범위 밖 (현 시점)

- `StatusLine.ps1` 자체의 C++ 재작성 (불필요, 유지).
- 프로젝트 단위(로컬 `.claude`) 설정 동기화 (글로벌만 대상).

---

## 2. 결정 로그 (확정)

| # | 결정 |
|---|---|
| D1 | 동기화 대상은 **글로벌 `~/.claude` 설정만** (`CLAUDE.md`, `settings.json`, `commands/`, `hooks/`, `output-styles/`) |
| D2 | 동기화 채널 = **git**. 데이터 repo = `SleighMaster99/claude-code-settings` (private), 화이트리스트 `.gitignore` |
| D3 | 스택 = **C++ + WebView2** (C# 아님) |
| D4 | 프로젝트 구조 = **네이티브 VS 솔루션** (.sln 루트 + 프로젝트 동명 하위폴더). CMake 아님 |
| D5 | 인스톨러 = **NSIS** (per-user, 관리자 불필요) |
| D6 | 최초 설정(git 부트스트랩)은 **앱의 "초기 설정" 플로우**가 담당 (인스톨러 아님) |
| D7 | 두 도구를 **한 앱으로 통합** (ClaudeCodeStudio) |
| D8 | 앱 repo = 기존 `ClaudeCodeStatusBar` 를 **`ClaudeCodeStudio` 로 리네임**. 데이터 repo(`claude-code-settings`)는 **불변** |
| D9 | 기능 분리 = **DLL**. exe 최소화 + `Core.dll`(호스트) + 기능별 DLL. 경계는 **C 문자열(JSON in/out)** |
| D10 | **statusLine 이식성** = 절대경로 대신 **환경변수 경로**(`$LOCALAPPDATA` 기반) 기록 → 동기화 문자열 PC 공통 + 각 PC 해석 (셸 확장은 P5 실측) |
| D11 | **모듈 로딩** = **암시적**(빌드 링크 + 시작 시 자동 로드). 플러그인 확장 필요 시 명시적 `modules/` 전환 |
| D12 | **탭 UI** = **모듈별 iframe** (각 모듈이 자기 web 소유) |
| D13 | **리네임** = **P0 검증 후** repo + 로컬 폴더 함께 `ClaudeCodeStudio` 로, statusLine 재적용 포함 |

---

## 3. repo 전략

| repo | 정체 | 조치 |
|---|---|---|
| `SleighMaster99/claude-code-settings` | 동기화되는 **설정 데이터**(~/.claude) | **변경 없음** (앱의 대상, 앱 아님) |
| `SleighMaster99/ClaudeCodeStatusBar` | 통합 앱 **소스** | **`ClaudeCodeStudio` 로 리네임** |

- GitHub 리네임은 **자동 리다이렉트**되어 기존 clone/URL 이 깨지지 않음.
- 리네임 후 로컬: `git remote set-url origin https://github.com/SleighMaster99/ClaudeCodeStudio.git` (선택, 리다이렉트로도 동작).
- 이 repo 는 BMad 규칙 적용: **main 직접 커밋 금지 → feature 브랜치 + 스쿼시 머지**.

---

## 4. 아키텍처

**플러그인식 모듈 구조** — 최소 exe 가 `Core.dll`(프레임워크)을 띄우고, `Core` 가 기능 DLL(모듈)들을 로드해 탭으로 노출.

```
[exe] ClaudeCodeStudio         최소 진입점 → Core 실행
   │
[dll] Core                     창 + WebView2 호스트 + 좌측 탭 셸 + 모듈 로더 + 메시지 라우터 + web 서빙
   │  ├── loads ─▶ [dll] SyncClaudeCodeSetting   (동기화 모듈: git + 부트스트랩)
   │  └── loads ─▶ [dll] ClaudeCodeStatusBar      (상태바 모듈: config 편집 + settings.json apply)
   │
[ps1] StatusLine.ps1           Claude Code 가 직접 호출하는 렌더러 (앱과 별개, 앱은 설정만)
```

- **결과물은 exe 하나** + DLL 들 (사용자는 exe 하나 실행, DLL 자동 로드).
- 경계는 **C 문자열(JSON)** — STL 을 DLL 경계로 넘기지 않아 ABI 안전 + 플러그인 친화.
- 세 프로젝트 모두 **동일 MSVC 툴셋(v143) + 동적 CRT** 로 빌드.

---

## 5. 솔루션 / 프로젝트 구조

```
D:\Repo\sleighmaster\ClaudeCodeStudio\        ← repo 루트 (ClaudeCodeStatusBar 리네임)
│
├─ ClaudeCodeStudio.sln                        ← 솔루션 (루트)
│
├─ ClaudeCodeStudio\                           ← [프로젝트1] exe (최소 부트스트래퍼)
│   ├─ ClaudeCodeStudio.vcxproj
│   └─ src\ main.cpp                            (WinMain → Core_Run 호출, ~30줄)
│
├─ Core\                                        ← [프로젝트2] dll (호스트 프레임워크)
│   ├─ Core.vcxproj
│   ├─ src\
│   │   ├─ host.cpp        창 + WebView2 생성/바운드
│   │   ├─ router.cpp      JS 메시지 파싱 → 대상 모듈로 분배 → 결과 회신
│   │   └─ modules.cpp     모듈 DLL 로드 + 레지스트리
│   └─ web\
│       ├─ index.html      좌측 탭 셸 (탭 바 + 콘텐츠 프레임)
│       └─ shell.css
│
├─ SyncClaudeCodeSetting\                       ← [프로젝트3] dll (동기화 모듈)
│   ├─ SyncClaudeCodeSetting.vcxproj
│   ├─ src\ sync.cpp       git status/log/pull/push + 부트스트랩(초기 설정)
│   └─ web\                동기화 탭 UI (현재 프로토타입 UI 이식)
│
├─ ClaudeCodeStatusBar\                         ← [프로젝트4] dll (상태바 모듈)
│   ├─ ClaudeCodeStatusBar.vcxproj
│   ├─ src\ statusbar.cpp  config.json 읽기/쓰기 + settings.json statusLine apply
│   └─ web\                상태바 설정 탭 UI (WPF 편집기 → 웹 포팅)
│
├─ runtime\
│   ├─ StatusLine.ps1       렌더러 (유지)
│   ├─ config.json          상태바 레이아웃 기본값
│   └─ usage_config.json
│
├─ installer\
│   └─ ClaudeCodeStudio.nsi
│
├─ docs\ (이 문서)
│
└─ bin\, obj\, .vs\         빌드 산출물 (.gitignore 제외)
```

> 공유 헤더 `module_api.h` 는 `Core\include\` 또는 solution 루트 `shared\` 에 두고 각 모듈이 참조.

---

## 6. 모듈 인터페이스 계약

모든 기능 DLL 이 export 하는 C 인터페이스 (`module_api.h`):

```cpp
extern "C" {
    // Core → 모듈: UI 로 결과를 보내는 콜백 (비동기 결과용)
    typedef void (*PostToUiFn)(const char* json);

    // 모듈 메타데이터(JSON): { "id":"sync", "tabTitle":"설정 동기화", "webEntry":"index.html" }
    __declspec(dllexport) const char* Module_Info();

    // 초기화 — Core 가 UI post 콜백을 넘김
    __declspec(dllexport) void Module_Init(PostToUiFn post);

    // UI 요청 처리 (JSON in). 결과는 post() 로 회신 (동기·비동기 모두 가능)
    __declspec(dllexport) void Module_Handle(const char* requestJson);
}
```

- 반환 문자열은 **모듈이 소유**(스레드 로컬 static 등), Core 는 즉시 복사 → 크로스-DLL free 회피.
- 이 계약은 현재 JS↔C++ 문자열 메시지 방식과 동일해 자연스럽게 확장됨.

---

## 7. 메시지 흐름

```
사용자 탭 클릭/버튼
  → (JS) window.chrome.webview.postMessage({ module:"sync", cmd:"pull" })
  → (Core/host) WebMessageReceived
  → (Core/router) module="sync" 조회 → Sync.dll Module_Handle(json)
  → (Sync 모듈) git 실행 → post({ type:"result", ... })  // Core 가 준 콜백
  → (Core) PostWebMessageAsString → (JS) 화면 갱신
```

---

## 8. 기능별 범위

### Core 모듈 (dll)
- 창 + WebView2 + `SetVirtualHostNameToFolderMapping` 으로 web 서빙.
- 좌측 탭 셸(`index.html`) — 탭 클릭 시 해당 모듈 web 로드(iframe 또는 콘텐츠 스왑).
- 모듈 로드(현재는 암시적 링크; 추후 `modules/` 폴더 명시적 로드로 확장 가능).
- 메시지 라우팅 + 모듈별 post 콜백 중계.

### Sync 모듈 (dll) — 현재 프로토타입 이식
- `status` / `log` (읽기): 브랜치·ahead/behind·clean·원격 URL·커밋 이력.
- `pull` / `push`(=변경 시 커밋 후 push) / `refresh`(fetch).
- **초기 설정(부트스트랩)**: `~/.claude` 제자리 git 초기화 → remote → fetch → 기존 설정 백업 → origin/main 정렬. (§10)
- `revert`(예정): 특정 시점 복원.

### StatusBar 모듈 (dll) — WPF 편집기 포팅
- `config.json`(레이아웃: rows → items) 로드/저장. (기존 `ConfigStore` 대체)
- 드래그드롭 레이아웃 편집 UI (웹이 적합).
- **"저장 & 적용"**: `settings.json` 의 `statusLine` 을 번들된 `StatusLine.ps1` 경로로 기록. (기존 `SettingsApplier` 대체)
- 항목 카탈로그/아이콘: 기존 `StatusLine.ps1` + `ItemCatalog` 규칙 승계.

---

## 9. StatusLine.ps1 (런타임) 취급

- **유지** — Claude Code 가 매 stdin 호출하는 렌더러. 앱과 독립 동작.
- 인스톨러가 앱과 함께 설치, StatusBar 모듈이 `settings.json` 의 statusLine 을 그 경로로 기록.
- **이식성 이슈(미결)**: statusLine 경로가 절대경로라 PC 마다 달라질 수 있음. §13 에서 전략 결정.

---

## 10. 최초 실행(부트스트랩) 플로우

새 PC 에서 앱 첫 실행 시 (동기화 미설정 감지):

1. 좌측 "설정 동기화" 탭에 **[초기 설정]** 버튼 하나만 노출.
2. 클릭 → 저장소 URL 확인(기본 `claude-code-settings`) → 진행:
   - `git init` (in `~/.claude`) → `remote add origin` → `git fetch`
   - 기존 `settings.json`·`CLAUDE.md` 등 **백업**(`~/.claude/.sync-backup-<시각>/`)
   - `origin/main` 으로 정렬 (credentials·sessions 는 `.gitignore` 보호)
3. 완료 → 일반 동기화 화면으로 전환. 이후 pull/push 사용.
- 인증: private repo → Git Credential Manager 가 첫 fetch 시 브라우저 로그인(이후 캐시).
- 전제(집 PC): Git for Windows(GCM 포함) + WebView2 런타임.
- 용어: "초기화" 대신 **"초기 설정"** (reset 오해 방지).

---

## 11. 인스톨러 & 배포

- **NSIS**(설치됨) → `ClaudeCodeStudio Setup.exe`. per-user(`%LOCALAPPDATA%\Programs\ClaudeCodeStudio`), 관리자 불필요.
- 번들: `ClaudeCodeStudio.exe` + `Core.dll` + `SyncClaudeCodeSetting.dll` + `ClaudeCodeStatusBar.dll` + `web/` + `StatusLine.ps1` + 기본 config.
- 시작 메뉴/바탕화면 바로가기 + 제거 프로그램 등록.
- 배포 경로: GitHub Release 로 setup.exe 업로드 → 새 PC 다운로드·실행.

---

## 12. 마이그레이션 작업 목록

1. GitHub repo `ClaudeCodeStatusBar` → `ClaudeCodeStudio` 리네임. 로컬 remote URL 갱신(선택).
2. (선택) 로컬 폴더 리네임 → statusLine 경로 재적용 필요.
3. 솔루션 `ClaudeCodeStudio.sln` + 4개 프로젝트 생성(exe / Core / Sync / StatusBar).
4. 현재 동기화 프로토타입(`~/.claude/SyncClaudeCodeSetting`) 이 repo 로 이동·분해:
   - host(main.cpp) → **Core**, git 로직 → **Sync dll**, web → **모듈 web**.
5. 기존 C# `Editor/`(WPF) 제거 → **ClaudeCodeStatusBar dll**(C++) + `web/statusbar/` 로 대체. `ConfigStore`/`SettingsApplier` 로직 C++ 이식.
6. `StatusLine.ps1` + config → `runtime/` 로 정리, 경로 탐색 로직 재조정.
7. NSIS 인스톨러 → `ClaudeCodeStudio.nsi` (exe + 전 DLL + web + StatusLine.ps1).
8. Sync 모듈에 초기 설정(부트스트랩) 구현.
9. repo `CLAUDE.md`/문서 통합 범위로 갱신.
10. `.gitignore` 정비(bin/obj/.vs 제외).

---

## 13. 단계별 구현 계획 (Phases)

| Phase | 내용 | 검증 |
|---|---|---|
| **P0** | repo 리네임 + 솔루션 골격(exe + Core, 빈 탭 셸) | 빈 창 + 좌측 탭 2개 렌더 |
| **P1** | Sync 모듈 DLL (프로토타입 git 로직 이식) | 동기화 탭이 실제 git 상태/이력 표시 |
| **P2** | StatusBar 모듈 DLL (WPF 편집기 포팅) | 상태바 탭에서 레이아웃 편집·저장&적용 동작 |
| **P3** | 초기 설정(부트스트랩) 플로우 | 미설정 PC 에서 [초기 설정] → 동기화 준비 완료 |
| **P4** | NSIS 인스톨러 (전체 번들) | setup.exe 설치 → 앱 실행 → 두 탭 동작 |
| **P5** | statusLine 이식성 전략 적용 | 다른 PC 에서 상태바 정상 표시 |

---

## 14. 빌드 & 검증

- 빌드: `MSBuild ClaudeCodeStudio.sln /p:Configuration=Release /p:Platform=x64` (VS 2022 v143).
- MSVC 한글 리터럴 → **`/utf-8` 필수** (코드페이지 949 오독 방지).
- WebView2 SDK: NuGet 캐시 `microsoft.web.webview2/1.0.3967.48` (정적 loader `WebView2LoaderStatic.lib`).
- 실행 검증: 창 핸들 `PrintWindow` 캡처로 실제 렌더 확인(다른 세션 포그라운드 경쟁 회피).

---

## 15. 테스트 전략 (검증됨)

3계층으로 나눈다:

1. **단위 테스트 (주력)** — 기능 DLL 을 GUI 없이 직접 호출. `Module_Handle(json)` 에 요청을 넣고 응답 JSON 검증. git 로직·config 편집 로직을 WebView2/데스크톱 없이 빠르게 검증. **DLL + C 문자열 경계 설계의 직접적 이점.**
2. **E2E (격리 데스크톱)** — 실제 앱을 **숨겨진 별도 Win32 데스크톱**(`CreateDesktop` + `STARTUPINFO.lpDesktop`)에서 실행 → 사용자 화면/포그라운드를 전혀 건드리지 않고 창을 `PrintWindow`(flag `PW_RENDERFULLCONTENT`=2)로 캡처·검증.
   - **실측 확인됨**: 우리 exe 를 숨김 데스크톱에서 실행 시 WebView2 정상 렌더(빈/검은 화면 아님), 캡처 성공, 활성 세션 무간섭. 라이브 git 상태도 정상 반영.
   - 앞서 겪은 "다른 세션 창의 포그라운드 탈취로 캡처 실패" 문제를 원천 차단.
3. **UI 자동화** — WebView2 콘텐츠는 `ExecuteScript`/CDP 로 DOM 조작·이벤트 주입 가능 → 화면 스크래핑 없이 UI 구동.

> 방침: 무거운 E2E 는 최소화, **단위 테스트(DLL 직접 호출)를 주력**으로. E2E 는 격리 데스크톱에서 실행해 개발 흐름을 방해하지 않는다.

---

## 16. 세부 결정 (확정)

§2 결정 로그 **D10~D13** 으로 전부 확정됨:

- **statusLine 이식성** (D10): 절대경로 대신 **환경변수 경로**(`$LOCALAPPDATA` 기반) 기록 → 동기화 문자열 PC 공통 + 각 PC 해석. 셸 확장 실제 동작은 **P5 착수 시 실측**(hook `$HOME` 검증과 동일 방식).
- **모듈 로딩** (D11): **암시적** (빌드 시 링크 + 시작 시 자동 로드). 향후 진짜 플러그인(앱 재빌드 없이 모듈 추가)이 필요하면 명시적 `modules/` 로 전환 — C 문자열 경계라 전환 용이.
- **탭 UI** (D12): **모듈별 iframe** — 각 모듈이 자기 web(HTML/CSS/JS) 소유, iframe 으로 격리.
- **리네임** (D13): **P0 골격 검증 후**, GitHub repo + 로컬 폴더를 함께 `ClaudeCodeStudio` 로. 폴더 리네임 → statusLine 경로 재적용 포함.

> 남은 미확인은 statusLine 환경변수 방식의 셸 확장뿐 — P5 착수 시 실측으로 닫는다.

---

## 17. 이미 완료된 작업 (참조 — 이 repo 밖)

- `claude-code-settings` repo 생성 + 최초 커밋 push(`cbd4920`, 13파일). 화이트리스트 `.gitignore` + `.gitattributes`(shell LF 고정).
- **설정 이식성 수정 완료**(검증됨):
  - `settings.json`: hook 경로 3곳 → `bash "$HOME/.claude/hooks/X.sh"`, OneDrive deny 10곳 → `~/OneDrive/**`, dh.kim 하드코딩 allow 2곳 제거.
  - `hooks/inject-claude-md.sh`: `HOME = os.path.expanduser('~')`.
  - `statusLine` 은 사용자명 무관(`D:\Repo\...`) — §13 에서 별도 처리.
- 동기화 앱 **프로토타입**(C++/WebView2, 네이티브 VS 솔루션) 빌드·실행 검증 완료 — 위치: `~/.claude/SyncClaudeCodeSetting` (→ 본 repo 로 이동 예정).
- NSIS 인스톨러 프로토타입(`SyncClaudeCodeSetup.exe`) 빌드 완료 (→ `ClaudeCodeStudio.nsi` 로 통합 예정).

---

## 18. 구현 반영 (P0~P5 완료 후, 실측·계약에 따른 조정)

계획 대비 다음이 실측/계약에 따라 조정됐다. 원본 결정(의도)은 위 섹션 그대로 두고, 실제 구현 결과를 여기 기록한다.

- **D10 statusLine 이식성** — `$LOCALAPPDATA` 환경변수 대신 **`~` 기반 forward-slash 경로**로 확정.
  P5 실측 결과 Git Bash 는 statusLine command 에서 `~` 를 각 PC home 으로 확장함(공식 문서 확인)하나, 임의 환경변수(`$LOCALAPPDATA`/`%LOCALAPPDATA%`) 확장은 문서에 없어 `~` 를 채택. 목표(PC 공통 문자열 + 각 PC 해석)는 동일 달성.
  → 설치본 기준 `<셸> -NoProfile -ExecutionPolicy Bypass -File ~/AppData/Local/Programs/ClaudeCodeStudio/StatusLine.ps1` (개발 위치면 절대경로 fallback).
  `<셸>` 은 아래 §19 참조 — 최초 구현은 `powershell` 고정이었고, 이후 선택 가능(기본 `pwsh`)으로 바뀌었다.
- **D11 모듈 로딩** — "빌드 링크" 대신 **`LoadLibrary` + `GetProcAddress`**(하드코딩 목록 `kModuleDlls`).
  §6 계약이 모듈마다 동일 함수명(`Module_*`)이라 암시적 링크는 심볼 충돌 → 런타임 로드로 구현. "암시적"의 의미(로드할 dll 목록을 코드에 하드코딩)는 유지. 향후 `modules/` 폴더 스캔으로 전환 용이.
- **§5 / §12-6 `runtime/` 폴더** — 별도 폴더로 이동하지 않고 **repo 루트에 유지**(StatusLine.ps1, config.json, usage_config.json).
  StatusLine.ps1 은 `$PSScriptRoot` 로 config 를 찾고, StatusBar 모듈은 **자기 dll 위치 기준**으로 StatusLine.ps1 을 상위 탐색하므로 폴더 분리가 불필요. 인스톨러는 exe 옆에 함께 배치.
- **§12-5 Editor/ 제거** — C# WPF 편집기 소스/tracked 제거 + 물리 폴더(빌드산출물)까지 삭제 완료.

> 결과: Phase P0~P5 전부 완료·검증. 남은 미확인(환경변수 셸 확장)은 `~` 채택으로 닫힘.

---

## 19. P5 이후 변경 (통합 완료 후 기능 확장)

P0~P5 로 통합이 끝난 뒤 추가된 것들. §1~§17 은 통합 당시 계획·결정 기록이므로 그대로 두고,
그 이후 달라진 사항만 여기 적는다.

- **좌측 탭에 ⚙ 설정 화면 추가** (§1 은 탭 2개 전제) — 모듈이 아니라 **셸이 직접 소유**한다.
  카테고리 탭(일반/화면/동기화) + 전 카테고리 검색 구조. 값은 웹 `localStorage` 에 저장하고,
  창 크기만 C++ 이 `%LOCALAPPDATA%\ClaudeCodeStudio\window_size.txt` 에 기록한다.
  셸 전용 명령이 필요해져 라우터에 `module=="core"` 분기(`Host_HandleCoreCmd`)를 추가했다.
- **`config.json` 에 `options` 추가** — 막대 폭 / 색 임계값 / 아이콘 세트 / 실행 셸.
  **선택 키**라 없으면 기본값으로 동작하므로 기존 config 와 호환된다(§16 의 rows 계약 유지).
  저장 시 `PrettyJson` 으로 들여쓴다 — 설치본에 배포되고 손으로도 고치는 파일이라서.
- **statusLine 실행 셸 선택** — `options.shell` 로 정하며 **기본 `pwsh`**(PowerShell 7),
  PATH 에 없으면 `powershell` 로 폴백하고 결과 메시지로 알린다. D10 의 경로 규칙은 그대로.
  이 과정에서 `StatusLine.ps1` 의 stdin 읽기를 `[Console]::In` → `OpenStandardInput()` +
  UTF-8 명시 디코딩으로 교체했다. PowerShell 7 은 `[Console]::InputEncoding` 설정이
  리다이렉트된 stdin 에 반영되지 않아 세션 JSON 파싱이 조용히 실패했다(실측).
- **아이콘 세트 이원화** — `$script:Icons`(이모지, 기본) / `$script:IconsNerd`(Nerd Font).
  `options.icon_set` 으로 전환. 새 아이콘은 두 세트 모두에 추가해야 한다.
- **동기화 UI** — 미커밋 변경을 이력 맨 위 별도 행으로 표시하고 '현재 PC' 태그를 그 행에 둔다
  (커밋 포인터가 서버와 같아도 앞섬이 보이도록). 원격 작업 중에는 오버레이로 화면을 덮어 중복 실행을 막는다.
- **`usage_config.json` 은 현재 미사용** — `Get-PlanLimits` 가 정의만 되어 있고 호출되지 않아
  렌더 결과에 영향이 없다. 사용률 표시는 stdin `rate_limits` 로만 계산된다.
- **테스트** — §15 의 E2E(격리 데스크톱 + CDP) 계층에 설정/동기화 시나리오를 추가해 14건 운용 중.
  §15 가 주력으로 제시한 **단위 테스트(모듈 DLL 직접 호출) 하네스는 아직 없다** — 남은 과제.

### 릴리스 파이프라인 (§11 인스톨러 & 배포의 후속)

§11 은 "NSIS 로 setup.exe 를 만들어 GitHub Release 에 올린다"까지만 정했다. 실제로는 그 앞뒤가 수작업이라
(버전을 nsi 에 손으로 박고, MSBuild 와 makensis 를 따로 돌림) 다음을 추가했다.

- **`Build.bat` — 5단계 파이프라인**: 버전 검증 → MSBuild → `Shipping\<ver>\` 스테이징 → makensis →
  `installer\VERSION` 갱신. 산출물은 `Shipping\ClaudeCodeStudio-Setup-<ver>.exe`.
  스테이징을 거치는 이유는 pdb/lib 유입 차단이 아니라(nsi 가 파일을 명시하므로 애초에 안 들어간다)
  **설치 없이 그대로 실행하거나 zip 으로 묶을 수 있는 배포본을 덤으로 얻기** 위해서다.
- **`installer\VERSION` — 발행 이력**: 마지막으로 만든 버전을 담고, 이보다 낮거나 같은 버전은 생성이 거부된다.
  git tag·GitHub Release 는 당시 둘 다 비어 있었고, 산출물 파일명 스캔은 `Shipping/` 이 .gitignore 라
  PC 를 옮기면 이력이 사라진다. 커밋되는 파일이 가장 확실해서 이 방식을 택했다.
- **`-t:ClaudeCodeStudio` 로 빌드**: 솔루션 전체를 빌드하면 `ReleaseTool` 도 대상이 되는데, 그 스크립트를
  구동하는 것이 바로 `ReleaseTool.exe` 라 자기 exe 를 덮어쓰다 실패한다. ProjectReference 로 Core 와
  모듈 2 개는 함께 빌드되므로 번들에 빠지는 것은 없다.
- **VC++ CRT 3종 동봉** (`installer\redist\`): §4 가 정한 **동적 CRT**(`/MD`) 때문에 대상 PC 에
  재배포 패키지가 없으면 실행 자체가 안 된다. D5 의 per-user 설치(관리자 불필요)를 유지하는 한
  재배포 패키지를 설치할 수 없으므로, Edge·Office·Blender 등이 쓰는 **앱 폴더 동봉** 방식을 골랐다.
  `/MT` 정적 링크도 후보였으나 vcxproj 4 개의 빌드 설정을 바꿔야 하고
  `WebView2LoaderStatic.lib` 와의 조합이 확인되지 않아 보류했다.
- **`ClaudeCodeStudio.nsi` 인자화**: `/DVERSION /DSTAGE_DIR /DREDIST_DIR /DOUT_FILE` 을 받고,
  없으면 `!error` 로 중단한다 — 버전이 하드코딩된 설치본이 실수로 나오는 것을 막는다.

- **`ReleaseTool` — 릴리스 전용 창(별도 exe)**: 버전을 입력받아 검증하고 `Build.bat` 을 띄운 뒤
  출력을 로그로 흘린다. 프로세스 실행은 Sync 모듈의 `RunCapture` 와 같은 방식(`CREATE_NO_WINDOW` +
  파이프)이라 콘솔 창이 뜨지 않는다.
  - **모듈(탭)이 아니라 별도 exe 인 이유**: 릴리스는 개발자 전용 기능이라 설치본에 노출되면 안 된다.
    탭으로 만들고 인스톨러에서 빼는 방법도 있었지만, `Core_Run` 이 탭 셸·모듈 로더를 전제로 하고 있어
    (창 클래스·시작 URL·상태 폴더·모듈 목록이 전부 앱 전용으로 고정) 재사용하려면 Core 를 파라미터화해야 했다.
    **동작 중인 앱의 핵심 DLL 을 건드리지 않는 쪽**을 택해, 창 + WebView2 만 자체 보유하는 독립 exe 로 두었다.
    §4 의 "결과물은 exe 하나 + DLL 들"은 **배포 기준으로는 그대로**다 — 이 exe 는 설치본에 들어가지 않는다.
  - 설치본 격리는 **`Build.bat` 과 nsi 어디에도 `ReleaseTool` 을 파일로 언급하지 않는 것**으로 달성한다.
    자기 web 도 `bin\Release\releasetool\` 에 두어 nsi 의 `web\` 복사 범위 밖에 있다.
  - 자동 검증은 격리 데스크톱 렌더·`init` 통신까지다. §15 의 격리 데스크톱에는 실제 OS 입력이 닿지 않고
    (`CLAUDE.md` 도 `SendInput` 사용을 금지한다) `ReleaseTool` 은 CDP 를 열지 않으므로,
    버전 입력 이후의 흐름은 **수동 확인 대상**으로 남는다.
