# probe_jkctl_init: jkctl init/install structural probe (docs/51 C candidates).
# ASCII-only PS5.1 (docs/15 convention). No server interaction needed beyond
# a ping guard - init/install are pure CLI (file copy + token substitution).
# Checks:
#   1. init creates the 3 template files with the name substituted
#   2. manifest.json parses and carries the scaffolded name
#   3. main.cmd is ASCII-only (OEM codepage lesson, docs/48)
#   4. init rejects an invalid name (exit 2)
#   5. install copies the folder into <exeDir>\apps\<name>
#   6. install refuses a duplicate install (exit 2)
#   7. teardown removes both trees

$ErrorActionPreference = "Stop"
$script:fails = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else     { $script:fails++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

$jkctl   = "I:\progwork\JKENGINE\engine\build\jkctl.exe"
$build   = "I:\progwork\JKENGINE\engine\build"
$work    = Join-Path $env:TEMP ("jkinit_probe_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
$appName = "jkinitt" + (Get-Random -Maximum 100)
$appDir  = Join-Path $work $appName

New-Item -ItemType Directory -Path $work -Force | Out-Null

try {
    # 1-3: init + substitution + ASCII body (init writes a RELATIVE <name>\
    # folder - run it from the probe work dir).
    Push-Location $work
    $null = & $jkctl init $appName 2>&1
    Check "init exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    $mPath = Join-Path $appDir "manifest.json"
    $rPath = Join-Path $appDir "README.md"
    $cPath = Join-Path $appDir "main.cmd"
    Check "init created 3 files" ((Test-Path $mPath) -and (Test-Path $rPath) -and (Test-Path $cPath)) $appDir

    $manifest = $null
    try { $manifest = Get-Content $mPath -Raw -Encoding UTF8 | ConvertFrom-Json } catch {}
    Check "manifest parses, name substituted" ($null -ne $manifest -and $manifest.name -eq $appName) ("name=" + $manifest.name)
    Check "manifest cmd is main.cmd" ($null -ne $manifest -and $manifest.cmd -eq "main.cmd") ""

    $cmdBytes = [IO.File]::ReadAllBytes($cPath)
    $nonAscii = ($cmdBytes | Where-Object { $_ -gt 127 }).Count
    Check "main.cmd ASCII-only" ($nonAscii -eq 0) ("non-ascii bytes=" + $nonAscii)
    $cmdText = [IO.File]::ReadAllText($cPath)
    Check "main.cmd name substituted" ($cmdText -match [regex]::Escape($appName)) ""

    # 4: invalid name rejected (stderr expected — relax EAP so the native
    # error line does not become a terminating NativeCommandError)
    $ErrorActionPreference = "Continue"
    $null = & $jkctl init "bad name!" 2>&1
    Check "init rejects invalid name" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"

    # 5: install copies into <exeDir>\apps\<name>
    $null = & $jkctl install $appDir 2>&1
    Check "install exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    $installed = Join-Path $build ("apps\" + $appName)
    Check "install landed in build apps" ((Test-Path (Join-Path $installed "manifest.json"))) $installed

    # 6: duplicate install refused
    $null = & $jkctl install $appDir 2>&1
    Check "install refuses duplicate" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"

    # 7: teardown of the installed copy happens in finally; server-side
    # scan is a restart-time action, so no launcher interaction here.
}
finally {
    Pop-Location
    if (Test-Path (Join-Path $build ("apps\" + $appName))) {
        Remove-Item -Recurse -Force (Join-Path $build ("apps\" + $appName))
    }
    if (Test-Path $work) { Remove-Item -Recurse -Force $work }
}

if ($script:fails -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: {0} FAILURE(S)" -f $script:fails); exit 1 }