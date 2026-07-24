<#
  격리데스크톱러너.ps1 — 임의 명령을 별도 Windows 데스크톱(CreateDesktop)에서 실행하는 범용 러너.
  테스트가 spawn 하는 GUI 창이 사용자 데스크톱에 뜨지 않아 포커스·수동 작업을 방해하지 않는다.

  원리:
  - CreateProcess 의 STARTUPINFO.lpDesktop 지정으로 자식 프로세스 트리 전체가 격리 데스크톱을 상속한다.
  - 격리 데스크톱에는 실제 OS 입력(SendInput 등)이 닿지 않는다 → 테스트의 앱 조작은 CDP(remote
    debugging) / 프레임워크 내부 이벤트(QTest 등) 같은 "OS 입력을 쓰지 않는 주입 방식" 이어야 한다.
  - 원본(검증됨): MDViewer 레포 Tests/E2E.Net/E2ETest.ps1 — WebDriver(CDP) 기반 E2E 를 이 방식으로
    상시 실행 중 (click/sendKeys/drag 동작 PoC 검증).

  주의:
  - Windows 전용. pwsh(PowerShell 7+) 로 실행 권장.
  - 타임아웃 초과 시 프로세스를 강제 종료하지 않고 반환한다(exit code 259 = 아직 실행 중).
    잔류 프로세스는 taskkill /T /PID <출력된 pid> 로 정리한다.

  사용:
    pwsh Tests/격리데스크톱러너.ps1 -Command 'dotnet test Tests/E2E.Net/ClaudeCodeStudio.E2E.csproj -c Debug'
    pwsh Tests/격리데스크톱러너.ps1 -Command 'dotnet test ...' -TimeoutSec 600
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Command,                                  # 격리 데스크톱 안에서 실행할 명령줄
    [string]$WorkingDir = (Get-Location).Path,         # 명령의 작업 디렉토리
    [int]$TimeoutSec = 900,
    [string]$DesktopName = 'ccs-e2e-iso',
    [string]$LogPath = (Join-Path $env:TEMP 'ccs-iso-e2e.log')
)
$ErrorActionPreference = 'Stop'

# 마샬링은 C# 메서드 내부에서 처리(PowerShell 구조체 [ref] 마샬링 아티팩트 회피).
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.ComponentModel;
public static class IsoDesk {
  [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern IntPtr CreateDesktop(string d, IntPtr dev, IntPtr dm, int flags, uint access, IntPtr sa);
  [DllImport("user32.dll", SetLastError=true)] static extern bool CloseDesktop(IntPtr h);
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
  struct STARTUPINFO { public int cb; public string lpReserved, lpDesktop, lpTitle;
    public int dwX,dwY,dwXSize,dwYSize,dwXCountChars,dwYCountChars,dwFillAttribute,dwFlags;
    public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError; }
  [StructLayout(LayoutKind.Sequential)]
  struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  static extern bool CreateProcess(string app, string cmd, IntPtr pa, IntPtr ta, bool inherit,
    uint flags, IntPtr env, string cwd, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll", SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h, uint ms);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h, out uint code);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool CloseHandle(IntPtr h);

  // 새 데스크톱 생성 → 그 안에서 cmd 실행(자식 트리 전체가 격리 데스크톱 상속) → 종료코드 반환.
  public static int RunInDesktop(string deskName, string cmdLine, string cwd, int timeoutMs, out int pid) {
    pid = 0;
    IntPtr hDesk = CreateDesktop(deskName, IntPtr.Zero, IntPtr.Zero, 0, 0x10000000 /*GENERIC_ALL*/, IntPtr.Zero);
    if (hDesk == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateDesktop");
    try {
      STARTUPINFO si = new STARTUPINFO();
      si.cb = Marshal.SizeOf(typeof(STARTUPINFO));
      si.lpDesktop = deskName;
      PROCESS_INFORMATION pi;
      // CREATE_NO_WINDOW(0x08000000) + CREATE_UNICODE_ENVIRONMENT(0x400): 콘솔 창을 만들지 않는다.
      // (기본 터미널이 Windows Terminal(DefTerm)이면 CREATE_NEW_CONSOLE 은 격리 데스크톱을 우회해 사용자
      //  기본 데스크톱의 Windows Terminal 탭으로 떠버린다. 콘솔 출력은 호출부가 > log 로 리다이렉트하므로
      //  콘솔이 불필요. GUI 창은 lpDesktop 으로 격리 데스크톱에 그대로 생성된다.)
      bool ok = CreateProcess(null, cmdLine, IntPtr.Zero, IntPtr.Zero, false, 0x08000000 | 0x400, IntPtr.Zero, cwd, ref si, out pi);
      if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateProcess");
      pid = pi.dwProcessId;
      WaitForSingleObject(pi.hProcess, (uint)timeoutMs);
      uint code; GetExitCodeProcess(pi.hProcess, out code);
      CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
      return (int)code;
    } finally { CloseDesktop(hDesk); }
  }
}
"@

$cmdLine = "cmd.exe /c $Command > `"$LogPath`" 2>&1"

$pid2 = 0
$code = [IsoDesk]::RunInDesktop($DesktopName, $cmdLine, $WorkingDir, [int]($TimeoutSec * 1000), [ref]$pid2)
Write-Host "격리 데스크톱 '$DesktopName' 에서 실행 완료 (pid=$pid2) — 사용자 데스크톱 무간섭"

if (Test-Path $LogPath) { Write-Host "--- 실행 로그 (tail) ---"; Get-Content $LogPath -Tail 30 }
Write-Host "exit code: $code"
exit $code
