# LIVE-STACK diagnosis: minesweeper difficulty-switch grid overflow
# (user report 2026-10-05: "B/I/E switch -> grid escapes the window; manual
# resize fixes it"). READ-ONLY + drive via agent tools ONLY - does NOT stop
# the live stack and does NOT touch permissions.json (live file has allow).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe = "$build\jkdesktop.exe"; $agnt = "$build\jkagentd.exe"
function Invoke-Mcp([string]$line) {
    return (($line | & $agnt) -join "`n")
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    $out = (& $exe agentctl $escaped) -join "`n"
    $idx = $out.IndexOf('{')
    if ($idx -lt 0) { return "" }
    return $out.Substring($idx)
}
function Get-MineWindow {
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"([^"\\]*)\\",\\"pid\\":(\d+),' +
                         '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value; title = $m.Groups[2].Value
        pid = [int]$m.Groups[3].Value; x = [int]$m.Groups[4].Value
        y = [int]$m.Groups[5].Value;   w = [int]$m.Groups[6].Value
        h = [int]$m.Groups[7].Value
    }
}
function Capture([int]$wid, [string]$tag) {
    $r = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $wid + ',"path":"' +
                          ($build + '\diag_mine_' + $tag + '.png').Replace('\','\\') + '"}}')
    Write-Output ("CAPTURE " + $tag + ": " + $r)
}
Write-Output "=== LIVE diag_mine_diff (no teardown) ==="
# (1) launch minesweeper on the LIVE server
 Invoke-Mcp '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}' | Out-Null
$win = $null
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 500
    $win = Get-MineWindow
    if ($null -ne $win) { break }
}
if ($null -eq $win) { Write-Output "FAIL no-window"; Write-Output $script:lastList; exit 1 }
Write-Output ("WINDOW-B " + ($win | Out-String).Trim())
Capture $win.id "b1"
# (2) click the I (Intermediate) button.
# Toolbar: client origin = (x+2, y+2+24); button "I" at toolbar-local (102,4) 28x26
# -> window-logical center = (x + 2 + 102 + 14, y + 2 + 24 + 4 + 13)
$cx = $win.x + 2 + 102 + 14
$cy = $win.y + 2 + 24 + 4 + 13
$r = Invoke-Mcp ('{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win.id + ',"op":"click","x":' + $cx + ',"y":' + $cy + '}}}')
Write-Output ("CLICK-I " + $r)
Start-Sleep -Milliseconds 800
$win2 = Get-MineWindow
Write-Output ("WINDOW-I " + ($win2 | Out-String).Trim())
Capture $win2.id "i1"
# (3) click E (Expert) button
$cx2 = $win2.x + 2 + 134 + 14
$cy2 = $win2.y + 2 + 24 + 4 + 13
$r2 = Invoke-Mcp ('{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"send_input","arguments":{"id":' + $win2.id + ',"op":"click","x":' + $cx2 + ',"y":' + $cy2 + '}}}')
Write-Output ("CLICK-E " + $r2)
Start-Sleep -Milliseconds 800
$win3 = Get-MineWindow
Write-Output ("WINDOW-E " + ($win3 | Out-String).Trim())
Capture $win3.id "e1"
Write-Output "=== done (captures in engine/build/diag_mine_*.png) ==="