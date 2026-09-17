# Trigger rate-limit acceptance probe (docs/38 spec, docs/32 Task 3):
# burst drop at the server connection cap, self-loop stop at the local
# handler cap, per-source budget isolation, timer cap. PASS/FAIL via exit
# code. Probe bundle rate_probe goes the PACKAGE path (--pack →
# build/apps/triggers/rate_probe.jkx): a dev .js would prompt-park at the
# trust gate, while the packer self-attests at pack time so the container
# is trusted from boot with no prompt.
#
# Conventions borrowed from probe_agent_trust.ps1:
# - Pack-record preservation: the packer attests at jkx-pack time (NOT at
#   load), so a wholesale trust.json delete would strand the pack
#   containers — the store is rewritten keeping only source:"pack"
#   records, and cleanup restores that same post-pack state.
# - Self-healing repack: the offline packer is re-run when pack records
#   are missing (container bytes are deterministic → fingerprints match).
#
# Caps under test (consume-on-allow on both sides):
#   server publish_event: 60/10s per connection → 70-burst = 60 broadcast
#   + 10 {"ok":true,"dropped":1} (dropped events never reach events_list).
#   local handler cap: 60 fires/60s per source (= container name — every
#   handler in rate_probe shares one budget) → the self-loop stops at the
#   61st invocation with one log + one notify per window. The isolation and
#   timer phases therefore run AFTER a 60s local-window reset.
$ErrorActionPreference = "Continue"
$exe   = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig  = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
$chat  = "I:\progwork\JKENGINE\engine\build\jkchat.exe"
$state = "I:\progwork\JKENGINE\engine\build\state"
$trustFile = "$state\trust.json"
$devDir = "$state\triggers"
$trigLog = "$env:TEMP\rate_probe.log"
$srvLog = "$env:TEMP\rate_probe_server.log"

