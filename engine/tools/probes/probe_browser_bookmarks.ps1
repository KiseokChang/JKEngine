# Task 2 e2e: browser bookmark bar + state/bookmarks.json persistence.
# ASCII-only on purpose (BOM-less ps1 with non-ASCII is read as cp949, docs/15).
# DPI context BEFORE Forms (else powershell stays virtualized; measured x1.25).
# The browser is composited into the SERVER desktop surface (its own hwnd is
# hidden), so clicks/screenshots target the server hwnd (probe_click
# convention: scale = serverClientW / 1280 logical) offset by the browser
# layer origin from list_windows (t1_e2e.ps1 convention, title "Browser").
#
# Phases:
#   setup    - kill desktop, clean state dir, start server + browser
#   e2e      - full task-2 gate: add -> json -> click-nav -> toggle off/on ->
#              client-restart persistence -> right-click delete ->
#              corrupt fail-open -> one-time .bak -> g_viewPageY page click
#   teardown - stop desktop
$ErrorActionPreference = 'Continue'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Dpi2 {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
}
"@
[Dpi2]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt2 {
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X; public int Y; }
}
"@

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-browser-bookmarks"
$jsonPath = "$build\state\bookmarks.json"
$script:srvHwnd = [IntPtr]::Zero
$script:srvScale = 1.0
$script:srvOx = 0; $script:srvOy = 0
$script:layerX = 0; $script:layerY = 0; $script:layerW = 0; $script:layerH = 0

# App-local logical coords (960x640 space), measured on this build.
$posInput = @(420, 56)   # URL text field
$posGo    = @(803, 56)
$posHome  = @(847, 56)   # Home button
$posStar  = @(892, 56)   # bookmark toggle star
$posBm1   = @(61, 94)    # first bookmark button in the second row
$posAdd   = @(257, 281)  # page-local "Add" button on browser_home (pageY gate)

function Stop-Desktop {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1
}

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

function Find-ServerWindow {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt2+RECT
    [Wt2]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt2+POINT; $co.X = 0; $co.Y = 0
    [Wt2]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:srvScale = ($crect.R - $crect.L) / 1280.0
    $script:srvOx = $co.X; $script:srvOy = $co.Y
}

# The Browser layer rect (title "Browser") — NOT the first list_windows row,
# which may be another window (measured: a desktop shell row).
function Find-BrowserLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, '[\\"]+id[\\"]+:(\d+),[\\"]+title[\\"]+:[\\"]+Browser[\\"]+.*?[\\"]+x[\\"]+:(-?\d+),[\\"]+y[\\"]+:(-?\d+),[\\"]+w[\\"]+:(\d+),[\\"]+h[\\"]+:(\d+)')
    if (-not $m.Success) { Write-Host ("FAIL: browser layer NOT FOUND: " + $list); exit 1 }
    $script:layerX = [int]$m.Groups[2].Value
    $script:layerY = [int]$m.Groups[3].Value
    $script:layerW = [int]$m.Groups[4].Value
    $script:layerH = [int]$m.Groups[5].Value
}

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    $px = [int]([math]::Round($script:srvOx + ($script:layerX + $lx) * $script:srvScale))
    $py = [int]([math]::Round($script:srvOy + ($script:layerY + $ly) * $script:srvScale))
    [Wt2]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [Wt2]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 250
    [Wt2]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt2]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
    Write-Host ("click ({0},{1}) -> screen ({2},{3})" -f $lx, $ly, $px, $py)
}

function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function RightClick-App([double]$lx, [double]$ly) { Send-Click $lx $ly 8 16 } # RM_DOWN/RM_UP

function Type-Keys([string]$keys) {
    [Wt2]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait($keys)
}

