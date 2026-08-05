"use strict";

// iframe 안에서 동작. C++ 호스트와는 부모(Core 셸)를 통해 중계로 통신한다.
//   보내기: window.parent.postMessage({module,cmd,arg})  -> 부모가 chrome.webview 로 전달
//   받기:   부모가 C++ 결과(JSON)를 이 iframe 으로 postMessage -> message 이벤트

// 테마를 로드 직후 적용 (셸 설정과 같은 출처 localStorage 공유 — 늦은 주입으로 인한 깜빡임 방지)
try {
  var __t = JSON.parse(localStorage.getItem("ccs.ui.settings.v1"));
  document.documentElement.setAttribute("data-theme", (__t && __t.theme === "dark") ? "dark" : "light");
} catch (_) {}

// 시간이 걸리는 원격 작업 — 진행 중 오버레이로 화면을 덮어 중복 실행을 막는다.
// (status/log 같은 즉시 응답 명령은 대상 아님)
const BUSY = {
  pull:      { title: "서버에서 가져오는 중…",    desc: "서버 설정을 이 PC 에 적용합니다" },
  push:      { title: "서버에 반영하는 중…",      desc: "커밋 후 업로드합니다" },
  refresh:    { title: "서버 상태를 확인하는 중…", desc: "원격에서 최신 정보를 가져옵니다" },
  bootstrap:  { title: "초기 설정 중…",            desc: "저장소를 준비하고 서버 설정을 가져옵니다" },
  createRepo: { title: "저장소를 만드는 중…",      desc: "GitHub 에 저장소를 만들고 설정을 올립니다" }
};
let busyCmd = null;
let busyTimer = null;

// 오버레이는 이 iframe 안에만 있어 셸의 좌측 탭 바까지 덮지 못한다.
// 오버레이(진행/확인)를 여닫을 때마다 현재 상태를 셸에 알려 탭 전환도 함께 막는다.
function syncShellLock() {
  const on = !$("busy").hidden || !$("confirm").hidden;
  window.parent.postMessage({ type: "busy", on: on }, "*");
}

function showBusy(cmd) {
  const t = BUSY[cmd];
  if (!t) return;
  busyCmd = cmd;
  $("busyTitle").textContent = t.title;
  $("busyDesc").textContent = t.desc;
  $("busy").hidden = false;
  syncShellLock();
  clearTimeout(busyTimer);
  // 안전장치 — 응답이 오지 않아도 화면이 영구히 잠기지 않게 한다
  busyTimer = setTimeout(() => {
    if (!busyCmd) return;
    hideBusy();
    showToast("응답이 지연되고 있습니다. 새로고침으로 상태를 확인하세요", false);
  }, 180000);
}
function hideBusy() {
  busyCmd = null;
  clearTimeout(busyTimer);
  busyTimer = null;
  $("busy").hidden = true;
  syncShellLock();
}

function send(cmd, arg) {
  if (BUSY[cmd]) showBusy(cmd);
  window.parent.postMessage({ module: "sync", cmd: cmd, arg: arg || "" }, "*");
}

// 진행 오버레이 없이 보낸다. 사용자가 시킨 일이 아니라 앱이 알아서 하는 확인에 쓴다.
function sendQuiet(cmd, arg) {
  window.parent.postMessage({ module: "sync", cmd: cmd, arg: arg || "" }, "*");
}

