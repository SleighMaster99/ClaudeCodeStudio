# StatusLine.ps1
# Claude Code statusLine runtime.
# Reads JSON from stdin, renders multi-line statusbar based on config.json.

$ErrorActionPreference = 'SilentlyContinue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
try { [Console]::InputEncoding = [System.Text.Encoding]::UTF8 } catch {}

$ConfigPath = Join-Path $PSScriptRoot 'config.json'

$stdin = [Console]::In.ReadToEnd()
try { $stdin | Set-Content (Join-Path $PSScriptRoot '.last_input.json') -Encoding UTF8 } catch {}
$ctx = $null
try { $ctx = $stdin | ConvertFrom-Json } catch {}

if (-not (Test-Path $ConfigPath)) { return }

$cfg = $null
try { $cfg = Get-Content $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json } catch { return }
if ($null -eq $cfg -or $null -eq $cfg.rows) { return }

# ----- Helpers -----
function Get-Field($path, $default = '') {
    if ($null -eq $ctx) { return $default }
    $cur = $ctx
    foreach ($p in ($path -split '\.')) {
        if ($null -eq $cur) { return $default }
        $cur = $cur.$p
    }
    if ($null -eq $cur) { return $default }
    return [string]$cur
}

function Get-FieldRaw($path) {
    if ($null -eq $ctx) { return $null }
    $cur = $ctx
    foreach ($p in ($path -split '\.')) {
        if ($null -eq $cur) { return $null }
        $cur = $cur.$p
    }
    return $cur
}

function In-Dir($dir, [scriptblock]$block) {
    if (-not $dir -or -not (Test-Path $dir)) { return $null }
    $prev = Get-Location
    try {
        Set-Location -LiteralPath $dir
        return & $block
    } catch { return $null }
    finally { Set-Location $prev }
}

function Get-GitBranch($dir) {
    $r = In-Dir $dir { git branch --show-current 2>$null }
    if ($null -eq $r) { return '' }
    return ([string]$r).Trim()
}

function Get-GitCommit($dir) {
    $r = In-Dir $dir { git rev-parse --short HEAD 2>$null }
    if ($null -eq $r) { return '' }
    return ([string]$r).Trim()
}

function Get-GitChangeCount($dir) {
    $r = In-Dir $dir { git status --porcelain 2>$null }
    if ($null -eq $r) { return '' }
    $lines = @($r) | Where-Object { $_ -ne '' }
    return [string]$lines.Count
}

function Get-GitAheadBehind($dir) {
    $r = In-Dir $dir { git rev-list --left-right --count '@{u}...HEAD' 2>$null }
    if ($null -eq $r) { return '' }
    $parts = ([string]$r).Trim() -split '\s+'
    if ($parts.Count -ne 2) { return '' }
    return ('↑{0} ↓{1}' -f $parts[1], $parts[0])
}

function Get-GitUser($dir) {
    $r = In-Dir $dir { git config user.name 2>$null }
    if ($null -eq $r) { return '' }
    return ([string]$r).Trim()
}

function Get-Version {
    $v = Get-Field 'version'
    if ($v) { return $v }
    $cachePath = Join-Path $PSScriptRoot '.version_cache.txt'
    if (Test-Path $cachePath) {
        try {
            $age = (Get-Date) - (Get-Item $cachePath).LastWriteTime
            if ($age.TotalHours -lt 24) {
                return ((Get-Content $cachePath -Raw -Encoding UTF8) -as [string]).Trim()
            }
        } catch {}
    }
    $ver = ''
    try {
        $cv = & claude --version 2>$null
        if ($cv) {
            $s = ([string]$cv).Trim()
            if ($s -match '(\d+\.\d+(?:\.\d+)?)') { $ver = $matches[1] } else { $ver = $s }
        }
    } catch {}
    try { $ver | Set-Content $cachePath -Encoding UTF8 } catch {}
    return $ver
}

