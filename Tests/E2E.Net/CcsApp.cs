// ClaudeCodeStudio E2E 하네스 — WebView2 앱을 --remote-debugging-port 로 spawn → EdgeDriver(CDP) attach.
// MDViewer(Tests/E2E.Net/MdvHarness.cs) 패턴의 축소판: 단일 인스턴스/세션 로직은 제외하고,
// Core 셸 top-frame ↔ 모듈 iframe 전환(SwitchToModule)을 추가했다.
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using OpenQA.Selenium;
using OpenQA.Selenium.Edge;

namespace ClaudeCodeStudio.E2E;

/// <summary>attach 된 앱 + EdgeDriver 한 인스턴스. 테스트마다 Launch → Dispose.</summary>
public sealed class CcsApp : IDisposable
{
    private readonly Process _proc;
    private readonly string? _tempClaudeDir;
    private readonly string _stateDir;       // CCSTUDIO_STATE_DIR — 창 크기/WebView2 프로필 격리 (사용자 상태 오염 방지)
    private readonly bool _ownsStateDir;     // 자동 생성한 경우만 Dispose 에서 삭제 (호출자 제공 시 재사용 가능 — 재시작 시나리오)
    public EdgeDriver Driver { get; }
    public int Port { get; }
    public IJavaScriptExecutor Js => (IJavaScriptExecutor)Driver;

    private CcsApp(Process proc, EdgeDriver driver, int port, string? tempClaudeDir, string stateDir, bool ownsStateDir)
    {
        _proc = proc; Driver = driver; Port = port; _tempClaudeDir = tempClaudeDir;
        _stateDir = stateDir; _ownsStateDir = ownsStateDir;
    }

