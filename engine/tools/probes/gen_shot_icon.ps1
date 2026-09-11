# gen_shot_icon.ps1 — launcher_shot@{1x,2x}.png (docs/35 스크린샷).
# The 2026-09-07 icon system (gen_notify_icon.ps1 conventions): dark slate
# tile #2b303c -> #22262f gradient, r14 corners with transparent outside,
# amber camera glyph #d8a24a. 64-space coords via ScaleTransform-style $s.
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
  $fill = New-Object System.Drawing.SolidBrush($amber)
  # Camera: flash nub + rounded body + lens bore + lens ring.
  $g.FillRectangle($fill, 24*$s, 14*$s, 16*$s, 6*$s)       # flash nub
  $br = 5 * $s
  $body = New-Object System.Drawing.Drawing2D.GraphicsPath
  $body.AddArc(12*$s, 20*$s, 2*$br, 2*$br, 180, 90)
  $body.AddArc(52*$s-2*$br, 20*$s, 2*$br, 2*$br, 270, 90)
  $body.AddArc(52*$s-2*$br, 46*$s-2*$br, 2*$br, 2*$br, 0, 90)
  $body.AddArc(12*$s, 46*$s-2*$br, 2*$br, 2*$br, 90, 90)
  $body.CloseFigure()
  $g.FillPath($fill, $body)
  $lensC = [System.Drawing.Color]::FromArgb(255, 0x22, 0x26, 0x2f)
  $lens = New-Object System.Drawing.SolidBrush($lensC)
  $g.FillEllipse($lens, 22*$s, 27*$s, 20*$s, 20*$s)        # lens bore
  $ring = New-Object System.Drawing.Pen($amber, (2.0 * $s))
  $g.DrawEllipse($ring, 25*$s, 30*$s, 14*$s, 14*$s)        # lens ring
  $g.Dispose()
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
}
MakeIcon 1 "I:\progwork\JKENGINE\engine\assets\icons\launcher_shot@1x.png"
MakeIcon 2 "I:\progwork\JKENGINE\engine\assets\icons\launcher_shot@2x.png"
Write-Host "icons written"