$script:ESC = [char]27
$script:CR_RESET = "$([char]27)[0m"
$script:CR_DIM   = "$([char]27)[90m"

# 아이콘 글리프 (icon_<key> 항목에서 참조). 같은 카테고리 항목을 한 줄에 묶을 때 사용.
$script:Icons = @{
    'model'    = '🤖'
    'version'  = '🏷️'
    'session'  = '🔖'
    'plan'     = '⭐'
    'ctx'      = '🧠'
    'effort'   = '💭'
    'cost'     = '💰'
    'duration' = '⏱️'
    'dir'      = '📁'
    'git'      = '🌿'
    'time'     = '🕐'
    'date'     = '📅'
    'user'     = '👤'
    'host'     = '🖥️'
    'os'       = '💻'
    'h5'       = '⏱️'
    'week'     = '🗓️'
    'fable'    = '🔮'
}

function Get-BarColor([double]$pct) {
    if ($pct -ge 80) { return "$([char]27)[91m" }   # bright red
    if ($pct -ge 50) { return "$([char]27)[93m" }   # bright yellow
    return "$([char]27)[92m"                         # bright green
}

# Track background (medium gray, visible on light & dark themes)
$script:BG_TRACK = "$([char]27)[48;5;246m"

function Render-Bar([double]$pct, [int]$width = 10) {
    if ($pct -lt 0) { $pct = 0 }
    if ($pct -gt 100) { $pct = 100 }
    $units = $width * 8.0 * ($pct / 100.0)
    $full = [int][Math]::Floor($units / 8.0)
    $rem  = [int][Math]::Round($units - $full * 8.0)
    if ($rem -eq 8) { $full++; $rem = 0 }
    $partials = @('','▏','▎','▍','▌','▋','▊','▉')
    $color = Get-BarColor $pct
    $bar = $color + $script:BG_TRACK + ('█' * $full)
    if ($full -lt $width -and $rem -gt 0) { $bar += $partials[$rem]; $full++ }
    if ($full -lt $width) { $bar += (' ' * ($width - $full)) }
    $bar += $script:CR_RESET
    return $bar
}

function Render-BarAscii([double]$pct, [int]$width = 10) {
    if ($pct -lt 0) { $pct = 0 }
    if ($pct -gt 100) { $pct = 100 }
    $filled = [int][Math]::Round($width * $pct / 100.0)
    $empty = $width - $filled
    $color = Get-BarColor $pct
    return ('[' + $color + $script:BG_TRACK + ('#' * $filled) + (' ' * $empty) + $script:CR_RESET + ']')
}

function Render-BarDot([double]$pct, [int]$width = 10) {
    if ($pct -lt 0) { $pct = 0 }
    if ($pct -gt 100) { $pct = 100 }
    $filled = [int][Math]::Round($width * $pct / 100.0)
    $empty = $width - $filled
    $color = Get-BarColor $pct
    return ($color + ('●' * $filled) + $script:CR_DIM + ('○' * $empty) + $script:CR_RESET)
}

function Format-Duration([double]$ms) {
    if ($ms -le 0) { return '0s' }
    $s = [int][Math]::Round($ms / 1000)
    if ($s -lt 60) { return "${s}s" }
    $m = [int][Math]::Floor($s / 60); $rs = $s % 60
    if ($m -lt 60) { return ('{0}m{1:D2}s' -f $m, $rs) }
    $h = [int][Math]::Floor($m / 60); $rm = $m % 60
    if ($h -lt 24) { return ('{0}h{1:D2}m' -f $h, $rm) }
    $d = [int][Math]::Floor($h / 24); $rh = $h % 24
    return ('{0}d{1:D2}h' -f $d, $rh)
}

