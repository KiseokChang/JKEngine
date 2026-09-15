# probe_theme_swap: P3 theme hot-swap e2e (docs/52). ASCII-only PS5.1
# (docs/15 convention). Verdicts use tool responses, files and stderr logs
# only - pixel confirmation is a user item (lesson 62-63 image-reading cost).
# Checks:
#   1. theme_set light -> {"ok":true,"preset":"light"}
#   2. theme.json (exe dir) content carries the preset
#   3. client (palette) stderr shows the poll reload line within ~500ms of
#      spawn (mtime poll path works end to end)
#   4. LIVE swap while the client runs -> second reload line (classic)
#   5. theme_set nope -> bad_preset
#   6. teardown kills server+client and restores the dark preset

$ErrorActionPreference = 'Continue'
$script:fails = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else     { $script:fails++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$themeJson = "$build\theme.json"
$cliLog = "$build\themeswap_client.log"

function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}

taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""

try {
    # 1: tool swap to light
    $r = Invoke-Agentctl '{"tool":"theme_set","args":{"preset":"light"}}'
    Check "theme_set light ok" ($r -match '"ok"\s*:\s*true' -and $r -match '"preset"\s*:\s*"light"') $r

    # 2: theme.json content (the boot-loader truth file)
    $fileOk = $false; $fileTxt = ""
    if (Test-Path $themeJson) {
        $fileTxt = Get-Content $themeJson -Raw
        $fileOk  = ($fileTxt -match '"preset"\s*:\s*"light"')
    }
    Check "theme.json carries light" $fileOk $fileTxt

    # 3: client poll load line (first poll tick seeds + loads). The loader
    # printf goes to STDOUT (theme_set tool path uses the same convention),
    # so capture both streams.
    $cliOut = "$build\themeswap_client_out.log"
    Start-Process -FilePath $exe -ArgumentList "--client","palette" `
        -WorkingDirectory $build -RedirectStandardError $cliLog `
        -RedirectStandardOutput $cliOut
    Start-Sleep -Seconds 5
    $cliLines = @()
    foreach ($f in @($cliLog, $cliOut)) {
        if (Test-Path $f) {
            $cliLines += Select-String -Path $f -Pattern "\[theme\] preset 'light' from"
        }
    }
    Check "client poll reloaded light" ($cliLines.Count -ge 1) ("lines={0}" -f $cliLines.Count)

    # 4: LIVE swap to classic while the client runs -> poll within ~1s
    $r2 = Invoke-Agentctl '{"tool":"theme_set","args":{"preset":"classic"}}'
    Check "theme_set classic ok" ($r2 -match '"ok"\s*:\s*true') $r2
    Start-Sleep -Seconds 3
    $cliLines2 = @()
    foreach ($f in @($cliLog, $cliOut)) {
        if (Test-Path $f) {
            $cliLines2 += Select-String -Path $f -Pattern "\[theme\] preset 'classic' from"
        }
    }
    Check "client live-swap classic" ($cliLines2.Count -ge 1) ("lines={0}" -f $cliLines2.Count)

    # 5: bad preset rejected
    $r3 = Invoke-Agentctl '{"tool":"theme_set","args":{"preset":"nope"}}'
    Check "bad preset rejected" ($r3 -match 'bad_preset') $r3
}
finally {
    # teardown: restore dark + kill everything spawned here
    $null = Invoke-Agentctl '{"tool":"theme_set","args":{"preset":"dark"}}' 2>&1
    taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
    foreach ($f in @($cliLog, "$build\themeswap_client_out.log")) {
        if (Test-Path $f) { Remove-Item -Force $f }
    }
}

if ($script:fails -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: {0} FAILURE(S)" -f $script:fails); exit 1 }