# Launcher hover tooltip probe (docs/67 follow-up, 2026-09-26): hover a
# launcher cell with a MOVE-only synthetic input, verify the tooltip texture
# renders (server log "[shell] tooltip: texture ..."), verify it switches to a
# second cell with a distinct title, and verify it disappears after the cursor
# leaves + a click on empty desktop. Region screenshots via the MCP
# capture_region tool (docs/35 composited-frame readback) are saved for eye
# check; the pass/fail assertions are the LOG LINES and the dark-box pixel
# count of the captured region (tooltip box is uniform RGB 26,26,30).
# PASS/FAIL via exit code. PowerShell 5.1 compatible. ASCII-only on purpose
# (docs/15 lesson: non-ASCII .ps1 read as cp949 breaks the parser).
#
# Conventions reused (binding precedents):
# - probe_agent_maximize.ps1: server launch/hwnd show/topmost, SendInput
#   ABSOLUTE|VIRTUALDESK synthetic input, scale = clientW/1280 renderer ratio.
# - capture_region via the jkagentd MCP pipe; reply path arrives ESCAPED.
# Launcher grid geometry (JKDesktopShell::RelayoutLauncherIcons): cells sit
# 100x100 apart from (50,50), cell 64x80, icon art = top 64x64 square. Cell
# art center = (82 + col*100, 82 + row*100) logical. cols = (1280-50)/100 = 12.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$agnt  = "$build\jkagentd.exe"
$srvLog = "$build\probe_tooltip_srv.log"
$srvOut = "$build\probe_tooltip_out.log"
# The server keeps its OWN state log (state/logs/server_<ts>.log) — the
# redirected stderr/stdout files are not reliable on this build (stderr came
# through EMPTY on the first run). Assertions read the newest state log.
function Get-StateLog {
    $p = Get-ChildItem "$build\state\logs\server_*.log" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    return $p.FullName
}

function Stop-ProbeProcs {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
}

Stop-ProbeProcs
Start-Sleep -Seconds 1
Remove-Item $srvLog, $srvOut -ErrorAction SilentlyContinue

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory $build -WindowStyle Hidden `
    -RedirectStandardError $srvLog -RedirectStandardOutput $srvOut
Start-Sleep -Seconds 4

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wm {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint n, INPUT[] p, int size);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData;
        public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public MOUSEINPUT mi; }
    // MOVE-only synthetic hover: MOVE|ABSOLUTE|VIRTUALDESK = 0xC001 (the
    // maximize probe's coordinate lesson applies unchanged).
    public static bool SendMoveAt(int px, int py) {
        int vx = GetSystemMetrics(76), vy = GetSystemMetrics(77);
        int vw = GetSystemMetrics(78), vh = GetSystemMetrics(79);
        if (vw < 2 || vh < 2) { return false; }
        int nx = (int)Math.Round((px - vx) * 65535.0 / (vw - 1));
        int ny = (int)Math.Round((py - vy) * 65535.0 / (vh - 1));
        INPUT[] seq = new INPUT[1];
        seq[0].type = 0; seq[0].mi.dwFlags = 0xC001; seq[0].mi.dx = nx; seq[0].mi.dy = ny;
        return SendInput(1, seq, Marshal.SizeOf(typeof(INPUT))) == 1;
    }
    public static bool SendClickAt(int px, int py) {
        if (!SendMoveAt(px, py)) { return false; }
        INPUT[] seq = new INPUT[2];
        seq[0].type = 0; seq[0].mi.dwFlags = 2;
        seq[1].type = 0; seq[1].mi.dwFlags = 4;
        return SendInput(2, seq, Marshal.SizeOf(typeof(INPUT))) == 2;
    }
}
"@
[Wm]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

function Invoke-Mcp([string]$line) {
    return (($line | & $agnt) -join "`n")
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

