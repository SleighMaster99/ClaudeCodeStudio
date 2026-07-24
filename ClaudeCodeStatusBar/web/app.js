"use strict";

// 항목 카탈로그 (Editor/Models/ItemCatalog.cs 승계). {key, name, ex, desc, cat}
var CATALOG = [
  // Claude
  { key: "model", name: "모델명", ex: "Opus 4.7", desc: "현재 모델 표시명", cat: "Claude" },
  { key: "version", name: "버전", ex: "2.5.1", desc: "Claude Code 버전", cat: "Claude" },
  { key: "session", name: "세션ID", ex: "abc123", desc: "세션 ID 앞 8자", cat: "Claude" },
  { key: "duration", name: "duration", ex: "12.3s", desc: "세션 누적 작업 시간", cat: "Claude" },
  { key: "plan", name: "플랜", ex: "max20x", desc: "감지된 구독 플랜", cat: "Claude" },
  { key: "ctx_size", name: "컨텍스트 크기", ex: "1M", desc: "컨텍스트 윈도우 크기", cat: "Claude" },
  { key: "effort", name: "effort", ex: "xhigh", desc: "thinking effort 수준", cat: "Claude" },
  // 워크스페이스
  { key: "dir_short", name: "현재폴더", ex: "project", desc: "현재 폴더 이름만", cat: "워크스페이스" },
  { key: "dir_full", name: "전체경로", ex: "D:\\Repo\\project", desc: "현재 폴더 전체 경로", cat: "워크스페이스" },
  { key: "project_dir", name: "프로젝트 폴더", ex: "D:\\Repo", desc: "Claude Code 실행 폴더", cat: "워크스페이스" },
  // Git
  { key: "git_branch", name: "브랜치", ex: "main", desc: "현재 git 브랜치", cat: "Git" },
  { key: "git_commit", name: "커밋(short)", ex: "a1b2c3d", desc: "HEAD 커밋 짧은 해시", cat: "Git" },
  { key: "git_changes", name: "변경파일수", ex: "3", desc: "변경된 파일 개수", cat: "Git" },
  { key: "git_ahead_behind", name: "ahead/behind", ex: "↑1 ↓0", desc: "원격과의 차이", cat: "Git" },
  { key: "git_user", name: "작업자", ex: "dh.kim", desc: "git user.name", cat: "Git" },
  // 시간
  { key: "time", name: "시각", ex: "14:30", desc: "HH:mm", cat: "시간" },
  { key: "date", name: "날짜", ex: "2026-05-02", desc: "yyyy-MM-dd", cat: "시간" },
  { key: "weekday", name: "요일", ex: "토", desc: "한글 요일 한 글자", cat: "시간" },
  { key: "session_elapsed", name: "경과시간", ex: "00:23", desc: "세션 작업 경과 시간", cat: "시간" },
  // 시스템
  { key: "user", name: "사용자", ex: "dh.kim", desc: "Windows 사용자명", cat: "시스템" },
  { key: "host", name: "호스트", ex: "PC-01", desc: "컴퓨터 이름", cat: "시스템" },
  { key: "os", name: "OS", ex: "Win11", desc: "OS 버전 약식", cat: "시스템" },
  // 사용률
  { key: "ctx_pct", name: "컨텍스트 %", ex: "12%", desc: "컨텍스트 사용률", cat: "사용률" },
  { key: "ctx_bar", name: "컨텍스트 막대 █", ex: "██▒▒▒▒▒▒▒▒", desc: "컨텍스트 블록 그래프", cat: "사용률" },
  { key: "ctx_bar_ascii", name: "컨텍스트 막대 [#]", ex: "[#---------]", desc: "컨텍스트 ASCII 그래프", cat: "사용률" },
  { key: "ctx_bar_dot", name: "컨텍스트 막대 ●", ex: "●○○○○○○○○○", desc: "컨텍스트 점 그래프", cat: "사용률" },
  { key: "h5_pct", name: "5시간 %", ex: "34%", desc: "5시간 한도 사용률", cat: "사용률" },
  { key: "h5_bar", name: "5시간 막대 █", ex: "███▒▒▒▒▒▒▒", desc: "5시간 블록 그래프", cat: "사용률" },
  { key: "h5_bar_ascii", name: "5시간 막대 [#]", ex: "[###-------]", desc: "5시간 ASCII 그래프", cat: "사용률" },
  { key: "h5_bar_dot", name: "5시간 막대 ●", ex: "●●●○○○○○○○", desc: "5시간 점 그래프", cat: "사용률" },
  { key: "week_pct", name: "주간 %", ex: "8%", desc: "주간 한도 사용률", cat: "사용률" },
  { key: "week_bar", name: "주간 막대 █", ex: "█▒▒▒▒▒▒▒▒▒", desc: "주간 블록 그래프", cat: "사용률" },
  { key: "week_bar_ascii", name: "주간 막대 [#]", ex: "[#---------]", desc: "주간 ASCII 그래프", cat: "사용률" },
  { key: "week_bar_dot", name: "주간 막대 ●", ex: "●○○○○○○○○○", desc: "주간 점 그래프", cat: "사용률" },
  { key: "fable_pct", name: "페이블 %", ex: "12%", desc: "페이블 주간 한도 사용률", cat: "사용률" },
  { key: "fable_bar", name: "페이블 막대 █", ex: "█▒▒▒▒▒▒▒▒▒", desc: "페이블 주간 블록 그래프", cat: "사용률" },
  { key: "fable_bar_ascii", name: "페이블 막대 [#]", ex: "[#---------]", desc: "페이블 주간 ASCII 그래프", cat: "사용률" },
  { key: "fable_bar_dot", name: "페이블 막대 ●", ex: "●○○○○○○○○○", desc: "페이블 주간 점 그래프", cat: "사용률" },
  { key: "ctx_tokens", name: "컨텍스트 토큰", ex: "123k/1000k", desc: "사용/전체 토큰 수", cat: "사용률" },
  { key: "ctx_cost", name: "컨텍스트 비용", ex: "$0.05", desc: "현재 세션 비용", cat: "사용률" },
  { key: "h5_cost", name: "5시간 비용", ex: "$1.20", desc: "최근 5시간 사용 비용", cat: "사용률" },
  { key: "week_cost", name: "주간 비용", ex: "$15.30", desc: "최근 7일 사용 비용", cat: "사용률" },
  { key: "h5_remain", name: "5시간 남은시간", ex: "~4h13m", desc: "5시간 초기화까지 남은시간", cat: "사용률" },
  { key: "week_remain", name: "주간 남은시간", ex: "~3d05h", desc: "주간 초기화까지 남은시간", cat: "사용률" },
  { key: "fable_remain", name: "페이블 남은시간", ex: "~1d07h", desc: "페이블 주간 초기화까지 남은시간", cat: "사용률" },
  // 아이콘
  { key: "icon_model", name: "모델 아이콘", ex: "🤖", desc: "모델 관련 항목 앞", cat: "아이콘" },
  { key: "icon_version", name: "버전 아이콘", ex: "🏷️", desc: "버전 항목 앞", cat: "아이콘" },
  { key: "icon_session", name: "세션 아이콘", ex: "🔖", desc: "세션 ID 앞", cat: "아이콘" },
  { key: "icon_plan", name: "플랜 아이콘", ex: "⭐", desc: "플랜 항목 앞", cat: "아이콘" },
  { key: "icon_ctx", name: "컨텍스트 아이콘", ex: "🧠", desc: "컨텍스트 관련 항목 앞", cat: "아이콘" },
  { key: "icon_effort", name: "effort 아이콘", ex: "💭", desc: "thinking effort 앞", cat: "아이콘" },
  { key: "icon_cost", name: "비용 아이콘", ex: "💰", desc: "비용 관련 항목 앞", cat: "아이콘" },
  { key: "icon_duration", name: "소요시간 아이콘", ex: "⏱️", desc: "duration/경과시간 앞", cat: "아이콘" },
  { key: "icon_dir", name: "폴더 아이콘", ex: "📁", desc: "경로/폴더 항목 앞", cat: "아이콘" },
  { key: "icon_git", name: "Git 아이콘", ex: "🌿", desc: "git 관련 항목 앞", cat: "아이콘" },
  { key: "icon_time", name: "시각 아이콘", ex: "🕐", desc: "시각 항목 앞", cat: "아이콘" },
  { key: "icon_date", name: "날짜 아이콘", ex: "📅", desc: "날짜/요일 항목 앞", cat: "아이콘" },
  { key: "icon_user", name: "사용자 아이콘", ex: "👤", desc: "사용자 관련 항목 앞", cat: "아이콘" },
  { key: "icon_host", name: "호스트 아이콘", ex: "🖥️", desc: "호스트 항목 앞", cat: "아이콘" },
  { key: "icon_os", name: "OS 아이콘", ex: "💻", desc: "OS 항목 앞", cat: "아이콘" },
  { key: "icon_h5", name: "5시간 아이콘", ex: "⏱️", desc: "5시간 사용률 항목 앞", cat: "아이콘" },
  { key: "icon_week", name: "주간 아이콘", ex: "🗓️", desc: "주간 사용률 항목 앞", cat: "아이콘" },
  { key: "icon_fable", name: "페이블 아이콘", ex: "🔮", desc: "페이블 사용률 항목 앞", cat: "아이콘" },
  // 구분자/포맷
  { key: "sep_pipe", name: "파이프", ex: "|", desc: "구분 기호", cat: "구분자/포맷" },
  { key: "sep_dot", name: "가운뎃점", ex: "•", desc: "구분 기호", cat: "구분자/포맷" },
  { key: "sep_dash", name: "대시", ex: "-", desc: "구분 기호", cat: "구분자/포맷" },
  { key: "sep_arrow", name: "꺾쇠", ex: ">", desc: "구분 기호", cat: "구분자/포맷" },
  { key: "sep_slash", name: "슬래시", ex: "/", desc: "구분 기호", cat: "구분자/포맷" },
  { key: "sep_colon", name: "콜론", ex: ":", desc: "구분 기호", cat: "구분자/포맷" },
  { key: "space", name: "공백", ex: "·", desc: "한 칸 공백", cat: "구분자/포맷" },
  { key: "text", name: "텍스트", ex: "TEXT", desc: "사용자 지정 텍스트", cat: "구분자/포맷" }
];
var CATEGORIES = ["Claude", "워크스페이스", "Git", "시간", "시스템", "사용률", "아이콘", "구분자/포맷"];
var CAT_OF = {};
CATALOG.forEach(function (c) { CAT_OF[c.key] = c; });

