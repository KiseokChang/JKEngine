param([int]$N = 18, [int]$Seconds = 60)
$jobs = @()
for ($i = 0; $i -lt $N; $i++) {
    $jobs += Start-Job -ScriptBlock {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $x = 1.0001
        while ($sw.Elapsed.TotalSeconds -lt $using:Seconds) { $x = $x * 1.0000001; if ($x -gt 10) { $x = 1.0 } }
    }
}
$jobs | Wait-Job -Timeout ($Seconds + 10) | Out-Null
$jobs | Remove-Job -Force
