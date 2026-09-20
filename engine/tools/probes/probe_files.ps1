# probe_files.ps1 - file hub end-to-end (specs/2026-09-18-file-hub).
# ASCII-only (PS5.1). Checks 12: list shape/subdir/not_found, path rejection
# (relative/UNC/../), read text/truncated/binary/missing, agent ask parking
# (event capture + approve + re-executed reply to the live requester), deny
# file value, files_audit rows (receipts tail filter), GUI spawn.
#
# Path convention: probe JSON uses FORWARD-slash paths — the CRT argument
# quoter halves backslash runs adjacent to the closing escaped quote, so
# backslash paths through `agentctl "<json>"` arrive mangled (server accepts
# both separators; the GUI browser feeds backslash paths natively).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$state = Join-Path $root "state"
$rcpt = Join-Path $state "receipts.jsonl"
$permFile = Join-Path $root "permissions.json"
$kvFile = Join-Path $state "settings.json"
$themeFile = Join-Path $root "theme.json"
$script:fail = 0

# docs/55 lesson (c): PS5.1 native arg assembly breaks on quoted values —
# ProcessStartInfo.Arguments raw command line + JSON line filter ([theme]
# stdout loader line, docs/52).
function Invoke-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.WorkingDirectory = $root
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return (($out -split "`n" | Where-Object { $_ -match '^\s*\{' }) -join "`n")
}
# Park: fire the call WITHOUT waiting for the reply — returns the process so
# the parked requester stays alive and its stdout carries the re-executed
# result after the approval resolves.
function Park-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.WorkingDirectory = $root
    return [System.Diagnostics.Process]::Start($psi)
}
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host "ok: $name" } else { Write-Host "FAIL: $name"; $script:fail++ }
}
function Write-NoBom([string]$path, [string]$text) {
    [IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding($false)))
}
function Wait-EventLine([string]$file, [string]$needle, [int]$maxSec) {
    $deadline = (Get-Date).AddSeconds($maxSec)
    while ((Get-Date) -lt $deadline) {
        $t = (Get-Content $file -Raw -ErrorAction SilentlyContinue)
        if ($t -and ($t -match $needle)) { return $t }
        Start-Sleep -Milliseconds 500
    }
    return ""
}
function Last-RequestId([string]$text) {
    if (-not $text) { return 0 }
    $all = [regex]::Matches($text, '"request":(\d+)')
    if ($all.Count -eq 0) { return 0 }
    return [int]$all[$all.Count - 1].Groups[1].Value
}

# --- state lifecycle: back up what we touch, restore at the end
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile (Join-Path $env:TEMP "perm_pre_files.json") -Force }
$hadRcpt = Test-Path $rcpt
if ($hadRcpt) { Copy-Item $rcpt (Join-Path $env:TEMP "rcpt_pre_files.json") -Force }
$hadKv = Test-Path $kvFile
if ($hadKv) { Copy-Item $kvFile (Join-Path $env:TEMP "kv_pre_files.json") -Force }
$hadTheme = Test-Path $themeFile
if ($hadTheme) { Copy-Item $themeFile (Join-Path $env:TEMP "theme_pre_files.json") -Force }

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
# Readiness poll, not a fixed sleep: agentctl before the pipe exists fails
# with CreateFileA(2) and the first checks record bogus FAILs (2026-09-20
# nested run: checks 1-2 died on a server that needed >4s to open the pipe).
$up = $false
foreach ($i in 1..40) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
if (-not $up) { Write-Host "FAIL: server never came up"; exit 1 }

# checks 1-8 run with explicit allow (control-only agentctl, no parking);
# checks 9/10 exercise ask/deny file values.
Write-NoBom $permFile '{"files_list":"allow","files_read":"allow"}'
Start-Sleep -Milliseconds 500

$tools = "I:/progwork/JKENGINE/engine/tools"

