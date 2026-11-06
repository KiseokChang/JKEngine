$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$r1 = (& $exe agentctl '{"tool":"ping","args":{}}' 2>&1) -join "`n"
Write-Host ("direct:   " + $r1)
$esc = $json -replace '"','\"'
$r2 = (& $exe agentctl ('{\"tool\":\"ping\",\"args\":{}}') 2>&1) -join "`n"
Write-Host ("escapes:  " + $r2)
$pr = Start-Process -FilePath $exe -ArgumentList 'agentctl','{"tool":"ping","args":{}}' -NoNewWindow -PassThru -Wait -RedirectStandardOutput I:\progwork\JKENGINE\tmp\a1.txt
Write-Host ("startpr:  " + (Get-Content I:\progwork\JKENGINE\tmp\a1.txt -Raw))
$r3 = cmd /c "$exe agentctl {`"tool`":`"ping`",`"args`":{}}" 2>&1
Write-Host ("cmdc:     " + ($r3 -join "`n"))