// 카테고리별 배경색 (ItemCatalog.GetBackground 승계)
function bgFor(type) {
  if (type === "space") return "#EEEEEE";
  if (type === "text") return "#DCF5DC";
  if (type.indexOf("sep_") === 0) return "#FAF0D7";
  var c = CAT_OF[type];
  switch (c ? c.cat : "") {
    case "Claude": return "#D6E4F8";
    case "워크스페이스": return "#FFE6CC";
    case "Git": return "#F5D8E0";
    case "시간": return "#E0DBF5";
    case "시스템": return "#E0E0E0";
    case "사용률": return "#CCE8DC";
    case "아이콘": return "#FFF0B8";
    default: return "#EEEEEE";
  }
}

var state = { rows: [] };
var $ = function (id) { return document.getElementById(id); };

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

// 미리보기 — 각 항목의 예시값(Sample)을 이어붙여 실제 statusLine 모양 근사 (RefreshLivePreview 승계)
function refreshPreview() {
  var lines = state.rows.map(function (row) {
    return row.map(function (it) {
      if (it.type === "text") return it.value || "";
      if (it.type === "space") return " ";
      var c = CAT_OF[it.type];
      return c ? c.ex : "";
    }).join("");
  });
  var t = lines.join("\n");
  $("preview").textContent = (t.trim().length > 0) ? t : "(레이아웃에 항목을 추가하세요)";
}

