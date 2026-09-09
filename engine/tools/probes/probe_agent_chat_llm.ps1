# Chat LLM MVP probe (stub engine — no network): NL message -> engine turn ->
# result appears in the transcript. Drives the chat UI with WM_SETTEXT (input)
# + BM_CLICK (send) and judges via the transcript text (GetWindowTextW).
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$chatExe = "I:\progwork\JKENGINE\engine\build\jkchat.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# stub engine config
$stateDir = "I:\progwork\JKENGINE\engine\build\state"
New-Item -ItemType Directory -Force -Path $stateDir | Out-Null
'{"engine":"stub","skip_permissions":1}' | Set-Content `
    -Path (Join-Path $stateDir "chat.json") -Encoding ASCII

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 4
Start-Process -FilePath $chatExe -WorkingDirectory (Split-Path $exe)
Start-Sleep -Seconds 3

Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class W3 {
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendMessageStr(IntPtr h, uint m, IntPtr w, string l);
    [DllImport("user32.dll", EntryPoint = "SendMessageW")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendMessageBuf(IntPtr h, uint m, IntPtr w, System.Text.StringBuilder l);
    // Read an EDIT control's full text cross-process (GetWindowTextW proved
    // unreliable for EM_REPLACESEL-filled multiline edits from PS). Note:
    // WM_GETTEXTLENGTH can under-report vs WM_GETTEXT on multiline edits
    // (CR/LF accounting), so over-allocate and pass the cap as wParam.
    public static string EditWindowText(IntPtr h) {
        int len = (int)(long)SendMessage(h, 0x000E, IntPtr.Zero, IntPtr.Zero);
        int cap = len * 2 + 1024;
        System.Text.StringBuilder sb = new System.Text.StringBuilder(cap);
        SendMessageBuf(h, 0x000D, (IntPtr)cap, sb);
        return sb.ToString().TrimEnd('\0');
    }
}
"@

$p = Get-Process jkchat | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$hMain = $p.MainWindowHandle
$hInput = [IntPtr]::Zero; $hLog = [IntPtr]::Zero; $hSend = [IntPtr]::Zero
$cb = [W3+EnumProc]{ param($h, $l)
    $cn = New-Object System.Text.StringBuilder 64
    [W3]::GetClassNameW($h, $cn, 64) | Out-Null
    $cls = $cn.ToString()
    if ($cls -eq 'Edit') {
        $txt = [W3]::EditWindowText($h)
        if ($txt.Length -gt 0) { $script:hLog = $h }   # transcript has text
        else { $script:hInput = $h }                    # input starts empty
    } elseif ($cls -eq 'Button') {
        # Button text lives in the window name — GetWindowTextW reads it fine;
        # WM_GETTEXT cross-process on buttons returned mojibake (ANSI marshaling).
        $bn = New-Object System.Text.StringBuilder 64
        [W3]::GetWindowTextW($h, $bn, 64) | Out-Null
        if ($bn.ToString() -eq '보내기') { $script:hSend = $h }
    }
    return $true
}
[W3]::EnumChildWindows($hMain, $cb, [IntPtr]::Zero) | Out-Null
if ($hInput -eq [IntPtr]::Zero -or $hLog -eq [IntPtr]::Zero -or $hSend -eq [IntPtr]::Zero) {
    Write-Host "controls: FAIL (input=$($hInput -ne [IntPtr]::Zero) log=$($hLog -ne [IntPtr]::Zero) send=$($hSend -ne [IntPtr]::Zero))"
    Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

# NL message -> send
[W3]::SendMessageStr($hInput, 0x000C, [IntPtr]::Zero, "안녕, 창 좀 보여줘") | Out-Null
[W3]::SendMessage($hSend, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 5

$sb = New-Object System.Text.StringBuilder 8192
$transcript = [W3]::EditWindowText($hLog)

Get-Process jkdesktop,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item (Join-Path $stateDir "chat.json") -ErrorAction SilentlyContinue

$ok = $true
if ($transcript -match 'stub ok') { Write-Host "stub-turn: PASS" }
else { $ok = $false; Write-Host "stub-turn: FAIL" }
if ($transcript -match 'LLM 실행 중') { Write-Host "busy-line: PASS" }
else { $ok = $false; Write-Host "busy-line: FAIL" }
if ($ok) { Write-Host "PASS: agent chat llm"; exit 0 } else { Write-Host "FAIL: agent chat llm"; exit 1 }