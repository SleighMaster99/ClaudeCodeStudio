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
