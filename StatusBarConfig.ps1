# StatusBarConfig.ps1
# WinForms GUI editor for Claude Code statusLine.
# - Top: multi-row preview (drag&drop to arrange)
# - Bottom: palette of available items (drag up to add, drag preview item here to delete)
# - Save & Apply: writes config.json + updates ~/.claude/settings.json statusLine

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

if (-not ('NativeWinUtil' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NativeWinUtil {
    [DllImport("user32.dll")] static extern int SendMessage(IntPtr hWnd, int Msg, int wParam, int lParam);
    const int WM_SETREDRAW = 0x000B;
    public static void SuspendDrawing(IntPtr handle) { SendMessage(handle, WM_SETREDRAW, 0, 0); }
    public static void ResumeDrawing(IntPtr handle)  { SendMessage(handle, WM_SETREDRAW, 1, 0); }
}
'@
}

function Enable-DoubleBuffer($ctrl) {
    try {
        $bf = [System.Reflection.BindingFlags]::Instance -bor [System.Reflection.BindingFlags]::NonPublic
        $prop = [System.Windows.Forms.Control].GetProperty('DoubleBuffered', $bf)
        $prop.SetValue($ctrl, $true, $null)
    } catch {}
}

if (-not ('HScrollWheelFilter' -as [type])) {
    Add-Type -ReferencedAssemblies System.Windows.Forms,System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
public class HScrollWheelFilter : IMessageFilter {
    [DllImport("user32.dll")] static extern short GetKeyState(int keyCode);
    public Control Target;
    public Action<int> OnHorizontalScroll;
    public bool PreFilterMessage(ref Message m) {
        if (m.Msg != 0x020A) return false;            // WM_MOUSEWHEEL
        if ((GetKeyState(0x10) & 0x8000) == 0) return false;  // VK_SHIFT not held
        if (Target == null || Target.IsDisposed) return false;
        try {
            Point clientPt = Target.PointToClient(Cursor.Position);
            if (!Target.ClientRectangle.Contains(clientPt)) return false;
        } catch { return false; }
        long wp = m.WParam.ToInt64();
        int delta = (short)((wp >> 16) & 0xFFFF);
        if (OnHorizontalScroll != null) OnHorizontalScroll(delta);
        return true;
    }
}
'@
}

$ScriptDir       = Split-Path -Parent $MyInvocation.MyCommand.Path
$ConfigPath      = Join-Path $ScriptDir 'config.json'
$StatusLineScript = Join-Path $ScriptDir 'StatusLine.ps1'
$ClaudeSettings  = Join-Path $env:USERPROFILE '.claude\settings.json'

# ===== Item Catalog =====
$ItemTypes = [ordered]@{
    # Claude
    model       = @{ Label = '모델명';        Sample = 'Opus 4.7';        Desc = '현재 모델 표시명';        Cat = 'Claude' }
    version     = @{ Label = '버전';          Sample = '2.5.1';           Desc = 'Claude Code 버전';        Cat = 'Claude' }
    session     = @{ Label = '세션ID';        Sample = 'abc123';          Desc = '세션 ID 앞 8자';          Cat = 'Claude' }
    duration    = @{ Label = 'duration';      Sample = '12.3s';           Desc = '세션 누적 작업 시간';     Cat = 'Claude' }
    plan        = @{ Label = '플랜';          Sample = 'max20x';          Desc = '감지된 구독 플랜';        Cat = 'Claude' }
    ctx_size    = @{ Label = '컨텍스트 크기'; Sample = '1M';              Desc = '컨텍스트 윈도우 크기';    Cat = 'Claude' }
    effort      = @{ Label = 'effort';        Sample = 'xhigh';           Desc = 'thinking effort 수준';    Cat = 'Claude' }
    # 워크스페이스
    dir_short   = @{ Label = '현재폴더';      Sample = 'project';         Desc = '현재 폴더 이름만';        Cat = '워크스페이스' }
    dir_full    = @{ Label = '전체경로';      Sample = 'D:\Repo\project'; Desc = '현재 폴더 전체 경로';     Cat = '워크스페이스' }
    project_dir = @{ Label = '프로젝트 폴더'; Sample = 'D:\Repo';         Desc = 'Claude Code 실행 폴더';   Cat = '워크스페이스' }
    # Git
    git_branch  = @{ Label = '브랜치';        Sample = 'main';            Desc = '현재 git 브랜치';         Cat = 'Git' }
    git_commit  = @{ Label = '커밋(short)';   Sample = 'a1b2c3d';         Desc = 'HEAD 커밋 짧은 해시';     Cat = 'Git' }
    git_changes = @{ Label = '변경파일수';    Sample = '3';               Desc = '변경된 파일 개수';        Cat = 'Git' }
    git_ahead_behind = @{ Label = 'ahead/behind'; Sample = '↑1 ↓0';       Desc = '원격과의 차이';           Cat = 'Git' }
    git_user    = @{ Label = '작업자';        Sample = 'dh.kim';          Desc = 'git user.name';           Cat = 'Git' }
    # 시간
    time        = @{ Label = '시각';          Sample = '14:30';           Desc = 'HH:mm';                   Cat = '시간' }
    date        = @{ Label = '날짜';          Sample = '2026-05-02';      Desc = 'yyyy-MM-dd';              Cat = '시간' }
    weekday     = @{ Label = '요일';          Sample = '토';              Desc = '한글 요일 한 글자';       Cat = '시간' }
    session_elapsed = @{ Label = '경과시간';  Sample = '00:23';           Desc = '세션 작업 경과 시간';     Cat = '시간' }
    # 시스템
    user        = @{ Label = '사용자';        Sample = 'dh.kim';          Desc = 'Windows 사용자명';        Cat = '시스템' }
    host        = @{ Label = '호스트';        Sample = 'PC-01';           Desc = '컴퓨터 이름';             Cat = '시스템' }
    os          = @{ Label = 'OS';            Sample = 'Win11';           Desc = 'OS 버전 약식';            Cat = '시스템' }
    # 사용률
    ctx_pct       = @{ Label = '컨텍스트 %';        Sample = '12%';             Desc = '컨텍스트 사용률';         Cat = '사용률' }
    ctx_bar       = @{ Label = '컨텍스트 막대 █';   Sample = '█▌░░░░░░░░';      Desc = '컨텍스트 블록 그래프';    Cat = '사용률' }
    ctx_bar_ascii = @{ Label = '컨텍스트 막대 [#]'; Sample = '[#---------]';    Desc = '컨텍스트 ASCII 그래프';   Cat = '사용률' }
    ctx_bar_dot   = @{ Label = '컨텍스트 막대 ●';   Sample = '●○○○○○○○○○';      Desc = '컨텍스트 점 그래프';      Cat = '사용률' }
    h5_pct        = @{ Label = '5시간 %';           Sample = '34%';             Desc = '5시간 한도 사용률';       Cat = '사용률' }
    h5_bar        = @{ Label = '5시간 막대 █';      Sample = '███▍░░░░░░';      Desc = '5시간 블록 그래프';       Cat = '사용률' }
    h5_bar_ascii  = @{ Label = '5시간 막대 [#]';    Sample = '[###-------]';    Desc = '5시간 ASCII 그래프';      Cat = '사용률' }
    h5_bar_dot    = @{ Label = '5시간 막대 ●';      Sample = '●●●○○○○○○○';      Desc = '5시간 점 그래프';         Cat = '사용률' }
    week_pct      = @{ Label = '주간 %';            Sample = '8%';              Desc = '주간 한도 사용률';        Cat = '사용률' }
    week_bar      = @{ Label = '주간 막대 █';       Sample = '▊░░░░░░░░░';      Desc = '주간 블록 그래프';        Cat = '사용률' }
    week_bar_ascii = @{ Label = '주간 막대 [#]';    Sample = '[#---------]';    Desc = '주간 ASCII 그래프';       Cat = '사용률' }
    week_bar_dot  = @{ Label = '주간 막대 ●';       Sample = '●○○○○○○○○○';      Desc = '주간 점 그래프';          Cat = '사용률' }
    ctx_cost    = @{ Label = '컨텍스트 비용'; Sample = '$0.05';           Desc = '현재 세션 비용';          Cat = '사용률' }
    h5_cost     = @{ Label = '5시간 비용';    Sample = '$1.20';           Desc = '최근 5시간 사용 비용';    Cat = '사용률' }
    week_cost   = @{ Label = '주간 비용';     Sample = '$15.30';          Desc = '최근 7일 사용 비용';      Cat = '사용률' }
    # 아이콘 (같은 카테고리 항목을 한 줄에 묶을 때 사용)
    icon_model    = @{ Label = '모델 아이콘';     Sample = '🤖'; Desc = '모델 관련 항목 앞';        Cat = '아이콘' }
    icon_version  = @{ Label = '버전 아이콘';     Sample = '🏷️'; Desc = '버전 항목 앞';             Cat = '아이콘' }
    icon_session  = @{ Label = '세션 아이콘';     Sample = '🔖'; Desc = '세션 ID 앞';               Cat = '아이콘' }
    icon_plan     = @{ Label = '플랜 아이콘';     Sample = '⭐'; Desc = '플랜 항목 앞';             Cat = '아이콘' }
    icon_ctx      = @{ Label = '컨텍스트 아이콘'; Sample = '🧠'; Desc = '컨텍스트 관련 항목 앞';    Cat = '아이콘' }
    icon_effort   = @{ Label = 'effort 아이콘';   Sample = '💭'; Desc = 'thinking effort 앞';       Cat = '아이콘' }
    icon_cost     = @{ Label = '비용 아이콘';     Sample = '💰'; Desc = '비용 관련 항목 앞';        Cat = '아이콘' }
    icon_duration = @{ Label = '소요시간 아이콘'; Sample = '⏱️'; Desc = 'duration/경과시간 앞';      Cat = '아이콘' }
    icon_dir      = @{ Label = '폴더 아이콘';     Sample = '📁'; Desc = '경로/폴더 항목 앞';        Cat = '아이콘' }
    icon_git      = @{ Label = 'Git 아이콘';      Sample = '🌿'; Desc = 'git 관련 항목 앞';         Cat = '아이콘' }
    icon_time     = @{ Label = '시각 아이콘';     Sample = '🕐'; Desc = '시각 항목 앞';             Cat = '아이콘' }
    icon_date     = @{ Label = '날짜 아이콘';     Sample = '📅'; Desc = '날짜/요일 항목 앞';        Cat = '아이콘' }
    icon_user     = @{ Label = '사용자 아이콘';   Sample = '👤'; Desc = '사용자 관련 항목 앞';      Cat = '아이콘' }
    icon_host     = @{ Label = '호스트 아이콘';   Sample = '🖥️'; Desc = '호스트 항목 앞';           Cat = '아이콘' }
    icon_os       = @{ Label = 'OS 아이콘';       Sample = '💻'; Desc = 'OS 항목 앞';               Cat = '아이콘' }
    icon_h5       = @{ Label = '5시간 아이콘';    Sample = '⏱️'; Desc = '5시간 사용률 항목 앞';      Cat = '아이콘' }
    icon_week     = @{ Label = '주간 아이콘';     Sample = '🗓️'; Desc = '주간 사용률 항목 앞';      Cat = '아이콘' }
    # 구분자/포맷
    sep_pipe    = @{ Label = '파이프';        Sample = '|';               Desc = '구분 기호';               Cat = '구분자/포맷' }
    sep_dot     = @{ Label = '가운뎃점';      Sample = '•';               Desc = '구분 기호';               Cat = '구분자/포맷' }
    sep_dash    = @{ Label = '대시';          Sample = '-';               Desc = '구분 기호';               Cat = '구분자/포맷' }
    sep_arrow   = @{ Label = '꺾쇠';          Sample = '>';               Desc = '구분 기호';               Cat = '구분자/포맷' }
    sep_slash   = @{ Label = '슬래시';        Sample = '/';               Desc = '구분 기호';               Cat = '구분자/포맷' }
    sep_colon   = @{ Label = '콜론';          Sample = ':';               Desc = '구분 기호';               Cat = '구분자/포맷' }
    space       = @{ Label = '공백';          Sample = '·';               Desc = '한 칸 공백';              Cat = '구분자/포맷' }
    text        = @{ Label = '텍스트';        Sample = 'TEXT';            Desc = '사용자 지정 텍스트';      Cat = '구분자/포맷' }
}

# Category display order (구분자/포맷은 별도 고정 영역에 표시)
$CategoryOrder = @('Claude', '워크스페이스', 'Git', '시간', '시스템', '사용률', '아이콘')
$AlwaysVisibleCategory = '구분자/포맷'

# ===== Layout Model =====
$script:Layout = $null
$script:DragSource = $null
$script:IsDirty = $false

function Set-Dirty {
    $script:IsDirty = $true
    if ($null -ne $script:btnReload) { $script:btnReload.Enabled = $true }
}

function Reset-Dirty {
    $script:IsDirty = $false
    if ($null -ne $script:btnReload) { $script:btnReload.Enabled = $false }
}

function New-EmptyRow { ,(New-Object System.Collections.ArrayList) }

function Load-Layout {
    if (Test-Path $ConfigPath) {
        try {
            $obj = Get-Content $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
            $rows = New-Object System.Collections.ArrayList
            foreach ($r in $obj.rows) {
                $row = New-Object System.Collections.ArrayList
                foreach ($it in $r) {
                    $h = @{ type = [string]$it.type }
                    if ($null -ne $it.value) { $h.value = [string]$it.value }
                    [void]$row.Add($h)
                }
                [void]$rows.Add($row)
            }
            if ($rows.Count -eq 0) { [void]$rows.Add((New-Object System.Collections.ArrayList)) }
            return @{ rows = $rows }
        } catch {}
    }
    # Default layout
    $rows = New-Object System.Collections.ArrayList
    $r1 = New-Object System.Collections.ArrayList
    [void]$r1.Add(@{type='model'})
    [void]$r1.Add(@{type='space'})
    [void]$r1.Add(@{type='sep_pipe'})
    [void]$r1.Add(@{type='space'})
    [void]$r1.Add(@{type='dir_short'})
    [void]$rows.Add($r1)
    $r2 = New-Object System.Collections.ArrayList
    [void]$r2.Add(@{type='git_branch'})
    [void]$r2.Add(@{type='space'})
    [void]$r2.Add(@{type='sep_pipe'})
    [void]$r2.Add(@{type='space'})
    [void]$r2.Add(@{type='time'})
    [void]$rows.Add($r2)
    return @{ rows = $rows }
}

function Save-Layout {
    $clean = @{ rows = @() }
    foreach ($row in $script:Layout.rows) {
        $r = @()
        foreach ($it in $row) {
            if ($it.ContainsKey('value')) {
                $r += @{ type = $it.type; value = $it.value }
            } else {
                $r += @{ type = $it.type }
            }
        }
        $clean.rows += ,$r
    }
    $clean | ConvertTo-Json -Depth 10 | Set-Content $ConfigPath -Encoding UTF8
}

function Apply-To-ClaudeSettings {
    $settingsDir = Split-Path -Parent $ClaudeSettings
    if (-not (Test-Path $settingsDir)) { New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null }

    $obj = $null
    if (Test-Path $ClaudeSettings) {
        try { $obj = Get-Content $ClaudeSettings -Raw -Encoding UTF8 | ConvertFrom-Json } catch {}
    }
    if ($null -eq $obj) { $obj = New-Object PSObject }

    $cmd = "powershell -NoProfile -ExecutionPolicy Bypass -File `"$StatusLineScript`""
    $statusLine = New-Object PSObject
    $statusLine | Add-Member -MemberType NoteProperty -Name type    -Value 'command'
    $statusLine | Add-Member -MemberType NoteProperty -Name command -Value $cmd

    if ($obj.PSObject.Properties.Match('statusLine').Count -gt 0) {
        $obj.statusLine = $statusLine
    } else {
        $obj | Add-Member -MemberType NoteProperty -Name statusLine -Value $statusLine
    }
    $obj | ConvertTo-Json -Depth 20 | Set-Content $ClaudeSettings -Encoding UTF8
}

# ===== Helpers =====
function Prompt-Text($title, $default = '') {
    $f = New-Object System.Windows.Forms.Form
    $f.Text = $title
    $f.ClientSize = New-Object System.Drawing.Size(380, 90)
    $f.StartPosition = 'CenterParent'
    $f.FormBorderStyle = 'FixedDialog'
    $f.MaximizeBox = $false; $f.MinimizeBox = $false

    $tb = New-Object System.Windows.Forms.TextBox
    $tb.Location = New-Object System.Drawing.Point(12, 14)
    $tb.Size     = New-Object System.Drawing.Size(355, 24)
    $tb.Text = $default
    $f.Controls.Add($tb)

    $ok = New-Object System.Windows.Forms.Button
    $ok.Text = '확인'
    $ok.Location = New-Object System.Drawing.Point(212, 50)
    $ok.Size     = New-Object System.Drawing.Size(75, 28)
    $ok.DialogResult = 'OK'
    $f.Controls.Add($ok); $f.AcceptButton = $ok

    $cc = New-Object System.Windows.Forms.Button
    $cc.Text = '취소'
    $cc.Location = New-Object System.Drawing.Point(292, 50)
    $cc.Size     = New-Object System.Drawing.Size(75, 28)
    $cc.DialogResult = 'Cancel'
    $f.Controls.Add($cc); $f.CancelButton = $cc

    if ($f.ShowDialog() -eq 'OK') { return $tb.Text }
    return $null
}

function New-ItemPanel($itemData, $isPalette, $r, $c) {
    $type = $itemData.type
    $info = $ItemTypes[$type]
    if ($null -eq $info) { $info = @{ Label = "?$type"; Sample = '?'; Desc = '' } }
    $sample = $info.Sample
    if ($type -eq 'text' -and $itemData.ContainsKey('value')) {
        $sample = if ($itemData.value -eq '') { '(빈 텍스트)' } else { $itemData.value }
    }
    $desc = if ($info.ContainsKey('Desc')) { [string]$info.Desc } else { '' }

    $bg = if ($isPalette) {
        [System.Drawing.Color]::FromArgb(232, 240, 252)
    } else {
        switch -Wildcard ($type) {
            'sep_*' { [System.Drawing.Color]::FromArgb(250, 240, 215) }
            'space' { [System.Drawing.Color]::FromArgb(238, 238, 238) }
            'text'  { [System.Drawing.Color]::FromArgb(220, 245, 220) }
            default { [System.Drawing.Color]::FromArgb(214, 228, 248) }
        }
    }

    $panel = New-Object System.Windows.Forms.Panel
    $panel.AutoSize = $true
    $panel.AutoSizeMode = 'GrowAndShrink'
    $panel.BorderStyle = 'FixedSingle'
    $panel.BackColor = $bg
    $panel.MinimumSize = New-Object System.Drawing.Size(140, 0)
    $panel.Margin = New-Object System.Windows.Forms.Padding(3)
    $panel.Padding = New-Object System.Windows.Forms.Padding(0)

    $stack = New-Object System.Windows.Forms.FlowLayoutPanel
    $stack.FlowDirection = 'TopDown'
    $stack.WrapContents = $false
    $stack.AutoSize = $true
    $stack.AutoSizeMode = 'GrowAndShrink'
    $stack.Margin = New-Object System.Windows.Forms.Padding(0)
    $stack.Padding = New-Object System.Windows.Forms.Padding(8, 6, 8, 6)
    $stack.BackColor = [System.Drawing.Color]::Transparent
    $panel.Controls.Add($stack)

    $lblLabel = New-Object System.Windows.Forms.Label
    $lblLabel.Text = $info.Label
    $lblLabel.Font = New-Object System.Drawing.Font('Segoe UI', 9, [System.Drawing.FontStyle]::Bold)
    $lblLabel.AutoSize = $true
    $lblLabel.Margin = New-Object System.Windows.Forms.Padding(0)
    $lblLabel.Padding = New-Object System.Windows.Forms.Padding(0)
    $lblLabel.Cursor = [System.Windows.Forms.Cursors]::Hand
    $stack.Controls.Add($lblLabel)

    $lblSample = New-Object System.Windows.Forms.Label
    $lblSample.Text = $sample
    $lblSample.Font = New-Object System.Drawing.Font('Consolas', 9)
    $lblSample.ForeColor = [System.Drawing.Color]::FromArgb(60, 90, 140)
    $lblSample.AutoSize = $true
    $lblSample.Margin = New-Object System.Windows.Forms.Padding(0, 2, 0, 0)
    $lblSample.Padding = New-Object System.Windows.Forms.Padding(0)
    $lblSample.Cursor = [System.Windows.Forms.Cursors]::Hand
    $stack.Controls.Add($lblSample)

    $lblDesc = $null
    if ($desc) {
        $lblDesc = New-Object System.Windows.Forms.Label
        $lblDesc.Text = $desc
        $lblDesc.Font = New-Object System.Drawing.Font('Segoe UI', 8)
        $lblDesc.ForeColor = [System.Drawing.Color]::Gray
        $lblDesc.AutoSize = $true
        $lblDesc.Margin = New-Object System.Windows.Forms.Padding(0, 3, 0, 0)
        $lblDesc.Padding = New-Object System.Windows.Forms.Padding(0)
        $lblDesc.Cursor = [System.Windows.Forms.Cursors]::Hand
        $stack.Controls.Add($lblDesc)
    }

    $tag = @{ IsItem = $true; IsPalette = $isPalette; Type = $type; Row = $r; Col = $c; Data = $itemData }
    $panel.Tag = $tag
    $stack.Tag = $tag
    $lblLabel.Tag = $tag
    $lblSample.Tag = $tag
    if ($lblDesc) { $lblDesc.Tag = $tag }

    $startDrag = {
        param($s, $e)
        if ($e.Button -ne [System.Windows.Forms.MouseButtons]::Left) { return }
        $t = $s.Tag
        if ($t.IsPalette) {
            $script:DragSource = @{ From = 'palette'; ItemType = $t.Type }
        } else {
            $script:DragSource = @{ From = 'preview'; Row = $t.Row; Col = $t.Col }
        }
        $ctrl = $s
        while ($ctrl -isnot [System.Windows.Forms.Panel] -or $ctrl -is [System.Windows.Forms.FlowLayoutPanel]) {
            if ($null -eq $ctrl.Parent) { break }
            $ctrl = $ctrl.Parent
        }
        [void]$ctrl.DoDragDrop("ITEM", [System.Windows.Forms.DragDropEffects]::Move)
    }
    $panel.Add_MouseDown($startDrag)
    $stack.Add_MouseDown($startDrag)
    $lblLabel.Add_MouseDown($startDrag)
    $lblSample.Add_MouseDown($startDrag)
    if ($lblDesc) { $lblDesc.Add_MouseDown($startDrag) }

    if (-not $isPalette) {
        $panel.Add_MouseEnter($script:FocusPreview)
        $stack.Add_MouseEnter($script:FocusPreview)
        $lblLabel.Add_MouseEnter($script:FocusPreview)
        $lblSample.Add_MouseEnter($script:FocusPreview)
        if ($lblDesc) { $lblDesc.Add_MouseEnter($script:FocusPreview) }
    }

    if (-not $isPalette) {
        $cms = New-Object System.Windows.Forms.ContextMenuStrip
        $delMi = $cms.Items.Add('삭제')
        $delMi.Tag = @{ Row = $r; Col = $c }
        $delMi.Add_Click({
            param($s, $e)
            $t = $s.Tag
            if ($null -eq $t) { return }
            if ($t.Row -ge $script:Layout.rows.Count) { return }
            if ($t.Col -ge $script:Layout.rows[$t.Row].Count) { return }
            $script:Layout.rows[$t.Row].RemoveAt($t.Col)
            Set-Dirty
            Render-Preview
        })
        if ($type -eq 'text') {
            $editMi = $cms.Items.Add('텍스트 편집')
            $editMi.Tag = @{ Row = $r; Col = $c }
            $editMi.Add_Click({
                param($s, $e)
                $t = $s.Tag
                if ($null -eq $t) { return }
                if ($t.Row -ge $script:Layout.rows.Count) { return }
                if ($t.Col -ge $script:Layout.rows[$t.Row].Count) { return }
                $cur = ''
                if ($script:Layout.rows[$t.Row][$t.Col].ContainsKey('value')) {
                    $cur = $script:Layout.rows[$t.Row][$t.Col].value
                }
                $new = Prompt-Text '텍스트 편집' $cur
                if ($null -ne $new) {
                    $script:Layout.rows[$t.Row][$t.Col].value = $new
                    Set-Dirty
                    Render-Preview
                }
            })
        }
        $panel.ContextMenuStrip = $cms
        $stack.ContextMenuStrip = $cms
        $lblLabel.ContextMenuStrip = $cms
        $lblSample.ContextMenuStrip = $cms
        if ($lblDesc) { $lblDesc.ContextMenuStrip = $cms }
    }
    return $panel
}

# ===== Form =====
$form = New-Object System.Windows.Forms.Form
$form.Text = 'Claude Code StatusBar 편집기'
$form.ClientSize = New-Object System.Drawing.Size(1100, 1000)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'Sizable'
$form.MinimumSize = New-Object System.Drawing.Size(700, 500)

$mainTable = New-Object System.Windows.Forms.TableLayoutPanel
$mainTable.Dock = 'Fill'
$mainTable.ColumnCount = 1
$mainTable.RowCount = 3
$mainTable.Padding = New-Object System.Windows.Forms.Padding(10, 10, 10, 10)
[void]$mainTable.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 100)))
[void]$mainTable.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Percent, 50)))
[void]$mainTable.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Percent, 50)))
[void]$mainTable.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Absolute, 50)))
$form.Controls.Add($mainTable)

# Preview
$previewBox = New-Object System.Windows.Forms.GroupBox
$previewBox.Text = '미리보기 (드래그 = 이동, 우클릭 = 삭제, 줄 사이로 드롭 = 새 줄)'
$previewBox.Dock = 'Fill'
$previewBox.Margin = New-Object System.Windows.Forms.Padding(0, 0, 0, 5)
$mainTable.Controls.Add($previewBox, 0, 0)

$previewFlow = New-Object System.Windows.Forms.FlowLayoutPanel
$previewFlow.Dock = 'Fill'
$previewFlow.Padding = New-Object System.Windows.Forms.Padding(2, 2, 2, 2)
$previewFlow.FlowDirection = 'TopDown'
$previewFlow.WrapContents = $false
$previewFlow.AutoScroll = $true
$previewFlow.BackColor = [System.Drawing.Color]::White
$previewFlow.BorderStyle = 'FixedSingle'
$previewFlow.TabStop = $true
$previewFlow.AllowDrop = $true
$previewBox.Controls.Add($previewFlow)
Enable-DoubleBuffer $previewFlow

$previewFlow.Add_DragEnter({
    param($s, $e)
    if ($script:DragSource) { $e.Effect = [System.Windows.Forms.DragDropEffects]::Move }
})
$previewFlow.Add_DragDrop({
    param($s, $e)
    if (-not $script:DragSource) { return }
    $idx = $script:Layout.rows.Count
    if ($script:DragSource.From -eq 'palette') {
        $newRow = New-Object System.Collections.ArrayList
        $item = @{ type = $script:DragSource.ItemType }
        if ($script:DragSource.ItemType -eq 'text') {
            $t = Prompt-Text '텍스트 입력' ''
            if ($null -eq $t) { $script:DragSource = $null; return }
            $item.value = $t
        }
        [void]$newRow.Add($item)
        [void]$script:Layout.rows.Insert($idx, $newRow)
        Set-Dirty
    } elseif ($script:DragSource.From -eq 'preview') {
        $sr = $script:DragSource.Row; $sc = $script:DragSource.Col
        if ($sr -lt $script:Layout.rows.Count -and $sc -lt $script:Layout.rows[$sr].Count) {
            $newRow = New-Object System.Collections.ArrayList
            $it = $script:Layout.rows[$sr][$sc]
            $script:Layout.rows[$sr].RemoveAt($sc)
            [void]$newRow.Add($it)
            [void]$script:Layout.rows.Add($newRow)
            if ($script:Layout.rows[$sr].Count -eq 0 -and $script:Layout.rows.Count -gt 1) {
                $script:Layout.rows.RemoveAt($sr)
            }
            Set-Dirty
        }
    } elseif ($script:DragSource.From -eq 'row') {
        $sr = $script:DragSource.Row
        if ($sr -lt $script:Layout.rows.Count) {
            $rowToMove = $script:Layout.rows[$sr]
            $script:Layout.rows.RemoveAt($sr)
            [void]$script:Layout.rows.Add($rowToMove)
            Set-Dirty
        }
    }
    $script:DragSource = $null
    Render-Preview
})

$previewFlow.Add_MouseEnter({
    param($s, $e)
    if (-not $s.Focused) { [void]$s.Focus() }
})

$script:FocusPreview = {
    param($s, $e)
    if (-not $previewFlow.Focused) { [void]$previewFlow.Focus() }
}

# Application-level message filter: Shift+wheel → horizontal scroll on previewFlow
$script:WheelFilter = New-Object HScrollWheelFilter
$script:WheelFilter.Target = $previewFlow
$script:WheelFilter.OnHorizontalScroll = [Action[int]]{
    param([int]$delta)
    if ($null -eq $previewFlow -or $previewFlow.IsDisposed) { return }
    $step = if ($delta -gt 0) { -60 } else { 60 }
    $curX = -$previewFlow.AutoScrollPosition.X
    $curY = -$previewFlow.AutoScrollPosition.Y
    $newX = $curX + $step
    if ($newX -lt 0) { $newX = 0 }
    $maxX = $previewFlow.HorizontalScroll.Maximum - $previewFlow.HorizontalScroll.LargeChange + 1
    if ($maxX -lt 0) { $maxX = 0 }
    if ($newX -gt $maxX) { $newX = $maxX }
    $previewFlow.AutoScrollPosition = New-Object System.Drawing.Point($newX, $curY)
}
[System.Windows.Forms.Application]::AddMessageFilter($script:WheelFilter)

# Palette
$paletteBox = New-Object System.Windows.Forms.GroupBox
$paletteBox.Text = '항목 (위로 드래그 = 추가, 미리보기 항목을 여기로 드래그 = 삭제)'
$paletteBox.Dock = 'Fill'
$paletteBox.Margin = New-Object System.Windows.Forms.Padding(0, 5, 0, 5)
$mainTable.Controls.Add($paletteBox, 0, 1)

# 항상 보이는 구분자/포맷 영역 (Dock=Bottom)
$sepBox = New-Object System.Windows.Forms.GroupBox
$sepBox.Text = $AlwaysVisibleCategory
$sepBox.Dock = 'Bottom'
$sepBox.Height = 180
$paletteBox.Controls.Add($sepBox)

$sepFlow = New-Object System.Windows.Forms.FlowLayoutPanel
$sepFlow.Dock = 'Fill'
$sepFlow.FlowDirection = 'LeftToRight'
$sepFlow.WrapContents = $true
$sepFlow.AutoScroll = $true
$sepFlow.AllowDrop = $true
$sepFlow.BackColor = [System.Drawing.Color]::White
$sepBox.Controls.Add($sepFlow)

$paletteTabs = New-Object System.Windows.Forms.TabControl
$paletteTabs.Dock = 'Fill'
$paletteTabs.AllowDrop = $true
$paletteBox.Controls.Add($paletteTabs)

# Equalize row + zone widths to longest row (or visible width if all shorter)
$script:EqualizeWidths = {
    $minW = $previewFlow.ClientSize.Width - 4
    if ($minW -lt 200) { $minW = 200 }
    $maxW = $minW
    foreach ($ctrl in $previewFlow.Controls) {
        if ($null -ne $ctrl.Tag -and $ctrl.Tag -is [hashtable] -and $ctrl.Tag.ContainsKey('IsRow')) {
            $natural = $ctrl.GetPreferredSize([System.Drawing.Size]::Empty).Width
            if ($natural -gt $maxW) { $maxW = $natural }
        }
    }
    foreach ($ctrl in $previewFlow.Controls) {
        if ($null -ne $ctrl.Tag -and $ctrl.Tag -is [hashtable]) {
            if ($ctrl.Tag.ContainsKey('IsRow')) {
                $ctrl.MinimumSize = New-Object System.Drawing.Size($maxW, 0)
                $ctrl.MaximumSize = New-Object System.Drawing.Size($maxW, 0)
            } elseif ($ctrl.Tag.ContainsKey('InsertIndex')) {
                $ctrl.Width = $maxW
            }
        }
    }
}
$previewFlow.Add_Resize({ & $script:EqualizeWidths })
$previewFlow.Add_Layout({ & $script:EqualizeWidths })

$script:CategoryFlows = @{}

$paletteDropEnter = {
    param($s, $e)
    $e.Effect = [System.Windows.Forms.DragDropEffects]::Move
}
$paletteDropHandler = {
    param($s, $e)
    if ($script:DragSource -and $script:DragSource.From -eq 'preview') {
        $r = $script:DragSource.Row; $c = $script:DragSource.Col
        if ($r -lt $script:Layout.rows.Count -and $c -lt $script:Layout.rows[$r].Count) {
            $script:Layout.rows[$r].RemoveAt($c)
            if ($script:Layout.rows[$r].Count -eq 0 -and $script:Layout.rows.Count -gt 1) {
                $script:Layout.rows.RemoveAt($r)
            }
            Set-Dirty
            Render-Preview
        }
    } elseif ($script:DragSource -and $script:DragSource.From -eq 'row') {
        $r = $script:DragSource.Row
        if ($script:Layout.rows.Count -gt 1 -and $r -lt $script:Layout.rows.Count) {
            $script:Layout.rows.RemoveAt($r)
            Set-Dirty
            Render-Preview
        }
    }
    $script:DragSource = $null
}

# Hook sepFlow as a palette drop target too
$sepFlow.Add_DragEnter($paletteDropEnter)
$sepFlow.Add_DragDrop($paletteDropHandler)


# Buttons (in TableLayoutPanel row 2)
$btnTable = New-Object System.Windows.Forms.TableLayoutPanel
$btnTable.Dock = 'Fill'
$btnTable.ColumnCount = 4
$btnTable.RowCount = 1
$btnTable.Margin = New-Object System.Windows.Forms.Padding(0, 8, 0, 0)
[void]$btnTable.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::AutoSize)))
[void]$btnTable.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 100)))
[void]$btnTable.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::AutoSize)))
[void]$btnTable.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::AutoSize)))
[void]$btnTable.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Percent, 100)))
$mainTable.Controls.Add($btnTable, 0, 2)

$btnAddRow = New-Object System.Windows.Forms.Button
$btnAddRow.Text = '+ 새 줄'
$btnAddRow.Size = New-Object System.Drawing.Size(90, 32)
$btnAddRow.Anchor = [System.Windows.Forms.AnchorStyles]::Left
$btnAddRow.Add_Click({
    [void]$script:Layout.rows.Add((New-Object System.Collections.ArrayList))
    Set-Dirty
    Render-Preview
})
$btnTable.Controls.Add($btnAddRow, 0, 0)

$script:btnReload = New-Object System.Windows.Forms.Button
$script:btnReload.Text = '되돌리기'
$script:btnReload.Size = New-Object System.Drawing.Size(110, 32)
$script:btnReload.Anchor = [System.Windows.Forms.AnchorStyles]::Right
$script:btnReload.Margin = New-Object System.Windows.Forms.Padding(3, 3, 6, 3)
$script:btnReload.Enabled = $false
$script:btnReload.Add_Click({
    $script:Layout = Load-Layout
    Reset-Dirty
    Render-Preview
})
$btnTable.Controls.Add($script:btnReload, 2, 0)

$btnSave = New-Object System.Windows.Forms.Button
$btnSave.Text = '저장 & 적용'
$btnSave.Size = New-Object System.Drawing.Size(130, 32)
$btnSave.Anchor = [System.Windows.Forms.AnchorStyles]::Right
$btnSave.BackColor = [System.Drawing.Color]::FromArgb(70, 140, 80)
$btnSave.ForeColor = [System.Drawing.Color]::White
$btnSave.FlatStyle = 'Flat'
$btnSave.Add_Click({
    try {
        Save-Layout
        Apply-To-ClaudeSettings
        Reset-Dirty
        [System.Windows.Forms.MessageBox]::Show("저장 완료.`nClaude Code 다음 실행 시 적용됩니다.", '완료', 'OK', 'Information') | Out-Null
    } catch {
        [System.Windows.Forms.MessageBox]::Show("저장 실패: $_", '오류', 'OK', 'Error') | Out-Null
    }
})
$btnTable.Controls.Add($btnSave, 3, 0)