function renderPalette() {
  var body = $("paletteBody");
  body.innerHTML = "";
  CATEGORIES.forEach(function (cat) {
    var items = CATALOG.filter(function (c) { return c.cat === cat; });
    if (!items.length) return;
    var grp = document.createElement("div"); grp.className = "pal-group";

    // 카테고리 헤더 — 클릭 시 접기/펼치기 토글
    var h = document.createElement("button"); h.className = "pal-cat"; h.type = "button";
    var arrow = document.createElement("span"); arrow.className = "pal-arrow"; arrow.textContent = "▾";
    var label = document.createElement("span"); label.className = "pal-cat-label"; label.textContent = cat;
    var count = document.createElement("span"); count.className = "pal-count"; count.textContent = "(" + items.length + ")";
    h.append(arrow, label, count);
    h.addEventListener("click", function () { grp.classList.toggle("collapsed"); });
    grp.appendChild(h);

    var itemsEl = document.createElement("div"); itemsEl.className = "pal-items";
    items.forEach(function (c) {
      var chip = document.createElement("div");
      chip.className = "pal-chip" + (c.cat === "아이콘" ? " is-icon" : "");
      chip.draggable = true;
      chip.style.background = bgFor(c.key);
      var nm = document.createElement("div"); nm.className = "chip-name"; nm.textContent = c.name;
      var sm = document.createElement("div"); sm.className = "chip-sample"; sm.textContent = c.ex;
      var ds = document.createElement("div"); ds.className = "chip-desc"; ds.textContent = c.desc;
      chip.append(nm, sm, ds);
      chip.addEventListener("dragstart", function (e) {
        e.dataTransfer.setData("text/plain", JSON.stringify({ src: "palette", type: c.key }));
      });
      itemsEl.appendChild(chip);
    });
    grp.appendChild(itemsEl);
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
    refreshPreview();
    return;
  }
  state.rows.forEach(function (row, ri) {
    var rEl = document.createElement("div"); rEl.className = "row";
    var drop = document.createElement("div"); drop.className = "dropzone";
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
      chip.style.background = bgFor(it.type);
      var lab = document.createElement("span"); lab.className = "chip-lab"; lab.textContent = labelFor(it);
      chip.appendChild(lab);

      if (it.type === "text") {
        chip.classList.add("editable");
        chip.title = "더블클릭하여 텍스트 편집";
        chip.addEventListener("dblclick", function () {
          var v = window.prompt("표시할 텍스트:", it.value || "");
          if (v !== null) { it.value = v; renderRows(); }
        });
      }

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
  refreshPreview();
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
    if (data.ri === ri && data.ii < at) insertAt--;
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

window.addEventListener("message", function (e) {
  var msg = e.data;
  if (typeof msg === "string") { try { msg = JSON.parse(msg); } catch (_) { return; } }
  if (msg && msg.type) handle(msg);
});

$("addRow").addEventListener("click", function () { state.rows.push([]); renderRows(); });
$("reloadBtn").addEventListener("click", function () { send("load"); });
$("saveBtn").addEventListener("click", function () { send("save", collectConfig()); });
$("applyBtn").addEventListener("click", function () { send("apply", collectConfig()); });

// 팔레트 ↔ 레이아웃 너비 조절 (splitter 드래그)
(function () {
  var splitter = $("splitter");
  var palette = document.querySelector(".palette");
  var main = document.querySelector(".main");
  var dragging = false;
  splitter.addEventListener("mousedown", function (e) {
    dragging = true;
    splitter.classList.add("dragging");
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";
    e.preventDefault();
  });
  window.addEventListener("mousemove", function (e) {
    if (!dragging) return;
    var rect = main.getBoundingClientRect();
    var w = e.clientX - rect.left;
    w = Math.max(160, Math.min(w, rect.width - 220));   // 최소 팔레트 160, 최소 레이아웃 220
    palette.style.flex = "0 0 " + w + "px";
  });
  window.addEventListener("mouseup", function () {
    if (!dragging) return;
    dragging = false;
    splitter.classList.remove("dragging");
    document.body.style.cursor = "";
    document.body.style.userSelect = "";
  });
})();

renderPalette();
send("load");