    // repo 루트 = ClaudeCodeStudio.sln 위치까지 상향 탐색.
    private static string RepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null && !File.Exists(Path.Combine(dir.FullName, "ClaudeCodeStudio.sln"))) dir = dir.Parent;
        if (dir == null) throw new InvalidOperationException("ClaudeCodeStudio.sln 미발견 — repo 루트 해석 실패");
        return dir.FullName;
    }

    public static string ExePath =>
        Environment.GetEnvironmentVariable("CCS_E2E_EXE")
        ?? Path.Combine(RepoRoot(), "bin", "Debug", "ClaudeCodeStudio.exe");

    /// <param name="unconfigured">true 면 빈 임시 폴더를 repoDir 로 지정해 Sync 를 '초기 설정' 화면으로 띄운다.</param>
    /// <param name="stateDir">앱 로컬 상태 폴더 재사용 (재시작 복원 시나리오). null 이면 테스트별 임시 폴더 자동 생성/삭제.</param>
    /// <param name="claudeDir">Sync 대상 ~/.claude 대체 폴더 (git 픽스처). 정리는 호출자 몫.</param>
    /// <param name="statusbarRoot">StatusBar 모듈 루트(config.json 기록 위치) 오버라이드. 정리는 호출자 몫.</param>
    /// <param name="settingsPath">settings.json 경로 오버라이드 (statusLine 적용 검증). 정리는 호출자 몫.</param>
    /// <param name="ghConfigDir">gh 설정 폴더 오버라이드. 빈 폴더를 주면 이 PC 의 로그인과 무관하게 '미로그인' 으로 보인다.</param>
    /// <param name="pathOverride">앱이 볼 PATH 를 통째로 교체 (도구를 PATH 없이 찾아내는지 확인할 때).</param>
    public static CcsApp Launch(bool unconfigured = false, string? stateDir = null,
                                string? claudeDir = null, string? statusbarRoot = null, string? settingsPath = null,
                                string? ghConfigDir = null, string? pathOverride = null)
    {
        int port = FreePort();
        var psi = new ProcessStartInfo(ExePath) { UseShellExecute = false };
        psi.EnvironmentVariables["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"] = $"--remote-debugging-port={port}";

        // 앱 로컬 상태(창 크기 파일 + WebView2 프로필/localStorage)를 테스트별 임시 폴더로 격리
        bool ownsState = stateDir == null;
        string stateTmp = stateDir ?? Path.Combine(Path.GetTempPath(), "ccs-e2e-state-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stateTmp);
        psi.EnvironmentVariables["CCSTUDIO_STATE_DIR"] = stateTmp;

        if (statusbarRoot != null) psi.EnvironmentVariables["CCSTUDIO_STATUSBAR_ROOT"] = statusbarRoot;
        if (settingsPath != null) psi.EnvironmentVariables["CCSTUDIO_SETTINGS_PATH"] = settingsPath;
        if (ghConfigDir != null) psi.EnvironmentVariables["GH_CONFIG_DIR"] = ghConfigDir;
        if (pathOverride != null) psi.EnvironmentVariables["PATH"] = pathOverride;

        string? tmp = null;
        if (claudeDir != null)
        {
            psi.EnvironmentVariables["CCSTUDIO_CLAUDE_DIR"] = claudeDir;
        }
        else if (unconfigured)
        {
            tmp = Path.Combine(Path.GetTempPath(), "ccs-e2e-empty-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tmp);
            psi.EnvironmentVariables["CCSTUDIO_CLAUDE_DIR"] = tmp;
        }

        var proc = Process.Start(psi) ?? throw new InvalidOperationException("앱 spawn 실패");
        if (!WaitForCdp(port, 20000)) { TryKill(proc); throw new TimeoutException($"CDP 미개방(port={port})"); }

        var driver = CreateDriver(port);
        if (!WaitForAppHandle(driver, 15000))
        {
            try { driver.Quit(); } catch { }
            TryKill(proc);
            throw new InvalidOperationException("app page(https://claudecodestudio.local) 미발견 — launch 실패");
        }

        var app = new CcsApp(proc, driver, port, tmp, stateTmp, ownsState);
        app.WaitForSelector(".tabbar", 10000);
        return app;
    }

    // 일반 Edge 실행 파일. Selenium 의 브라우저 검증을 통과시키는 용도다(실행되지는 않는다).
    private static string? EdgeBinary()
    {
        foreach (string p in new[] {
            @"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
            @"C:\Program Files\Microsoft\Edge\Application\msedge.exe" })
            if (File.Exists(p)) return p;
        return null;
    }

    // 설치된 WebView2 런타임 중 가장 높은 버전의 실행 파일. 드라이버 버전을 맞추는 데 쓴다.
    private static string? WebView2Binary()
    {
        foreach (string root in new[] {
            @"C:\Program Files (x86)\Microsoft\EdgeWebView\Application",
            @"C:\Program Files\Microsoft\EdgeWebView\Application" })
        {
            if (!Directory.Exists(root)) continue;
            string? best = null;
            var bestVer = new Version(0, 0);
            foreach (string dir in Directory.GetDirectories(root))
            {
                string exe = Path.Combine(dir, "msedgewebview2.exe");
                if (!File.Exists(exe)) continue;
                if (Version.TryParse(Path.GetFileName(dir), out var v) && v > bestVer) { bestVer = v; best = exe; }
            }
            if (best != null) return best;
        }
        return null;
    }

    // msedgedriver.exe 가 있는 폴더. CCS_MSEDGEDRIVER_DIR 이 우선이고, 없으면 Selenium Manager 가
    // 받아 둔 캐시에서 고른다 — WebView2 런타임과 같은 버전을 먼저, 없으면 가장 높은 버전을 쓴다.
    private static string MsEdgeDriverDir()
    {
        string? pinned = Environment.GetEnvironmentVariable("CCS_MSEDGEDRIVER_DIR");
        if (!string.IsNullOrEmpty(pinned)) return pinned;

        string cache = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            ".cache", "selenium", "msedgedriver", "win64");
        if (!Directory.Exists(cache))
            throw new InvalidOperationException(
                $"msedgedriver 캐시 미발견: {cache} — CCS_MSEDGEDRIVER_DIR 로 폴더를 지정하세요.");

        var dirs = Directory.GetDirectories(cache)
            .Where(d => File.Exists(Path.Combine(d, "msedgedriver.exe")))
            .ToArray();

        string? want = WebView2Binary() is string exe ? Path.GetFileName(Path.GetDirectoryName(exe)!) : null;
        string? exact = dirs.FirstOrDefault(d => Path.GetFileName(d) == want);
        if (exact != null) return exact;

        string? newest = dirs
            .Where(d => Version.TryParse(Path.GetFileName(d), out _))
            .OrderByDescending(d => Version.Parse(Path.GetFileName(d)))
            .FirstOrDefault();
        return newest ?? throw new InvalidOperationException($"msedgedriver.exe 미발견: {cache}");
    }

    private static EdgeDriver CreateDriver(int port)
    {
        // UseWebView 는 브라우저를 새로 띄울 때 쓰는 옵션이라 켜지 않는다 — DebuggerAddress 로
        // 이미 떠 있는 앱에 붙기만 한다.
        // 그런데도 Selenium 은 브라우저 바이너리를 확인한다. webview2 로 해석되면 Selenium Manager 가
        // "<WebView2 설치폴더>\msedge.exe" 를 조립해 내놓는데 그런 파일은 없다 — 그 폴더의 실행 파일은
        // msedgewebview2.exe 다(selenium-manager 를 직접 돌려 확인). 그래서 일반 Edge 를 일러 준다.
        // attach 전용이라 이 바이너리가 실행되는 일은 없다. 드라이버는 아래에서 따로 고른다.
        var options = new EdgeOptions { DebuggerAddress = $"127.0.0.1:{port}" };
        string? binary = EdgeBinary();
        if (binary != null) options.BinaryLocation = binary;

        var service = EdgeDriverService.CreateDefaultService(MsEdgeDriverDir(), "msedgedriver.exe");
        service.HideCommandPromptWindow = true;
        return new EdgeDriver(service, options, TimeSpan.FromSeconds(60));
    }

    private static int FreePort()
    {
        var l = new TcpListener(IPAddress.Loopback, 0);
        l.Start();
        int p = ((IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return p;
    }

    private static bool WaitForCdp(int port, int timeoutMs)
    {
        using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(2) };
        var sw = Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < timeoutMs)
        {
            try { if (http.GetAsync($"http://127.0.0.1:{port}/json/version").GetAwaiter().GetResult().IsSuccessStatusCode) return true; }
            catch { /* not ready */ }
            Thread.Sleep(200);
        }
        return false;
    }

    private static bool WaitForAppHandle(EdgeDriver driver, int timeoutMs)
    {
        var sw = Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < timeoutMs)
        {
            foreach (var h in driver.WindowHandles)
            {
                try { driver.SwitchTo().Window(h); } catch { continue; }
                if (driver.Url.StartsWith("https://claudecodestudio.local")) return true;
            }
            Thread.Sleep(300);
        }
        return false;
    }

    public IWebElement WaitForSelector(string css, int timeoutMs)
    {
        var sw = Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < timeoutMs)
        {
            var els = Driver.FindElements(By.CssSelector(css));
            if (els.Count > 0) return els[0];
            Thread.Sleep(150);
        }
        throw new WebDriverTimeoutException($"selector 미발견: {css}");
    }

    /// <summary>모듈 iframe(sync/statusbar) 으로 컨텍스트 전환. top 복귀는 Driver.SwitchTo().DefaultContent().</summary>
    public void SwitchToModule(string id)
    {
        Driver.SwitchTo().DefaultContent();
        var frame = WaitForSelector($"#frame-{id}", 10000);
        Driver.SwitchTo().Frame(frame);
    }

    /// <summary>조건 충족까지 poll(없으면 마지막 값 반환).</summary>
    public static T WaitUntil<T>(Func<T> sample, Func<T, bool> ok, int timeoutMs, int pollMs = 200)
    {
        var sw = Stopwatch.StartNew();
        T last = sample();
        while (sw.ElapsedMilliseconds < timeoutMs)
        {
            if (ok(last)) return last;
            Thread.Sleep(pollMs);
            last = sample();
        }
        return last;
    }

    public void Dispose()
    {
        try { Driver.Quit(); } catch { }
        // 우선 정상 종료(WM_CLOSE) 시도 — WebView2 프로필(localStorage)이 디스크에 플러시되어
        // stateDir 재사용(재시작 복원) 시나리오가 안정된다. 실패 시 강제 종료.
        try { if (!_proc.HasExited && _proc.CloseMainWindow()) _proc.WaitForExit(5000); } catch { }
        TryKill(_proc);
        try { _proc.WaitForExit(10000); } catch { }
        if (_tempClaudeDir != null) { try { Directory.Delete(_tempClaudeDir, true); } catch { } }
        if (_ownsStateDir) { try { Directory.Delete(_stateDir, true); } catch { } }
    }

    private static void TryKill(Process proc)
    {
        try { if (!proc.HasExited) proc.Kill(entireProcessTree: true); } catch { }
    }
}