# ===== Render =====
function Add-InterRowZone($insertIndex) {
    $zone = New-Object System.Windows.Forms.Panel
    $w = $previewFlow.ClientSize.Width - 4
    if ($w -lt 200) { $w = 200 }
    $zone.Size = New-Object System.Drawing.Size($w, 10)
    $zone.Margin = New-Object System.Windows.Forms.Padding(0, 0, 0, 0)
    $zone.BackColor = [System.Drawing.Color]::White
    $zone.AllowDrop = $true
    $zone.Tag = @{ InsertIndex = $insertIndex }

    $zone.Add_DragEnter({
        param($s, $e)
        $e.Effect = [System.Windows.Forms.DragDropEffects]::Move
        $s.BackColor = [System.Drawing.Color]::FromArgb(120, 180, 250)
    })
    $zone.Add_DragLeave({
        param($s, $e)
        $s.BackColor = [System.Drawing.Color]::White
    })
    $zone.Add_DragDrop({
        param($s, $e)
        $idx = $s.Tag.InsertIndex
        if ($script:DragSource.From -eq 'row') {
            $sr = $script:DragSource.Row
            if ($sr -lt $script:Layout.rows.Count) {
                $rowToMove = $script:Layout.rows[$sr]
                $script:Layout.rows.RemoveAt($sr)
                $newIdx = $idx
                if ($idx -gt $sr) { $newIdx-- }
                if ($newIdx -lt 0) { $newIdx = 0 }
                if ($newIdx -gt $script:Layout.rows.Count) { $newIdx = $script:Layout.rows.Count }
                $script:Layout.rows.Insert($newIdx, $rowToMove)
                Set-Dirty
            }
            $script:DragSource = $null
            Render-Preview
            return
        }
        $newRow = New-Object System.Collections.ArrayList
        if ($script:DragSource.From -eq 'palette') {
            $item = @{ type = $script:DragSource.ItemType }
            if ($script:DragSource.ItemType -eq 'text') {
                $t = Prompt-Text '텍스트 입력' ''
                if ($null -eq $t) { $script:DragSource = $null; Render-Preview; return }
                $item.value = $t
            }
            [void]$newRow.Add($item)
            $script:Layout.rows.Insert($idx, $newRow)
            Set-Dirty
        } elseif ($script:DragSource.From -eq 'preview') {
            $sr = $script:DragSource.Row; $sc = $script:DragSource.Col
            $script:Layout.rows.Insert($idx, $newRow)
            if ($idx -le $sr) { $sr++ }
            if ($sr -lt $script:Layout.rows.Count -and $sc -lt $script:Layout.rows[$sr].Count) {
                $it = $script:Layout.rows[$sr][$sc]
                $script:Layout.rows[$sr].RemoveAt($sc)
                [void]$newRow.Add($it)
                if ($script:Layout.rows[$sr].Count -eq 0 -and $script:Layout.rows.Count -gt 1) {
                    $script:Layout.rows.RemoveAt($sr)
                }
                Set-Dirty
            }
        }
        $script:DragSource = $null
        Render-Preview
    })
    $previewFlow.Controls.Add($zone)
}