Get-Process jkdesktop,jktriggers,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Rewrite the trust store keeping only source:"pack" records (4 after the
# rate_probe bundle landed: 3 originals + rate_probe).
function Write-PackOnlyStore {
    $packs = @()
    if (Test-Path $trustFile) {
        try {
            $store = Get-Content $trustFile -Raw | ConvertFrom-Json
            $packs = @($store.records | Where-Object { $_.source -eq 'pack' })
        } catch { }
    }
    $parts = foreach ($p in $packs) {
        '{"fingerprint":"' + $p.fingerprint + '","name":"' + $p.name +
        '","source":"pack","ts":' + [int64]$p.ts + '}'
    }
    ('{"records":[' + ($parts -join ',') + ']}') |
        Set-Content -Path $trustFile -Encoding ASCII
    return $packs.Count
}

$packCount0 = Write-PackOnlyStore
if ($packCount0 -lt 4) {
    # Self-healing: re-run the offline packer (never contacts the server,
    # CMake's own build step) to regenerate the containers and re-attest.
    Write-Host "pack records missing — re-running the offline packer to re-attest"
    & $trig --pack "I:\progwork\JKENGINE\engine\tools\triggers" `
        "I:\progwork\JKENGINE\engine\build\apps\triggers" | Out-Null
    $packCount0 = Write-PackOnlyStore
}
if ($packCount0 -lt 4) {
    Write-Host "WARNING: only $packCount0 pack record(s) after re-pack"
}
# A leftover threshold-0 idle file would fire a stray trig_idle notify into
# the transcript mid-run (probe_agent_triggers hygiene).
Remove-Item "$state\idle_minutes" -ErrorAction SilentlyContinue

# Read a file the child process still holds open (redirected stdout).
function Read-Shared([string]$path) {
    try {
        $fs = [System.IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
        $sr = New-Object System.IO.StreamReader($fs)
        $t = $sr.ReadToEnd()
        $sr.Close()
        return $t
    } catch { return "" }
}

# server up — stdout redirected: "[server] publish_event rate-capped" is
# the server-side drop evidence (flushed per line by the server).
Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden `
    -RedirectStandardOutput $srvLog -RedirectStandardError "$env:TEMP\rate_probe_server.err"
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    # raw JSON in (it escapes); no spaces in data — native argv (lesson 26).
    # [theme] stdout loader line (docs/52) breaks ConvertFrom-Json — probe_agent_shot
    # same fix. Only JSON lines pass.
    $escaped = $json -replace '"', '\"'
    $raw = (& $exe agentctl $escaped) -join "`n"
    return ($raw -split "`n" | Where-Object { $_ -match '^\s*\{' }) -join "`n"
}

# --- jkchat FIRST (lesson 28: subscribers up before the kicks fire) -------
$chatProc = Start-Process -FilePath $chat -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 3

# --- Win32 helpers for cross-process transcript reading -------------------
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class W8 {
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll", EntryPoint = "SendMessageW")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendMessageBuf(IntPtr h, uint m, IntPtr w, System.Text.StringBuilder l);
    public static string EditWindowText(IntPtr h) {
        int len = (int)(long)SendMessage(h, 0x000E, IntPtr.Zero, IntPtr.Zero);
        int cap = len * 2 + 4096;
        System.Text.StringBuilder sb = new System.Text.StringBuilder(cap);
        SendMessageBuf(h, 0x000D, (IntPtr)cap, sb);
        return sb.ToString().TrimEnd('\0');
    }
}
"@

$p = Get-Process jkchat -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$hLog = [IntPtr]::Zero
$global:edits = @()
$cb = {
    param($h, $l)
    $sb = New-Object System.Text.StringBuilder 64
    [W8]::GetClassNameW($h, $sb, 64) | Out-Null
    if ($sb.ToString() -eq "Edit") { $global:edits += $h }
    return $true
}
if ($p) { [W8]::EnumChildWindows($p.MainWindowHandle, $cb, [IntPtr]::Zero) | Out-Null }
foreach ($h in $edits) {
    $t = [W8]::EditWindowText($h)
    if ($t -match '연결') { $hLog = $h; break }
}
if ($hLog -eq [IntPtr]::Zero -and $edits.Count -gt 0) { $hLog = $edits[0] }

# --- jktriggers second (redirected stdout = the probe log) ----------------
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) `
    -WindowStyle Hidden `
    -RedirectStandardOutput $trigLog -RedirectStandardError "$env:TEMP\rate_probe.err"
Start-Sleep -Seconds 3

$t0 = Get-Date

# --- 1. burst: 70 publishes -> 60 broadcast + 10 server drops -------------
$r1 = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"ratelimit.kick_burst","data":{"n":1}}}'
Start-Sleep -Seconds 5
$evJson = Invoke-Agentctl '{"tool":"events_list","args":{}}'
$fired = -1
try {
    $ev = $evJson | ConvertFrom-Json
    $entry = $ev.events | Where-Object { $_.topic -eq 'ratelimit.server' }
    if ($entry) { $fired = [int]$entry.fired }
} catch { }
$host1 = Read-Shared $trigLog
$burstDone = ($host1 -match 'burst done')
$capped = ([regex]::Matches((Read-Shared $srvLog), 'publish_event rate-capped')).Count
$check1 = ($fired -eq 60) -and ($capped -eq 1) -and $burstDone

# --- 2. server window reset (10s) -> self-loop stops at the local cap -----
$elapsed = ((Get-Date) - $t0).TotalSeconds
$wait2 = 11 - $elapsed
if ($wait2 -gt 0) { Start-Sleep -Seconds ([int][Math]::Ceiling($wait2)) }
$r2 = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"ratelimit.kick_loop","data":{"n":1}}}'
Start-Sleep -Seconds 7   # 50ms tick per ping hop + jkchat transcript latency
$host2 = Read-Shared $trigLog
$pings = ([regex]::Matches($host2, 'ping:\d+')).Count
$check2 = ($pings -ge 55) -and ($pings -le 61)
$limitLogs = ([regex]::Matches($host2, 'rate limit:')).Count
$check3 = ($limitLogs -eq 1)

