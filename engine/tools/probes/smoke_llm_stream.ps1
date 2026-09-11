# Real-engine token streaming smoke (docs/36): default (ollama) engine,
# one NL turn — the transcript must show a live "[LLM] ..." line typed by
# stream deltas, closed by "[LLM 완료]". Judges via transcript (lessons 29/30:
# GetWindowTextW with over-allocated buffer). Costs real quota (~1 turn).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$chatExe = "I:\progwork\JKENGINE\engine\build\jkchat.exe"
$stateDir = "I:\progwork\JKENGINE\engine\build\state"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Default engine (ollama / kimi-k2.7-code:cloud) — streaming must ride the
# same path claude does (ollama launch claude -- <claude args>).
Remove-Item (Join-Path $stateDir "chat.json") -ErrorAction SilentlyContinue

Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4
Start-Process -FilePath $chatExe -WorkingDirectory (Split-Path $exe)
Start-Sleep -Seconds 3

Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class WAS {
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
    $cb = [WAS+EnumProc]{ param($h, $l)
        $cn = New-Object System.Text.StringBuilder 64
        [WAS]::GetClassNameW($h, $cn, 64) | Out-Null
        $cls = $cn.ToString()
        if ($cls -eq 'Edit') {
            $t = [WAS]::EditWindowText($h)
            if ($t.Length -gt 0) { $script:rLog = $h } else { $script:rInput = $h }
        } elseif ($cls -eq 'Button') {
            $bn = New-Object System.Text.StringBuilder 64
            [WAS]::GetWindowTextW($h, $bn, 64) | Out-Null
            if ($bn.ToString() -eq '보내기') { $script:rSend = $h }
        }
        return $true
    }
    $script:rLog = [IntPtr]::Zero; $script:rInput = [IntPtr]::Zero; $script:rSend = [IntPtr]::Zero
    [WAS]::EnumChildWindows($r.Main, $cb, [IntPtr]::Zero) | Out-Null
    $r.Log = $script:rLog; $r.Input = $script:rInput; $r.Send = $script:rSend
    return $r
}

function Send-Chat($ctl, $text) {
    [WAS]::SendMessageStr($ctl.Input, 0x000C, [IntPtr]::Zero, $text) | Out-Null
    [WAS]::SendMessage($ctl.Send, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
}

function Wait-Turn($ctl, $timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $prev = ""
    $stable = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        $t = [WAS]::EditWindowText($ctl.Log)
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

# One NL turn — streaming deltas type the "[LLM] " line live, [LLM 완료] closes.
Send-Chat $ctl "한 문장으로 안녕하세요라고만 답해줘"
$t = Wait-Turn $ctl 300

Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force

$live = $t -match '\[LLM\] \S'
$done = $t -match '\[LLM 완료\]'
$noRetype = $true
# After the streamed line, the result must NOT be re-printed in full: the
# turn should end right after [LLM 완료].
if ($t -match '\[LLM 완료\]\r?\n[^>\r\n]+') { $noRetype = $false }

if ($live) { Write-Host "stream-line: PASS" } else { Write-Host "stream-line: FAIL — transcript:"; Write-Host $t }
if ($done) { Write-Host "turn-done: PASS" } else { Write-Host "turn-done: FAIL" }
if ($noRetype) { Write-Host "no-retype: PASS" } else { Write-Host "no-retype: FAIL (result re-printed after streaming)" }

if ($live -and $done -and $noRetype) { Write-Host "PASS: smoke llm stream"; exit 0 } else { Write-Host "FAIL: smoke llm stream"; exit 1 }