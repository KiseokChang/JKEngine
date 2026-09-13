# filedlg fix-round probe: spawn desktop, open filedlg via file_open with the
# Windows-label filter over the user's real folder, capture the dialog PNG via
# the capture_window agent tool (server framebuffer — no Win32 visibility
# dependency; ImGui apps are not WM_GETTEXT-readable, lesson 34).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"

# Korean literals built from code points (encoding-proof).
$dong  = -join [char[]](0xB3D9,0xC601,0xC0C1)                 # dong-yeong-sang
$pildo = -join [char[]](0xD30C,0xC77C,0x20,0xC5F4,0xAE30)     # title
$filter = "$dong (*" + ".mp4;*.mkv;*.avi;*.webm;*.mov)"
$bs = [string][char]92
$start = 'I:' + $bs + '@keep' + $bs + '200GANA-3420'
$startEsc = $start.Replace($bs, $bs + $bs)

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4

# file_open blocks until resolved (600s expiry) — run detached, dialog shows meanwhile.
$json = '{"tool":"file_open","args":{"filter":"' + $filter + '","start":"' + $startEsc + '"}}'
$job = Start-Job -ScriptBlock {
    param($exe, $json)
    $escaped = $json -replace '"', '\"'
    & $exe agentctl $escaped 2>&1 | Out-String
} -ArgumentList $exe, $json
Start-Sleep -Seconds 6

$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
Write-Host "list_windows: $list"
$wins = $list | ConvertFrom-Json
$dlg = $wins.windows | Where-Object { $_.title -eq $pildo } | Select-Object -First 1
if (-not $dlg) {
    Write-Host "FAIL: filedlg window not found in list_windows"
    if ($job.State -eq 'Running') { Stop-Job $job }
    Remove-Job $job -Force -ErrorAction SilentlyContinue
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

$cap = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $dlg.id + '}}')
Write-Host "capture_window: $cap"
$capPath = ""
if ($cap -match '"path\\?":\\?"([^"]*)"') {
    $capPath = $Matches[1] -replace '\\\\', '\'
}
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
if ($job.State -eq 'Running') { Stop-Job $job }
$out = Remove-Job $job -Force -ErrorAction SilentlyContinue -PassThru
if ($capPath -and (Test-Path $capPath)) {
    $dest = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-13-file-dialog\fix-title-list.png"
    Copy-Item $capPath $dest -Force
    Write-Host "saved $dest"
    exit 0
}
Write-Host "FAIL: capture_window returned no path"
exit 1