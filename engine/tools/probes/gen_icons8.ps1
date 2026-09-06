Add-Type -AssemblyName System.Drawing
$apps = @(
    @{ n = 'jango';    c = [System.Drawing.Color]::FromArgb(0,0,170)      ; l = 'J' },
    @{ n = 'occ';      c = [System.Drawing.Color]::FromArgb(150,90,40)    ; l = 'O' },
    @{ n = 'pcx';      c = [System.Drawing.Color]::FromArgb(0,110,140)    ; l = 'P' },
    @{ n = 'vector';   c = [System.Drawing.Color]::FromArgb(110,60,160)   ; l = 'V' },
    @{ n = 'iconedit'; c = [System.Drawing.Color]::FromArgb(200,120,30)   ; l = 'I' },
    @{ n = 'recog';    c = [System.Drawing.Color]::FromArgb(40,120,60)    ; l = 'R' },
    @{ n = 'vfont';    c = [System.Drawing.Color]::FromArgb(60,70,90)     ; l = 'F' },
    @{ n = 'vpres';    c = [System.Drawing.Color]::FromArgb(170,40,70)    ; l = 'S' }
)
foreach ($app in $apps) {
    foreach ($size in 16, 32) {
        $bmp = New-Object System.Drawing.Bitmap($size, $size)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = 'AntiAlias'
        $brush = New-Object System.Drawing.SolidBrush($app.c)
        $g.FillRectangle($brush, 0, 0, $size, $size)
        $font = New-Object System.Drawing.Font('Arial', ($size * 0.62), [System.Drawing.FontStyle]::Bold)
        $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
        $fmt = New-Object System.Drawing.StringFormat
        $fmt.Alignment = 'Center'; $fmt.LineAlignment = 'Center'
        $rect = New-Object System.Drawing.RectangleF(0, ($size * 0.02), $size, $size)
        $g.DrawString($app.l, $font, $white, $rect, $fmt)
        $g.Dispose()
        $tag = if ($size -eq 16) { '1x' } else { '2x' }
        $out = "I:\progwork\JKENGINE\prototype\sdl2_jkwindow\assets\icons\launcher_$($app.n)@$tag.png"
        $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "saved $out"
    }
}