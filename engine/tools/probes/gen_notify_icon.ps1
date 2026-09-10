# gen_notify_icon.ps1 — launcher_notify@{1x,2x}.png (docs/33 알림 센터).
# The 2026-09-07 icon system (tmp/make_icons.ps1 conventions): dark slate
# tile #2b303c -> #22262f gradient, r14 corners with transparent outside,
# amber bell glyph #d8a24a. 64-space coords via ScaleTransform.
Add-Type -AssemblyName System.Drawing
function MakeIcon([int]$scale, [string]$path) {
  $s = $scale
  $w = 64 * $s
  $bmp = New-Object System.Drawing.Bitmap($w, $w)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = 'AntiAlias'
  $g.Clear([System.Drawing.Color]::Transparent)
  $c1 = [System.Drawing.Color]::FromArgb(255, 0x2b, 0x30, 0x3c)
  $c2 = [System.Drawing.Color]::FromArgb(255, 0x22, 0x26, 0x2f)
  $tile = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Rectangle(0, 0, $w, $w)), $c1, $c2, 90)
  $r = 14 * $s
  $gp = New-Object System.Drawing.Drawing2D.GraphicsPath
  $gp.AddArc(0, 0, 2*$r, 2*$r, 180, 90)
  $gp.AddArc($w-2*$r, 0, 2*$r, 2*$r, 270, 90)
  $gp.AddArc($w-2*$r, $w-2*$r, 2*$r, 2*$r, 0, 90)
  $gp.AddArc(0, $w-2*$r, 2*$r, 2*$r, 90, 90)
  $gp.CloseFigure()
  $g.FillPath($tile, $gp)
  $amber = [System.Drawing.Color]::FromArgb(255, 0xd8, 0xa2, 0x4a)
  $pen = New-Object System.Drawing.Pen($amber, (4.0 * $s))
  $g.DrawArc($pen, 16*$s, 14*$s, 32*$s, 34*$s, 180, 180)   # dome
  $g.DrawLine($pen, 14*$s, 44*$s, 50*$s, 44*$s)            # skirt
  $g.DrawLine($pen, 32*$s, 11*$s, 32*$s, 15*$s)            # nub
  $dot = New-Object System.Drawing.SolidBrush($amber)
  $g.FillEllipse($dot, 29*$s, 49*$s, 6*$s, 6*$s)           # clapper
  $g.Dispose()
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
}
MakeIcon 1 "I:\progwork\JKENGINE\engine\assets\icons\launcher_notify@1x.png"
MakeIcon 2 "I:\progwork\JKENGINE\engine\assets\icons\launcher_notify@2x.png"
Write-Host "icons written"