function Add-RowPanel($r) {
    $row = New-Object System.Windows.Forms.FlowLayoutPanel
    $row.FlowDirection = 'LeftToRight'
    $row.WrapContents = $false
    $row.AutoSize = $true
    $row.AutoSizeMode = 'GrowAndShrink'
    $row.MinimumSize = New-Object System.Drawing.Size(0, 0)
    $row.MaximumSize = New-Object System.Drawing.Size(0, 0)
    $row.BackColor = [System.Drawing.Color]::FromArgb(252, 252, 252)
    $row.BorderStyle = 'FixedSingle'
    $row.Margin = New-Object System.Windows.Forms.Padding(0, 0, 0, 0)
    $row.Padding = New-Object System.Windows.Forms.Padding(5)
    $row.AllowDrop = $true
    $row.Tag = @{ RowIndex = $r; IsRow = $true }
    $row.Add_MouseEnter($script:FocusPreview)

    $hdr = New-Object System.Windows.Forms.Label
    $hdr.Text = "≡ 줄 $($r + 1)"
    $hdr.AutoSize = $true
    $hdr.ForeColor = [System.Drawing.Color]::Gray
    $hdr.Margin = New-Object System.Windows.Forms.Padding(4, 9, 4, 4)
    $hdr.Cursor = [System.Windows.Forms.Cursors]::SizeAll
    $hdr.Add_MouseDown({
        param($s, $e)
        if ($e.Button -ne [System.Windows.Forms.MouseButtons]::Left) { return }
        $rIdx = $s.Parent.Tag.RowIndex
        $script:DragSource = @{ From = 'row'; Row = $rIdx }
        [void]$s.DoDragDrop("ROW", [System.Windows.Forms.DragDropEffects]::Move)
    })
    $row.Controls.Add($hdr)

    if ($script:Layout.rows.Count -gt 1) {
        $delBtn = New-Object System.Windows.Forms.Button
        $delBtn.Text = '×'
        $delBtn.Size = New-Object System.Drawing.Size(22, 22)
        $delBtn.Margin = New-Object System.Windows.Forms.Padding(2, 8, 6, 4)
        $delBtn.FlatStyle = 'Flat'
        $delBtn.Font = New-Object System.Drawing.Font('Segoe UI', 8)
        $rRef = $r
        $delBtn.Add_Click({
            $script:Layout.rows.RemoveAt($rRef)
            if ($script:Layout.rows.Count -eq 0) {
                [void]$script:Layout.rows.Add((New-Object System.Collections.ArrayList))
            }
            Set-Dirty
            Render-Preview
        }.GetNewClosure())
        $row.Controls.Add($delBtn)
    }

    for ($c = 0; $c -lt $script:Layout.rows[$r].Count; $c++) {
        $itemPanel = New-ItemPanel $script:Layout.rows[$r][$c] $false $r $c
        $row.Controls.Add($itemPanel)
    }

    if ($script:Layout.rows[$r].Count -eq 0) {
        $hint = New-Object System.Windows.Forms.Label
        $hint.Text = '여기로 항목을 드래그하세요'
        $hint.ForeColor = [System.Drawing.Color]::Silver
        $hint.AutoSize = $true
        $hint.Margin = New-Object System.Windows.Forms.Padding(15, 11, 10, 4)
        $row.Controls.Add($hint)
    }

    $row.Add_DragEnter({
        param($s, $e)
        if ($script:DragSource -and $script:DragSource.From -eq 'row') {
            $e.Effect = [System.Windows.Forms.DragDropEffects]::None
            return
        }
        $e.Effect = [System.Windows.Forms.DragDropEffects]::Move
        $s.BackColor = [System.Drawing.Color]::FromArgb(232, 242, 255)
    })
    $row.Add_DragLeave({
        param($s, $e)
        $s.BackColor = [System.Drawing.Color]::FromArgb(252, 252, 252)
    })
    $row.Add_DragDrop({
        param($s, $e)
        if ($script:DragSource.From -eq 'row') {
            $script:DragSource = $null
            Render-Preview
            return
        }
        $rowIdx = $s.Tag.RowIndex
        $clientPt = $s.PointToClient([System.Drawing.Point]::new($e.X, $e.Y))

        $itemCtrls = @()
        foreach ($ctrl in $s.Controls) {
            if ($ctrl -is [System.Windows.Forms.Panel] -and
                $null -ne $ctrl.Tag -and
                $ctrl.Tag -is [hashtable] -and
                $ctrl.Tag.ContainsKey('IsItem')) {
                $itemCtrls += $ctrl
            }
        }
        $insertIndex = $itemCtrls.Count
        for ($i = 0; $i -lt $itemCtrls.Count; $i++) {
            $ctrl = $itemCtrls[$i]
            if ($clientPt.X -lt ($ctrl.Left + $ctrl.Width / 2)) { $insertIndex = $i; break }
        }

        if ($script:DragSource.From -eq 'palette') {
            $newItem = @{ type = $script:DragSource.ItemType }
            if ($script:DragSource.ItemType -eq 'text') {
                $t = Prompt-Text '텍스트 입력' ''
                if ($null -eq $t) { $script:DragSource = $null; Render-Preview; return }
                $newItem.value = $t
            }
            $script:Layout.rows[$rowIdx].Insert($insertIndex, $newItem)
            Set-Dirty
        } elseif ($script:DragSource.From -eq 'preview') {
            $sr = $script:DragSource.Row; $sc = $script:DragSource.Col
            if ($sr -lt $script:Layout.rows.Count -and $sc -lt $script:Layout.rows[$sr].Count) {
                $it = $script:Layout.rows[$sr][$sc]
                $script:Layout.rows[$sr].RemoveAt($sc)
                $adj = $insertIndex
                if ($sr -eq $rowIdx -and $sc -lt $insertIndex) { $adj-- }
                if ($adj -gt $script:Layout.rows[$rowIdx].Count) { $adj = $script:Layout.rows[$rowIdx].Count }
                if ($adj -lt 0) { $adj = 0 }
                $script:Layout.rows[$rowIdx].Insert($adj, $it)
                if ($sr -ne $rowIdx -and $script:Layout.rows[$sr].Count -eq 0 -and $script:Layout.rows.Count -gt 1) {
                    $script:Layout.rows.RemoveAt($sr)
                }
                Set-Dirty
            }
        }
        $script:DragSource = $null
        Render-Preview
    })

    $previewFlow.Controls.Add($row)
}