function Get-ContextLimit {
    $size = Get-FieldRaw 'context_window.context_window_size'
    if ($null -ne $size) { return [int]$size }
    $id = Get-Field 'model.id'
    if ($id -and $id -match '\[(\d+)([km])\]') {
        $n = [int]$matches[1]
        $unit = $matches[2].ToLower()
        if ($unit -eq 'm') { return $n * 1000000 }
        if ($unit -eq 'k') { return $n * 1000 }
    }
    return 200000
}

function Get-ContextLabel {
    $limit = Get-ContextLimit
    if ($limit -ge 1000000) {
        $m = $limit / 1000000.0
        if ($m -eq [int]$m) { return ('{0}M' -f [int]$m) }
        return ('{0}M' -f $m)
    }
    if ($limit -ge 1000) { return ('{0}k' -f [int]($limit / 1000)) }
    return [string]$limit
}

function Get-Effort {
    $v = Get-Field 'effort.level'
    if ($v) { return $v }
    foreach ($p in @('thinking.effort','model.thinking_effort','model.effort','thinking_effort','reasoning.effort','effort')) {
        $vv = Get-FieldRaw $p
        if ($null -ne $vv -and $vv -ne '') { return [string]$vv }
    }
    return ''
}

function Get-ContextPercent {
    $direct = Get-FieldRaw 'context_window.used_percentage'
    if ($null -ne $direct) { return [int][Math]::Round([double]$direct) }
    $limit = Get-ContextLimit
    foreach ($p in @('usage.context.tokens','usage.input_tokens','context.tokens')) {
        $v = Get-FieldRaw $p
        if ($null -ne $v) {
            $pct = [int][Math]::Round(([double]$v / $limit) * 100.0)
            if ($pct -gt 100) { $pct = 100 } elseif ($pct -lt 0) { $pct = 0 }
            return $pct
        }
    }
    $tp = Get-Field 'transcript_path'
    if ($tp -and (Test-Path $tp)) {
        try {
            $size = (Get-Item $tp).Length
            $tokensEst = $size / 4.0
            $pct = [int][Math]::Round(($tokensEst / [double]$limit) * 100.0)
            if ($pct -gt 100) { $pct = 100 } elseif ($pct -lt 0) { $pct = 0 }
            return $pct
        } catch {}
    }
    return $null
}

# ----- Account / Plan Detection -----
function Get-PlanType {
    if ($env:ANTHROPIC_API_KEY) { return 'api' }
    $f = Join-Path $env:USERPROFILE '.claude.json'
    if (Test-Path $f) {
        try {
            $j = Get-Content $f -Raw -Encoding UTF8 | ConvertFrom-Json
            $tier = $null
            if ($j.oauthAccount) {
                $tier = $j.oauthAccount.organizationRateLimitTier
                if (-not $tier) { $tier = $j.oauthAccount.userRateLimitTier }
            }
            if ($tier) {
                $low = ([string]$tier).ToLower()
                if ($low -match 'max[_\-]?20x')   { return 'max20x' }
                if ($low -match 'max[_\-]?5x')    { return 'max5x' }
                if ($low -match 'team')           { return 'team' }
                if ($low -match 'enterprise')     { return 'enterprise' }
                if ($low -match 'pro')            { return 'pro' }
            }
            if ($j.oauthAccount) {
                $ot = $j.oauthAccount.organizationType
                if ($ot) {
                    $lo = ([string]$ot).ToLower()
                    if ($lo -match 'max')  { return 'max20x' }
                    if ($lo -match 'pro')  { return 'pro' }
                    if ($lo -match 'team') { return 'team' }
                }
                $bt = $j.oauthAccount.billingType
                if ($bt -and ([string]$bt).ToLower() -eq 'stripe_subscription') { return 'pro' }
            }
        } catch {}
    }
    return 'unknown'
}

