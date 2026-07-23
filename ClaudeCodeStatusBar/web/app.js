"use strict";

// 항목 카탈로그 (Editor/Models/ItemCatalog.cs 승계). {key, name, ex, cat}
var CATALOG = [
  // Claude
  { key: "model", name: "모델명", ex: "Opus 4.7", cat: "Claude" },
  { key: "version", name: "버전", ex: "2.5.1", cat: "Claude" },
  { key: "session", name: "세션ID", ex: "abc123", cat: "Claude" },
  { key: "duration", name: "duration", ex: "12.3s", cat: "Claude" },
  { key: "plan", name: "플랜", ex: "max20x", cat: "Claude" },
  { key: "ctx_size", name: "컨텍스트 크기", ex: "1M", cat: "Claude" },
  { key: "effort", name: "effort", ex: "xhigh", cat: "Claude" },
  // 워크스페이스
  { key: "dir_short", name: "현재폴더", ex: "project", cat: "워크스페이스" },
  { key: "dir_full", name: "전체경로", ex: "D:\\Repo\\project", cat: "워크스페이스" },
  { key: "project_dir", name: "프로젝트 폴더", ex: "D:\\Repo", cat: "워크스페이스" },
  // Git
  { key: "git_branch", name: "브랜치", ex: "main", cat: "Git" },
  { key: "git_commit", name: "커밋(short)", ex: "a1b2c3d", cat: "Git" },
  { key: "git_changes", name: "변경파일수", ex: "3", cat: "Git" },
  { key: "git_ahead_behind", name: "ahead/behind", ex: "↑1 ↓0", cat: "Git" },
  { key: "git_user", name: "작업자", ex: "dh.kim", cat: "Git" },
  // 시간
  { key: "time", name: "시각", ex: "14:30", cat: "시간" },
  { key: "date", name: "날짜", ex: "2026-05-02", cat: "시간" },
  { key: "weekday", name: "요일", ex: "토", cat: "시간" },
  { key: "session_elapsed", name: "경과시간", ex: "00:23", cat: "시간" },
  // 시스템
  { key: "user", name: "사용자", ex: "dh.kim", cat: "시스템" },
  { key: "host", name: "호스트", ex: "PC-01", cat: "시스템" },
  { key: "os", name: "OS", ex: "Win11", cat: "시스템" },
  // 사용률
  { key: "ctx_pct", name: "컨텍스트 %", ex: "12%", cat: "사용률" },
  { key: "ctx_bar", name: "컨텍스트 막대 █", ex: "██▒▒▒▒▒▒▒▒", cat: "사용률" },
  { key: "ctx_bar_ascii", name: "컨텍스트 막대 [#]", ex: "[#---------]", cat: "사용률" },
  { key: "ctx_bar_dot", name: "컨텍스트 막대 ●", ex: "●○○○○○○○○○", cat: "사용률" },
  { key: "h5_pct", name: "5시간 %", ex: "34%", cat: "사용률" },
  { key: "h5_bar", name: "5시간 막대 █", ex: "███▒▒▒▒▒▒▒", cat: "사용률" },
  { key: "h5_bar_ascii", name: "5시간 막대 [#]", ex: "[###-------]", cat: "사용률" },
  { key: "h5_bar_dot", name: "5시간 막대 ●", ex: "●●●○○○○○○○", cat: "사용률" },
  { key: "week_pct", name: "주간 %", ex: "8%", cat: "사용률" },
  { key: "week_bar", name: "주간 막대 █", ex: "█▒▒▒▒▒▒▒▒▒", cat: "사용률" },
  { key: "week_bar_ascii", name: "주간 막대 [#]", ex: "[#---------]", cat: "사용률" },
  { key: "week_bar_dot", name: "주간 막대 ●", ex: "●○○○○○○○○○", cat: "사용률" },
  { key: "fable_pct", name: "페이블 %", ex: "12%", cat: "사용률" },
  { key: "fable_bar", name: "페이블 막대 █", ex: "█▒▒▒▒▒▒▒▒▒", cat: "사용률" },
  { key: "fable_bar_ascii", name: "페이블 막대 [#]", ex: "[#---------]", cat: "사용률" },
  { key: "fable_bar_dot", name: "페이블 막대 ●", ex: "●○○○○○○○○○", cat: "사용률" },
  { key: "ctx_tokens", name: "컨텍스트 토큰", ex: "123k/1000k", cat: "사용률" },
  { key: "ctx_cost", name: "컨텍스트 비용", ex: "$0.05", cat: "사용률" },
  { key: "h5_cost", name: "5시간 비용", ex: "$1.20", cat: "사용률" },
  { key: "week_cost", name: "주간 비용", ex: "$15.30", cat: "사용률" },
  { key: "h5_remain", name: "5시간 남은시간", ex: "~4h13m", cat: "사용률" },
  { key: "week_remain", name: "주간 남은시간", ex: "~3d05h", cat: "사용률" },
  { key: "fable_remain", name: "페이블 남은시간", ex: "~1d07h", cat: "사용률" },
  // 아이콘
  { key: "icon_model", name: "모델 아이콘", ex: "🤖", cat: "아이콘" },
  { key: "icon_version", name: "버전 아이콘", ex: "🏷️", cat: "아이콘" },
  { key: "icon_session", name: "세션 아이콘", ex: "🔖", cat: "아이콘" },
  { key: "icon_plan", name: "플랜 아이콘", ex: "⭐", cat: "아이콘" },
  { key: "icon_ctx", name: "컨텍스트 아이콘", ex: "🧠", cat: "아이콘" },
  { key: "icon_effort", name: "effort 아이콘", ex: "💭", cat: "아이콘" },
  { key: "icon_cost", name: "비용 아이콘", ex: "💰", cat: "아이콘" },
  { key: "icon_duration", name: "소요시간 아이콘", ex: "⏱️", cat: "아이콘" },
  { key: "icon_dir", name: "폴더 아이콘", ex: "📁", cat: "아이콘" },
  { key: "icon_git", name: "Git 아이콘", ex: "🌿", cat: "아이콘" },
  { key: "icon_time", name: "시각 아이콘", ex: "🕐", cat: "아이콘" },
  { key: "icon_date", name: "날짜 아이콘", ex: "📅", cat: "아이콘" },
  { key: "icon_user", name: "사용자 아이콘", ex: "👤", cat: "아이콘" },
  { key: "icon_host", name: "호스트 아이콘", ex: "🖥️", cat: "아이콘" },
  { key: "icon_os", name: "OS 아이콘", ex: "💻", cat: "아이콘" },
  { key: "icon_h5", name: "5시간 아이콘", ex: "⏱️", cat: "아이콘" },
  { key: "icon_week", name: "주간 아이콘", ex: "🗓️", cat: "아이콘" },
  { key: "icon_fable", name: "페이블 아이콘", ex: "🔮", cat: "아이콘" },
  // 구분자/포맷
  { key: "sep_pipe", name: "파이프", ex: "|", cat: "구분자/포맷" },
  { key: "sep_dot", name: "가운뎃점", ex: "•", cat: "구분자/포맷" },
  { key: "sep_dash", name: "대시", ex: "-", cat: "구분자/포맷" },
  { key: "sep_arrow", name: "꺾쇠", ex: ">", cat: "구분자/포맷" },
  { key: "sep_slash", name: "슬래시", ex: "/", cat: "구분자/포맷" },
  { key: "sep_colon", name: "콜론", ex: ":", cat: "구분자/포맷" },
  { key: "space", name: "공백", ex: "·", cat: "구분자/포맷" },
  { key: "text", name: "텍스트", ex: "TEXT", cat: "구분자/포맷" }
];
var CATEGORIES = ["Claude", "워크스페이스", "Git", "시간", "시스템", "사용률", "아이콘", "구분자/포맷"];
var CAT_OF = {};
CATALOG.forEach(function (c) { CAT_OF[c.key] = c; });

