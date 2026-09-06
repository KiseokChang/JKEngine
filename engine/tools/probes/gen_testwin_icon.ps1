Add-Type -AssemblyName System.Drawing
foreach ($size in 16, 32) {
  $bmp = New-Object System.Drawing.Bitmap($size, $size)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = 'AntiAlias'
  $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(0,110,110))
  $g.FillRectangle($brush, 0, 0, $size, $size)
  $font = New-Object System.Drawing.Font('Arial', ($size * 0.62), [System.Drawing.FontStyle]::Bold)
  $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
  $fmt = New-Object System.Drawing.StringFormat
  $fmt.Alignment = 'Center'; $fmt.LineAlignment = 'Center'
  $rect = New-Object System.Drawing.RectangleF(0, ($size * 0.02), $size, $size)
  $g.DrawString('T', $font, $white, $rect, $fmt)
  $g.Dispose()
  $out = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow\assets\icons\launcher_testwin@$size" + "x$size.png"
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Host "saved $out"
}