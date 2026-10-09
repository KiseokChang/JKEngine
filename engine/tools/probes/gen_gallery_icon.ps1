# gen_gallery_icon.ps1 — launcher_gallery@{1x,2x}.png (spec 2026-10-09-gallery-
# design T1). The 2026-09-07 icon system conventions (gen_notify_icon.ps1 /
# gen_shot_icon.ps1): dark slate tile #2b303c -> #22262f gradient, r14 corners
# with transparent outside. Glyph: a 3x3 photo-grid motif — eight dim slate
# tiles + one amber tile (the "selected photo") — program-generated art, not a
# copy of any existing launcher icon.
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

  # 3x3 grid of rounded squares, 12..52 in 64-space: cell pitch 13.5, cell 12.
  $amber = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(255, 0xd8, 0xa2, 0x4a))
  $dim = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(255, 0x53, 0x5b, 0x6b))
  $cell = 12 * $s
  $pitch = 13.5 * $s
  for ($row = 0; $row -lt 3; $row++) {
    for ($col = 0; $col -lt 3; $col++) {
      $x = 12 * $s + [math]::Round($col * $pitch)
      $y = 12 * $s + [math]::Round($row * $pitch)
      # The amber "current photo" sits at row 1, col 2 (upper-right) — the
      # rest of the grid reads as folded thumbnails.
      $brush = if ($row -eq 1 -and $col -eq 2) { $amber } else { $dim }
      $cr = 3 * $s
      $sq = New-Object System.Drawing.Drawing2D.GraphicsPath
      $sq.AddArc($x, $y, 2*$cr, 2*$cr, 180, 90)
      $sq.AddArc($x+$cell-2*$cr, $y, 2*$cr, 2*$cr, 270, 90)
      $sq.AddArc($x+$cell-2*$cr, $y+$cell-2*$cr, 2*$cr, 2*$cr, 0, 90)
      $sq.AddArc($x, $y+$cell-2*$cr, 2*$cr, 2*$cr, 90, 90)
      $sq.CloseFigure()
      $g.FillPath($brush, $sq)
      $sq.Dispose()
    }
  }
  $g.Dispose()
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
}
MakeIcon 1 "I:\progwork\JKENGINE\engine\assets\icons\launcher_gallery@1x.png"
MakeIcon 2 "I:\progwork\JKENGINE\engine\assets\icons\launcher_gallery@2x.png"
Write-Host "gallery icons written"