# --- 1. list root shape: ok + entries + dirs first + capped field
$r1 = Invoke-Agentctl ('{"tool":"files_list","args":{"path":"' + $tools + '"}}')
$firstKind = ""
if ($r1 -match '"entries":\[\{"name":"[^"]*","kind":"([^"]*)"') { $firstKind = $Matches[1] }
Check "1-list-shape" ($r1 -match '"ok":true' -and $r1 -match '"entries"' -and
                      $firstKind -eq "dir" -and $r1 -match '"capped":0')

# --- 2. list subdirectory: known child dirs present
$r2 = Invoke-Agentctl ('{"tool":"files_list","args":{"path":"' + $tools + '/probes"}}')
Check "2-list-subdir" ($r2 -match '"ok":true' -and $r2 -match '"probe_files\.ps1"')

# --- 3. list not_found
$r3 = Invoke-Agentctl '{"tool":"files_list","args":{"path":"I:/no_such_dir_xyz_probe"}}'
Check "3-list-notfound" ($r3 -match '"error":"not_found"')

# --- 4. path rejection: relative / UNC / ..
$r4a = Invoke-Agentctl '{"tool":"files_list","args":{"path":"relative"}}'
$r4b = Invoke-Agentctl '{"tool":"files_list","args":{"path":"//server/share"}}'
$r4c = Invoke-Agentctl '{"tool":"files_list","args":{"path":"I:/progwork/JKENGINE/engine/../x"}}'
Check "4-path-reject" ($r4a -match '"error":"bad_path"' -and
                       $r4b -match '"error":"bad_path"' -and
                       $r4c -match '"error":"bad_path"')

# --- 5. read text
$r5 = Invoke-Agentctl '{"tool":"files_read","args":{"path":"I:/progwork/JKENGINE/engine/build/state/settings.json"}}'
Check "5-read-text" ($r5 -match '"ok":true' -and $r5 -match '"binary":0' -and
                     $r5 -match 'mute')   # text 필드 안에서는 \"mute\"로 이스케이프


# --- 6. read truncated (maxBytes=8)
$r6 = Invoke-Agentctl '{"tool":"files_read","args":{"path":"I:/progwork/JKENGINE/engine/build/state/settings.json","maxBytes":8}}'
Check "6-read-truncated" ($r6 -match '"ok":true' -and $r6 -match '"truncated":1')

# --- 7. read binary (NUL sniff -> binary:1, empty text)
$r7 = Invoke-Agentctl '{"tool":"files_read","args":{"path":"I:/progwork/JKENGINE/engine/build/jkchat.exe"}}'
Check "7-read-binary" ($r7 -match '"ok":true' -and $r7 -match '"binary":1' -and
                       $r7 -match '"text":""')

# --- 8. read missing
$r8 = Invoke-Agentctl '{"tool":"files_read","args":{"path":"I:/no_such_file_probe.txt"}}'
Check "8-read-missing" ($r8 -match '"error":"not_found"')

# --- 9. agent ask parking: park (live requester) -> approval_request event ->
#        approve -> parked requester receives the re-executed result
# subscriber availability counts control-only conns (docs/54 M3) — the
# agent-events harness is one.
Write-NoBom $permFile '{"files_list":"ask","files_read":"ask"}'
Start-Sleep -Milliseconds 500
$evPath = Join-Path $env:TEMP "files_events.txt"
Remove-Item $evPath -Force -ErrorAction SilentlyContinue
$evProc = Start-Process -FilePath $exe -ArgumentList "agent-events","90" `
    -WorkingDirectory $root -WindowStyle Hidden `
    -RedirectStandardOutput $evPath -PassThru