function Render-Preview {
    $h = $previewFlow.Handle
    [NativeWinUtil]::SuspendDrawing($h)
    try {
        $previewFlow.SuspendLayout()
        $previewFlow.Controls.Clear()
        if ($script:Layout.rows.Count -eq 0) {
            [void]$script:Layout.rows.Add((New-Object System.Collections.ArrayList))
        }
        Add-InterRowZone 0
        for ($r = 0; $r -lt $script:Layout.rows.Count; $r++) {
            Add-RowPanel $r
            Add-InterRowZone ($r + 1)
        }
        $previewFlow.ResumeLayout()
        & $script:EqualizeWidths
    } finally {
        [NativeWinUtil]::ResumeDrawing($h)
        $previewFlow.Invalidate($true)
    }
}

function Render-Palette {
    $paletteTabs.TabPages.Clear()
    $script:CategoryFlows.Clear()

    foreach ($cat in $CategoryOrder) {
        $tab = New-Object System.Windows.Forms.TabPage
        $tab.Text = $cat
        $tab.BackColor = [System.Drawing.Color]::White

        $scrollHost = New-Object System.Windows.Forms.Panel
        $scrollHost.Dock = 'Fill'
        $scrollHost.AutoScroll = $true
        $scrollHost.BackColor = [System.Drawing.Color]::White
        Enable-DoubleBuffer $scrollHost

        $flow = New-Object System.Windows.Forms.FlowLayoutPanel
        $flow.Dock = 'Top'
        $flow.FlowDirection = 'LeftToRight'
        $flow.WrapContents = $true
        $flow.AutoSize = $true
        $flow.AutoSizeMode = 'GrowAndShrink'
        $flow.AllowDrop = $true
        $flow.BackColor = [System.Drawing.Color]::White
        $flow.Add_DragEnter($paletteDropEnter)
        $flow.Add_DragDrop($paletteDropHandler)

        foreach ($key in $ItemTypes.Keys) {
            if ($ItemTypes[$key].Cat -ne $cat) { continue }
            $p = New-ItemPanel @{ type = $key } $true -1 -1
            $flow.Controls.Add($p)
        }
        $scrollHost.Controls.Add($flow)
        $tab.Controls.Add($scrollHost)
        $paletteTabs.TabPages.Add($tab)
        $script:CategoryFlows[$cat] = $flow
    }

    # Always-visible separator/format strip
    $sepFlow.Controls.Clear()
    foreach ($key in $ItemTypes.Keys) {
        if ($ItemTypes[$key].Cat -ne $AlwaysVisibleCategory) { continue }
        $p = New-ItemPanel @{ type = $key } $true -1 -1
        $sepFlow.Controls.Add($p)
    }
}

# ===== Init =====
$script:Layout = Load-Layout
Reset-Dirty
Render-Palette
Render-Preview

[void]$form.ShowDialog()
[System.Windows.Forms.Application]::RemoveMessageFilter($script:WheelFilter)
$form.Dispose()
