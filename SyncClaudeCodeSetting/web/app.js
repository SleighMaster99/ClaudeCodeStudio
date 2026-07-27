"use strict";

// iframe 안에서 동작. C++ 호스트와는 부모(Core 셸)를 통해 중계로 통신한다.
//   보내기: window.parent.postMessage({module,cmd,arg})  -> 부모가 chrome.webview 로 전달
//   받기:   부모가 C++ 결과(JSON)를 이 iframe 으로 postMessage -> message 이벤트

// 테마를 로드 직후 적용 (셸 설정과 같은 출처 localStorage 공유 — 늦은 주입으로 인한 깜빡임 방지)
try {
  var __t = JSON.parse(localStorage.getItem("ccs.ui.settings.v1"));
  document.documentElement.setAttribute("data-theme", (__t && __t.theme === "dark") ? "dark" : "light");
} catch (_) {}

function send(cmd, arg) {
  window.parent.postMessage({ module: "sync", cmd: cmd, arg: arg || "" }, "*");
}

// ----- 앱 설정 (셸 설정 탭이 저장, 여기서 소비) -----
var SYNC_KEY = "ccs.sync.settings.v1";
function normalizeSettings(s) {
  var out = { autoMin: 0, startupFetch: false, histCount: 8, commitMsg: "", repoUrl: "" };
  if (s) {
    if ([1, 5, 15, 30].indexOf(+s.autoMin) >= 0) out.autoMin = +s.autoMin;
    out.startupFetch = !!s.startupFetch;
    if (s.histCount != null) out.histCount = Math.min(50, Math.max(1, Math.round(+s.histCount) || 8));
    if (typeof s.commitMsg === "string") out.commitMsg = s.commitMsg;
    if (typeof s.repoUrl === "string") out.repoUrl = s.repoUrl.trim();
  }
  return out;
}
function loadAppSettings() {
  try { return normalizeSettings(JSON.parse(localStorage.getItem(SYNC_KEY))); }
  catch (_) { return normalizeSettings(null); }
}
var appSettings = loadAppSettings();
var isConfigured = null;   // 마지막 status 의 configured (자동 새로고침 가드)

// C++ sync 모듈에 설정 전달 — 이후 log/push/bootstrap 명령부터 반영된다
function sendConfigure() {
  window.parent.postMessage({
    module: "sync", cmd: "configure",
    logCount: String(appSettings.histCount),
    commitMsg: appSettings.commitMsg,
    defaultRepo: appSettings.repoUrl
  }, "*");
}

var autoTimer = null;
function applyAutoRefresh() {
  clearInterval(autoTimer);
  autoTimer = null;
  if (appSettings.autoMin > 0) {
    autoTimer = setInterval(function () {
      if (isConfigured) send("refresh");   // 미설정(초기 설정 화면)이면 건너뜀
    }, appSettings.autoMin * 60000);
  }
}

const $ = (id) => document.getElementById(id);
let toastTimer = null;

function showToast(text, ok) {
  const t = $("toast");
  t.textContent = text;
  t.className = "toast " + (ok ? "ok" : "err");
  t.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.hidden = true; }, 4000);
}

function renderStatus(s) {
  const setup = $("setup");
  const body = document.querySelector(".body");

  // 미설정(git repo 아님) → 초기 설정 화면만 노출
  if (s.configured === false) {
    setup.hidden = false;
    if (body) body.style.display = "none";
    const sb = $("setupBtn");
    if (sb) { sb.disabled = false; sb.textContent = "초기 설정 시작"; }
    return;
  }
  setup.hidden = true;
  if (body) body.style.display = "";

  const el = $("status");
  const wire = $("wire");
  const badge = $("statusBadge");
  const pull = $("pullBtn"), push = $("pushBtn");
  const pullBadge = $("pullBadge"), pushBadge = $("pushBadge");

  $("nodeRemote").textContent = s.remoteHash ? s.remoteHash : "—";
  $("nodeLocal").textContent = s.headHash ? s.headHash : "—";
  $("repoUrlFoot").textContent = s.remote || "—";
  var ri = $("remoteUrl");
  if (ri && document.activeElement !== ri) ri.value = s.remote || "";

  const behind = s.behind | 0, ahead = s.ahead | 0;
  const dirty = !s.clean;

  el.className = "status";
  wire.className = "wire dir-none";
  badge.hidden = true;
  pull.classList.remove("primary"); push.classList.remove("primary");
  pull.disabled = true; push.disabled = true;
  pullBadge.hidden = true; pushBadge.hidden = true;

  if (behind > 0) {
    el.classList.add("is-warn");
    $("statusTitle").textContent = "동기화 필요";
    $("statusSub").textContent = "서버에 새 변경 " + behind + "건";
    badge.textContent = "주의"; badge.hidden = false;
    wire.className = "wire dir-pull";
    $("wireLabel").textContent = "가져오기";
    pull.disabled = false; pull.classList.add("primary");
    pullBadge.textContent = behind; pullBadge.hidden = false;
    $("pullSub").textContent = "새 변경 " + behind + "건 가져오기";
  } else if (ahead > 0 || dirty) {
    el.classList.add("is-push");
    $("statusTitle").textContent = "반영할 변경 있음";
    $("statusSub").textContent = dirty
      ? "저장되지 않은 로컬 변경이 있습니다"
      : ahead + "건이 서버에 반영되지 않았습니다";
    wire.className = "wire dir-push";
    $("wireLabel").textContent = "반영하기";
    push.disabled = false; push.classList.add("primary");
    if (ahead > 0) { pushBadge.textContent = ahead; pushBadge.hidden = false; }
    $("pushSub").textContent = dirty ? "커밋 후 반영" : ahead + "건 반영";
  } else {
    el.classList.add("is-good");
    $("statusTitle").textContent = "최신 상태";
    $("statusSub").textContent = "모든 설정이 서버와 일치합니다";
    $("wireLabel").textContent = "동기화됨";
  }

  if (behind === 0) $("pullSub").textContent = "가져올 변경 없음";
  if (ahead === 0 && !dirty) $("pushSub").textContent = "반영할 변경 없음";
}