function Save-ServerShot([string]$name) {
    Find-ServerWindow
    $crect = New-Object Wt2+RECT
    [Wt2]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt2+POINT; $co.X = 0; $co.Y = 0
    [Wt2]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $w = $crect.R - $crect.L; $hh = $crect.B - $crect.T
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    # crop to the browser layer for a readable shot
    $cx = [int]([math]::Round($script:layerX * $script:srvScale))
    $cy = [int]([math]::Round($script:layerY * $script:srvScale))
    $cw = [int]([math]::Round($script:layerW * $script:srvScale))
    $ch = [int]([math]::Round($script:layerH * $script:srvScale))
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot {0}" -f $name)
}

function Get-Json([string]$path) {
    if (Test-Path $path) { return [IO.File]::ReadAllText($path) }
    return "<missing>"
}

function Check([string]$label, [bool]$ok) {
    if ($ok) { Write-Host ("PASS: " + $label) }
    else     { Write-Host ("FAIL: " + $label) }
    $script:fails += (-not $ok)
}

# Client-only restart: the browser app lives in the jkdesktop --client process;
# the server stays up and relaunches it from the .jkx package.
function Restart-Browser {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 2
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"browser"}}' | Out-Null
    Start-Sleep -Seconds 6
    Find-BrowserLayer
}

# Differing-pixel count between two shots inside the app-local page region
# (g_viewPageY gate: a page button click must change the page rendering).
function Page-DiffCount([string]$a, [string]$b) {
    $imgA = [System.Drawing.Bitmap]::FromFile((Join-Path $shotDir $a))
    $imgB = [System.Drawing.Bitmap]::FromFile((Join-Path $shotDir $b))
    $x0 = 0  # full page region below the bar (list lines start near x=30 app-local)
    $y0 = [int](115 * $script:srvScale)
    $w  = [int](700 * $script:srvScale)
    $h  = [int](450 * $script:srvScale)
    $count = 0
    for ($y = $y0; $y -lt $y0 + $h; $y += 2) {
        for ($x = $x0; $x -lt $x0 + $w; $x += 2) {
            $ca = $imgA.GetPixel($x, $y); $cb = $imgB.GetPixel($x, $y)
            if ([math]::Abs($ca.R - $cb.R) -gt 40 -or
                [math]::Abs($ca.G - $cb.G) -gt 40 -or
                [math]::Abs($ca.B - $cb.B) -gt 40) { $count++ }
        }
    }
    $imgA.Dispose(); $imgB.Dispose()
    return $count
}

$script:fails = 0