var state = { rows: [] };
var $ = function (id) { return document.getElementById(id); };

// 부모(Core 셸) 경유로 C++ 모듈과 통신
function send(cmd, config) {
  var m = { module: "statusbar", cmd: cmd };
  if (config) m.config = config;
  window.parent.postMessage(m, "*");
}

var toastTimer = null;
function showToast(text, ok) {
  var e = $("toast");
  e.textContent = text;
  e.className = "toast " + (ok ? "ok" : "err");
  e.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(function () { e.hidden = true; }, 4000);
}

function labelFor(it) {
  if (it.type === "text") return it.value || "TEXT";
  var c = CAT_OF[it.type];
  return c ? c.name : it.type;
}

function renderPalette() {
  var body = $("paletteBody");
  body.innerHTML = "";
  CATEGORIES.forEach(function (cat) {
    var items = CATALOG.filter(function (c) { return c.cat === cat; });
    if (!items.length) return;
    var grp = document.createElement("div"); grp.className = "pal-group";
    var h = document.createElement("div"); h.className = "pal-cat"; h.textContent = cat;
    grp.appendChild(h);
    items.forEach(function (c) {
      var chip = document.createElement("div");
      chip.className = "pal-chip"; chip.draggable = true;
      chip.textContent = c.name; chip.title = c.ex || "";
      chip.addEventListener("dragstart", function (e) {
        e.dataTransfer.setData("text/plain", JSON.stringify({ src: "palette", type: c.key }));
      });
      grp.appendChild(chip);
    });
    body.appendChild(grp);
  });
}

