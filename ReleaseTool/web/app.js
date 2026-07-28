// ReleaseTool UI — 버전 검증(로컬) + Build.bat / gh 실행 요청·로그 표시(C++ 경유).
//
//   JS  -> C++ : {cmd:"init"}
//                {cmd:"build",   version, skipBuild}
//                {cmd:"publish", version}
//   C++ -> JS  : {type:"init", latest, repo, msbuild, nsis, gh, running,
//                              setup, setupPath, setupKb, branch}
//                {type:"log",       line}
//                {type:"done",      code, latest?, setup?, setupPath?, setupKb?, branch?}
//                {type:"pubdone",   code}
//                {type:"pubstatus", data:{released, url, version}}
(function () {
  "use strict";

  var latest     = "0.0.0";      // installer\VERSION (C++ 이 준다)
  var latestNums = [0, 0, 0];
  var running    = false;

  // 배포 카드가 보고 판단하는 상태 전부
  var pub = {
    setup:    false,   // Shipping 에 설치파일이 있는가
    setupKb:  0,
    branch:   "",
    gh:       false,   // gh.exe 가 PATH 에 있는가
    released: false,   // 이미 배포된 버전인가 (원격 조회 결과)
    url:      ""
  };

  var card    = document.getElementById("verCard");
  var input   = document.getElementById("verInput");
  var title   = document.getElementById("verTitle");
  var sub     = document.getElementById("verSub");
  var badge   = document.getElementById("verBadge");
  var msg     = document.getElementById("verMsg");
  var msgText = document.getElementById("verMsgText");
  var msgMark = msg.querySelector(".mark");
  var goBtn   = document.getElementById("goBtn");
  var goSub   = document.getElementById("goSub");
  var logBody = document.getElementById("logBody");
  var logStep = document.getElementById("logStep");
  var copyBtn = document.getElementById("copyBtn");
  var skipBox = document.getElementById("optSkipBuild");
  var latestEl = document.getElementById("latestVal");

  var pubCard   = document.getElementById("pubCard");
  var pubTag    = document.getElementById("pubTag");
  var pubFile   = document.getElementById("pubFile");
  var pubNote   = document.getElementById("pubNote");
  var pubBtn    = document.getElementById("pubBtn");
  var pubBtnSub = document.getElementById("pubBtnSub");
  var sbBranch  = document.getElementById("sbBranch");

  // ---------- host bridge ----------
  function post(obj) {
    if (window.chrome && window.chrome.webview)
      window.chrome.webview.postMessage(JSON.stringify(obj));
  }

  // ---------- 버전 파싱 / 비교 (Build.bat 의 규칙과 동일) ----------
  function parse(raw) {
    var v = String(raw).trim();
    if (v === "") return { ok: false, code: "empty" };
    var parts = v.split(".");
    if (parts.length !== 3) return { ok: false, code: "shape" };
    var nums = [];
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i];
      if (!/^[0-9]+$/.test(p)) return { ok: false, code: "nondigit" };
      if (p.length > 1 && p.charAt(0) === "0") return { ok: false, code: "leadzero" };
      nums.push(parseInt(p, 10));
    }
    return { ok: true, nums: nums, text: nums.join(".") };
  }

  function compare(a, b) {
    for (var i = 0; i < 3; i++) {
      if (a[i] > b[i]) return 1;
      if (a[i] < b[i]) return -1;
    }
    return 0;
  }

  function nextOf(nums) { return nums[0] + "." + nums[1] + "." + (nums[2] + 1); }

  function setLatest(v) {
    var r = parse(v);
    latest     = r.ok ? r.text : "0.0.0";
    latestNums = r.ok ? r.nums : [0, 0, 0];
    latestEl.textContent = latest;
    // 현재 발행분의 다음 패치를 입력칸에 미리 보여준다 (그대로 쓰지 않아도 된다).
    input.placeholder = nextOf(latestNums);
  }

  // ---------- 생성 카드 ----------
  function setState(kind, o) {
    card.classList.remove("is-ok", "is-bad", "is-busy");
    if (kind !== "idle") card.classList.add("is-" + kind);
    title.textContent   = o.title;
    sub.textContent     = o.sub;
    badge.textContent   = o.badge;
    msgMark.textContent = o.mark;
    msgText.textContent = o.msg;
    goBtn.disabled      = !o.enable;
    goSub.textContent   = o.goSub;
  }

  function validate() {
    if (running) return;
    var raw = input.value;
    var r   = parse(raw);

    if (!r.ok && r.code === "empty") {
      setState("idle", {
        title: "버전을 입력하세요",
        sub:   "최신 버전보다 높은 값이어야 생성할 수 있습니다.",
        badge: "대기", mark: "·",
        msg:   "권장 " + nextOf(latestNums) + " — 최신 " + latest + " 의 다음 패치입니다.",
        enable: false, goSub: "권장 " + nextOf(latestNums)
      });
      return;
    }

    if (!r.ok) {
      var why = {
        shape:    "세 자리가 필요합니다 — MAJOR.MINOR.PATCH 형식으로 입력하세요. 예: " + nextOf(latestNums),
        nondigit: "각 자리는 숫자여야 합니다. 예: " + nextOf(latestNums),
        leadzero: "0으로 시작하는 자리는 쓸 수 없습니다. 1.02.0 이 아니라 1.2.0 입니다."
      }[r.code];
      setState("bad", {
        title: "형식이 올바르지 않습니다",
        sub:   "입력한 값 “" + raw.trim() + "” 을 버전으로 읽을 수 없습니다.",
        badge: "형식 오류", mark: "✕", msg: why,
        enable: false, goSub: "형식 오류 — 생성 불가"
      });
      return;
    }

    var cmp = compare(r.nums, latestNums);
    if (cmp <= 0) {
      setState("bad", {
        title: cmp === 0 ? "이미 발행된 버전입니다" : "최신 버전보다 낮습니다",
        sub:   "최신 버전 " + latest + " 보다 높은 값을 입력하세요.",
        badge: cmp === 0 ? "중복" : "역행",
        mark:  "✕",
        msg:   r.text + (cmp === 0 ? " 는 최신 버전과 같습니다. " : " 는 최신 버전 " + latest + " 보다 낮습니다. ")
               + "다음 버전은 " + nextOf(latestNums) + " 입니다.",
        enable: false,
        goSub:  cmp === 0 ? "같은 버전 — 생성 불가" : "이전 버전 — 생성 불가"
      });
      return;
    }

    setState("ok", {
      title: "생성할 수 있습니다",
      sub:   latest + " → " + r.text + " 로 올립니다.",
      badge: "통과", mark: "✓",
      msg:   "산출물 Shipping\\ClaudeCodeStudio-Setup-" + r.text + ".exe",
      enable: true,
      goSub:  latest + " → " + r.text
    });
  }

  // ---------- 배포 카드 ----------
  // 상태는 pub 객체 하나에서 파생된다. 순서가 곧 우선순위다.
  function refreshPub() {
    if (running) return;

    sbBranch.textContent = "브랜치 " + (pub.branch || "—");
    pubTag.textContent   = pub.setup ? ("v" + latest) : "—";
    pubFile.textContent  = pub.setup
      ? ("ClaudeCodeStudio-Setup-" + latest + ".exe · " + pub.setupKb.toLocaleString() + " KB")
      : "";

    var cls, note, sub2, enable;
    if (!pub.setup) {
      cls = "none";  note = "먼저 인스톨러를 만드세요.";  sub2 = "산출물 없음";  enable = false;
    } else if (pub.released) {
      cls = "done";  note = "배포됨 · " + (pub.url || "GitHub Release");  sub2 = "배포 완료";  enable = false;
    } else if (!pub.gh) {
      cls = "block"; note = "gh 를 찾을 수 없습니다. GitHub CLI 를 설치하세요.";  sub2 = "gh 없음";  enable = false;
    } else if (pub.branch !== "main") {
      cls = "block";
      note = "현재 " + (pub.branch || "?") + " — main 에서만 배포할 수 있습니다.";
      sub2 = "main 아님"; enable = false;
    } else {
      cls = "ready"; note = "아직 배포되지 않았습니다.";  sub2 = "태그 + Release 생성";  enable = true;
    }

    pubCard.className   = "pubcard p-" + cls;
    pubNote.textContent = note;
    pubBtnSub.textContent = sub2;
    pubBtn.disabled     = !enable;
  }

  function setPubBusy(note) {
    pubCard.className     = "pubcard p-busy";
    pubNote.textContent   = note;
    pubBtn.disabled       = true;
    pubBtnSub.textContent = "실행 중…";
  }

  // ---------- 로그 ----------
  function clearLog() {
    logBody.innerHTML = "";
    logStep.textContent = "—";
    copyBtn.hidden = false;
  }

  function line(text) {
    var cls = "";
    if (/^\[ERROR\]/.test(text))         cls = "err";
    else if (/^\[OK\]/.test(text))       cls = "ok";
    else if (/^>/.test(text))            cls = "cmd";
    else if (/^\[\d+\/\d+\]/.test(text)) {
      cls = "step";
      var m = text.match(/^\[(\d+)\/(\d+)\]/);
      if (m) logStep.textContent = m[1] + "/" + m[2];
    }

    var row = document.createElement("div");
    row.className = "ln" + (cls ? " " + cls : "");
    var t = document.createElement("span");
    t.className = "ln-txt";
    t.textContent = text;
    row.appendChild(t);

    var atBottom = logBody.scrollHeight - logBody.scrollTop - logBody.clientHeight < 40;
    logBody.appendChild(row);
    if (atBottom) logBody.scrollTop = logBody.scrollHeight;   // 위로 올려 읽는 중이면 방해하지 않는다
  }

  // ---------- 인스톨러 생성 ----------
  function run() {
    if (running || goBtn.disabled) return;
    var r = parse(input.value);
    if (!r.ok) return;

    running = true;
    clearLog();
    input.disabled   = true;
    skipBox.disabled = true;
    setPubBusy("인스톨러를 만드는 중입니다.");

    setState("busy", {
      title: "인스톨러 생성 중",
      sub:   latest + " → " + r.text,
      badge: "진행 중", mark: "·",
      msg:   "Build.bat 출력을 받아오는 중입니다.",
      enable: false, goSub: "실행 중…"
    });

    post({ cmd: "build", version: r.text, skipBuild: skipBox.checked ? "true" : "false" });
  }

  function finish(m) {
    running = false;
    input.disabled   = false;
    skipBox.disabled = false;

    if (m.code === 0) {
      if (m.latest) setLatest(m.latest);
      // 새 버전이 나왔으니 배포 상태를 처음부터 다시 본다 (released 는 원격 조회가 채운다).
      pub.setup    = !!m.setup;
      pub.setupKb  = m.setupKb || 0;
      pub.branch   = m.branch || pub.branch;
      pub.released = false;
      pub.url      = "";
      logStep.textContent = "완료";
      setState("ok", {
        title: "생성 완료",
        sub:   "installer\\VERSION 이 " + latest + " 로 갱신되었습니다.",
        badge: "완료", mark: "✓",
        msg:   m.setupPath || "",
        enable: false, goSub: "다음 버전을 입력하세요"
      });
      input.value = "";
    } else {
      logStep.textContent = "실패";
      setState("bad", {
        title: "생성 실패",
        sub:   "Build.bat 이 종료 코드 " + m.code + " 로 끝났습니다.",
        badge: "실패", mark: "✕",
        msg:   "로그의 [ERROR] 줄을 확인하세요.",
        enable: false, goSub: "로그 확인 후 다시 시도"
      });
    }
    refreshPub();
  }

  // ---------- 배포 ----------
  function publish() {
    if (running || pubBtn.disabled) return;
    running = true;
    clearLog();
    input.disabled   = true;
    skipBox.disabled = true;
    goBtn.disabled   = true;
    setPubBusy("GitHub 에 배포하는 중입니다.");
    logStep.textContent = "배포";
    post({ cmd: "publish", version: latest });
  }

  function pubFinish(m) {
    running = false;
    input.disabled   = false;
    skipBox.disabled = false;
    if (m.code === 0) {
      logStep.textContent = "완료";
      pub.released = true;          // URL 은 뒤이어 오는 pubstatus 가 채운다
    } else {
      logStep.textContent = "실패";
    }
    refreshPub();
    validate();                     // 생성 버튼 상태 복구
  }

  // ---------- 호스트 메시지 ----------
  function handle(m) {
    if (!m || !m.type) return;

    if (m.type === "init") {
      setLatest(m.latest || "0.0.0");
      document.getElementById("repoPath").textContent = m.repo || "(repo 를 찾지 못했습니다)";
      document.getElementById("probeMsbuild").classList.toggle("off", !m.msbuild);
      document.getElementById("probeNsis").classList.toggle("off", !m.nsis);
      document.getElementById("probeGh").classList.toggle("off", !m.gh);
      pub.setup   = !!m.setup;
      pub.setupKb = m.setupKb || 0;
      pub.branch  = m.branch || "";
      pub.gh      = !!m.gh;
      running     = !!m.running;
      refreshPub();
      validate();
    } else if (m.type === "log") {
      line(m.line);
    } else if (m.type === "done") {
      finish(m);
    } else if (m.type === "pubdone") {
      pubFinish(m);
    } else if (m.type === "pubstatus") {
      var d = m.data || {};
      // 조회를 보낸 뒤 버전이 바뀌었을 수 있다 — 지금 버전에 대한 답만 받는다.
      if (d.version && d.version !== latest) return;
      pub.released = !!d.released;
      pub.url      = d.url || "";
      refreshPub();
    }
  }

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener("message", function (e) {
      var raw = e.data;
      try { handle(typeof raw === "string" ? JSON.parse(raw) : raw); } catch (err) { /* 무시 */ }
    });
  }

  // ---------- 배선 ----------
  input.addEventListener("input", validate);
  input.addEventListener("keydown", function (e) {
    if (e.key === "Enter" && !goBtn.disabled) run();
  });
  goBtn.addEventListener("click", run);
  pubBtn.addEventListener("click", publish);

  copyBtn.addEventListener("click", function () {
    var text = Array.prototype.map.call(logBody.querySelectorAll(".ln-txt"),
                                        function (n) { return n.textContent; }).join("\n");
    if (!text) return;
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(function () {
        copyBtn.textContent = "복사됨";
        setTimeout(function () { copyBtn.textContent = "복사"; }, 1400);
      }, function () { /* 무시 */ });
    }
  });

  // WebView2 는 시스템 테마를 따라온다 — 앱 설정과 별개로 OS 설정만 반영한다.
  function applyTheme() {
    var dark = window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches;
    document.documentElement.setAttribute("data-theme", dark ? "dark" : "light");
  }
  if (window.matchMedia) {
    var mq = window.matchMedia("(prefers-color-scheme: dark)");
    if (mq.addEventListener) mq.addEventListener("change", applyTheme);
  }
  applyTheme();

  validate();
  refreshPub();
  post({ cmd: "init" });
})();
