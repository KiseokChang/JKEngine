# Launcher icon art for apps whose .jkx / console manifest carries no ICON
# entry (snap, settings, notes, files, workshop, sampletodo). Same letter-on-
# colored-square style as gen_icons8.ps1, written to engine/assets/icons so
# the CMake asset copy ships it next to jkdesktop.exe.
# See docs/67 워크숍 단 1 후속 — launcher icon dedup (2026-09-26).
Add-Type -AssemblyName System.Drawing
$apps = @(
    @{ n = 'settings';   c = [System.Drawing.Color]::FromArgb(90,100,115)  ; l = 'S' },
    @{ n = 'notes';      c = [System.Drawing.Color]::FromArgb(230,180,60)  ; l = 'N' },
    @{ n = 'files';      c = [System.Drawing.Color]::FromArgb(70,130,90)   ; l = 'F' },
    @{ n = 'workshop';   c = [System.Drawing.Color]::FromArgb(120,80,160)  ; l = 'W' },
    @{ n = 'snap';       c = [System.Drawing.Color]::FromArgb(40,90,160)   ; l = 'P' },
    @{ n = 'sampletodo'; c = [System.Drawing.Color]::FromArgb(170,60,60)   ; l = 'T' }
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