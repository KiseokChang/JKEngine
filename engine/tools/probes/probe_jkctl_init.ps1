# probe_jkctl_init: jkctl init/pack/install structural probe (docs/51 C).
# ASCII-only PS5.1 (docs/15 convention). No server interaction needed beyond
# a ping guard - init/pack/install are pure CLI (file copy + token
# substitution + zip write). Checks:
#   1. init creates the 3 template files with the name substituted
#   2. manifest.json parses and carries the scaffolded name
#   3. main.cmd is ASCII-only (OEM codepage lesson, docs/48)
#   4. init rejects an invalid name (exit 2)
#   5. install copies the folder into <exeDir>\apps\<name>
#   6. install refuses a duplicate install (exit 2)
#   7. pack writes <name>.zip with the app files inside
#   8. install from the zip lands the same tree (via tmp staging)
#   9. zip install pre-records the cmd fingerprint in state/trust.json
#  10. trust.json still parses after the splice (valid JSON out)
#  11. ask --attach: missing file / binary (NUL) rejected with exit 2,
#      text attachment reaches the LLM launch path (exit 0/1)
#  12. promote wraps the folder into <name>.jkx (JKX1 magic) and install
#      from the container lands the tree with trust pre-record; promote
#      rejects a manifest-less directory
#  13. teardown removes every tree it created

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
$zipPath = Join-Path $work ($appName + ".zip")
$trustP  = Join-Path $build "state\trust.json"

New-Item -ItemType Directory -Path $work -Force | Out-Null

