# fix_bom.ps1 — UTF-8 파일의 BOM 확인/부착 유틸
# 사용: powershell -NoProfile -ExecutionPolicy Bypass -File tools\fix_bom.ps1 -Path <대상 파일>
# 배경: editor 도구로 .ps1을 저장하면 BOM이 빠질 수 있다. 한글이 포함된 .ps1에서 BOM이
#       없으면 PowerShell 5.1이 cp949로 읽어 파싱 에러/문자 깨짐이 발생한다.
#       (.clinerules\30_tool-workarounds.md 참조)
param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

if (-not (Test-Path $Path)) { throw "file not found: $Path" }

$bytes = [System.IO.File]::ReadAllBytes($Path)
if ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191) {
    Write-Host "BOM already present: $Path"
} else {
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    [System.IO.File]::WriteAllText($Path, $text, (New-Object System.Text.UTF8Encoding $true))
    Write-Host "BOM added (was missing): $Path"
}