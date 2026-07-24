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
    public EdgeDriver Driver { get; }
    public int Port { get; }
    public IJavaScriptExecutor Js => (IJavaScriptExecutor)Driver;

    private CcsApp(Process proc, EdgeDriver driver, int port, string? tempClaudeDir)
    {
        _proc = proc; Driver = driver; Port = port; _tempClaudeDir = tempClaudeDir;
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
    public static CcsApp Launch(bool unconfigured = false)
    {
        int port = FreePort();
        var psi = new ProcessStartInfo(ExePath) { UseShellExecute = false };
        psi.EnvironmentVariables["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"] = $"--remote-debugging-port={port}";

        string? tmp = null;
        if (unconfigured)
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

        var app = new CcsApp(proc, driver, port, tmp);
        app.WaitForSelector(".tabbar", 10000);
        return app;
    }

    private static EdgeDriver CreateDriver(int port)
    {
        var options = new EdgeOptions { UseWebView = true, DebuggerAddress = $"127.0.0.1:{port}" };
        string? driverDir = Environment.GetEnvironmentVariable("CCS_MSEDGEDRIVER_DIR");
        // 핀 디렉터리 지정 시 그 드라이버, 미지정 시 Selenium Manager 가 설치 Edge 버전 정합 드라이버를 해석.
        EdgeDriverService service = !string.IsNullOrEmpty(driverDir)
            ? EdgeDriverService.CreateDefaultService(driverDir, "msedgedriver.exe")
            : EdgeDriverService.CreateDefaultService();
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
        TryKill(_proc);
        try { _proc.WaitForExit(10000); } catch { }
        if (_tempClaudeDir != null) { try { Directory.Delete(_tempClaudeDir, true); } catch { } }
    }

    private static void TryKill(Process proc)
    {
        try { if (!proc.HasExited) proc.Kill(entireProcessTree: true); } catch { }
    }
}
