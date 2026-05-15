using System.Collections.Generic;

namespace StatusBarEditor.Models;

public static class ItemCatalog
{
    public const string SeparatorCategory = "구분자/포맷";

    public static readonly string[] CategoryOrder =
    {
        "Claude", "워크스페이스", "Git", "시간", "시스템", "사용률", "아이콘"
    };

    public static readonly IReadOnlyList<ItemTypeInfo> All = new ItemTypeInfo[]
    {
        // Claude
        new("model",       "모델명",        "Opus 4.7",        "현재 모델 표시명",        "Claude"),
        new("version",     "버전",          "2.5.1",           "Claude Code 버전",        "Claude"),
        new("session",     "세션ID",        "abc123",          "세션 ID 앞 8자",          "Claude"),
        new("duration",    "duration",      "12.3s",           "세션 누적 작업 시간",     "Claude"),
        new("plan",        "플랜",          "max20x",          "감지된 구독 플랜",        "Claude"),
        new("ctx_size",    "컨텍스트 크기", "1M",              "컨텍스트 윈도우 크기",    "Claude"),
        new("effort",      "effort",        "xhigh",           "thinking effort 수준",    "Claude"),
        // 워크스페이스
        new("dir_short",   "현재폴더",      "project",         "현재 폴더 이름만",        "워크스페이스"),
        new("dir_full",    "전체경로",      @"D:\Repo\project","현재 폴더 전체 경로",     "워크스페이스"),
        new("project_dir", "프로젝트 폴더", @"D:\Repo",        "Claude Code 실행 폴더",   "워크스페이스"),
        // Git
        new("git_branch",  "브랜치",        "main",            "현재 git 브랜치",         "Git"),
        new("git_commit",  "커밋(short)",   "a1b2c3d",         "HEAD 커밋 짧은 해시",     "Git"),
        new("git_changes", "변경파일수",    "3",               "변경된 파일 개수",        "Git"),
        new("git_ahead_behind","ahead/behind","↑1 ↓0",         "원격과의 차이",           "Git"),
        new("git_user",    "작업자",        "dh.kim",          "git user.name",           "Git"),
        // 시간
        new("time",        "시각",          "14:30",           "HH:mm",                   "시간"),
        new("date",        "날짜",          "2026-05-02",      "yyyy-MM-dd",              "시간"),
        new("weekday",     "요일",          "토",              "한글 요일 한 글자",       "시간"),
        new("session_elapsed","경과시간",   "00:23",           "세션 작업 경과 시간",     "시간"),
        // 시스템
        new("user",        "사용자",        "dh.kim",          "Windows 사용자명",        "시스템"),
        new("host",        "호스트",        "PC-01",           "컴퓨터 이름",             "시스템"),
        new("os",          "OS",            "Win11",           "OS 버전 약식",            "시스템"),
        // 사용률
        new("ctx_pct",         "컨텍스트 %",         "12%",            "컨텍스트 사용률",         "사용률"),
        new("ctx_bar",         "컨텍스트 막대 █",    "██▒▒▒▒▒▒▒▒",     "컨텍스트 블록 그래프",    "사용률"),
        new("ctx_bar_ascii",   "컨텍스트 막대 [#]",  "[#---------]",   "컨텍스트 ASCII 그래프",   "사용률"),
        new("ctx_bar_dot",     "컨텍스트 막대 ●",    "●○○○○○○○○○",     "컨텍스트 점 그래프",      "사용률"),
        new("h5_pct",          "5시간 %",            "34%",            "5시간 한도 사용률",       "사용률"),
        new("h5_bar",          "5시간 막대 █",       "███▒▒▒▒▒▒▒",     "5시간 블록 그래프",       "사용률"),
        new("h5_bar_ascii",    "5시간 막대 [#]",     "[###-------]",   "5시간 ASCII 그래프",      "사용률"),
        new("h5_bar_dot",      "5시간 막대 ●",       "●●●○○○○○○○",     "5시간 점 그래프",         "사용률"),
        new("week_pct",        "주간 %",             "8%",             "주간 한도 사용률",        "사용률"),
        new("week_bar",        "주간 막대 █",        "█▒▒▒▒▒▒▒▒▒",     "주간 블록 그래프",        "사용률"),
        new("week_bar_ascii",  "주간 막대 [#]",      "[#---------]",   "주간 ASCII 그래프",       "사용률"),
        new("week_bar_dot",    "주간 막대 ●",        "●○○○○○○○○○",     "주간 점 그래프",          "사용률"),
        new("ctx_tokens",      "컨텍스트 토큰",      "123k/1000k",     "사용/전체 토큰 수",           "사용률"),
        new("ctx_cost",    "컨텍스트 비용", "$0.05",           "현재 세션 비용",          "사용률"),
        new("h5_cost",     "5시간 비용",    "$1.20",           "최근 5시간 사용 비용",    "사용률"),
        new("week_cost",   "주간 비용",     "$15.30",          "최근 7일 사용 비용",      "사용률"),
        new("h5_remain",       "5시간 남은시간",      "~4h13m",         "5시간 초기화까지 남은시간","사용률"),
        new("week_remain",     "주간 남은시간",       "~3d05h",         "주간 초기화까지 남은시간", "사용률"),
        // 아이콘
        new("icon_model",   "모델 아이콘",     "🤖",  "모델 관련 항목 앞",     "아이콘"),
        new("icon_version", "버전 아이콘",     "🏷️","버전 항목 앞",       "아이콘"),
        new("icon_session", "세션 아이콘",     "🔖",  "세션 ID 앞",            "아이콘"),
        new("icon_plan",    "플랜 아이콘",     "⭐",  "플랜 항목 앞",          "아이콘"),
        new("icon_ctx",     "컨텍스트 아이콘", "🧠",  "컨텍스트 관련 항목 앞", "아이콘"),
        new("icon_effort",  "effort 아이콘",   "💭",  "thinking effort 앞",    "아이콘"),
        new("icon_cost",    "비용 아이콘",     "💰",  "비용 관련 항목 앞",     "아이콘"),
        new("icon_duration","소요시간 아이콘", "⏱️","duration/경과시간 앞","아이콘"),
        new("icon_dir",     "폴더 아이콘",     "📁",  "경로/폴더 항목 앞",     "아이콘"),
        new("icon_git",     "Git 아이콘",      "🌿",  "git 관련 항목 앞",      "아이콘"),
        new("icon_time",    "시각 아이콘",     "🕐",  "시각 항목 앞",          "아이콘"),
        new("icon_date",    "날짜 아이콘",     "📅",  "날짜/요일 항목 앞",     "아이콘"),
        new("icon_user",    "사용자 아이콘",   "👤",  "사용자 관련 항목 앞",   "아이콘"),
        new("icon_host",    "호스트 아이콘",   "🖥️","호스트 항목 앞",     "아이콘"),
        new("icon_os",      "OS 아이콘",       "💻",  "OS 항목 앞",            "아이콘"),
        new("icon_h5",      "5시간 아이콘",    "⏱️","5시간 사용률 항목 앞","아이콘"),
        new("icon_week",    "주간 아이콘",     "🗓️","주간 사용률 항목 앞","아이콘"),
        // 구분자/포맷
        new("sep_pipe",    "파이프",       "|",    "구분 기호",          SeparatorCategory),
        new("sep_dot",     "가운뎃점",     "•",    "구분 기호",          SeparatorCategory),
        new("sep_dash",    "대시",         "-",    "구분 기호",          SeparatorCategory),
        new("sep_arrow",   "꺾쇠",         ">",    "구분 기호",          SeparatorCategory),
        new("sep_slash",   "슬래시",       "/",    "구분 기호",          SeparatorCategory),
        new("sep_colon",   "콜론",         ":",    "구분 기호",          SeparatorCategory),
        new("space",       "공백",         "·",    "한 칸 공백",         SeparatorCategory),
        new("text",        "텍스트",       "TEXT", "사용자 지정 텍스트", SeparatorCategory),
    };

    private static readonly Dictionary<string, ItemTypeInfo> _byKey = BuildIndex();
    private static Dictionary<string, ItemTypeInfo> BuildIndex()
    {
        var d = new Dictionary<string, ItemTypeInfo>(All.Count);
        foreach (var info in All) d[info.Key] = info;
        return d;
    }

    public static ItemTypeInfo? Find(string key) => _byKey.TryGetValue(key, out var v) ? v : null;
}
