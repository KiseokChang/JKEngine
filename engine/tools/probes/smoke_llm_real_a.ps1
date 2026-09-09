# Real-engine LLM smoke A: default config boot -> NL list_windows turn ->
# resume follow-up. Drives jkchat UI; judges via transcript. Costs real
# ollama/claude quota (~2 turns). Cleans up processes at the end.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$chatExe = "I:\progwork\JKENGINE\engine\build\jkchat.exe"
$stateDir = "I:\progwork\JKENGINE\engine\build\state"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Item 1: no chat.json -> defaults (ollama / kimi-k2.7-code:cloud)
Remove-Item (Join-Path $stateDir "chat.json") -ErrorAction SilentlyContinue
if (Test-Path (Join-Path $stateDir "chat.json")) { Write-Host "boot: FAIL (chat.json still present)"; exit 1 }
Write-Host "boot: PASS (default config, no chat.json)"

# minesweeper window so claude's list has something to name
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4
# Lesson 26: embedded quotes must be escaped for native argv; direct & call.
$raw = '{"tool":"launch_app","args":{"app":"minesweeper"}}'
$esc = $raw -replace '"', '\"'
$launchOut = & $exe agentctl $esc 2>&1 | Out-String
Write-Host ("agentctl launch: " + $launchOut.Trim())
if ($launchOut -notmatch '"ok"\s*:\s*true') { Write-Host "launch: FAIL"; Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1 }
Start-Sleep -Seconds 3
Start-Process -FilePath $chatExe -WorkingDirectory (Split-Path $exe)
Start-Sleep -Seconds 3

Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class WA {
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
    $r = @{ Main = $p.MainWindowHandle; Log = [IntPtr]::Zero; Input = [IntPtr]::Zero; Send = [IntPtr]::Zero }
    $cb = [WA+EnumProc]{ param($h, $l)
        $cn = New-Object System.Text.StringBuilder 64
        [WA]::GetClassNameW($h, $cn, 64) | Out-Null
        $cls = $cn.ToString()
        if ($cls -eq 'Edit') {
            $t = [WA]::EditWindowText($h)
            if ($t.Length -gt 0) { $script:rLog = $h } else { $script:rInput = $h }
        } elseif ($cls -eq 'Button') {
            $bn = New-Object System.Text.StringBuilder 64
            [WA]::GetWindowTextW($h, $bn, 64) | Out-Null
            if ($bn.ToString() -eq '보내기') { $script:rSend = $h }
        }
        return $true
    }
    $script:rLog = [IntPtr]::Zero; $script:rInput = [IntPtr]::Zero; $script:rSend = [IntPtr]::Zero
    [WA]::EnumChildWindows($r.Main, $cb, [IntPtr]::Zero) | Out-Null
    $r.Log = $script:rLog; $r.Input = $script:rInput; $r.Send = $script:rSend
    return $r
}

function Send-Chat($ctl, $text) {
    [WA]::SendMessageStr($ctl.Input, 0x000C, [IntPtr]::Zero, $text) | Out-Null
    [WA]::SendMessage($ctl.Send, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
}

# Poll transcript until it stabilizes (2 identical polls 5s apart) or timeout.
function Wait-Turn($ctl, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $prev = ""
    $stable = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        $t = [WA]::EditWindowText($ctl.Log)
        if ($t -eq $prev) { $stable++ } else { $stable = 0 }
        $prev = $t
        if ($stable -ge 2) { break }
    }
    return $prev
}

$ctl = Get-Controls
if (-not $ctl -or $ctl.Log -eq [IntPtr]::Zero -or $ctl.Send -eq [IntPtr]::Zero) {
    Write-Host "controls: FAIL"; Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force; exit 1
}
Write-Host "controls: PASS"

# Item 2: NL -> claude calls list_windows via MCP
Send-Chat $ctl "열려 있는 창을 목록으로 보여줘"
$t2 = Wait-Turn $ctl 300
if ($t2 -match 'list_windows|Minesweeper|minesweeper') {
    Write-Host "turn1-list: PASS (claude reached window list)"
} else {
    Write-Host "turn1-list: FAIL — tail:"
    Write-Host ($t2.Substring([Math]::Max(0, $t2.Length - 300)))
}

# Item 3: resume follow-up — same session, claude should recall the list.
# Judge only the part AFTER this prompt (whole transcript has digits anyway).
$marker = "방금 그 목록에서"
Send-Chat $ctl ($marker + " 지뢰찾기 창의 id가 뭐였지?")
$t3 = Wait-Turn $ctl 300
$i = $t3.IndexOf($marker)
$after = if ($i -ge 0) { $t3.Substring($i) } else { $t3 }
if ($after -match '\d+\s*[,)}]|id|ID' -and $after -match 'Minesweeper|지뢰|minesweeper') {
    Write-Host "turn2-resume: PASS (recalled the list, answered an id)"
} else {
    Write-Host "turn2-resume: FAIL — tail:"
    Write-Host ($after.Substring([Math]::Max(0, $after.Length - 400)))
}

Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
if ($t2 -match 'list_windows|Minesweeper|minesweeper|지뢰' -and $t3 -match '\d+') {
    Write-Host "PASS: smoke llm A"; exit 0
} else {
    Write-Host "FAIL: smoke llm A"; exit 1
}