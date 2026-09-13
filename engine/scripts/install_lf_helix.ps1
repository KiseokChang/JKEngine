# engine/scripts/install_lf_helix.ps1 — Phase A TUI 흡수 (docs/44).
# lf (gokcehan/lf) 최신 Windows 릴리스와 helix 최신 Windows 릴리스를
# engine/build/apps-bin 아래에 풀어둔다. 바이너리는 git에 넣지 않는다
# (build/ 는 이미 ignore). 재실행하면 최신으로 갱신된다.
$ErrorActionPreference = "Stop"

$dest = Join-Path $PSScriptRoot "..\build\apps-bin"
New-Item -ItemType Directory -Force -Path "$dest\lf", "$dest\helix" | Out-Null

function Get-LatestAsset($repo, $pattern) {
    $rel = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest"
    $asset = $rel.assets | Where-Object { $_.name -match $pattern } | Select-Object -First 1
    if (-not $asset) { throw "no asset matching '$pattern' in $repo latest ($($rel.tag_name))" }
    Write-Host "$repo $($rel.tag_name) -> $($asset.name)"
    return $asset.browser_download_url
}

# lf: lf-windows-amd64.zip -> lf.exe
$lfZip = Join-Path $env:TEMP "lf-install.zip"
Invoke-WebRequest (Get-LatestAsset "gokcehan/lf" "lf-windows-amd64\.zip$") -OutFile $lfZip
Expand-Archive $lfZip "$dest\lf" -Force
Remove-Item $lfZip

# helix: *x86_64-windows.zip -> hx.exe + runtime/ (runtime은 exe 옆에서 자동 탐색됨)
$hxZip = Join-Path $env:TEMP "helix-install.zip"
Invoke-WebRequest (Get-LatestAsset "helix-editor/helix" "x86_64-windows\.zip$") -OutFile $hxZip
Expand-Archive $hxZip "$dest\helix" -Force
Remove-Item $hxZip

# helix zip은 최상위 폴더(helix-<ver>-x86_64-windows/) 하나에 담겨 온다 —
# hx.exe가 runtime/ 옆에 위치해야 자동 탐색되므로 플래튼한다.
$hxRoot = Get-ChildItem "$dest\helix" -Directory | Select-Object -First 1
if ($hxRoot -and -not (Test-Path "$dest\helix\hx.exe")) {
    Move-Item "$($hxRoot.FullName)\*" "$dest\helix" -Force
    Remove-Item $hxRoot.FullName
}

Write-Host "installed:"
Get-ChildItem "$dest\lf", "$dest\helix" | ForEach-Object { Write-Host "  $($_.FullName)" }