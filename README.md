<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo/logo-dark.svg">
    <img src="assets/logo/logo.svg" width="112" alt="ClaudeCodeStudio 로고">
  </picture>
</p>

# ClaudeCodeStudio

Claude Code 설정을 관리하는 Windows용 통합 데스크톱 앱. **C++ + WebView2** 로 만들어졌으며,
좌측 탭으로 세 화면을 제공한다:

- **설정 동기화** — 글로벌 `~/.claude` 설정을 git 으로 서버와 동기화 (초기 설정 포함)
- **상태바 설정** — statusLine 레이아웃(`config.json`)을 시각적으로 편집
- **⚙ 설정** — 앱 자체 설정. 카테고리 탭(일반 / 화면 / 동기화)과 전 카테고리 검색을 제공

statusLine 렌더링은 PowerShell 런타임(`StatusLine.ps1`)이 그대로 담당한다 — 앱은 번들 + 설정만 관리.

## 구조

| 프로젝트 | 종류 | 역할 |
| --- | --- | --- |
| `ClaudeCodeStudio` | exe | 최소 부트스트래퍼 → `Core_Run` 호출 |
| `Core` | dll | 창 + WebView2 호스트 + 좌측 탭 셸 + 설정 화면 + 모듈 로더/라우터 |
| `SyncClaudeCodeSetting` | dll | 설정 동기화 모듈 (git status/log/pull/push + 초기 설정) |
| `ClaudeCodeStatusBar` | dll | 상태바 설정 모듈 (config.json 편집 + settings.json 적용) |
| `ReleaseTool` | exe | 릴리스 도구 — 버전을 검증하고 인스톨러를 만든다 (개발용, 설치본에는 없음) |
| `StatusLine.ps1` | ps1 | statusLine 런타임 (Claude Code 가 매 stdin 호출) |

각 기능 모듈은 `shared/module_api.h` 의 C 계약(`Module_Info/Init/Handle`)을 export 하고,
자기 web UI(HTML/JS)를 iframe 으로 소유한다. 경계는 C 문자열(JSON)뿐이라 ABI 안전 + 플러그인 친화.
설정 화면은 모듈이 아니라 셸이 직접 소유한다.

## 설정

**⚙ 설정** 탭에서 조절한다. 변경은 즉시 적용되고 이 PC 에 저장된다.

| 카테고리 | 항목 |
| --- | --- |
| 일반 | 시작 탭(마지막 사용 탭 기억 포함), 창 크기(해상도 선택) |
| 화면 | 테마(라이트/다크), 글자 크기, 글꼴 |
| 동기화 | 자동 새로고침 주기, 시작 시 원격 확인, 이력 표시 개수, 커밋 메시지 형식, 기본 저장소 URL |

검색창에 입력하면 카테고리를 가로질러 항목을 찾는다. 한글·영문 동의어를 함께 검색하므로
`해상도`, `resolution`, `다크`, `font` 같은 말로도 걸린다.

상태바 표시 옵션(막대 폭, 색 임계값, 아이콘 세트, 실행 셸)은 **상태바 설정** 탭 상단에 있다.

## 빌드

- 요구: VS 2022 (v143 툴셋), WebView2 SDK(NuGet), WebView2 런타임
- ```powershell
  MSBuild ClaudeCodeStudio.sln /p:Configuration=Release /p:Platform=x64
  ```
- 산출물: `bin/Release/` 에 `ClaudeCodeStudio.exe` + DLL 3개 + `web/` (그리고 개발용 `ReleaseTool.exe`)

GUI 자동 테스트(E2E)는 `Tests/E2E.Net/` 에 있다. 앱 창이 화면에 뜨지 않도록 격리 데스크톱에서 실행한다:

```powershell
pwsh Tests\격리데스크톱러너.ps1 -Command 'dotnet test Tests\E2E.Net\ClaudeCodeStudio.E2E.csproj -c Debug'
```

## 인스톨러 만들기

`bin/Release/ReleaseTool.exe` 를 실행하면 창이 뜬다. 새 버전을 입력하고 **[인스톨러 생성]** 을 누르면
빌드부터 패키징까지 진행되고 로그가 그 자리에 표시된다. 콘솔에서 바로 돌려도 된다:

```powershell
Build.bat 1.0.1                # 빌드 + 스테이징 + 인스톨러
Build.bat 1.0.1 --skip-build   # 기존 bin\Release 재사용
```

버전은 `MAJOR.MINOR.PATCH` 형식이어야 하고, **직전 발행 버전(`installer/VERSION`)보다 높아야** 한다.
낮거나 같으면 생성되지 않는다. 성공하면 `installer/VERSION` 이 갱신되어 다음 기준이 된다.

산출물은 `Shipping/ClaudeCodeStudio-Setup-<버전>.exe` 이며, 같은 폴더의 `Shipping/<버전>/` 에는
설치 없이 바로 실행할 수 있는 파일 일습이 남는다. NSIS 3.x 가 필요하다.

설치본은 `%LOCALAPPDATA%\Programs\SleighMaster\ClaudeCodeStudio` 에 설치되며 관리자 권한이 필요 없다.
설치 위치를 바꿔도 `{회사}\{프로그램}` 구조는 유지된다.
Visual C++ 런타임(`vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`)을 함께 담으므로
재배포 패키지가 없는 PC 에서도 실행된다.

## 요구 사항

- Windows 10/11
- PowerShell — 기본은 **PowerShell 7(`pwsh`)** 을 쓰고, 설치되어 있지 않으면 Windows 내장 PowerShell 5 로 자동 전환된다
- Microsoft Edge WebView2 런타임
- git (설정 동기화 + `git_*` 항목)
- [Claude Code CLI](https://docs.claude.com/en/docs/claude-code)

## statusLine 적용

**상태바 설정** 탭에서 레이아웃 편집 후 **"저장 & 적용"** → `config.json` 저장 +
`~/.claude/settings.json` 의 `statusLine` 을 `StatusLine.ps1` 경로로 갱신한다.
다른 설정은 건드리지 않고 `statusLine` 항목만 교체한다.

수동 적용:
```json
{
  "statusLine": {
    "type": "command",
    "command": "pwsh -NoProfile -ExecutionPolicy Bypass -File \"...\\StatusLine.ps1\""
  }
}
```

PowerShell 7 이 없다면 `pwsh` 대신 `powershell` 을 쓰면 된다. 앱에서 적용할 때는
**상태바 설정** 탭의 실행 셸 항목으로 고르며 기본값은 `pwsh` 다.

## 표시 옵션 (`config.json`)

레이아웃(`rows`)과 함께 `options` 를 저장한다. 없어도 동작하며 그때는 기본값이 쓰인다.

```json
{
  "rows": [ ... ],
  "options": {
    "bar_width": 10,
    "warn_pct": 50,
    "crit_pct": 80,
    "icon_set": "emoji",
    "shell": "pwsh"
  }
}
```

- `bar_width` — 사용률 막대 칸 수 (4~40)
- `warn_pct` / `crit_pct` — 막대 색이 노랑/빨강으로 바뀌는 기준 (%)
- `icon_set` — `emoji`(기본, 폰트 무관) 또는 `nerd`(터미널 폰트가 Nerd Font 일 때만 표시)
- `shell` — statusLine 실행 셸. `pwsh`(기본) 또는 `powershell`

## 계획 문서

통합 계획·결정 로그·단계(P0~P5)는 `docs/ClaudeCodeStudio-Integration-Plan.md` 가 SSOT.

## 라이선스

MIT