if ($args.Count -eq 0 -or $args[0] -eq 'setup') {
    Stop-Desktop
    if (Test-Path "$build\state") { Remove-Item -Recurse -Force "$build\state" }
    Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
    Start-Sleep -Seconds 4
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"browser"}}' | Out-Null
    Start-Sleep -Seconds 6
    Find-ServerWindow
    Find-BrowserLayer
    Save-ServerShot "task2-empty.png"
}
elseif ($args[0] -eq 'e2e') {
    Find-ServerWindow
    Find-BrowserLayer

    # 1) navigate to a site through the URL bar
    Click-App $posInput[0] $posInput[1]
    Start-Sleep -Milliseconds 500
    Type-Keys "example.com{ENTER}"
    Start-Sleep -Seconds 4

    # 2) star -> bookmark saved (first save ever: no pre-existing file, no .bak)
    Click-App $posStar[0] $posStar[1]
    Start-Sleep -Seconds 1
    $json = Get-Json $jsonPath
    Check "bookmarks.json created with Example Domain" `
        ($json -match 'Example Domain' -and $json -match 'example\.com/')
    Check ".bak NOT created when no pre-existing file" (-not (Test-Path "$jsonPath.bak"))
    Save-ServerShot "task2-bar.png"

    # 3) bookmark button click navigates (still example.com; page reloaded)
    Click-App $posBm1[0] $posBm1[1]
    Start-Sleep -Seconds 3
    Save-ServerShot "task2-bmnav.png"

    # 4) toggle off / on (toggle-off is the FIRST save over a pre-existing
    #    file -> the one-time .bak must now hold the pre-save content)
    Click-App $posStar[0] $posStar[1]
    Start-Sleep -Seconds 1
    Check "toggle-off empties bookmarks" ((Get-Json $jsonPath) -match '"bookmarks":\[\]')
    $bakBefore = Get-Json "$jsonPath.bak"
    Check ".bak created at first save over pre-existing file (pre-save content)" `
        ($bakBefore -match 'Example Domain')
    Click-App $posStar[0] $posStar[1]
    Start-Sleep -Seconds 1
    Check "toggle-on re-adds bookmark" ((Get-Json $jsonPath) -match 'Example Domain')

    # 5) client restart -> bookmark persists (BEFORE the delete check)
    Restart-Browser
    Check "bookmark survives app restart" ((Get-Json $jsonPath) -match 'Example Domain')
    Save-ServerShot "task2-restart.png"

    # 6) right-click -> delete menu
    RightClick-App $posBm1[0] $posBm1[1]
    Start-Sleep -Seconds 1
    Save-ServerShot "task2-ctx.png"
    # popup opens at the cursor: padding ~8 + item half-height ~14, x + half item width
    Click-App ($posBm1[0] + 38) ($posBm1[1] + 22)
    Start-Sleep -Seconds 1
    Check "context-menu delete empties bookmarks" ((Get-Json $jsonPath) -match '"bookmarks":\[\]')
    Save-ServerShot "task2-deleted.png"

    # 7) corrupt file -> fail-open (empty bar, no crash, file untouched)
    Restart-Browser   # relaunch clean, then kill before writing garbage
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 2
    [IO.File]::WriteAllText($jsonPath, "CORRUPT{{{ not json")
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"browser"}}' | Out-Null
    Start-Sleep -Seconds 6
    Find-BrowserLayer
    Start-Sleep -Seconds 2
    $alive = (Get-Process jkdesktop -ErrorAction SilentlyContinue) -ne $null
    Check "corrupt bookmarks.json fail-open (client alive, file untouched)" `
        ($alive -and ((Get-Json $jsonPath) -match 'CORRUPT'))
    Save-ServerShot "task2-corrupt.png"

    # 8) one-time .bak: .bak already exists (created at step 4), so the corrupt
    #    pre-existing file must NOT replace it, and later saves never refresh it.
    Click-App $posHome[0] $posHome[1]      # need a non-empty currentUrl_
    Start-Sleep -Seconds 3
    Click-App $posStar[0] $posStar[1]      # save #1 over the corrupt file
    Start-Sleep -Seconds 1
    Check ".bak still holds the original (corrupt file not backed up)" `
        ((Get-Json "$jsonPath.bak") -eq $bakBefore)
    Check "bookmarks.json valid again after save" `
        ((Get-Json $jsonPath) -match '"bookmarks":\[')
    Click-App $posStar[0] $posStar[1]      # save #2
    Start-Sleep -Seconds 1
    Click-App $posStar[0] $posStar[1]      # save #3
    Start-Sleep -Seconds 1
    Check ".bak unchanged after later saves (one-time preservation)" `
        ((Get-Json "$jsonPath.bak") -eq $bakBefore)

    # 9) g_viewPageY regression: a page click still lands at the intended
    #    spot with the 2-row bar active (Add button prints a new list line).
    Save-ServerShot "task2-pagey-before.png"
    Click-App $posAdd[0] $posAdd[1]
    Start-Sleep -Seconds 2
    Save-ServerShot "task2-pagey-after.png"
    $diff = Page-DiffCount "task2-pagey-before.png" "task2-pagey-after.png"
    Check ("g_viewPageY: page click lands (pixel diff {0} > 80)" -f $diff) ($diff -gt 80)

    Write-Host ("E2E DONE: {0} failure(s)" -f $script:fails)
}
elseif ($args[0] -eq 'teardown') {
    Stop-Desktop
    Write-Host "stopped"
}