# ----- Pricing per 1M tokens (USD), best-effort defaults -----
$script:Pricing = @{
    'opus'   = @{ input = 15.0; output = 75.0; cache_write = 18.75; cache_read = 1.50 }
    'sonnet' = @{ input = 3.0;  output = 15.0; cache_write = 3.75;  cache_read = 0.30 }
    'haiku'  = @{ input = 1.0;  output = 5.0;  cache_write = 1.25;  cache_read = 0.10 }
}

function Get-PricingForModel($modelId) {
    if (-not $modelId) { return $script:Pricing['sonnet'] }
    $low = ([string]$modelId).ToLower()
    if ($low -match 'opus')  { return $script:Pricing['opus'] }
    if ($low -match 'haiku') { return $script:Pricing['haiku'] }
    return $script:Pricing['sonnet']
}

function Compute-MessageCost($modelId, $usage) {
    if ($null -eq $usage) { return 0.0 }
    $p = Get-PricingForModel $modelId
    $cost = 0.0
    $it  = [double]($usage.input_tokens               -as [double]); if ($it)  { $cost += $it / 1e6 * $p.input }
    $ot  = [double]($usage.output_tokens              -as [double]); if ($ot)  { $cost += $ot / 1e6 * $p.output }
    $cw  = [double]($usage.cache_creation_input_tokens -as [double]); if ($cw) { $cost += $cw / 1e6 * $p.cache_write }
    $cr  = [double]($usage.cache_read_input_tokens    -as [double]); if ($cr)  { $cost += $cr / 1e6 * $p.cache_read }
    return $cost
}

# ----- Plan limits (USD), override via usage_config.json -----
$script:PlanLimits = @{
    'pro'    = @{ '5h' = 35.0;  'week' = 245.0  }
    'max5x'  = @{ '5h' = 140.0; 'week' = 980.0  }
    'max20x' = @{ '5h' = 280.0; 'week' = 1960.0 }
}

function Get-PlanLimits($plan) {
    $cfgPath = Join-Path $PSScriptRoot 'usage_config.json'
    if (Test-Path $cfgPath) {
        try {
            $u = Get-Content $cfgPath -Raw -Encoding UTF8 | ConvertFrom-Json
            $node = $u.$plan
            if ($node) {
                $h5 = $node.'5h'; if ($null -eq $h5) { $h5 = $node.h5 }
                $wk = $node.week
                if ($null -ne $h5 -and $null -ne $wk) {
                    return @{ '5h' = [double]$h5; 'week' = [double]$wk }
                }
            }
        } catch {}
    }
    if ($script:PlanLimits.ContainsKey($plan)) { return $script:PlanLimits[$plan] }
    return $null
}

