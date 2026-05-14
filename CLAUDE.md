# Claude Code StatusBar

Claude Code의 하단 statusLine을 커스터마이징하기 위한 도구. PowerShell 런타임 + WinForms GUI 편집기로 구성되어 있다.

## 파일 구조

| 파일 | 역할 |
| --- | --- |
| `StatusLine.ps1` | statusLine 런타임. stdin으로 JSON을 받아 `config.json` 레이아웃에 따라 멀티라인 출력 |
| `StatusBarConfig.ps1` | WinForms 기반 GUI 편집기 (드래그&드롭 레이아웃 편집) |
| `Edit-StatusBar.vbs` | GUI를 콘솔 없이 띄우는 런처 |
| `config.json` | 레이아웃 정의 (`rows` → 각 줄의 항목 배열) |
| `usage_config.json` | 플랜별 5시간/주간 한도 (pro / max5x / max20x), API 환산 USD |
| `.last_input.json` | 직전 호출에서 받은 stdin JSON (디버깅용 자동 저장) |
| `.usage_cache.json` | `~/.claude/projects/**/*.jsonl` 집계 결과 캐시 (60초 TTL) |
| `.version_cache.txt` | `claude --version` 결과 캐시 (24시간 TTL) |

## 동작 방식

1. Claude Code가 매 stdin마다 `StatusLine.ps1`을 실행하면서 세션 정보 JSON을 표준입력으로 전달
2. `StatusLine.ps1`이 stdin을 파싱해 `$ctx` 전역으로 보관 → `config.json`의 `rows`를 순회하며 항목별 렌더링
3. 줄(row) 단위로 `Write-Output`하므로 출력된 각 라인이 statusLine의 한 줄이 된다

## stdin JSON 주요 필드

`.last_input.json`을 참고하면 실제 형태를 확인할 수 있다. 주요 경로:

- `model.id`, `model.display_name`, `version`, `session_id`
- `workspace.current_dir`, `workspace.project_dir`
- `context_window.context_window_size`, `context_window.used_percentage`
- `cost.total_cost_usd`, `cost.total_duration_ms`
- `effort.level`
- `rate_limits.five_hour.used_percentage`, `rate_limits.seven_day.used_percentage`
- `transcript_path`

필드 접근은 `Get-Field`(문자열, 기본값) 또는 `Get-FieldRaw`(raw 값, null 가능) 헬퍼를 사용한다.

## 항목 추가/수정 절차

새로운 항목 타입을 추가할 때는 **반드시 두 파일을 동시에 수정**해야 한다:

1. **`StatusLine.ps1`** — `# ----- Render -----` 섹션의 `switch ($it.type)`에 case 추가
2. **`StatusBarConfig.ps1`** — `$ItemTypes` 해시테이블에 동일한 키로 `Label`, `Sample`, `Desc`, `Cat` 등록

`Cat`은 GUI 팔레트의 탭 분류 (`Claude`, `워크스페이스`, `Git`, `시간`, `시스템`, `사용률`, `아이콘`, `구분자/포맷`).

값이 null일 때 `?` 같은 placeholder 대신 `0` 또는 빈 문자열을 쓰는 게 기존 패턴 (예: `ctx_pct`, `ctx_bar`).

## 아이콘 시스템

`icon_*` 타입은 같은 카테고리 항목들을 한 줄에 묶을 때 사용하는 독립 항목이다.
한 줄에 `ctx_bar`와 `ctx_pct`를 같이 쓸 때 아이콘은 `icon_ctx` 하나만 두면 된다.

- `StatusLine.ps1`의 `$script:Icons` 해시테이블에 `<key> → 글리프` 매핑 정의
- `switch`에서 `{ $_ -like 'icon_*' }` 케이스가 키를 잘라내 매핑 조회 후 출력
- 새 아이콘 추가 시: `$script:Icons`에 키/글리프 추가 + `$ItemTypes`에 `icon_<key>` 항목 등록 (`Cat = '아이콘'`)

기본 글리프는 이모지를 사용한다 (폰트 의존성 없음). Nerd Font 글리프로 바꾸려면 `$script:Icons`만 교체하면 된다.

### VS16 (Variation Selector-16, U+FE0F)

Unicode 이모지는 두 종류로 갈린다:

- **emoji_presentation = Yes**: 그냥 써도 2칸 폭 컬러 이모지로 렌더링 (예: 🤖 🧠 📁)
- **emoji_presentation = No**: 그냥 쓰면 1칸 폭 흑백 텍스트 글리프로 렌더링됨. 옆에 공백을 줘도 다음 문자와 시각적으로 붙어 보임 (예: ⏱ 🗓 🖥 🏷)

후자 그룹은 글리프 뒤에 `U+FE0F`(VS16)를 붙여 2칸 폭 이모지로 강제해야 한다. 예: `'⏱'` → `'⏱️'`.

새 아이콘을 추가할 때는 Unicode 공식 emoji-data.txt의 `Emoji_Presentation` 속성을 확인하고, `No`라면 VS16을 함께 붙여야 statusLine에서 일정한 간격을 유지한다.

현재 VS16이 적용된 키: `version`, `duration`, `host`, `h5`, `week`.

## 설치/적용

GUI에서 **"저장 & 적용"** 버튼을 누르면:

1. `config.json`에 레이아웃 저장
2. `~/.claude/settings.json`의 `statusLine`을 `powershell -NoProfile -ExecutionPolicy Bypass -File <StatusLine.ps1>` 으로 갱신

수동 적용 시에도 동일한 `statusLine` 객체를 `~/.claude/settings.json`에 넣어주면 된다.

## 디자인 제약

- **Windows 전용**: PowerShell 5+ 및 WinForms 기반
- **외부 의존성 없음**: 표준 .NET, git, claude CLI만 사용
- **에러는 조용히**: `$ErrorActionPreference = 'SilentlyContinue'`, 대부분 try/catch로 감싸 statusLine이 깨지지 않도록 함
- **출력 인코딩 UTF-8** 고정 (한글/이모지 깨짐 방지)
