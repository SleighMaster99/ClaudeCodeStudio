<#
.SYNOPSIS
    BMad6GitHub Git Hooks 설치 스크립트
.DESCRIPTION
    pre-commit, commit-msg hook을 .git/hooks에 설치합니다.
.PARAMETER Uninstall
    hook 제거
.EXAMPLE
    .\install-hooks.ps1
    .\install-hooks.ps1 -Uninstall
#>

param(
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

# 색상 출력 함수
function Write-Info { param($msg) Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Success { param($msg) Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warn { param($msg) Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err { param($msg) Write-Host "[ERROR] $msg" -ForegroundColor Red }

# Git 저장소 확인
$gitDir = git rev-parse --git-dir 2>$null
if (-not $gitDir) {
    Write-Err "Git 저장소가 아닙니다."
    exit 1
}

$hooksDir = Join-Path $gitDir "hooks"
$sourceDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$hooks = @("pre-commit", "commit-msg", "pre-push", "post-merge")

if ($Uninstall) {
    Write-Info "Git Hooks 제거 중..."
    foreach ($hook in $hooks) {
        $target = Join-Path $hooksDir $hook
        if (Test-Path $target) {
            Remove-Item $target -Force
            Write-Success "$hook 제거됨"
        }
    }
    Write-Success "Git Hooks 제거 완료"
    exit 0
}

Write-Info "BMad6GitHub Git Hooks 설치 중..."

# hooks 폴더 생성
if (-not (Test-Path $hooksDir)) {
    New-Item -ItemType Directory -Path $hooksDir | Out-Null
}

foreach ($hook in $hooks) {
    $source = Join-Path $sourceDir $hook
    $target = Join-Path $hooksDir $hook

    if (-not (Test-Path $source)) {
        Write-Warn "$hook 소스 파일 없음: $source"
        continue
    }

    # 기존 hook 백업
    if (Test-Path $target) {
        $backup = "$target.backup"
        Copy-Item $target $backup -Force
        Write-Warn "기존 $hook 백업됨: $backup"
    }

    # hook 복사 (CRLF → LF 변환 — bash 스크립트는 LF 필수)
    $content = Get-Content $source -Raw
    $content = $content -replace "`r`n", "`n"
    [System.IO.File]::WriteAllText($target, $content, [System.Text.UTF8Encoding]::new($false))
    Write-Success "$hook 설치됨"
}

Write-Host ""
Write-Success "Git Hooks 설치 완료!"
Write-Host ""
Write-Info "설치된 Hook:"
Write-Host "  - pre-commit: master 브랜치 직접 커밋 차단" -ForegroundColor White
Write-Host "  - commit-msg: Conventional Commits 형식 검증" -ForegroundColor White
Write-Host "  - pre-push: master 브랜치 직접 푸시 차단" -ForegroundColor White
Write-Host "  - post-merge: 머지된 로컬 브랜치 자동 정리" -ForegroundColor White
Write-Host ""
Write-Info "Hook 제거: .\install-hooks.ps1 -Uninstall"
