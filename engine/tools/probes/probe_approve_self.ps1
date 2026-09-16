# probe_approve_self: approve self-approve gate (spec 7 review follow-up,
# docs/53 s9). One raw-pipe connection parks permission_set (fixed-ask — no
# permissions.json seeding needed) and then approves its OWN parked request
# from the SAME connection -> expect {"ok":false,"error":"self_approve"}.
# A second approve from agentctl (different connection) must still resolve
# it and the parked reply must land on the raw connection (regression).
# Wire format (JKWireProtocol): 12B header {magic 0x4A4B0001, type, length}
# + payload. AgentQuery payload = queryId(4)+jsonLen(4)+json. AgentReply
# payload = queryId(4)+ok(4)+jsonLen(4)+json. AgentEvent payload =
# jsonLen(4)+json (header length = 4+jsonLen).
# ASCII-only PS5.1 (lesson 50). No jktriggers needed.
$ErrorActionPreference = "Continue"
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host "ok: $name" } else { $script:fail++; Write-Host "FAIL: $name ($detail)" }
}
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"

# guard: the approved RMW creates permissions.json - this probe owns it.
if (Test-Path $permFile) { Write-Host "ABORT - permissions.json exists"; exit 1 }

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3

try {
    # --- raw pipe client (JKX wire) -----------------------------------------
    $pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $pipe.Connect(5000)
    # Blocking read. PS5.1 pipe lessons (this session): Available returns
    # null (polling loops never read) and ReadAsync+Wait stalls on event
    # frames — a plain blocking .Read is the only reliable pattern here.
    # Hang risk is bounded by the server's 60s approval expiry.
    function ReadExact([System.IO.Pipes.NamedPipeClientStream]$s, [int]$n) {
        $buf = New-Object byte[] $n; $off = 0
        while ($off -lt $n) {
            $r = $s.Read($buf, $off, $n - $off)
            if ($r -le 0) { throw "pipe closed" }
            $off += $r
        }
        return ,$buf
    }
    function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
        $hdr = New-Object byte[] 12
        [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
        [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
        [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
        $s.Write($hdr, 0, 12)
        if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
        $s.Flush()
    }
    # Hello (type 1): HelloPayload = protocolVersion(4)=2 + pid(4).
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsg $pipe 1 $hello
    # AgentEventSubscribe (type 19): subscribe=1 so the approval_request push
    # arrives on THIS connection (pushes are not replayed — subscribe first).
    $sub = New-Object byte[] 4
    [BitConverter]::GetBytes([uint32]1).CopyTo($sub, 0)
    SendMsg $pipe 19 $sub
    function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
        $body = [Text.Encoding]::UTF8.GetBytes($json)
        $payload = New-Object byte[] (8 + $body.Length)
        [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
        [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
        [Array]::Copy($body, 0, $payload, 8, $body.Length)
        SendMsg $s 17 $payload
    }
    function ReadFrame([System.IO.Pipes.NamedPipeClientStream]$s) {
        $hdr = ReadExact $s 12
        return @{
            type = [int][BitConverter]::ToUInt32($hdr, 4)
            len  = [int][BitConverter]::ToUInt32($hdr, 8)
        }
    }
    # Reads the FULL payload of the current frame (header length = payload
    # size) and returns it as bytes.
    function ReadPayloadBytes([System.IO.Pipes.NamedPipeClientStream]$s, [int]$len) {
        if ($len -le 0) { return (New-Object byte[] 0) }
        return (ReadExact $s $len)
    }

    # --- 1. park permission_set (fixed-ask -> parks unconditionally) --------
    SendQuery $pipe 1 '{"tool":"permission_set","args":{"tool":"close_window","decision":"allow"}}'
    # read frames until the approval_request push (type 20) arrives
    $reqId = 0
    foreach ($i in 1..20) {
        $f = ReadFrame $pipe
        if ($f.type -eq 20) {
            $pl = ReadPayloadBytes $pipe $f.len
            $ev = [Text.Encoding]::UTF8.GetString($pl, 4, $pl.Length - 4)
            # PS5.1 trap: a second -match overwrites $Matches — capture the
            # request id from the FIRST pattern before testing the kind.
            $rid = 0
            if ($ev -match '"request":(\d+)') { $rid = [int]$Matches[1] }
            if ($rid -gt 0 -and $ev -match '"kind":"permission_set"') {
                $reqId = $rid
                break
            }
            Write-Host ("  (event: " + $ev + ")")
        } elseif ($f.type -eq 18) {
            $pl = ReadPayloadBytes $pipe $f.len
            $qid = [BitConverter]::ToUInt32($pl, 0)
            $jlen = [int][BitConverter]::ToUInt32($pl, 8)
            $j = [Text.Encoding]::UTF8.GetString($pl, 12, $jlen)
            Write-Host ("  (reply qid=$qid : " + $j + ")")
        } else {
            ReadPayloadBytes $pipe $f.len | Out-Null
            Write-Host ("  (frame type=" + $f.type + " len=" + $f.len + ")")
        }
    }
    Check "1-parked" ($reqId -gt 0) ("request=" + $reqId)

    # --- 2. approve from the SAME connection -> self_approve ----------------
    SendQuery $pipe 2 ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
    $gotSelf = $false
    foreach ($i in 1..10) {
        $f = ReadFrame $pipe
        if ($f.type -ne 18) { ReadPayloadBytes $pipe $f.len | Out-Null; continue }
        $pl = ReadPayloadBytes $pipe $f.len
        $qid = [BitConverter]::ToUInt32($pl, 0)
        $jlen = [BitConverter]::ToUInt32($pl, 8)
        $reply = [Text.Encoding]::UTF8.GetString($pl, 12, $jlen)
        if ($qid -eq 2) { $gotSelf = ($reply -match 'self_approve'); break }
    }
    Check "2-self-approve-rejected" $gotSelf "expect self_approve"

    # --- 3. approve from agentctl (different connection) -> resolves --------
    $ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
    Check "3-cross-approve" ($ap -match 'approved\\?":true') $ap

    # --- 4. the parked reply lands on OUR connection (queryId 1) ------------
    $gotParked = $false
    foreach ($i in 1..10) {
        $f = ReadFrame $pipe
        if ($f.type -ne 18) { ReadPayloadBytes $pipe $f.len | Out-Null; continue }
        $pl = ReadPayloadBytes $pipe $f.len
        $qid = [BitConverter]::ToUInt32($pl, 0)
        $jlen = [BitConverter]::ToUInt32($pl, 8)
        $reply = [Text.Encoding]::UTF8.GetString($pl, 12, $jlen)
        if ($qid -eq 1) { $gotParked = ($reply -match '"written":true'); break }
    }
    Check "4-parked-reply" $gotParked "parked permission_set resolved by cross-approve"

    # --- 5. the RMW wrote the file (permission_set approved by the human
    #     surface path — the self-approve rejection must not have side effects)
    $fraw = Get-Content $permFile -Raw -ErrorAction SilentlyContinue
    Check "5-file" ($fraw -match '"close_window":"allow"') "RMW wrote the file"

    $pipe.Close()
} catch {
    Write-Host ("FAIL: pipe error — " + $_.Exception.Message)
    $script:fail++
}

# cleanup: the probe created permissions.json via the approved RMW
Remove-Item $permFile -Force -ErrorAction SilentlyContinue
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
if ($script:fail -eq 0) { Write-Host "RESULT: ALL PASS" } else { Write-Host ("RESULT: {0} FAILURE(S)" -f $script:fail); exit 1 }