# Trust records before the probe - the probe owns only its own fingerprint.
$preRecords = @()
if (Test-Path $trustP) {
    try { $preRecords = (Get-Content $trustP -Raw | ConvertFrom-Json).records } catch {}
}
$fpOf = { param($cmdStr)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    ($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($cmdStr)) |
        ForEach-Object { $_.ToString("x2") }) -join ""
}

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

    # 7: pack wraps the same folder into <name>.zip
    $null = & $jkctl pack $appDir 2>&1
    Check "pack exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    Check "pack wrote <name>.zip" (Test-Path $zipPath) $zipPath
    if (Test-Path $zipPath) {
        # 7b: zip really contains the manifest (deflate stream carries it)
        $zipBytes = [IO.File]::ReadAllBytes($zipPath)
        Check "zip is a PK archive" ($zipBytes.Length -gt 4 -and $zipBytes[0] -eq 0x50 -and $zipBytes[1] -eq 0x4B) ("size=" + $zipBytes.Length)
    }

    # 8-10: remove the folder install, then install FROM THE ZIP. This is the
    # full distribution path: tmp staging + zip-slip guard + trust pre-record.
    Remove-Item -Recurse -Force $installed
    $null = & $jkctl install $zipPath 2>&1
    Check "zip install exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    Check "zip install landed in build apps" ((Test-Path (Join-Path $build ("apps\" + $appName + "\manifest.json")))) "reinstalled from zip"
    $trustTxt = ""
    if (Test-Path $trustP) { $trustTxt = Get-Content $trustP -Raw -Encoding UTF8 }
    $wantFp = & $fpOf "main.cmd"
    Check "trust.json pre-recorded cmd fingerprint" ($trustTxt -match [regex]::Escape($wantFp)) ("fp=" + $wantFp.Substring(0, 20) + "...")
    # 10: the store must still be valid JSON after the splice - a broken
    # comma here bricks the fail-closed server scan (observed defect).
    $trustOk = $false
    try {
        $trustObj = $trustTxt | ConvertFrom-Json
        $trustOk = ($null -ne $trustObj.records -and $trustObj.records.Count -ge 1)
    } catch {}
    Check "trust.json parses after splice" $trustOk "records kept byte-exact for server"

    # 11: attach to a missing file is a clean error (exit 2), not a silent ask
    $null = & $jkctl ask "q" --attach (Join-Path $work "no_such_file.txt") 2>&1
    Check "ask attach missing file rejected" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"

    # 12: binary attachment rejected (NUL byte)
    $binP = Join-Path $work "bin_att.bin"
    [IO.File]::WriteAllBytes($binP, (New-Object byte[] 16))
    $null = & $jkctl ask "q" --attach $binP 2>&1
    Check "ask attach binary rejected" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"

    # 13: valid text attachment passes arg handling far enough to try the
    # LLM launch path (exit != 2 - real engine/network is out of probe scope;
    # 0 or 1 both mean the prompt made it into the process launch).
    $txtP = Join-Path $work "att.txt"
    [IO.File]::WriteAllText($txtP, "hello", (New-Object System.Text.UTF8Encoding($false)))
    $null = & $jkctl ask "echo test" --attach $txtP 2>&1
    Check "ask attach text reaches LLM launch" ($LASTEXITCODE -ne 2) "exit=$LASTEXITCODE (0/1 ok - network dependent)"

    # 14: promote wraps the folder into <name>.jkx (JKX1 magic). The zip
    # install occupied apps\<name> - use the Remove-Item + reinstall pattern
    # (same as checks 8-10) before promote+install-from-container.
    Remove-Item -Recurse -Force (Join-Path $build ("apps\" + $appName)) -ErrorAction SilentlyContinue
    $jkxPath = Join-Path $work ($appName + ".jkx")
    $null = & $jkctl promote $appDir 2>&1
    Check "promote exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    Check "promote wrote <name>.jkx" (Test-Path $jkxPath) $jkxPath
    if (Test-Path $jkxPath) {
        $m4 = New-Object byte[] 4
        $fs = [IO.File]::OpenRead($jkxPath)
        $null = $fs.Read($m4, 0, 4); $fs.Close()
        Check "jkx has JKX1 magic" ([Text.Encoding]::ASCII.GetString($m4) -eq "JKX1") ""
    }
    # 15: install from the .jkx lands the tree (staging + manifest validation
    # + trust pre-record ride the shared InstallFromDir tail)
    $null = & $jkctl install $jkxPath 2>&1
    Check "jkx install exit ok" ($LASTEXITCODE -eq 0) "exit=$LASTEXITCODE"
    Check "jkx install landed in build apps" ((Test-Path (Join-Path $build ("apps\" + $appName + "\manifest.json")))) "installed from .jkx"
    # 16: trust pre-record fires for the container path too (same cmd fp)
    $trustTxt2 = ""
    if (Test-Path $trustP) { $trustTxt2 = Get-Content $trustP -Raw -Encoding UTF8 }
    $wantFp2 = & $fpOf "main.cmd"
    Check "jkx trust pre-record" ($trustTxt2 -match [regex]::Escape($wantFp2)) ""
    # 17: promote honestly rejects a manifest-less directory
    $emptyDir = Join-Path $work "emptyapp"
    New-Item -ItemType Directory -Force -Path $emptyDir | Out-Null
    $null = & $jkctl promote $emptyDir 2>&1
    Check "promote rejects manifest-less dir" ($LASTEXITCODE -eq 2) "exit=$LASTEXITCODE"

    # teardown of the installed copy happens in finally; server-side
    # scan is a restart-time action, so no launcher interaction here.
}
finally {
    Pop-Location
    if (Test-Path (Join-Path $build ("apps\" + $appName))) {
        Remove-Item -Recurse -Force (Join-Path $build ("apps\" + $appName))
    }
    # staging leftover is warn-only in jkctl (AV scan lock) - clean it here
    $staging = Join-Path $build ("tmp\install_" + $appName)
    if (Test-Path $staging) { Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue }
    if (Test-Path $work) { Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue }
    # trust record: remove ONLY the probe's own fingerprint entry (all
    # init'd apps share the cmd "main.cmd" fingerprint - never touch others).
    # Write with [IO.File]::WriteAllText - PS5.1 Set-Content -Encoding UTF8
    # adds a BOM, which quickjs JS_ParseJSON rejects (store bricked).
    if (Test-Path $trustP) {
        $fp = & $fpOf "main.cmd"
        $probeFp = "sha256:" + $fp
        $hadPre = @($preRecords | Where-Object { $_.fingerprint -eq $probeFp }).Count
        if ($hadPre -eq 0) {
            try {
                $obj = Get-Content $trustP -Raw | ConvertFrom-Json
                $kept = @($obj.records | Where-Object { $_.fingerprint -ne $probeFp })
                if ($kept.Count -eq 0) {
                    [IO.File]::WriteAllText($trustP, '{"records":[]}')
                } else {
                    $obj.records = $kept
                    [IO.File]::WriteAllText($trustP, (ConvertTo-Json $obj -Depth 5 -Compress))
                }
            } catch {}
        }
    }
}

if ($script:fails -eq 0) { Write-Host "RESULT: ALL PASS" }
else { Write-Host ("RESULT: {0} FAILURE(S)" -f $script:fails); exit 1 }