# --- 3. isolation: other kick fires in the reset local budget window ------
# The local cap is per source (= container): after the self-loop the shared
# rate_probe budget is exhausted, so phases 3/4 need the 60s window reset.
$elapsed = ((Get-Date) - $t0).TotalSeconds
$wait3 = 63 - $elapsed
if ($wait3 -gt 0) { Start-Sleep -Seconds ([int][Math]::Ceiling($wait3)) }
$r3 = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"ratelimit.other","data":{"n":1}}}'
Start-Sleep -Seconds 3
$host3 = Read-Shared $trigLog
$check5 = ($host3 -match 'other fired')

# --- 4. timer cap: 64 slots (trig_idle's interval holds one) -> 7 drops ---
$r4 = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"ratelimit.kick_timers","data":{"n":1}}}'
Start-Sleep -Seconds 3
$host4 = Read-Shared $trigLog
$drops = ([regex]::Matches($host4, 'timer dropped')).Count
$check6 = ($drops -eq 7)

# --- transcript read BEFORE killing processes (lesson 33) -----------------
$transcript = ""
if ($hLog -ne [IntPtr]::Zero) { $transcript = [W8]::EditWindowText($hLog) }
$notifyCount = ([regex]::Matches($transcript, '트리거 발화 제한')).Count
$check4 = ($notifyCount -eq 1)

# --- cleanup ---------------------------------------------------------------
Get-Process jkdesktop,jktriggers,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
# restore the post-pack steady state: pack records only
Write-PackOnlyStore | Out-Null

# --- judge -----------------------------------------------------------
if ($r1 -match 'ok\\?":true') { Write-Host "kick-burst-published: ok" } else { Write-Host "kick-burst-published: FAIL $r1" }
if ($check1) {
    Write-Host "burst-drop: PASS (fired=$fired server-capped=$capped burst-done=yes)"
} else {
    Write-Host "burst-drop: FAIL (fired=$fired server-capped=$capped burstDone=$burstDone)"
    Write-Host "--- trigger host log ---"; Write-Host $host1
    Write-Host "--- server log ---"; Write-Host (Read-Shared $srvLog)
    exit 1
}
if ($check2) { Write-Host "self-loop-cap: PASS (pings=$pings, expect 58 = 60 budget - 2 kicks)" }
else {
    Write-Host "self-loop-cap: FAIL (pings=$pings)"
    Write-Host "--- trigger host log (tail) ---"; Write-Host ($host2.Substring([Math]::Max(0, $host2.Length - 1500)))
    exit 1
}
if ($check3) { Write-Host "rate-limit-log-once: PASS" } else { Write-Host "rate-limit-log-once: FAIL (count=$limitLogs)"; exit 1 }
if ($check4) { Write-Host "notify-once: PASS" } else { Write-Host "notify-once: FAIL (transcript count=$notifyCount)"; Write-Host $transcript; exit 1 }
if ($check5) { Write-Host "isolation: PASS" } else { Write-Host "isolation: FAIL (no 'other fired')"; Write-Host $host3; exit 1 }
if ($check6) { Write-Host "timer-cap: PASS (7 drops)" } else { Write-Host "timer-cap: FAIL (drops=$drops, expect 7)"; Write-Host $host4; exit 1 }

# --- 7. regression: triggers + trust (separate runs) -----------------------
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
    "I:\progwork\JKENGINE\engine\tools\probes\probe_agent_triggers.ps1" 2>&1 | Out-Host
$regTriggers = $LASTEXITCODE
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
    "I:\progwork\JKENGINE\engine\tools\probes\probe_agent_trust.ps1" 2>&1 | Out-Host
$regTrust = $LASTEXITCODE
if ($regTriggers -eq 0 -and $regTrust -eq 0) { Write-Host "regression: PASS" }
else { Write-Host "regression: FAIL (triggers=$regTriggers trust=$regTrust)"; exit 1 }

Write-Host "PASS: agent ratelimit"
exit 0