// ----- 앱 설정 (셸 설정 탭이 저장, 여기서 소비) -----
var SYNC_KEY = "ccs.sync.settings.v1";
function normalizeSettings(s) {
  var out = { autoMin: 0, startupFetch: true, histCount: 8, commitMsg: "", repoUrl: "" };
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

// ----- 초기 설정 화면 상태 -----
var ghAccount = "";        // gh 로그인 계정 ('새로 만들기' 모드의 소유자)
var ghAsked = false;       // ghInfo 는 미설정을 처음 확인했을 때 한 번만 조회한다

// 이력의 태그 위치가 status(미커밋 여부)에 의존하므로 두 메시지의 마지막 값을 함께 보관한다.
var lastStatus = null;
var lastLog = null;

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
      // 미설정(초기 설정 화면)이거나 다른 작업이 진행 중이면 건너뜀
      if (isConfigured && !busyCmd) send("refresh");
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

// 초기 설정 방식(직접 입력 / 새로 만들기) 전환 — 입력 영역과 안내 문구를 함께 바꾼다.
function applySetupMode() {
  const isNew = $("modeNew").checked;
  $("paneUrl").hidden = isNew;
  $("paneNew").hidden = !isNew;
  $("setupNote").textContent = isNew
    ? "저장소를 만든 뒤 이 PC 의 설정을 첫 커밋으로 올립니다."
    : "서버에 설정이 있으면 가져오고, 빈 저장소면 이 PC 설정을 올립니다. 덮어쓰기 전 기존 설정은 .sync-backup 에 백업됩니다.";
}

// 승인은 브라우저에서 일어나므로, 끝났는지는 앱이 주기적으로 물어봐서 안다.
// 5초는 GitHub 이 device flow 응답에서 권하는 간격이다 — 더 조르면 slow_down 이 돌아온다.
var ghPollTimer = null;
function stopGhPoll() { clearInterval(ghPollTimer); ghPollTimer = null; }
function startGhPoll() {
  stopGhPoll();
  ghPollTimer = setInterval(function () { send("ghPoll"); }, 5000);
}

// 승인 대기 화면으로 전환한다. 창을 겹치지 않고 설정 본문 자리를 그대로 쓴다.
function showLoginWait(code) {
  $("ghCode").textContent = code || "—";
  $("ghWaitText").textContent = "승인을 기다리는 중…";
  $("ghWait").hidden = false;
  $("ghEnded").hidden = true;
  $("ghRetry").hidden = true;
  $("ghCopy").textContent = "코드 복사";
  $("setupMain").hidden = true;
  $("setupLogin").hidden = false;
  startGhPoll();
}

// 코드가 만료됐거나 거부된 경우 — 기다림을 멈추고 다시 시도할 길을 연다.
function endLoginWait(text) {
  stopGhPoll();
  $("ghWait").hidden = true;
  $("ghEnded").textContent = text;
  $("ghEnded").hidden = false;
  $("ghRetry").hidden = false;
}

// 설정 본문으로 복귀.
function backToSetupMain() {
  stopGhPoll();
  $("setupLogin").hidden = true;
  $("setupMain").hidden = false;
  const b = $("ghLoginBtn");
  b.disabled = false;
  b.textContent = "GitHub 로그인";
}

// 로그인 여부에 따라 '새로 만들기' 모드를 열거나 잠근다.
function applyGhInfo(m) {
  ghAccount = m.account || "";
  const radio = $("modeNew");
  const ready = !!(m.loggedIn && ghAccount);
  radio.disabled = !ready;
  $("ghState").textContent = ready ? "✓ " + ghAccount : "로그인 필요";
  if (!ready && radio.checked) { $("modeUrl").checked = true; }
  applySetupMode();

  // 로그인 전이면 앱에서 바로 로그인할 수 있게 연다
  $("ghLoginBox").hidden = ready;
  if (ready) backToSetupMain();   // 승인이 끝났으니 설정 본문으로 돌아간다

  // 인스톨러가 남긴 주소는 아직 아무것도 채워지지 않았을 때만 제안한다
  const ru = $("repoUrl");
  if (ru && !ru.value && m.suggestedRepo) ru.value = m.suggestedRepo;
}

function renderStatus(s) {
  const setup = $("setup");
  const body = document.querySelector(".body");

  // 미설정(git repo 아님) → 초기 설정 화면만 노출. 상태바도 숨긴다 — 연결된 서버가 아직 없다.
  if (s.configured === false) {
    setup.hidden = false;
    if (body) body.style.display = "none";
    $("statusbar").hidden = true;
    const sb = $("setupBtn");
    if (sb) { sb.disabled = false; sb.textContent = "시작"; }
    if (!ghAsked) { ghAsked = true; send("ghInfo"); }
    return;
  }
  setup.hidden = true;
  if (body) body.style.display = "";
  $("statusbar").hidden = false;

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
  // 서버 것으로 맞추면 사라지는 이 PC 변경 — 저장 안 된 수정 + 이 PC 에만 있는 커밋.
  const localCount = (dirty ? Math.max(1, s.dirtyCount | 0) : 0) + ahead;

  el.className = "status";
  wire.className = "wire dir-none";
  badge.hidden = true;
  pull.classList.remove("primary"); push.classList.remove("primary");
  pull.disabled = true; push.disabled = true;
  pullBadge.hidden = true; pushBadge.hidden = true;

  // '서버 → 이 PC 적용' 은 이름 그대로 동작한다 — 서버와 다르기만 하면 누를 수 있다.
  // 커밋이 뒤처졌는지가 아니라 설정이 다른지를 본다. 서버에 새 변경이 없어도
  // 이 PC 만 달라졌으면 눌러서 서버 상태로 되돌릴 수 있어야 한다.
  if (behind > 0 || localCount > 0) {
    pull.disabled = false;
    if (behind > 0) { pullBadge.textContent = behind; pullBadge.hidden = false; }
    $("pullSub").textContent = localCount > 0
      ? "이 PC 변경 " + localCount + "건을 버림"
      : "새 변경 " + behind + "건 가져오기";
  } else {
    $("pullSub").textContent = "가져올 변경 없음";
  }

  if (localCount > 0) {
    push.disabled = false;
    if (ahead > 0) { pushBadge.textContent = ahead; pushBadge.hidden = false; }
    $("pushSub").textContent = dirty ? "커밋 후 반영" : ahead + "건 반영";
  } else {
    $("pushSub").textContent = "반영할 변경 없음";
  }

  if (behind > 0 && localCount > 0) {
    el.classList.add("is-warn");
    $("statusTitle").textContent = "양쪽이 갈렸습니다";
    $("statusSub").textContent = "서버 새 변경 " + behind + "건 · 이 PC 변경 " + localCount + "건";
    badge.textContent = "주의"; badge.hidden = false;
    wire.className = "wire dir-pull";
    $("wireLabel").textContent = "가져오기";
    pull.classList.add("primary");
  } else if (behind > 0) {
    el.classList.add("is-warn");
    $("statusTitle").textContent = "동기화 필요";
    $("statusSub").textContent = "서버에 새 변경 " + behind + "건";
    badge.textContent = "주의"; badge.hidden = false;
    wire.className = "wire dir-pull";
    $("wireLabel").textContent = "가져오기";
    pull.classList.add("primary");
  } else if (localCount > 0) {
    el.classList.add("is-push");
    $("statusTitle").textContent = "서버와 설정이 다름";
    $("statusSub").textContent = dirty
      ? "이 PC 에서 " + Math.max(1, s.dirtyCount | 0) + "개 파일이 바뀌었습니다"
      : ahead + "건이 서버에 반영되지 않았습니다";
    wire.className = "wire dir-push";
    $("wireLabel").textContent = "반영하기";
    push.classList.add("primary");
  } else {
    el.classList.add("is-good");
    $("statusTitle").textContent = "최신 상태";
    $("statusSub").textContent = "모든 설정이 서버와 일치합니다";
    $("wireLabel").textContent = "동기화됨";
  }
}

// 서버 것으로 맞추면 사라지는 이 PC 변경 건수. 확인 카드를 띄울지 여기서 정한다.
function localChangeCount() {
  const s = lastStatus;
  if (!s || s.configured === false) return 0;
  return ((!s.clean) ? Math.max(1, s.dirtyCount | 0) : 0) + (s.ahead | 0);
}

// 사라질 것이 있을 때만 한 번 묻는다. 버튼을 누른 뒤 무슨 일이 벌어지는지
// 실행 전에 알려주는 자리다 — 백업이 남더라도 되돌리려면 탐색기를 열어야 한다.
function showApplyConfirm(count) {
  $("confirmDesc").textContent =
    "이 PC 에서 바뀐 " + count + "건이 서버 것으로 덮여 씁니다. " +
    "덮기 전에 .sync-backup 폴더에 복사해 둡니다.";
  $("confirm").hidden = false;
  syncShellLock();
  $("confirmYes").focus();
}
function hideApplyConfirm() { $("confirm").hidden = true; syncShellLock(); }

// 아직 커밋되지 않은 로컬 변경을 나타내는 이력 행.
// 커밋 포인터(HEAD)가 서버와 같은 자리에 있어도 "이 PC 가 앞서 있다"를 태그 위치로 보이게 한다.
function makeUncommittedRow(count) {
  const li = document.createElement("li");
  li.className = "hist-row uncommitted";

  const time = document.createElement("span");
  time.className = "hist-time";
  time.textContent = "미반영";

  const desc = document.createElement("span");
  desc.className = "hist-desc";
  const subj = document.createElement("span");
  subj.className = "hist-subject";
  subj.textContent = "아직 서버에 반영되지 않은 변경 " + count + "건";
  desc.appendChild(subj);
  desc.appendChild(makeTag("현재 PC", "tag-current"));

  const hash = document.createElement("span");
  hash.className = "hist-hash";
  hash.textContent = "";

  // 커밋이 아니라 되돌릴 대상이 없다. 다른 행과 열 정렬을 맞추기 위해 자리만 유지(disabled = 숨김).
  const revert = document.createElement("button");
  revert.className = "revert";
  revert.textContent = "되돌리기";
  revert.disabled = true;

  li.append(time, desc, hash, revert);
  return li;
}

function renderLog(m) {
  const list = $("histList");
  list.innerHTML = "";
  const items = m.items || [];
  $("histCount").textContent = items.length ? "최근 " + items.length + "건" : "";

  // 미커밋 변경이 있으면 맨 위에 별도 행을 얹고 '현재 PC' 태그를 그 행이 가진다.
  const dirtyCount = (lastStatus && lastStatus.configured !== false && !lastStatus.clean)
    ? (lastStatus.dirtyCount | 0) : 0;
  if (dirtyCount > 0) list.appendChild(makeUncommittedRow(dirtyCount));

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
    // 미커밋 행이 있으면 '현재 PC' 는 그쪽이 가지므로 커밋 행에는 붙이지 않는다.
    if (isHead && dirtyCount === 0) desc.appendChild(makeTag("현재 PC", "tag-current"));
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
      lastStatus = msg;
      renderStatus(msg);
      if (lastLog) renderLog(lastLog);   // 태그 위치가 미커밋 여부에 의존 → 이력 재렌더
      break;
    case "gh":
      applyGhInfo(msg);
      break;
    case "ghLogin":
      showLoginWait(msg.code);
      break;
    case "ghPending":
      // 승인 전 authorization_pending 은 정상 흐름이라 문구를 그대로 둔다.
      // 그 밖(연결 실패·slow_down)은 되묻고 있다는 것만 알린다 — 원문은 감춘다.
      $("ghWaitText").textContent = (!msg.note || msg.note === "authorization_pending")
        ? "승인을 기다리는 중…" : "승인을 기다리는 중… (연결 재시도)";
      // 화면에는 감춘 응답 종류를 검증용으로 남긴다 (slow_down 이면 간격을 어기고 있다는 뜻).
      $("ghWait").setAttribute("data-last", msg.note || "");
      break;
    case "ghExpired":
      endLoginWait(msg.reason === "access_denied"
        ? "승인이 거부되었습니다." : "코드가 만료되었습니다.");
      break;
    case "log":
      hideBusy();          // 모든 원격 작업이 마지막에 log 를 보낸다
      lastLog = msg;
      renderLog(msg);
      break;
    case "result":
      hideBusy();          // 실패로 조기 종료(log 미수신)하는 경로까지 확실히 해제
      showToast(msg.message + (msg.detail ? " — " + msg.detail : ""), msg.ok);
      // 로그인 시작이 실패했으면 버튼을 되살린다 (성공은 ghLogin 이 받는다)
      if (!msg.ok && $("ghLoginBtn").disabled) {
        $("ghLoginBtn").disabled = false;
        $("ghLoginBtn").textContent = "GitHub 로그인";
      }
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
$("pullBtn").addEventListener("click", () => {
  if ($("pullBtn").disabled) return;
  const count = localChangeCount();
  if (count > 0) { showApplyConfirm(count); return; }   // 사라질 것이 있으면 먼저 묻는다
  send("pull");
});
$("pushBtn").addEventListener("click", () => { if (!$("pushBtn").disabled) send("push"); });

$("confirmNo").addEventListener("click", hideApplyConfirm);
$("confirmYes").addEventListener("click", () => { hideApplyConfirm(); send("pull"); });
$("confirm").addEventListener("keydown", (e) => { if (e.key === "Escape") hideApplyConfirm(); });
// 초기 설정 화면에서는 gh 상태를 다시 조회한다 — 터미널에서 gh auth login 을 마친 뒤
// 앱을 다시 켜지 않고 이 버튼만으로 '새로 만들기' 를 열 수 있게 한다.
$("refreshBtn").addEventListener("click", () => {
  if (isConfigured === false) { send("ghInfo"); send("status"); return; }
  send("refresh");
});
$("remoteSave").addEventListener("click", () => {
  const u = $("remoteUrl").value.trim();
  if (u) send("setRemote", u);
});

$("modeUrl").addEventListener("change", applySetupMode);
$("modeNew").addEventListener("change", applySetupMode);

$("ghLoginBtn").addEventListener("click", function () {
  this.disabled = true;
  this.textContent = "로그인 준비 중…";
  send("ghLogin");
});

$("ghBack").addEventListener("click", function () {
  send("ghCancel");        // 기다리던 코드를 버린다 — 돌아온 뒤 유령 폴링이 남지 않게
  backToSetupMain();
});

$("ghRetry").addEventListener("click", function () {
  $("ghRetry").hidden = true;
  $("ghEnded").hidden = true;
  $("ghWaitText").textContent = "새 코드를 받는 중…";
  $("ghWait").hidden = false;
  send("ghLogin");
});

$("ghCopy").addEventListener("click", function () {
  const btn = this, code = $("ghCode").textContent;
  function mark() { btn.textContent = "복사됨"; setTimeout(function () { btn.textContent = "코드 복사"; }, 1600); }
  function fallback() {
    // clipboard API 가 막힌 환경 대비 — 임시 입력에 담아 복사한다.
    const t = document.createElement("textarea");
    t.value = code;
    document.body.appendChild(t);
    t.select();
    try { document.execCommand("copy"); mark(); } catch (_) {}
    document.body.removeChild(t);
  }
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(code).then(mark, fallback);
  } else {
    fallback();
  }
});

// 브라우저에서 승인하고 앱으로 돌아오면 다음 주기를 기다리지 않고 바로 확인한다.
// 실제 질의 간격은 C++ 이 지키므로(GitHub 이 정한 최소 간격) 여기서 더 불러도 안전하다.
function pollNowIfWaiting() { if (ghPollTimer) send("ghPoll"); }
window.addEventListener("focus", pollNowIfWaiting);
document.addEventListener("visibilitychange", function () {
  if (!document.hidden) pollNowIfWaiting();
});

$("setupBtn").addEventListener("click", function () {
  // '새로 만들기' 는 owner/repo/private 을 함께 보내야 해서 send() 대신 직접 구성한다.
  if ($("modeNew").checked) {
    const name = $("newRepoName").value.trim();
    if (!name)      { showToast("저장소 이름을 입력하세요", false); return; }
    if (!ghAccount) { showToast("GitHub 로그인을 확인할 수 없습니다", false); return; }
    this.disabled = true;
    this.textContent = "설정 중…";
    showBusy("createRepo");
    window.parent.postMessage({
      module: "sync", cmd: "createRepo",
      owner: ghAccount, repo: name,
      private: $("newRepoPrivate").checked ? "true" : "false"
    }, "*");
    return;
  }
  const url = $("repoUrl").value.trim();
  if (!url) { showToast("저장소 URL 을 입력하세요", false); return; }
  this.disabled = true;
  this.textContent = "설정 중…";
  send("bootstrap", url);
});

// 시작 시퀀스: 설정을 C++ 모듈에 먼저 전달(이력 개수 등) → 상태 조회.
// '시작 시 원격 확인' 이 켜져 있으면 fetch 포함 새로고침으로 최신 여부를 바로 반영한다.
sendConfigure();
applySetupMode();
if (appSettings.repoUrl) {
  var __ru = $("repoUrl");
  if (__ru) __ru.value = appSettings.repoUrl;
}
if (appSettings.startupFetch) {
  // 시작 확인은 화면을 덮지 않는다 — 켜자마자 오버레이가 뜨면 오프라인일 때
  // 기다림이 그대로 노출된다. 상태 카드 문구만 바꿔 두고 뒤에서 확인한다.
  $("statusTitle").textContent = "서버 확인 중…";
  $("statusSub").textContent = "최신 여부를 알아보는 중입니다";
  sendQuiet("refresh");
} else {
  send("status");
  send("log");
}
applyAutoRefresh();
