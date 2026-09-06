param([string]$Root = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow")
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$icons = Join-Path $Root 'assets\icons'
$bgs = Join-Path $Root 'assets\backgrounds'
New-Item -ItemType Directory -Force -Path $icons, $bgs | Out-Null

function Save-Png([System.Drawing.Bitmap]$bmp, [string]$path) {
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("saved {0} ({1} bytes)" -f $path, (Get-Item $path).Length)
}

# --- Background photo: @2x 2560x1440, @1x 1280x720 ---
$jpg = Join-Path $env:TEMP 'jk_bg.jpg'
$photo = [System.Drawing.Image]::FromFile($jpg)
Write-Host ("photo: {0}x{1}" -f $photo.Width, $photo.Height)

foreach ($spec in @(@(2560, 'desktop@2x.png'), @(1280, 'desktop@1x.png'))) {
    $bmp = New-Object System.Drawing.Bitmap $spec[0], ([int]($spec[0] * 720 / 1280))
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($photo, 0, 0, $bmp.Width, $bmp.Height)
    $g.Dispose()
    Save-Png $bmp (Join-Path $bgs $spec[1])
}
$photo.Dispose()

# --- Launcher icons (transparent background) ---
function New-MineIcon([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $s = $size / 64.0
    $black = [System.Drawing.Color]::FromArgb(255, 0, 0, 0)
    $pen = New-Object System.Drawing.Pen ($black), ([float](3.0 * $s))
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $brush = New-Object System.Drawing.SolidBrush ($black)
    $cx = 32.0 * $s; $cy = 34.0 * $s; $r = 15.0 * $s
    $dirs = @(@(0,-1),@(0,1),@(-1,0),@(1,0),@(-0.7071,-0.7071),@(0.7071,-0.7071),@(-0.7071,0.7071),@(0.7071,0.7071))
    foreach ($d in $dirs) {
        $x1 = $cx + $d[0] * ($r + 1.0 * $s)
        $y1 = $cy + $d[1] * ($r + 1.0 * $s)
        $x2 = $cx + $d[0] * ($r + 8.0 * $s)
        $y2 = $cy + $d[1] * ($r + 8.0 * $s)
        $g.DrawLine($pen, [float]$x1, [float]$y1, [float]$x2, [float]$y2)
    }
    $g.FillEllipse($brush, [float]($cx - $r), [float]($cy - $r), [float](2 * $r), [float](2 * $r))
    $hlBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 200, 200, 200))
    $g.FillEllipse($hlBrush, [float](24 * $s), [float](22 * $s), [float](7 * $s), [float](7 * $s))
    # Fuse: short stub up-right.
    $g.DrawLine($pen, [float](38 * $s), [float](13 * $s), [float](45 * $s), [float](6 * $s))
    $g.Dispose(); $pen.Dispose(); $brush.Dispose(); $hlBrush.Dispose()
    return $bmp
}

