# ClaudeCodeStudio

Claude Code 설정을 관리하는 Windows용 통합 데스크톱 앱. **C++ + WebView2** 로 만들어졌으며,
좌측 탭으로 두 기능을 제공한다:

- **설정 동기화** — 글로벌 `~/.claude` 설정을 git 으로 서버와 동기화 (초기 설정 포함)
- **상태바 설정** — statusLine 레이아웃(`config.json`)을 시각적으로 편집

statusLine 렌더링은 PowerShell 런타임(`StatusLine.ps1`)이 그대로 담당한다 — 앱은 번들 + 설정만 관리.

## 구조

| 프로젝트 | 종류 | 역할 |
| --- | --- | --- |
| `ClaudeCodeStudio` | exe | 최소 부트스트래퍼 → `Core_Run` 호출 |
| `Core` | dll | 창 + WebView2 호스트 + 좌측 탭 셸 + 모듈 로더/라우터 |
| `SyncClaudeCodeSetting` | dll | 설정 동기화 모듈 (git status/log/pull/push + 초기 설정) |
| `ClaudeCodeStatusBar` | dll | 상태바 설정 모듈 (config.json 편집 + settings.json 적용) |
| `StatusLine.ps1` | ps1 | statusLine 런타임 (Claude Code 가 매 stdin 호출) |

각 기능 모듈은 `shared/module_api.h` 의 C 계약(`Module_Info/Init/Handle`)을 export 하고,
자기 web UI(HTML/JS)를 iframe 으로 소유한다. 경계는 C 문자열(JSON)뿐이라 ABI 안전 + 플러그인 친화.

## 빌드

- 요구: VS 2022 (v143 툴셋), WebView2 SDK(NuGet), WebView2 런타임
- ```powershell
  MSBuild ClaudeCodeStudio.sln /p:Configuration=Release /p:Platform=x64
  ```
- 산출물: `bin/Release/` 에 `ClaudeCodeStudio.exe` + DLL 3개 + `web/`

## 요구 사항

- Windows 10/11
- PowerShell 5+ (statusLine 런타임)
- Microsoft Edge WebView2 런타임
- git (설정 동기화 + `git_*` 항목)
- [Claude Code CLI](https://docs.claude.com/en/docs/claude-code)

## statusLine 적용

**상태바 설정** 탭에서 레이아웃 편집 후 **"저장 & 적용"** → `config.json` 저장 +
`~/.claude/settings.json` 의 `statusLine` 을 `StatusLine.ps1` 경로로 갱신.

수동 적용:
```json
{
  "statusLine": {
    "type": "command",
    "command": "powershell -NoProfile -ExecutionPolicy Bypass -File \"...\\StatusLine.ps1\""
  }
}
```

## 사용률 한도 조정

`usage_config.json` 을 본인 플랜에 맞게 (값은 USD/API 환산 기준):
```json
{
  "pro":    { "5h":  35, "week":  245 },
  "max5x":  { "5h": 140, "week":  980 },
  "max20x": { "5h": 280, "week": 1960 }
}
```

## 계획 문서

통합 계획·결정 로그·단계(P0~P5)는 `docs/ClaudeCodeStudio-Integration-Plan.md` 가 SSOT.

## 라이선스

MIT
