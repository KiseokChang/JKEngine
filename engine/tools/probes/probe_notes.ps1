# probe_notes.ps1 - notes hub end-to-end (specs/2026-09-18-notes-hub).
# ASCII-only (PS5.1). 14 checks: notes_read shape, add_note, add_note+win,
# agent src badge, add_item, move_item, del, id_not_found/bad_state, bad_op,
# bad_text, .bak 1st gen, 256KiB cap (write_failed + file intact), agent.notify
# event capture, GUI spawn.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$state = Join-Path $root "state"
$notesFile = Join-Path $state "notes.json"
$notesBak = Join-Path $state "notes.json.bak"
$script:fail = 0

function Invoke-Agentctl([string]$json) {
    # PS5.1 native quoting trap: values with spaces get split by argv
    # re-parsing when PS builds the command line (probe_settings only sends
    # spaceless values, notes text does not). Write the raw command line
    # ourselves: agentctl "{escaped-json}" — CRT turns \" into a literal ".
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return ($out -replace '(?m)^\[theme\][^\r\n]*[\r\n]*', '')
}
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host "ok: $name" } else { Write-Host "FAIL: $name"; $script:fail++ }
}
function Write-NoBom([string]$path, [string]$text) {
    [IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding($false)))
}

# --- state lifecycle: notes.json is ours (fresh), other state files are
# restore-or-remove (probe_settings MINOR-6 lesson: never burn user state).
$permFile = Join-Path $root "permissions.json"
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile (Join-Path $env:TEMP "perm_pre_notes.json") -Force }
$themeFile = Join-Path $root "theme.json"
$hadTheme = Test-Path $themeFile
if ($hadTheme) { Copy-Item $themeFile (Join-Path $env:TEMP "theme_pre_notes.json") -Force }
$kvFile = Join-Path $state "settings.json"
$hadKv = Test-Path $kvFile
if ($hadKv) { Copy-Item $kvFile (Join-Path $env:TEMP "kv_pre_notes.json") -Force }
# notes.json + .bak: back up first (opus MINOR-3 — a future user may have
# real notes; never burn them), then reset for a deterministic baseline.
$hadNotes = Test-Path $notesFile
if ($hadNotes) { Copy-Item $notesFile (Join-Path $env:TEMP "notes_pre.json") -Force }
$hadNotesBak = Test-Path $notesBak
if ($hadNotesBak) { Copy-Item $notesBak (Join-Path $env:TEMP "notes_pre_bak.json") -Force }
Remove-Item $notesFile -ErrorAction SilentlyContinue
Remove-Item $notesBak -ErrorAction SilentlyContinue

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 4

# --- 1. server up: notes_read shape (empty ok + two arrays)
$r1 = Invoke-Agentctl '{"tool":"notes_read","args":{}}'
Check "1-read-shape" ($r1 -match '"ok":true' -and $r1 -match '"notes":\[\]' -and $r1 -match '"backlog":\[\]')

# --- 2. add_note (server numbering from 1)
$r2 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"add_note","text":"probe comment one"}}'
Check "2-add-note" ($r2 -match '"ok":true' -and $r2 -match '"id":1')

# --- 3. add_note with win (window link id)
$r3 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"add_note","text":"probe linked comment","win":777}}'
Check "3-add-note-win" ($r3 -match '"id":2')
$r3b = Invoke-Agentctl '{"tool":"notes_read","args":{}}'
$linked = [regex]::Match($r3b, '\{"id":2,"text":"probe linked comment","win":777').Success
Check "3b-read-win" $linked

# --- 4. src badge: agentctl is a control-only connection -> "agent"
$agentBadge = ($r3b -match '"text":"probe comment one","win":0,"ts":\d+,"src":"agent"')
Check "4-src-agent" $agentBadge

# --- 5. add_item (backlog push, state 0)
$r5 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"add_item","text":"probe backlog A"}}'
$r5b = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"add_item","text":"probe backlog B"}}'
Check "5-add-item" ($r5 -match '"id":3' -and $r5b -match '"id":4')

# --- 6. move_item round-trip (state 0 -> 2)
$r6 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"move_item","id":3,"state":2}}'
$r6b = Invoke-Agentctl '{"tool":"notes_read","args":{}}'
$moved = [regex]::Match($r6b, '\{"id":3,"title":"probe backlog A","state":2').Success
Check "6-move-item" ($r6 -match '"ok":true' -and $moved)