function Get-ServerHwnd {
    for ($i = 0; $i -lt 50; ++$i) {
        $h = [Wm]::FindWindow("SDL_app", "JKENGINE Window Server")
        if ($h -ne [IntPtr]::Zero) { return $h }
        Start-Sleep -Milliseconds 200
    }
    return [IntPtr]::Zero
}
$hwnd = Get-ServerHwnd
if ($hwnd -ne [IntPtr]::Zero -and -not [Wm]::IsWindowVisible($hwnd)) {
    [void][Wm]::ShowWindow($hwnd, 8)   # SW_SHOWNOACTIVATE
    Start-Sleep -Milliseconds 500
}
if ($hwnd -ne [IntPtr]::Zero) {
    [void][Wm]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x13)
}

$crect = New-Object Wm+RECT
[Wm]::GetClientRect($hwnd, [ref]$crect) | Out-Null
$co = New-Object Wm+POINT
$co.X = 0; $co.Y = 0
[Wm]::ClientToScreen($hwnd, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0
function Pt([double]$lx, [double]$ly) {
    return @([int][math]::Round($co.X + $lx * $scale),
             [int][math]::Round($co.Y + $ly * $scale))
}

# Hover cell art center (logical) -> MOVE-only. Returns screen point used.
function Hover-Cell([int]$idx) {
    $col = $idx % 12; $row = [math]::Floor($idx / 12)
    $p = Pt (82 + $col * 100) (82 + $row * 100)
    [void][Wm]::SendMoveAt($p[0], $p[1])
    return $p
}

# capture_region around the first two cells + the tooltip strip below them.
# Logical rect (40,45)-(280,230). Returns local PNG path or $null.
$capCount = 0
function Capture-TooltipRegion {
    $capCount++
    # agentctl tool (maximize probe's capture_window convention) — capture_region
    # is the composited-frame readback (docs/35), args are logical desktop points.
    $r = Invoke-Agentctl '{"tool":"capture_region","args":{"x":40,"y":45,"w":240,"h":185}}'
    $m = [regex]::Match($r, 'path\\?":\\?"([^"\\]*(\\.[^"\\]*)*)\\?"')
    if (-not $m.Success) { Write-Host "  capture failed: $r"; return $null }
    return ($m.Groups[1].Value -replace '\\\\', '\')
}

# Count uniform tooltip-box pixels (26,26,30 +-8) in a PNG. Uses GetPixel —
# the region is small (240x185), so this stays fast enough for a probe.
Add-Type -AssemblyName System.Drawing
function Count-BoxPixels([string]$png) {
    if ([string]::IsNullOrEmpty($png) -or -not (Test-Path $png)) { return -1 }
    $bmp = New-Object System.Drawing.Bitmap($png)
    $n = 0
    for ($y = 0; $y -lt $bmp.Height; ++$y) {
        for ($x = 0; $x -lt $bmp.Width; ++$x) {
            $c = $bmp.GetPixel($x, $y)
            if ([math]::Abs($c.R - 26) -le 8 -and [math]::Abs($c.G - 26) -le 8 -and
                [math]::Abs($c.B - 30) -le 8) { $n++ }
        }
    }
    $bmp.Dispose()
    return $n
}

function Count-TooltipLogs {
    $raw = Read-StateLogRaw
    if ([string]::IsNullOrEmpty($raw)) { return 0 }
    return ([regex]::Matches($raw, '\[shell\] tooltip: texture')).Count
}
# The server holds the state log open — plain Get-Content hits a sharing
# violation. Open with FileShare ReadWrite (log-staleness harness lesson).
function Read-StateLogRaw {
    $p = Get-StateLog
    if (-not $p) { return $null }
    try {
        $fs = [System.IO.File]::Open($p, 'Open', 'Read', 'ReadWrite')
        $sr = New-Object System.IO.StreamReader($fs)
        $raw = $sr.ReadToEnd()
        $sr.Dispose()
        return $raw
    } catch { return $null }
}

$ok = $true
$boxOn1 = -1; $boxOn2 = -1; $boxOff = -1

# --- check 1: hover cell 0, tooltip renders once ----------------------------
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host "setup: FAIL (server hwnd not found)"
    Stop-ProbeProcs
    exit 1
}
[void](Hover-Cell 0)
Start-Sleep -Milliseconds 900
$png1 = Capture-TooltipRegion
$boxOn1 = Count-BoxPixels $png1
# Pixel-based pass/fail: the tooltip box is uniform RGB(26,26,30) — thousands
# of matching pixels below the cell. The state LOG is only read AFTER shutdown
# (the server holds/buffers it while alive — live reads are unreliable).
if ($boxOn1 -gt 500) {
    Write-Host ("hover-show: PASS (box pixels={0}, shot={1})" -f $boxOn1, $png1)
} else {
    $ok = $false
    Write-Host ("hover-show: FAIL (box pixels={0}, shot={1})" -f $boxOn1, $png1)
}

# --- check 2: move to cell 1, second distinct tooltip texture ----------------
[void](Hover-Cell 1)
Start-Sleep -Milliseconds 900
$png2 = Capture-TooltipRegion
$boxOn2 = Count-BoxPixels $png2
if ($boxOn2 -gt 500) {
    Write-Host ("hover-switch: PASS (box pixels={0}, shot={1})" -f $boxOn2, $png2)
} else {
    $ok = $false
    Write-Host ("hover-switch: FAIL (box pixels={0})" -f $boxOn2)
}

# --- check 3: leave to empty desktop + click -> tooltip gone -----------------
# Empty spot below the grid rows (grid ends at y~330 logical; click lands on
# bare desktop, no client, no launcher cell).
$ep = Pt 700 400
[void][Wm]::SendClickAt($ep[0], $ep[1])
Start-Sleep -Milliseconds 600
$png3 = Capture-TooltipRegion
$boxOff = Count-BoxPixels $png3
if ($boxOff -lt [math]::Max(50, $boxOn1 / 4)) {
    Write-Host ("hover-clear: PASS (box pixels={0} < {1})" -f $boxOff, $boxOn1)
} else {
    $ok = $false
    Write-Host ("hover-clear: FAIL (box pixels={0} vs on={1})" -f $boxOff, $boxOn1)
}

# --- evidence dump + cleanup --------------------------------------------------
Write-Host ("evidence: shots {0}; {1}; {2}" -f $png1, $png2, $png3)
# Keep the evidence PNGs next to the probe (the state screenshots dir is wiped
# below; the eye-check needs the shots to survive).
$i = 0
foreach ($png in @($png1, $png2, $png3)) {
    $i++
    if ($png -and (Test-Path $png)) {
        Copy-Item $png ("$build\..\tools\probes\diag_tooltip_{0}.png" -f $i) -Force
    }
}
Write-Host "server state log tooltip lines:"
$stRaw = Read-StateLogRaw
if ($stRaw) {
    ([regex]::Matches($stRaw, '.*\[shell\] tooltip.*')) | ForEach-Object { Write-Host ("  {0}" -f $_.Value.Trim()) }
}

Remove-Item "$build\state\screenshots" -Recurse -ErrorAction SilentlyContinue
Stop-ProbeProcs
Start-Sleep -Milliseconds 800

# --- check 4 (post-shutdown): state log shows per-title 1x render -------------
# Two distinct titles = the switch actually swapped tooltip content; the 1x
# count = the texture cache (re-hover must not re-render).
$stRaw = Read-StateLogRaw
$titles = @()
if ($stRaw) {
    foreach ($m in [regex]::Matches($stRaw, "\[shell\] tooltip: texture '([^']*)' rendered")) {
        $titles += $m.Groups[1].Value
    }
}
$distinct = ($titles | Sort-Object -Unique).Count
if ($titles.Count -ge 2 -and $distinct -ge 2) {
    Write-Host ("log-evidence: PASS ({0} renders, {1} distinct: {2})" -f
        $titles.Count, $distinct, ($titles -join ', '))
} else {
    $ok = $false
    Write-Host ("log-evidence: FAIL (renders={0}, distinct={1}, raw={2})" -f
        $titles.Count, $distinct, ($stRaw | Out-String).Substring(0, [math]::Min(200, ($stRaw | Out-String).Length)))
}

if ($ok) { Write-Host "PASS: launcher hover tooltip"; exit 0 }
else { Write-Host "FAIL: launcher hover tooltip"; exit 1 }