function New-TetrisIcon([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
    $s = $size / 64.0
    $purple = [System.Drawing.Color]::FromArgb(255, 128, 0, 128)
    $light  = [System.Drawing.Color]::FromArgb(255, 200, 110, 200)
    $dark   = [System.Drawing.Color]::FromArgb(255, 55, 0, 55)
    $bLight = New-Object System.Drawing.SolidBrush ($light)
    $bDark  = New-Object System.Drawing.SolidBrush ($dark)
    $bMain  = New-Object System.Drawing.SolidBrush ($purple)
    $cell = 14.0 * $s
    $ox = 9.0 * $s; $oy = 12.0 * $s
    $cells = @(@(1,0),@(0,1),@(1,1),@(2,1))  # T piece
    foreach ($c in $cells) {
        $x = $ox + $c[0] * $cell
        $y = $oy + $c[1] * $cell
        $g.FillRectangle($bMain, [float]$x, [float]$y, [float]$cell, [float]$cell)
        $hl = 2.0 * $s
        $g.FillRectangle($bLight, [float]$x, [float]$y, [float]$cell, [float]$hl)
        $g.FillRectangle($bLight, [float]$x, [float]$y, [float]$hl, [float]$cell)
        $g.FillRectangle($bDark, [float]$x, [float]($y + $cell - $hl), [float]$cell, [float]$hl)
        $g.FillRectangle($bDark, [float]($x + $cell - $hl), [float]$y, [float]$hl, [float]$cell)
    }
    $g.Dispose(); $bLight.Dispose(); $bDark.Dispose(); $bMain.Dispose()
    return $bmp
}

foreach ($pair in @(@(64, '1x'), @(128, '2x'))) {
    $size = $pair[0]; $tag = $pair[1]
    Save-Png (New-MineIcon $size)   (Join-Path $icons ("launcher_mine@{0}.png" -f $tag))
    Save-Png (New-TetrisIcon $size) (Join-Path $icons ("launcher_tetris@{0}.png" -f $tag))
}

# --- 16x16 in-game UI icons (must stay pixel-identical to the procedural
# fallbacks in ClientMineSweeperApp.cpp / MineSweeperApp.cpp) ---
function New-GameMine16 {
    $bmp = New-Object System.Drawing.Bitmap 16, 16
    $set = { param($x, $y, $r, $g, $b)
        if ($x -ge 0 -and $x -lt 16 -and $y -ge 0 -and $y -lt 16) {
            $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $r, $g, $b)) } }
    for ($y = -5; $y -le 5; ++$y) { for ($x = -5; $x -le 5; ++$x) {
        if ($x*$x + $y*$y -le 25 + 2) { & $set (8+$x) (8+$y) 0 0 0 } } }
    for ($y = -1; $y -le 1; ++$y) { for ($x = -1; $x -le 1; ++$x) {
        if ($x*$x + $y*$y -le 1 + 0) { & $set (6+$x) (6+$y) 192 192 192 } } }
    foreach ($l in @(@(8,1,8,4),@(8,11,8,14),@(1,8,4,8),@(11,8,14,8),
                     @(3,3,5,5),@(11,3,13,5),@(3,13,5,11),@(11,13,13,11))) {
        # Bresenham matching the C++ drawLine helper.
        $x1 = $l[0]; $y1 = $l[1]; $x2 = $l[2]; $y2 = $l[3]
        $dx = [math]::Abs($x2-$x1); $sx = if ($x1 -lt $x2) { 1 } else { -1 }
        $dy = -[math]::Abs($y2-$y1); $sy = if ($y1 -lt $y2) { 1 } else { -1 }
        $err = $dx + $dy
        while ($true) {
            & $set $x1 $y1 0 0 0
            if ($x1 -eq $x2 -and $y1 -eq $y2) { break }
            $e2 = 2 * $err
            if ($e2 -ge $dy) { $err += $dy; $x1 += $sx }
            if ($e2 -le $dx) { $err += $dx; $y1 += $sy }
        }
    }
    return $bmp
}

function New-GameFlag16 {
    $bmp = New-Object System.Drawing.Bitmap 16, 16
    for ($y = 2; $y -lt 14; ++$y) { $bmp.SetPixel(5, $y, [System.Drawing.Color]::FromArgb(255,0,0,0)) }
    for ($x = 3; $x -lt 8; ++$x)  { $bmp.SetPixel($x, 13, [System.Drawing.Color]::FromArgb(255,0,0,0)) }
    for ($y = 2; $y -lt 7; ++$y) {
        $w = 6 - ($y - 2)
        for ($x = 6; $x -lt 6 + $w; ++$x) { $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255,255,0,0)) }
    }
    return $bmp
}

function New-GameQuestion16 {
    $bmp = New-Object System.Drawing.Bitmap 16, 16
    foreach ($r in @(@(5,3,6,2),@(9,3,2,6),@(5,7,6,2),@(5,7,2,4),@(6,12,3,2))) {
        for ($yy = $r[1]; $yy -lt $r[1]+$r[3]; ++$yy) {
            for ($xx = $r[0]; $xx -lt $r[0]+$r[2]; ++$xx) {
                $bmp.SetPixel($xx, $yy, [System.Drawing.Color]::FromArgb(255,0,0,0))
            }
        }
    }
    return $bmp
}

Save-Png (New-GameMine16)     (Join-Path $icons 'mine@1x.png')
Save-Png (New-GameFlag16)     (Join-Path $icons 'flag@1x.png')
Save-Png (New-GameQuestion16) (Join-Path $icons 'question@1x.png')
Write-Host "done"