# --- 7. del (note id 2)
$r7 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"del","id":2}}'
$r7b = Invoke-Agentctl '{"tool":"notes_read","args":{}}'
$gone = -not ([regex]::Match($r7b, '"id":2').Success)
Check "7-del" ($r7 -match '"ok":true' -and $gone)

# --- 8. id_not_found (del + move_item on unknown id)
$r8 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"del","id":99999}}'
$r8b = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"move_item","id":99999,"state":1}}'
Check "8-id-not-found" ($r8 -match '"error":"id_not_found"' -and $r8b -match '"error":"id_not_found"')

# --- 9. bad_op (whitelist)
$r9 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"zap"}}'
Check "9-bad-op" ($r9 -match '"error":"bad_op"')

# --- 10. bad_text (add_note text 600 > 512)
$long = "x" * 600
$r10 = Invoke-Agentctl ('{"tool":"notes_write","args":{"op":"add_note","text":"' + $long + '"}}')
Check "10-bad-text" ($r10 -match '"error":"bad_text"')

# --- 11. .bak 1st generation exists after writes
Check "11-bak-exists" (Test-Path $notesBak)

# --- 12. 256KiB cap: seed an oversized file, next add -> write_failed, file intact
$bigText = "B" * 280000   # seed row alone must push the next write past 256KiB
$seed = '{"notes":[{"id":900,"text":"' + $bigText + '","win":0,"ts":1700000000000,"src":"agent"}],"backlog":[],"next":901}'
Write-NoBom $notesFile $seed
$sizeBefore = (Get-Item $notesFile).Length
$r12 = Invoke-Agentctl '{"tool":"notes_write","args":{"op":"add_note","text":"cap check"}}'
$afterLen = (Get-Item $notesFile).Length
$r12b = Invoke-Agentctl '{"tool":"notes_read","args":{}}'
$intact = ($r12b -match '"id":900')
# refused write must leave the file byte-identical (no truncation — M5 vector)
Check "12-cap" ($r12 -match '"error":"write_failed"' -and $intact -and $afterLen -eq $sizeBefore)

# --- 13. agent.notify broadcast on agent add_note (control-only -> notify)
$evFile = Join-Path $env:TEMP "notes_ev.log"
Remove-Item $evFile -ErrorAction SilentlyContinue
Remove-Item $notesFile -ErrorAction SilentlyContinue
Remove-Item $notesBak -ErrorAction SilentlyContinue
$evProc = Start-Process -FilePath $exe -ArgumentList "agent-events", "8" `
    -WorkingDirectory $root -RedirectStandardOutput $evFile `
    -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 2
Invoke-Agentctl '{"tool":"notes_write","args":{"op":"add_note","text":"notify-check-marker-42"}}' | Out-Null
$gotNotify = $false
$deadline = (Get-Date).AddSeconds(8)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ((Test-Path $evFile) -and
        (Get-Content $evFile -Raw -ErrorAction SilentlyContinue) -match
        '"topic":"agent.notify"[^\r\n]*notify-check-marker-42') {
        $gotNotify = $true
        break
    }
}
Stop-Process -Id $evProc.Id -Force -ErrorAction SilentlyContinue
Check "13-agent-notify" $gotNotify

# --- 14. GUI spawn: launch_app notes -> Notes in list_windows
$r14 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"notes"}}'
Start-Sleep -Seconds 5
$l14 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$noteWin = [regex]::Match($l14, '"title\\?":\\?"Notes"').Success
Check "14-gui-spawn" ($r14 -match '"ok":true' -and $noteWin)

# --- cleanup: restore-or-remove state (MINOR-6 lesson)
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item $notesFile -ErrorAction SilentlyContinue
Remove-Item $notesBak -ErrorAction SilentlyContinue
if ($hadNotes) { Copy-Item (Join-Path $env:TEMP "notes_pre.json") $notesFile -Force }
if ($hadNotesBak) { Copy-Item (Join-Path $env:TEMP "notes_pre_bak.json") $notesBak -Force }
if ($hadPerm) { Copy-Item (Join-Path $env:TEMP "perm_pre_notes.json") $permFile -Force }
else { Remove-Item $permFile -ErrorAction SilentlyContinue }
if ($hadTheme) { Copy-Item (Join-Path $env:TEMP "theme_pre_notes.json") $themeFile -Force }
if ($hadKv) { Copy-Item (Join-Path $env:TEMP "kv_pre_notes.json") $kvFile -Force }
else { Remove-Item $kvFile -ErrorAction SilentlyContinue }

if ($script:fail -eq 0) { Write-Host "ALL PASS" } else { Write-Host "FAILED: $script:fail" }
exit ([int]($script:fail -gt 0))