function renderRows() {
  var host = $("rows");
  host.innerHTML = "";
  if (!state.rows.length) {
    var hint = document.createElement("div");
    hint.className = "empty-hint";
    hint.textContent = "줄이 없습니다. '+ 줄 추가' 로 시작하세요.";
    host.appendChild(hint);
    return;
  }
  state.rows.forEach(function (row, ri) {
    var rEl = document.createElement("div"); rEl.className = "row";
    var drop = document.createElement("div"); drop.className = "dropzone"; drop.dataset.ri = ri;
    drop.addEventListener("dragover", function (e) { e.preventDefault(); drop.classList.add("over"); });
    drop.addEventListener("dragleave", function () { drop.classList.remove("over"); });
    drop.addEventListener("drop", function (e) {
      e.preventDefault(); drop.classList.remove("over");
      handleDrop(e, ri, state.rows[ri].length);
    });

    if (!row.length) {
      var eh = document.createElement("span"); eh.className = "empty-hint"; eh.textContent = "여기로 항목을 끌어다 놓기";
      drop.appendChild(eh);
    }
    row.forEach(function (it, ii) {
      var chip = document.createElement("span"); chip.className = "chip"; chip.draggable = true;
      var lab = document.createElement("span"); lab.textContent = labelFor(it); chip.appendChild(lab);
      var del = document.createElement("button"); del.className = "chip-del"; del.textContent = "×"; del.title = "삭제";
      del.addEventListener("click", function () { state.rows[ri].splice(ii, 1); renderRows(); });
      chip.appendChild(del);
      chip.addEventListener("dragstart", function (e) {
        chip.classList.add("dragging");
        e.dataTransfer.setData("text/plain", JSON.stringify({ src: "canvas", ri: ri, ii: ii }));
      });
      chip.addEventListener("dragend", function () { chip.classList.remove("dragging"); });
      chip.addEventListener("dragover", function (e) { e.preventDefault(); });
      chip.addEventListener("drop", function (e) { e.preventDefault(); e.stopPropagation(); handleDrop(e, ri, ii); });
      drop.appendChild(chip);
    });
    rEl.appendChild(drop);

    var rdel = document.createElement("button"); rdel.className = "row-del"; rdel.textContent = "줄 삭제";
    rdel.addEventListener("click", function () { state.rows.splice(ri, 1); renderRows(); });
    rEl.appendChild(rdel);
    host.appendChild(rEl);
  });
}

function handleDrop(e, ri, at) {
  var data;
  try { data = JSON.parse(e.dataTransfer.getData("text/plain")); } catch (_) { return; }
  if (data.src === "palette") {
    var it = { type: data.type };
    if (data.type === "text") it.value = "TEXT";
    state.rows[ri].splice(at, 0, it);
  } else if (data.src === "canvas") {
    var moved = state.rows[data.ri][data.ii];
    if (!moved) return;
    state.rows[data.ri].splice(data.ii, 1);
    var insertAt = at;
    if (data.ri === ri && data.ii < at) insertAt--;   // 같은 줄에서 앞 항목 제거 보정
    state.rows[ri].splice(insertAt, 0, moved);
  }
  renderRows();
}

function collectConfig() {
  return {
    rows: state.rows.map(function (row) {
      return row.map(function (it) {
        return (it.value != null) ? { type: it.type, value: it.value } : { type: it.type };
      });
    })
  };
}

function handle(msg) {
  if (msg.type === "config") {
    var rows = (msg.config && msg.config.rows) ? msg.config.rows : [];
    state.rows = rows.map(function (row) {
      return (row || []).map(function (it) {
        return (it.value != null) ? { type: it.type, value: it.value } : { type: it.type };
      });
    });
    renderRows();
  } else if (msg.type === "result") {
    showToast(msg.message + (msg.detail ? " — " + msg.detail : ""), msg.ok);
  }
}

// 부모(Core 셸)가 중계한 C++ 결과 수신
window.addEventListener("message", function (e) {
  var msg = e.data;
  if (typeof msg === "string") { try { msg = JSON.parse(msg); } catch (_) { return; } }
  if (msg && msg.type) handle(msg);
});

// wiring
$("addRow").addEventListener("click", function () { state.rows.push([]); renderRows(); });
$("reloadBtn").addEventListener("click", function () { send("load"); });
$("saveBtn").addEventListener("click", function () { send("save", collectConfig()); });
$("applyBtn").addEventListener("click", function () { send("apply", collectConfig()); });

renderPalette();
send("load");
