# pack_workshop.ps1 — 수기 JKX1 pack (docs/60 §3 "수기 MANI+MODL").
# jkx-pack의 script 분기는 생성 매니페스트에서 scriptfile/watch를 떨궈
# 버리므로(run 무수정 전제) 워크숍 컨테이너는 원문 매니페스트를 그대로
# 싣는다: MANI + MODL(jkapp_script.dll) + SCRI(app.js, 워크숍 모드 무시 —
# main.cpp 추출 분기 유발용). 레이아웃: JKJkxFile::Write와 바이트 동일.
#   [20B 헤더: JKX1|tocOffset=20|count|version=1|codec=0]
#   [128B 엔트리 × N: type[4] name[60] offset@64 size@68] [페이로드 순서대로]
param([string]$BuildDir = "")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $root "build" }

$srcDir = Join-Path $root "scripts\apps\workshop"
$maniPath = Join-Path $srcDir "manifest.txt"
$appJs = Join-Path $srcDir "app.js"
$dllPath = Join-Path $BuildDir "jkapp_script.dll"
$outDir = Join-Path $BuildDir "apps"
$outPath = Join-Path $outDir "workshop.jkx"

foreach ($f in @($maniPath, $appJs, $dllPath)) {
    if (-not (Test-Path -LiteralPath $f)) {
        throw "pack_workshop: missing input: $f"
    }
}

$maniBytes = [IO.File]::ReadAllBytes($maniPath)
$appBytes = [IO.File]::ReadAllBytes($appJs)
$dllBytes = [IO.File]::ReadAllBytes($dllPath)

# (type, name, bytes) — 순서가 곧 페이로드 순서
$entries = @(
    , @("MANI", "manifest.txt", $maniBytes)
    , @("MODL", "jkapp_script.dll", $dllBytes)
    , @("SCRI", "app.js", $appBytes)
)

function Add-U32([byte[]]$buf, [int]$pos, [uint32]$v) {
    $buf[$pos]     = $v -band 0xFF
    $buf[$pos + 1] = ($v -shr 8) -band 0xFF
    $buf[$pos + 2] = ($v -shr 16) -band 0xFF
    $buf[$pos + 3] = ($v -shr 24) -band 0xFF
}

$count = [uint32]$entries.Count
$tocOffset = [uint32]20
$payloadOffset = [uint32]($tocOffset + 128 * $count)

$fs = [IO.File]::Create($outPath)
try {
    $bw = New-Object IO.BinaryWriter($fs)
    # 헤더
    $bw.Write([byte[]][char[]]"JKX1")
    $bw.Write($tocOffset)
    $bw.Write($count)
    $bw.Write([uint32]1)   # version
    $bw.Write([uint32]0)   # codec (raw/stored)
    # TOC
    foreach ($e in $entries) {
        $type = [string]$e[0]; $name = [string]$e[1]; $bytes = [byte[]]$e[2]
        $raw = New-Object byte[] 128
        for ($i = 0; $i -lt 4; $i++) { $raw[$i] = [byte][char]$type[$i] }
        $nb = [Text.Encoding]::ASCII.GetBytes($name)
        if ($nb.Length -gt 59) { throw "entry name too long: $name" }
        [Array]::Copy($nb, 0, $raw, 4, $nb.Length)
        Add-U32 $raw 64 ([uint32]$payloadOffset)
        Add-U32 $raw 68 ([uint32]$bytes.Length)
        $bw.Write($raw)
        $payloadOffset += [uint32]$bytes.Length
    }
    # 페이로드
    foreach ($e in $entries) { $bw.Write([byte[]]$e[2]) }
    $bw.Flush()
} finally {
    $fs.Close()
}

Write-Host ("packed {0} ({1} entries, {2} bytes)" -f $outPath, $count, (Get-Item -LiteralPath $outPath).Length)