Start-Sleep -Seconds 2
$parked = Park-Agentctl ('{"tool":"files_list","args":{"path":"' + $tools + '"}}')
Start-Sleep -Seconds 2
$evText = Wait-EventLine $evPath '"topic":"agent\.approval_request".*?"kind":"files_access"' 20
$reqId = Last-RequestId $evText
$approved = ""
if ($reqId -gt 0) {
    $approved = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
}
$parkedOk = $false
$reexec = ""
if ($approved -match '"approved":true') {
    $parked.WaitForExit(15000) | Out-Null
    $reexec = (($parked.StandardOutput.ReadToEnd()) -split "`n" |
               Where-Object { $_ -match '^\s*\{' }) -join "`n"
    $parkedOk = ($reexec -match '"ok":true' -and $reexec -match '"entries"')
}
Check "9-ask-park-approve" ($evText -ne "" -and $approved -match '"approved":true' -and $parkedOk)
if (-not $parkedOk) {
    Write-Host "  (ev=$($evText -ne '') req=$reqId app=$approved reexec=$reexec)"
}

# --- 10. deny file value -> immediate permission_denied
Write-NoBom $permFile '{"files_list":"deny","files_read":"deny"}'
Start-Sleep -Milliseconds 500
$r10 = Invoke-Agentctl ('{"tool":"files_list","args":{"path":"' + $tools + '"}}')
$r10b = Invoke-Agentctl '{"tool":"files_read","args":{"path":"I:/progwork/JKENGINE/engine/build/state/settings.json"}}'
Check "10-deny-both" ($r10 -match '"error":"permission_denied"' -and
                      $r10b -match '"error":"permission_denied"')

# --- 11. files_audit: receipts tail scan, files_* rows only, newest first
Write-NoBom $permFile '{}'
$nowMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
Write-NoBom $rcpt ("{`"ts`":" + ($nowMs - 2000) + ",`"tool`":`"files_list`",`"args`":{`"path`":`"I:/old/path`"},`"result`":{`"ok`":true}}`n" +
                   "{`"ts`":" + $nowMs + ",`"tool`":`"files_read`",`"args`":{`"path`":`"I:/new/path`"},`"result`":{`"ok`":false}}`n" +
                   "{`"ts`":" + $nowMs + ",`"tool`":`"notes_write`",`"args`":{`"op`":`"add_note`",`"text`":`"x`"},`"result`":{`"ok`":true}}`n")
Start-Sleep -Milliseconds 500
$r11 = Invoke-Agentctl '{"tool":"files_audit","args":{"limit":10}}'
$auditOk = ($r11 -match '"ok":true') -and
           ($r11 -match '"tool":"files_read"[^}]*"ok":"0"') -and
           ($r11 -match '"path":"I:/new/path"') -and
           ($r11 -notmatch 'notes_write')
# newest first: files_read row precedes files_list row
$idxNew = $r11.IndexOf('I:/new/path')
$idxOld = $r11.IndexOf('I:/old/path')
Check "11-audit-rows" ($auditOk -and $idxNew -ge 0 -and $idxOld -gt $idxNew)

# --- 12. GUI spawn
Write-NoBom $permFile '{"files_list":"allow","files_read":"allow"}'
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"files"}}' | Out-Null
Start-Sleep -Seconds 4
$wins = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
Check "12-gui-spawn" ($wins -match '"title":"Files"')

# --- cleanup (opus 리뷰 NIT-9: agent-events 하니스 + 이벤트 캡처 파일도 회수)
if ($evProc -and -not $evProc.HasExited) { Stop-Process -Id $evProc.Id -Force -ErrorAction SilentlyContinue }
Remove-Item $evPath -Force -ErrorAction SilentlyContinue
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
if ($hadPerm) { Copy-Item (Join-Path $env:TEMP "perm_pre_files.json") $permFile -Force }
else { Remove-Item $permFile -Force -ErrorAction SilentlyContinue }
if ($hadRcpt) { Copy-Item (Join-Path $env:TEMP "rcpt_pre_files.json") $rcpt -Force }
else { Remove-Item $rcpt -Force -ErrorAction SilentlyContinue }
if ($hadKv) { Copy-Item (Join-Path $env:TEMP "kv_pre_files.json") $kvFile -Force }
if ($hadTheme) { Copy-Item (Join-Path $env:TEMP "theme_pre_files.json") $themeFile -Force }

if ($script:fail -eq 0) { Write-Host "PASS: file hub" ; exit 0 }
Write-Host "FAIL: file hub ($script:fail)"
exit 1