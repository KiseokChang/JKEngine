# gen_lf_helix_icons.ps1 — Phase A 흡수 앱 런처 아이콘 (docs/44, gen_icons8.ps1 패턴).
# lf(파일 매니저, 청록 'F') / helix(에디터, 블루 'H') — 16/32px, @1x/@2x.
Add-Type -AssemblyName System.Drawing
$apps = @(
    @{ n = 'lf';    c = [System.Drawing.Color]::FromArgb(30,160,110)  ; l = 'F' },
    @{ n = 'helix'; c = [System.Drawing.Color]::FromArgb(70,110,200)   ; l = 'H' }
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
        $out = "I:\progwork\JKENGINE\engine\assets\icons\launcher_$($app.n)@$tag.png"
        $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "saved $out"
    }
}