function renderLog(m) {
  const list = $("histList");
  list.innerHTML = "";
  const items = m.items || [];
  $("histCount").textContent = items.length ? "최근 " + items.length + "건" : "";

  for (const it of items) {
    const li = document.createElement("li");
    li.className = "hist-row";

    const time = document.createElement("span");
    time.className = "hist-time";
    time.textContent = it.when || "";

    const desc = document.createElement("span");
    desc.className = "hist-desc";
    const subj = document.createElement("span");
    subj.className = "hist-subject";
    subj.textContent = it.subject || "";
    desc.appendChild(subj);
    const isHead = it.hash === m.head;
    const isRemote = it.hash === m.remote;
    if (isHead) desc.appendChild(makeTag("현재 PC", "tag-current"));
    if (isRemote) desc.appendChild(makeTag("서버", "tag-remote"));

    const hash = document.createElement("span");
    hash.className = "hist-hash";
    hash.textContent = it.hash || "";

    const revert = document.createElement("button");
    revert.className = "revert";
    revert.textContent = "되돌리기";
    revert.disabled = isHead || isRemote;
    revert.addEventListener("click", () => send("revert", it.hash));

    li.append(time, desc, hash, revert);
    list.appendChild(li);
  }
}

function makeTag(text, cls) {
  const s = document.createElement("span");
  s.className = "tag " + cls;
  s.textContent = text;
  return s;
}

function handle(msg) {
  switch (msg.type) {
    case "status":
      isConfigured = msg.configured !== false;
      renderStatus(msg);
      break;
    case "log":    renderLog(msg); break;
    case "result":
      showToast(msg.message + (msg.detail ? " — " + msg.detail : ""), msg.ok);
      // 부트스트랩 등 결과 직후, 초기 설정 화면이면 상태를 다시 조회(성공 시 전환/실패 시 버튼 복구)
      if (!$("setup").hidden) send("status");
      break;
    case "settings":   // 셸 설정 탭에서 변경 통지 → 즉시 반영
      appSettings = normalizeSettings(msg.settings);
      sendConfigure();
      applyAutoRefresh();
      if (isConfigured) send("log");   // 이력 개수 변경 반영
      var ru = $("repoUrl");
      if (ru && appSettings.repoUrl && document.activeElement !== ru) ru.value = appSettings.repoUrl;
      break;
    default: break;
  }
}

// 부모(Core 셸)가 중계한 C++ 결과 수신
window.addEventListener("message", (e) => {
  let msg = e.data;
  if (typeof msg === "string") {
    try { msg = JSON.parse(msg); } catch (_) { return; }
  }
  if (msg && msg.type) handle(msg);
});

// wiring
$("pullBtn").addEventListener("click", () => { if (!$("pullBtn").disabled) send("pull"); });
$("pushBtn").addEventListener("click", () => { if (!$("pushBtn").disabled) send("push"); });
$("refreshBtn").addEventListener("click", () => send("refresh"));
$("remoteSave").addEventListener("click", () => {
  const u = $("remoteUrl").value.trim();
  if (u) send("setRemote", u);
});

$("setupBtn").addEventListener("click", function () {
  const url = $("repoUrl").value.trim();
  this.disabled = true;
  this.textContent = "설정 중…";
  send("bootstrap", url);
});

// 시작 시퀀스: 설정을 C++ 모듈에 먼저 전달(이력 개수 등) → 상태 조회.
// '시작 시 원격 확인' 이 켜져 있으면 fetch 포함 새로고침으로 최신 여부를 바로 반영한다.
sendConfigure();
if (appSettings.repoUrl) {
  var __ru = $("repoUrl");
  if (__ru) __ru.value = appSettings.repoUrl;
}
if (appSettings.startupFetch) {
  send("refresh");
} else {
  send("status");
  send("log");
}
applyAutoRefresh();
