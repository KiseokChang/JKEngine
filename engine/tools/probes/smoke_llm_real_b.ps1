# Real-engine LLM smoke B: ask-gated close_window triggered BY the LLM ->
# chat approval strip -> 허용 click -> claude reports. Then /new reset.
# Costs real ollama/claude quota (~1-2 turns). Restores permissions.json.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$chatExe = "I:\progwork\JKENGINE\engine\build\jkchat.exe"
$permsPath = "I:\progwork\JKENGINE\engine\build\permissions.json"
$stateDir = "I:\progwork\JKENGINE\engine\build\state"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# ask gate on close_window
Remove-Item (Join-Path $stateDir "chat.json") -ErrorAction SilentlyContinue
'{"close_window":"ask"}' | Set-Content -Path $permsPath -Encoding ASCII

Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4
$raw = '{"tool":"launch_app","args":{"app":"minesweeper"}}'
$esc = $raw -replace '"', '\"'
$launchOut = & $exe agentctl $esc 2>&1 | Out-String
Write-Host ("agentctl launch: " + $launchOut.Trim())
Start-Sleep -Seconds 3
Start-Process -FilePath $chatExe -WorkingDirectory (Split-Path $exe)
Start-Sleep -Seconds 3

Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class WB {
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendMessageStr(IntPtr h, uint m, IntPtr w, string l);
    [DllImport("user32.dll", EntryPoint = "SendMessageW")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendMessageBuf(IntPtr h, uint m, IntPtr w, System.Text.StringBuilder l);
    public static string EditWindowText(IntPtr h) {
        IntPtr raw = SendMessage(h, 0x000E, IntPtr.Zero, IntPtr.Zero);
        int len = (int)raw;
        int cap = len * 2 + 4096;
        System.Text.StringBuilder sb = new System.Text.StringBuilder(cap);
        SendMessageBuf(h, 0x000D, (IntPtr)cap, sb);
        return sb.ToString().TrimEnd('\0');
    }
}
"@

function Get-Controls {
    $p = Get-Process jkchat -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if (-not $p) { return $null }
    $script:rLog = [IntPtr]::Zero; $script:rInput = [IntPtr]::Zero; $script:rSend = [IntPtr]::Zero
    $script:rAllow = [IntPtr]::Zero; $script:rStrip = $false
    $cb = [WB+EnumProc]{ param($h, $l)
        $cn = New-Object System.Text.StringBuilder 64
        [WB]::GetClassNameW($h, $cn, 64) | Out-Null
        $cls = $cn.ToString()
        if ($cls -eq 'Edit') {
            $t = [WB]::EditWindowText($h)
            if ($t.Length -gt 0) { $script:rLog = $h } else { $script:rInput = $h }
        } elseif ($cls -eq 'Button') {
            $bn = New-Object System.Text.StringBuilder 64
            [WB]::GetWindowTextW($h, $bn, 64) | Out-Null
            $txt = $bn.ToString()
            if ($txt -eq '보내기') { $script:rSend = $h }
            elseif ($txt -eq '허용') { $script:rAllow = $h }
        } elseif ($cls -eq 'Static') {
            $bn = New-Object System.Text.StringBuilder 128
            [WB]::GetWindowTextW($h, $bn, 128) | Out-Null
            if ($bn.ToString() -match '닫을까요|Minesweeper') { $script:rStrip = $true }
        }
        return $true
    }
    [WB]::EnumChildWindows($p.MainWindowHandle, $cb, [IntPtr]::Zero) | Out-Null
    return @{ Main = $p.MainWindowHandle; Log = $script:rLog; Input = $script:rInput;
              Send = $script:rSend; Allow = $script:rAllow; Strip = $script:rStrip }
}

function Send-Chat($ctl, $text) {
    [WB]::SendMessageStr($ctl.Input, 0x000C, [IntPtr]::Zero, $text) | Out-Null
    [WB]::SendMessage($ctl.Send, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
}

function Wait-Turn($ctl, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $prev = ""
    $stable = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        $t = [WB]::EditWindowText($ctl.Log)
        if ($t -eq $prev) { $stable++ } else { $stable = 0 }
        $prev = $t
        if ($stable -ge 2) { break }
    }
    return $prev
}

$ctl = Get-Controls
if (-not $ctl -or $ctl.Log -eq [IntPtr]::Zero -or $ctl.Send -eq [IntPtr]::Zero) {
    Write-Host "controls: FAIL"; exit 1
}
Write-Host "controls: PASS"

# Item 4: NL close -> claude calls close_window -> ask -> strip -> 허용
$pre = [WB]::EditWindowText($ctl.Log)
Send-Chat $ctl "지뢰찾기 창을 닫아줘"

# Poll for the approval strip while the LLM turn is in flight
$stripSeen = $false
$deadline = (Get-Date).AddSeconds(300)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 4
    $c2 = Get-Controls
    if ($c2 -and $c2.Strip -and $c2.Allow -ne [IntPtr]::Zero) {
        $stripSeen = $true
        break
    }
}
if ($stripSeen) {
    Write-Host "approval-strip: PASS (ask surfaced on claude's tool call)"
    [WB]::SendMessage($ctl.Allow, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Write-Host "allow-click: sent"
} else {
    Write-Host "approval-strip: FAIL (no strip in 300s) — transcript tail:"
    Write-Host ([WB]::EditWindowText($ctl.Log).Substring(0))
}

$t4 = Wait-Turn $ctl 300
# Authoritative check: the minesweeper window must be gone from the server.
$raw2 = '{"tool":"list_windows","args":{}}'
$esc2 = $raw2 -replace '"', '\"'
$listOut = & $exe agentctl $esc2 2>&1 | Out-String
if ($stripSeen -and $listOut -notmatch 'Minesweeper' -and $t4.Length -gt $pre.Length) {
    Write-Host "llm-close: PASS (window gone after allow, LLM reported)"
} else {
    Write-Host "llm-close: FAIL"
    Write-Host ("list_windows: " + $listOut.Trim())
    Write-Host ("transcript tail: " + $t4.Substring([Math]::Max(0, $t4.Length - 300)))
}

# Item 5: /new — deterministic slash, transcript should show the reset notice
Send-Chat $ctl "/new"
Start-Sleep -Seconds 2
$t5 = [WB]::EditWindowText($ctl.Log)
$afterNew = $t5.Substring([Math]::Max(0, $t5.Length - 200))
if ($afterNew -match '새 LLM 세션') {
    Write-Host "new-reset: PASS"
} else {
    Write-Host "new-reset: FAIL — tail: $afterNew"
}

Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permsPath -ErrorAction SilentlyContinue
if ($stripSeen -and $listOut -notmatch 'Minesweeper' -and $t4.Length -gt $pre.Length -and $afterNew -match '새 LLM 세션') {
    Write-Host "PASS: smoke llm B"; exit 0
} else {
    Write-Host "FAIL: smoke llm B"; exit 1
}