# ----- Usage aggregation from ~/.claude/projects/**/*.jsonl -----
function Compute-Usage {
    $projectsDir = Join-Path $env:USERPROFILE '.claude\projects'
    $result = @{ h5_cost = 0.0; week_cost = 0.0; h5_tokens = 0.0; week_tokens = 0.0; h5_oldest = $null; week_oldest = $null }
    if (-not (Test-Path $projectsDir)) { return $result }

    $now = Get-Date
    $h5Cutoff   = $now.AddHours(-5)
    $weekCutoff = $now.AddDays(-7)

    Get-ChildItem -Path $projectsDir -Filter '*.jsonl' -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $weekCutoff } |
        ForEach-Object {
            try {
                $reader = [System.IO.StreamReader]::new($_.FullName)
                $prevUsageKey = ''
                try {
                    while ($null -ne ($line = $reader.ReadLine())) {
                        if ([string]::IsNullOrWhiteSpace($line)) { continue }
                        $obj = $null
                        try { $obj = $line | ConvertFrom-Json } catch { continue }
                        if ($null -eq $obj) { continue }

                        $ts = $obj.timestamp
                        if (-not $ts) { continue }
                        $msgTime = $null
                        try { $msgTime = [datetime]$ts } catch { continue }
                        if ($msgTime -lt $weekCutoff) { continue }

                        $usage = $null
                        if ($obj.message -and $obj.message.usage) { $usage = $obj.message.usage }
                        elseif ($obj.usage) { $usage = $obj.usage }
                        if ($null -eq $usage) { continue }

                        $usageKey = "$($usage.input_tokens)|$($usage.output_tokens)|$($usage.cache_creation_input_tokens)|$($usage.cache_read_input_tokens)"
                        if ($usageKey -eq $prevUsageKey) { continue }
                        $prevUsageKey = $usageKey

                        $modelId = $null
                        if ($obj.message -and $obj.message.model) { $modelId = $obj.message.model }
                        elseif ($obj.model) { $modelId = $obj.model }

                        $c = Compute-MessageCost $modelId $usage
                        $tok = 0.0
                        foreach ($k in @('input_tokens','output_tokens','cache_creation_input_tokens','cache_read_input_tokens')) {
                            $v = $usage.$k -as [double]
                            if ($v) { $tok += $v }
                        }

                        $result.week_cost   += $c
                        $result.week_tokens += $tok
                        if ($null -eq $result.week_oldest -or $msgTime -lt $result.week_oldest) { $result.week_oldest = $msgTime }
                        if ($msgTime -ge $h5Cutoff) {
                            $result.h5_cost   += $c
                            $result.h5_tokens += $tok
                            if ($null -eq $result.h5_oldest -or $msgTime -lt $result.h5_oldest) { $result.h5_oldest = $msgTime }
                        }
                    }
                } finally { $reader.Dispose() }
            } catch {}
        }
    return $result
}

function Get-CachedUsage {
    $cachePath = Join-Path $PSScriptRoot '.usage_cache.json'
    if (Test-Path $cachePath) {
        try {
            $cache = Get-Content $cachePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $age = (Get-Date) - [datetime]$cache.at
            if ($age.TotalSeconds -lt 60) {
                $h5o = $null; if ($cache.h5_oldest) { $h5o = [datetime]$cache.h5_oldest }
                $wko = $null; if ($cache.week_oldest) { $wko = [datetime]$cache.week_oldest }
                return @{
                    h5_cost     = [double]$cache.h5_cost
                    week_cost   = [double]$cache.week_cost
                    h5_tokens   = [double]$cache.h5_tokens
                    week_tokens = [double]$cache.week_tokens
                    h5_oldest   = $h5o
                    week_oldest = $wko
                }
            }
        } catch {}
    }
    $u = Compute-Usage
    try {
        $h5os = $null; if ($u.h5_oldest) { $h5os = $u.h5_oldest.ToString('o') }
        $wkos = $null; if ($u.week_oldest) { $wkos = $u.week_oldest.ToString('o') }
        @{
            at          = (Get-Date).ToString('o')
            h5_cost     = $u.h5_cost
            week_cost   = $u.week_cost
            h5_tokens   = $u.h5_tokens
            week_tokens = $u.week_tokens
            h5_oldest   = $h5os
            week_oldest = $wkos
        } | ConvertTo-Json | Set-Content $cachePath -Encoding UTF8
    } catch {}
    return $u
}

# ----- Fable weekly usage via Anthropic usage API -----
# stdin rate_limits에는 모델별 주간 한도가 없어 /usage 화면과 동일한 API를 직접 조회한다.
function Compute-FableUsage {
    $credPath = Join-Path $env:USERPROFILE '.claude\.credentials.json'
    if (-not (Test-Path $credPath)) { return $null }
    $tok = $null
    try {
        $cred = Get-Content $credPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $oauth = $cred.claudeAiOauth
        if ($null -eq $oauth -or -not $oauth.accessToken) { return $null }
        if ($oauth.expiresAt -and [DateTimeOffset]::FromUnixTimeMilliseconds([long]$oauth.expiresAt) -le [DateTimeOffset]::UtcNow) { return $null }
        $tok = $oauth.accessToken
    } catch { return $null }
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        $resp = Invoke-RestMethod -Uri 'https://api.anthropic.com/api/oauth/usage' -TimeoutSec 5 -Headers @{
            'Authorization'  = "Bearer $tok"
            'anthropic-beta' = 'oauth-2025-04-20'
            'Content-Type'   = 'application/json'
        }
        foreach ($l in $resp.limits) {
            if ($l.kind -eq 'weekly_scoped' -and $l.scope -and $l.scope.model -and ([string]$l.scope.model.display_name) -match 'fable') {
                return @{ pct = [double]$l.percent; resets_at = [string]$l.resets_at }
            }
        }
    } catch {}
    return $null
}

