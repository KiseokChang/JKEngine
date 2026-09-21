# semc4_geom_adhoc.ps1 - one-off diagnostic (Task 4): verify the REAL
# minesweeper client's measured cursor declaration geometry against the
# server-drawn highlight. Move the platform cursor to a distinctive cell,
# then capture_region the exact cell rect implied by the declared
# origin/cellW (client==logical px for this unscaled layer) and a control
# rect 3 cells away. The blue cursor-cell ring (0,120,212) must appear in the
# target rect and be absent from the control rect. Run on the LIVE desktop
# (no teardown). ASCII-only (PS5.1).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}
function Invoke-Agentctl([string]$json) {
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
function Capture-Region([int]$x, [int]$y, [int]$w, [int]$h) {
    $r = Invoke-Agentctl ('{"tool":"capture_region","args":{"x":' + $x + ',"y":' + $y + ',"w":' + $w + ',"h":' + $h + '}}')
    $pm = [regex]::Match($r, '"path\\?":\\?"([^"]*)"')
    if (-not $pm.Success) { return "" }
    return ($pm.Groups[1].Value -replace '\\\\', '\')
}
function Count-Color([string]$png, [int]$r, [int]$g, [int]$b) {
    Add-Type -AssemblyName System.Drawing
    $bmp = New-Object System.Drawing.Bitmap($png)
    $n = 0
    for ($y = 0; $y -lt $bmp.Height; $y++) {
        for ($x = 0; $x -lt $bmp.Width; $x++) {
            $c = $bmp.GetPixel($x, $y)
            if ([int]$c.R -eq $r -and [int]$c.G -eq $g -and [int]$c.B -eq $b) { $n++ }
        }
    }
    $bmp.Dispose()
    return $n
}

# Live desktop - start from exactly one minesweeper instance: kill any
# existing clients (agentctl list_windows is plain JSON, not MCP-escaped) and
# launch once (the server rejects multi-instance relays as ambiguous).
$lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
[regex]::Matches($lw, '"id":(\d+),"title":"Minesweeper","pid":(\d+)') | ForEach-Object {
    Stop-Process -Id ([int]$_.Groups[2].Value) -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Seconds 1
$winId = 0; $wx = 0; $wy = 0; $lw = ""
[void](Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}')
for ($i = 0; $i -lt 16; $i++) {
    Start-Sleep -Milliseconds 500
    $lw = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $w2 = [regex]::Match($lw, '"id":(\d+),"title":"Minesweeper","pid":\d+,"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if ($w2.Success) {
        $winId = [int]$w2.Groups[1].Value
        $wx = [int]$w2.Groups[2].Value
        $wy = [int]$w2.Groups[3].Value
        break
    }
}
Check "geom-launch" ($winId -gt 0) ("list=" + $lw)
if ($winId -le 0) { Write-Output "FAILED: launch"; exit 1 }

# Move the platform cursor to (4,4); the ring must appear on that cell.
$mv = Invoke-Agentctl '{"tool":"app_tool","args":{"app":"minesweeper","tool":"move","args":{"to_row":4,"to_col":4}}}'
Check "geom-move" ($mv -match '"ok":true' -and $mv -match '"row":4') $mv
Start-Sleep -Milliseconds 1500   # let the next composite frame paint the ring

# Declared geometry measured at spawn (MineGrid::GetBoardGeometry, 320x380
# surface, Beginner 9x9): origin (16,75), cell 32x32. Cell (4,4) rect:
# x=16+4*32=144, y=75+4*32=203, 32x32 - in LOGICAL desktop coords this layer
# has scale 1, so the cell rect is (wx+144, wy+203, 32, 32).
$cx = $wx + 144; $cy = $wy + 203
$ringPng = Capture-Region $cx $cy 32 32
$ringCount = 0
if ($ringPng -ne "" -and (Test-Path $ringPng)) {
    $ringCount = Count-Color $ringPng 0 120 212
}
Check "geom-ring-on-declared-cell" ($ringCount -gt 0) ("png=" + $ringPng + " blue=" + $ringCount)

# Control: 3 cells right (x+96) - no declared cursor there, no blue ring.
$ctlPng = Capture-Region ($cx + 96) $cy 32 32
$ctlCount = -1
if ($ctlPng -ne "" -and (Test-Path $ctlPng)) { $ctlCount = Count-Color $ctlPng 0 120 212 }
Check "geom-control-no-ring" ($ctlCount -eq 0) ("png=" + $ctlPng + " blue=" + $ctlCount)

if ($script:fail -gt 0) { Write-Output "FAILED: $($script:fail)"; exit 1 }
Write-Output "SEMC4-GEOM PASS"
exit 0