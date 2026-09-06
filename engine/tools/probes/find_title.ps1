Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap('I:\progwork\JKENGINE\tmp\smoke_now.png')
# Dark-blue title bar (0,0,128). Report per-row-band min/max x.
$minX = @{}; $maxX = @{}; $minY = @{}; $maxY = @{}
for ($y = 0; $y -lt $bmp.Height; ++$y) {
    for ($x = 0; $x -lt $bmp.Width; ++$x) {
        $p = $bmp.GetPixel($x, $y)
        if ($p.R -eq 0 -and $p.G -eq 0 -and $p.B -eq 128) {
            $band = [int]([math]::Floor($y / 30))
            if (-not $minX.ContainsKey($band)) { $minX[$band] = $x; $maxX[$band] = $x; $minY[$band] = $y; $maxY[$band] = $y }
            else {
                if ($x -lt $minX[$band]) { $minX[$band] = $x }
                if ($x -gt $maxX[$band]) { $maxX[$band] = $x }
                if ($y -gt $maxY[$band]) { $maxY[$band] = $y }
            }
        }
    }
}
$bands = $minX.Keys | Sort-Object
foreach ($b in $bands) {
    Write-Host ("band y {0}..{1}: x {2}..{3}" -f $minY[$b], $maxY[$b], $minX[$b], $maxX[$b])
}
$bmp.Dispose()