function Get-CachedFableUsage {
    $cachePath = Join-Path $PSScriptRoot '.fable_cache.json'
    if (Test-Path $cachePath) {
        try {
            $cache = Get-Content $cachePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $age = (Get-Date) - [datetime]$cache.at
            if ($age.TotalSeconds -lt 60) {
                if ($null -ne $cache.pct) {
                    return @{ pct = [double]$cache.pct; resets_at = [string]$cache.resets_at }
                }
                return $null
            }
        } catch {}
    }
    $u = Compute-FableUsage
    try {
        $obj = @{ at = (Get-Date).ToString('o') }
        if ($u) { $obj.pct = $u.pct; $obj.resets_at = $u.resets_at }
        $obj | ConvertTo-Json | Set-Content $cachePath -Encoding UTF8
    } catch {}
    return $u
}

$script:_planType = $null
function Get-CurrentPlan {
    if ($null -eq $script:_planType) { $script:_planType = Get-PlanType }
    return $script:_planType
}

# ----- Render -----
$dir = Get-Field 'workspace.current_dir'

foreach ($row in $cfg.rows) {
    $sb = New-Object System.Text.StringBuilder
    foreach ($it in $row) {
        switch ($it.type) {
            # Claude
            'model'      { [void]$sb.Append((Get-Field 'model.display_name')) }
            'version'    { [void]$sb.Append((Get-Version)) }
            'session'    {
                $s = Get-Field 'session_id'
                if ($s.Length -gt 8) { $s = $s.Substring(0,8) }
                [void]$sb.Append($s)
            }
            'cost'       {
                $c = Get-FieldRaw 'cost.total_cost_usd'
                if ($null -ne $c) { [void]$sb.Append(('${0:N2}' -f [double]$c)) }
            }
            'duration'   {
                $d = Get-FieldRaw 'cost.total_duration_ms'
                if ($null -ne $d) { [void]$sb.Append((Format-Duration ([double]$d))) }
            }
            'plan'       { [void]$sb.Append((Get-CurrentPlan)) }
            # 워크스페이스
            'dir_short'  { if ($dir) { [void]$sb.Append((Split-Path -Leaf $dir)) } }
            'dir_full'   { if ($dir) { [void]$sb.Append($dir) } }
            'project_dir'{ [void]$sb.Append((Get-Field 'workspace.project_dir')) }
            # Git
            'git_branch' { $b = Get-GitBranch $dir; if ($b) { [void]$sb.Append($b) } }
            'git_commit' { $b = Get-GitCommit $dir; if ($b) { [void]$sb.Append($b) } }
            'git_changes'{ $b = Get-GitChangeCount $dir; if ($b) { [void]$sb.Append($b) } }
            'git_ahead_behind' { $b = Get-GitAheadBehind $dir; if ($b) { [void]$sb.Append($b) } }
            'git_user'   { $b = Get-GitUser $dir; if ($b) { [void]$sb.Append($b) } }
            # 시간
            'time'       { [void]$sb.Append((Get-Date -Format 'HH:mm')) }
            'date'       { [void]$sb.Append((Get-Date -Format 'yyyy-MM-dd')) }
            'weekday'    {
                $kor = @('일','월','화','수','목','금','토')
                [void]$sb.Append($kor[[int](Get-Date).DayOfWeek])
            }
            'session_elapsed' {
                $d = Get-FieldRaw 'cost.total_duration_ms'
                if ($null -ne $d) { [void]$sb.Append((Format-Duration ([double]$d))) }
            }
            # 시스템
            'user'       { [void]$sb.Append($env:USERNAME) }
            'host'       { [void]$sb.Append($env:COMPUTERNAME) }
            'os'         {
                $os = (Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue).Caption
                if ($os) {
                    if ($os -match 'Windows\s+(\d+)') { [void]$sb.Append("Win$($matches[1])") }
                    else { [void]$sb.Append($os) }
                } else { [void]$sb.Append('Windows') }
            }
            # 사용률
            'ctx_size'   { [void]$sb.Append((Get-ContextLabel)) }
            'effort'     { [void]$sb.Append((Get-Effort)) }
            'ctx_info'   {
                $label = Get-ContextLabel
                $eff = Get-Effort
                if ($eff) { $label = "$label / $eff" }
                [void]$sb.Append($label)
            }
            'ctx_pct'    {
                $p = Get-ContextPercent
                if ($null -eq $p) { $p = 0 }
                [void]$sb.Append("${p}%")
            }
            'ctx_bar'    {
                $p = Get-ContextPercent
                if ($null -eq $p) { $p = 0 }
                [void]$sb.Append((Render-Bar ([double]$p)))
            }
            'ctx_bar_ascii' {
                $p = Get-ContextPercent
                if ($null -eq $p) { $p = 0 }
                [void]$sb.Append((Render-BarAscii ([double]$p)))
            }
            'ctx_bar_dot' {
                $p = Get-ContextPercent
                if ($null -eq $p) { $p = 0 }
                [void]$sb.Append((Render-BarDot ([double]$p)))
            }
            'ctx_tokens' {
                $limit = Get-ContextLimit
                $limitK = [int]($limit / 1000)
                $p = Get-ContextPercent
                if ($null -eq $p) { $p = 0 }
                $usedK = [int][Math]::Round($limitK * $p / 100.0)
                [void]$sb.Append("${usedK}k/${limitK}k")
            }
            { $_ -in 'h5_bar','h5_bar_ascii','h5_bar_dot' } {
                $direct = Get-FieldRaw 'rate_limits.five_hour.used_percentage'
                $pct = if ($null -ne $direct) { [double]$direct } else { 0 }
                $bar = switch ($_) {
                    'h5_bar_ascii' { Render-BarAscii $pct }
                    'h5_bar_dot'   { Render-BarDot $pct }
                    default        { Render-Bar $pct }
                }
                [void]$sb.Append($bar)
            }
            { $_ -in 'week_bar','week_bar_ascii','week_bar_dot' } {
                $direct = Get-FieldRaw 'rate_limits.seven_day.used_percentage'
                if ($null -eq $direct) { $direct = Get-FieldRaw 'rate_limits.weekly.used_percentage' }
                $pct = if ($null -ne $direct) { [double]$direct } else { 0 }
                $bar = switch ($_) {
                    'week_bar_ascii' { Render-BarAscii $pct }
                    'week_bar_dot'   { Render-BarDot $pct }
                    default          { Render-Bar $pct }
                }
                [void]$sb.Append($bar)
            }
            { $_ -in 'fable_bar','fable_bar_ascii','fable_bar_dot' } {
                $u = Get-CachedFableUsage
                $pct = if ($null -ne $u) { [double]$u.pct } else { 0 }
                $bar = switch ($_) {
                    'fable_bar_ascii' { Render-BarAscii $pct }
                    'fable_bar_dot'   { Render-BarDot $pct }
                    default           { Render-Bar $pct }
                }
                [void]$sb.Append($bar)
            }
            'h5_pct'     {
                $direct = Get-FieldRaw 'rate_limits.five_hour.used_percentage'
                $pct = if ($null -ne $direct) { [int][Math]::Round([double]$direct) } else { 0 }
                [void]$sb.Append("${pct}%")
            }
            'week_pct'   {
                $direct = Get-FieldRaw 'rate_limits.seven_day.used_percentage'
                if ($null -eq $direct) { $direct = Get-FieldRaw 'rate_limits.weekly.used_percentage' }
                $pct = if ($null -ne $direct) { [int][Math]::Round([double]$direct) } else { 0 }
                [void]$sb.Append("${pct}%")
            }
            'fable_pct'  {
                $u = Get-CachedFableUsage
                $pct = if ($null -ne $u) { [int][Math]::Round([double]$u.pct) } else { 0 }
                [void]$sb.Append("${pct}%")
            }
            'ctx_cost'   {
                $c = Get-FieldRaw 'cost.total_cost_usd'
                if ($null -ne $c) { [void]$sb.Append(('${0:N2}' -f [double]$c)) } else { [void]$sb.Append('$0.00') }
            }
            'h5_cost'    {
                $u = Get-CachedUsage
                [void]$sb.Append(('${0:N2}' -f $u.h5_cost))
            }
            'week_cost'  {
                $u = Get-CachedUsage
                [void]$sb.Append(('${0:N2}' -f $u.week_cost))
            }
            'h5_remain'  {
                $ra = Get-FieldRaw 'rate_limits.five_hour.resets_at'
                if ($null -ne $ra) {
                    $resetAt = [DateTimeOffset]::FromUnixTimeSeconds([long]$ra).LocalDateTime
                    $remain = $resetAt - (Get-Date)
                    if ($remain.TotalSeconds -gt 0) {
                        [void]$sb.Append((Format-Duration ($remain.TotalMilliseconds)))
                    } else { [void]$sb.Append('0s') }
                } else { [void]$sb.Append('-') }
            }
            'week_remain' {
                $ra = Get-FieldRaw 'rate_limits.seven_day.resets_at'
                if ($null -ne $ra) {
                    $resetAt = [DateTimeOffset]::FromUnixTimeSeconds([long]$ra).LocalDateTime
                    $remain = $resetAt - (Get-Date)
                    if ($remain.TotalSeconds -gt 0) {
                        [void]$sb.Append((Format-Duration ($remain.TotalMilliseconds)))
                    } else { [void]$sb.Append('0s') }
                } else { [void]$sb.Append('-') }
            }
            'fable_remain' {
                $u = Get-CachedFableUsage
                $resetAt = $null
                if ($null -ne $u -and $u.resets_at) {
                    try { $resetAt = ([DateTimeOffset]::Parse([string]$u.resets_at, [System.Globalization.CultureInfo]::InvariantCulture)).LocalDateTime } catch {}
                }
                if ($null -ne $resetAt) {
                    $remain = $resetAt - (Get-Date)
                    if ($remain.TotalSeconds -gt 0) {
                        [void]$sb.Append((Format-Duration ($remain.TotalMilliseconds)))
                    } else { [void]$sb.Append('0s') }
                } else { [void]$sb.Append('-') }
            }
            # 구분자/포맷
            'sep_pipe'   { [void]$sb.Append('|') }
            'sep_dot'    { [void]$sb.Append('•') }
            'sep_dash'   { [void]$sb.Append('-') }
            'sep_arrow'  { [void]$sb.Append('>') }
            'sep_slash'  { [void]$sb.Append('/') }
            'sep_colon'  { [void]$sb.Append(':') }
            'space'      { [void]$sb.Append(' ') }
            'text'       { if ($null -ne $it.value) { [void]$sb.Append([string]$it.value) } }
            { $_ -like 'icon_*' } {
                $key = ([string]$_).Substring(5)
                if ($script:Icons.ContainsKey($key)) { [void]$sb.Append($script:Icons[$key]) }
            }
        }
    }
    Write-Output $sb.ToString()
}
