# Claude Code StatusBar

Claude Code 하단 `statusLine`을 멀티라인으로 커스터마이징하는 Windows용 도구.
PowerShell 런타임 + WinForms GUI 편집기로 구성되어 있다.

## 구성

| 파일 | 역할 |
| --- | --- |
| `StatusLine.ps1` | statusLine 런타임. stdin JSON을 받아 `config.json` 레이아웃으로 출력 |
| `StatusBarConfig.ps1` | WinForms GUI 편집기 (드래그&드롭 레이아웃 편집) |
| `Edit-StatusBar.vbs` | 콘솔 없이 GUI를 띄우는 런처 |
| `config.json` | 레이아웃 정의 (`rows` → 각 줄의 항목 배열) |
| `usage_config.json` | 플랜별 5시간/주간 한도 (pro / max5x / max20x) |

## 요구 사항

- Windows 10/11
- PowerShell 5+ (기본 내장)
- [Claude Code CLI](https://docs.claude.com/en/docs/claude-code)
- (선택) `git` — `git_user`, `git_branch` 항목을 쓰려면 필요

## 설치

1. 원하는 위치에 clone:
   ```powershell
   git clone https://github.com/SleighMaster99/ClaudeCodeStatusBar.git
   cd ClaudeCodeStatusBar
   ```
2. `Edit-StatusBar.vbs` 더블클릭 → GUI 편집기 실행
3. 레이아웃 조정 후 **"저장 & 적용"** 버튼 클릭

"저장 & 적용"은 다음 두 가지를 수행한다:

1. `config.json`에 레이아웃 저장
2. `~/.claude/settings.json`의 `statusLine`을 이 폴더의 `StatusLine.ps1`을 실행하도록 갱신

## 수동 적용

GUI를 쓰지 않고 직접 `~/.claude/settings.json`에 추가해도 된다:

```json
{
  "statusLine": {
    "type": "command",
    "command": "powershell -NoProfile -ExecutionPolicy Bypass -File \"D:\\path\\to\\ClaudeCodeStatusBar\\StatusLine.ps1\""
  }
}
```

## 지원 항목

GUI 팔레트의 카테고리별로:

- **Claude**: `model`, `version`, `effort`, `ctx_size`, `ctx_bar`, `ctx_pct`, `cost`, `duration`, `session_id`, `transcript`
- **워크스페이스**: `project_dir`, `current_dir`
- **Git**: `git_branch`, `git_user`
- **시간**: `time`, `date`
- **시스템**: `host`, `user`
- **사용률**: `h5_bar`, `h5_pct`, `week_bar`, `week_pct` — `~/.claude/projects/**/*.jsonl`을 60초 캐시로 집계
- **아이콘**: `icon_*` (같은 줄 내 아이콘은 하나로 묶을 때 사용)
- **구분자/포맷**: `space`, `sep_pipe`, `sep_slash`, `newline`

## 사용률 한도 조정

`usage_config.json`을 본인 플랜에 맞게 수정:

```json
{
  "pro":    { "5h":  35, "week":  245 },
  "max5x":  { "5h": 140, "week":  980 },
  "max20x": { "5h": 280, "week": 1960 }
}
```

값은 USD(API 환산) 기준이다.

## 디자인 제약

- **Windows 전용** (PowerShell + WinForms)
- **외부 의존성 없음** — .NET 표준, git, claude CLI만 사용
- **에러는 조용히** — statusLine이 깨지지 않도록 모든 항목이 try/catch로 감싸져 있음
- **출력 인코딩 UTF-8** 고정 (한글/이모지 깨짐 방지)

## 라이선스

MIT
