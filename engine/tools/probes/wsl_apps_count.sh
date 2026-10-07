#!/usr/bin/env bash
# WSL 앱 커버리지 probe (앱 커버리지 라인 Task 2 — wsl_chat_boot.sh 원준,
# tmp/wsl_repack_receipt.sh 후속 영수증 대체. Task 3 갱신 — 설치 트윈).
# 영수증 목표: ① WSL ninja 리빌드 ② WSL selftest 편승(399 기준선 무회귀 —
# T2 실측 391 + T3 신설 1m-t 케이스 8행)
# ③ library-list count=32 + 구 jkx-pack 미제공이던 앱 행 source=jkx 실측 +
# Task 3 설치 트윈 3행(콘솔 sampletodo 1 + 내장 lf/hx 2) 실측
# ④ 비ASCII 파일명 바이트 어설션(T1 리뷰 I1 수리 방법론 — tmp 영수증의
# `od -c | grep -F '\ 3 4 5'` 공허 패턴 폐기, 실매치 카운트로 진단)
# ⑤ jkx-pack negative path(nosuchapp → rc≠0 + dlerror 상세 1행)
# ⑥ setsid 서버 부팅 → launch_app settings/notes(ok:true×2) → list_windows에
# 창 2개 오브젝트 → 브래킷 pkill 철수 + 잔존 검사.
# count 기대치=32의 근거: T2 실측 29(auto-repack 기준선 — 24개 launcher .jkx
# + legacy drvfs 트리거 .jkx 4개 + chat builtin) + Task 3 설치 트윈 3
# (콘솔 sampletodo 트윈 1 — manifest cmd_posix 스폰 키 + 내장 lf/hx 2 —
# buildwsl/apps-bin 조달, wsl_apps_bin_setup.sh가 확보. workshop은 어느
# 플랫폼에서도 auto-repack 산출이 아니므로(수동 pack 유산) 여기서 기대하지
# 않는다).
#   템플릿 관습 그대로 — wsl.exe는 인라인 인용을 찢으니 이 스크립트는 항상
#   파일로 실행한다(Git Bash/MSYS 경로 변환이 /mnt/ 경로를 찢으니
#   MSYS_NO_PATHCONV=1 접두 필수 — 실측: 접두 없으면 "No such file or
#   directory"):
#     MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_apps_count.sh
#   정리는 WSL 표준 pkill(cdb q kill은 윈도 프로브 패턴 — 여기서 금지).
#   legacy 트리거 4개는 이중층 이름이다 — NTFS에는 U+F05C(EF 81 9C)로 기록됐고
#   윈도 측(Git Bash) cat -A 실측은 그 바이트를 보여준다(triggersM-oM-^AM-^\...),
#   그러나 WSL drvfs readdir은 0x5C로 투명 디코드해 돌려준다(T1 리뷰 I1 실측,
#   2026-10-07 재실측 — WSL 측 ls-1은 전부 ASCII인 28행을 인쇄). 그러므로
#   WSL 측 `ls|grep -P` 어설션은 drvfs 뷰의 ASCII 청결을 단정한다. U+F05x
#   백슬래시 이름은 drvfs readdir가 투명 디코드해 이 어설션에 안 잡힌다 —
#   Windows 측 직검 어설션(§4b)이 그 트립와이어다(T2 리뷰 I1 수리). legacy
#   4개는 WSL ASCII 관점 파일명(triggers\<name>.jkx)으로 별도 카운트 기록.
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "APPS-COUNT-FAIL: $*"; exit 1; }

echo "=== 1. ninja rebuild (buildwsl) ==="
ninja -C buildwsl -j3 2>&1 | tail -6
BUILD_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || FAIL "ninja rebuild rc=$BUILD_RC"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"

echo "=== 1b. lf/hx posix 조달 (Task 3 설치 트윈 — 멱등, 부재 시에만 네트워크) ==="
bash scripts/install_lf_helix_posix.sh \
    || FAIL "apps-bin setup failed (lf/hx posix provisioning)"

echo "=== 2. WSL selftest 편승 (T1 리뷰 carryover — 기준선 무회귀 기록) ==="
timeout 300 ./buildwsl/jkdesktop test >/tmp/apps_st.log 2>&1
ST_RC=$?
echo "selftest rc=$ST_RC"
ST_PASS=$(grep -ac '^\[PASS\]' /tmp/apps_st.log)
ST_FAIL=$(grep -ac '^\[FAIL\]' /tmp/apps_st.log)
echo "selftest PASS=$ST_PASS FAIL=$ST_FAIL"
grep -a 'AppSelfTest' /tmp/apps_st.log | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' /tmp/apps_st.log \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)' — log: /tmp/apps_st.log"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (WSL축 회귀) — log: /tmp/apps_st.log"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
# 하한 앵커=T3 실측 399 (T2 실측 391 + T3 신설 1m-t 콘솔 트윈 케이스 8행
# 증분 — T2 리뷰 M3 하한 운용 계승)
[ "$ST_PASS" -ge 399 ] || FAIL "selftest PASS=$ST_PASS < 기준선 399 (WSL축 회귀)"

echo "=== 3. library-list (serverless catalog CLI) ==="
timeout 30 ./buildwsl/jkdesktop library-list >/tmp/apps_lib_list.out 2>/tmp/apps_lib_list.err
LIST_RC=$?
echo "library-list rc=$LIST_RC"
if [ "$LIST_RC" -ne 0 ]; then
    echo "--- stderr:"; cat /tmp/apps_lib_list.err
    FAIL "library-list rc=$LIST_RC (expected 0)"
fi
cat /tmp/apps_lib_list.out
NAMED=$(grep -c '^name=' /tmp/apps_lib_list.out)
COUNTLINE=$(grep -aE '^count=[0-9]+ base=' /tmp/apps_lib_list.out | head -1)
[ -n "$COUNTLINE" ] || FAIL "library-list has no 'count=<n> base=' tail (malformed output)"
COUNT=$(printf '%s' "$COUNTLINE" | sed -n 's/^count=\([0-9]*\) .*$/\1/p')
[ "$NAMED" -eq "$COUNT" ] || FAIL "library-list line/count mismatch: name= lines=$NAMED count=$COUNT"
echo "LIBRARY-COUNT=$COUNT (근거: T2 auto-repack 기준 29 + Task 3 설치 트윈 3 — 헤더 코멘트 원장 진술)"
[ "$COUNT" -eq 32 ] || FAIL "library-list count=$COUNT expected 32 (T3 설치 트윈 반영 기준선 이탈)"

# 신규 jkx-pack이 이전에 미제공이던 앱 행 실측 — source=jkx로 잠근다.
for APP in settings notes terminal; do
    ROW=$(grep -aE "^name=$APP title=.+ source=jkx caps= size=[0-9]+ path=" /tmp/apps_lib_list.out | head -1)
    [ -n "$ROW" ] || FAIL "library-list has no source=jkx row for name=$APP — output: $(tr '\n' ' ' </tmp/apps_lib_list.out)"
    echo "row: $ROW"
done

# Task 3 설치 트윈 3행 실측 — 콘솔 sampletodo(1) + 내장 lf/hx(2).
# 콘솔 스폰 키 계약 = manifest cmd_posix(basePath 상대) — 스폰되면
# /bin/sh -c <키>로 살아난다(JKConPtyBridge_posix). 내장 lf/hx는 무접미 경로
# (JKLibraryCatalog.cpp:217-223 플랫폼 접미 게이트 — win32 .exe / posix 무접미).
TWIN_ROW=$(grep -aF 'name=terminal:apps/sampletodo/sampletodo.sh' /tmp/apps_lib_list.out | head -1)
[ -n "$TWIN_ROW" ] || FAIL "library-list has no console twin row (name=terminal:apps/sampletodo/sampletodo.sh) — output: $(tr '\n' ' ' </tmp/apps_lib_list.out)"
printf '%s' "$TWIN_ROW" | grep -aq ' source=console caps= size=0 ' \
    || FAIL "console twin row is not source=console — row: $TWIN_ROW"
echo "row: $TWIN_ROW"
LF_ROW=$(grep -aF 'name=terminal:apps-bin/lf/lf title=lf source=builtin' /tmp/apps_lib_list.out | head -1)
[ -n "$LF_ROW" ] || FAIL "library-list has no builtin lf row (posix 무접미 경로 게이트 미충족?) — output: $(tr '\n' ' ' </tmp/apps_lib_list.out)"
echo "row: $LF_ROW"
HX_ROW=$(grep -aF 'name=terminal:apps-bin/helix/hx title=hx source=builtin' /tmp/apps_lib_list.out | head -1)
[ -n "$HX_ROW" ] || FAIL "library-list has no builtin hx row (posix 무접미 경로 게이트 미충족?) — output: $(tr '\n' ' ' </tmp/apps_lib_list.out)"
echo "row: $HX_ROW"

# 콘솔 트윈 존재 게이트 negative path — 트윈 .sh 파일이 없으면 카탈로그가
# 스킵하는 것(fail-closed)을 잠근다: staged 트리의 .sh를 잠깐 옮겨 두었다가
# 되돌려 두고, 그 사이 count가 31로 떨어졌다가 되돌아오는지 실측.
mv buildwsl/apps/sampletodo/sampletodo.sh /tmp/apps_twin_hidden.sh
HIDDEN_OUT=$(timeout 30 ./buildwsl/jkdesktop library-list 2>/dev/null | grep -aE '^count=' | head -1)
mv /tmp/apps_twin_hidden.sh buildwsl/apps/sampletodo/sampletodo.sh
echo "twin-hidden count line: $HIDDEN_OUT"
[ "$(printf '%s' "$HIDDEN_OUT" | sed -n 's/^count=\([0-9]*\) .*$/\1/p')" = "31" ] \
    || FAIL "twin-hidden negative path: count did not drop to 31 (fail-closed 게이트 미작동) — line: $HIDDEN_OUT"
[ -x buildwsl/apps/sampletodo/sampletodo.sh ] || FAIL "twin-hidden restore failed (staged .sh lost)"

echo "=== 4. 비ASCII 파일명 바이트 어설션 (WSL readdir 관점 — 총 0 히트) ==="
ls -1 buildwsl/apps/*.jkx >/tmp/apps_names.txt
NA_TOTAL=$(grep -aPc '[^\x20-\x7E]' /tmp/apps_names.txt)
echo "NONASCII-HITS-ALL=$NA_TOTAL"
echo "--- full top-level .jkx name list (wsldrvfs readdir view; NTFS 측 기록 바이트는 U+F05C — 헤더 원장 진술):"
cat /tmp/apps_names.txt
[ "$NA_TOTAL" -eq 0 ] || FAIL "non-ASCII filename hit(s)=$NA_TOTAL (U+F05x 재발 또는 drvfs 디코드 변화) — hits: $(grep -a '[^\x20-\x7E]' /tmp/apps_names.txt | tr '\n' ' ')"
LEGACY_TRIG=$(ls -1 buildwsl/apps/*.jkx | grep -aF 'triggers\' | wc -l)
echo "LEGACY-TRIGGER-FILES=$LEGACY_TRIG (drvfs 0x5C 투명 디코드 관점 파일명 — T1 실측 4개 기준선)"
[ "$LEGACY_TRIG" -eq 4 ] || FAIL "legacy trigger .jkx count=$LEGACY_TRIG expected 4 (T1 실측 기준선 이탈 — 원장 진술 갱신 필요)"

# --- 4b. Windows 측 바이트 직검 (T2 리뷰 I1 수리 — drvfs 투명 디코드 밖의
# 트립와이어). WSL readdir은 U+F05C를 0x5C로 디코드해 §4 어설션에 안 잡히므로,
# WSL interop으로 powershell.exe를 불러 NTFS 저장 이름을 직접 본다: 0x20-0x7E
# 밖 문자만 U+XXXX 이스케이프해 인쇄. 히트가 legacy U+F05C 패밀리 4종이면
# PASS(legacy 유산 — 위생 시 이 어설션과 함께 갱신), 그 외·초과면 hard FAIL.
echo "=== 4b. Windows 측 바이트 직검 (U+F05x 트립와이어 — NTFS 저장 바이트 뷰) ==="
PS_EXE=/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
[ -x "$PS_EXE" ] || FAIL "Windows interop powershell.exe 없음 (WSL interop 비활성 — Windows 측 직검 불가)"
WIN_APPS=$(wslpath -w buildwsl/apps)
[ -n "$WIN_APPS" ] || FAIL "wslpath -w buildwsl/apps returned empty"
PS_CMD='[Console]::OutputEncoding=[System.Text.Encoding]::ASCII; Get-ChildItem -LiteralPath "'"$WIN_APPS"'" -Filter "*.jkx" | ForEach-Object { $n=$_.Name; $bad=[regex]::Matches($n,"[^\x20-\x7E]"); if ($bad.Count -gt 0) { [regex]::Replace($n,"[^\x20-\x7E]",{ param($m) ("U+{0:X4}" -f [int]$m.Value[0]) }) } }'
WIN_HITS_RAW=$(timeout 60 "$PS_EXE" -NoProfile -Command "$PS_CMD")
PS_RC=$?
echo "WIN-BYTESCAN-RC=$PS_RC"
[ "$PS_RC" -eq 0 ] || FAIL "powershell.exe byte scan rc=$PS_RC (Windows 측 직검 사망)"
# powershell.exe 콘솔 출력은 CRLF — \r을 벗겨야 bash [ -eq ]·grep -E '$' 앵커가
# 산다(fix r1 실측: \r 잔류 시 "숫자 표현이 아닙니다"로 전수 FAIL). WIN_HITS_RAW
# 무가치화 방지용으로도 같은 경유.
WIN_HITS=$(printf '%s' "$WIN_HITS_RAW" | tr -d '\r')
# 공허 패턴 방지: Windows 뷰 총 .jkx 수와 WSL drvfs 뷰 행수가 동치여야 한다 —
# PS가 조용히 실패해 빈 히트만 돌려도 여기서 잡힌다.
WIN_TOTAL_RAW=$("$PS_EXE" -NoProfile -Command '[Console]::OutputEncoding=[System.Text.Encoding]::ASCII; (Get-ChildItem -LiteralPath "'"$WIN_APPS"'" -Filter "*.jkx").Count')
[ $? -eq 0 ] || FAIL "powershell.exe total-count scan failed (Windows 측 직검 사망)"
PS_TOTAL=$(printf '%s' "$WIN_TOTAL_RAW" | tr -d '\r\n')
WIN_VIEW_COUNT=$(wc -l < /tmp/apps_names.txt)
echo "WIN-VIEW-COUNT=$PS_TOTAL WSL-VIEW-COUNT=$WIN_VIEW_COUNT"
[ "$PS_TOTAL" -eq "$WIN_VIEW_COUNT" ] || FAIL "Windows 뷰 .jkx 수=$PS_TOTAL != WSL 뷰=$WIN_VIEW_COUNT (직검이 전 집합을 못 봄 — 바이트 스캔 공허)"
# grep rc=1(매치 0)은 정상 경로 — 단정 운반체는 인쇄된 카운트다.
WIN_HIT_N=$(printf '%s\n' "$WIN_HITS" | grep -ac '.')
FAM_MATCH_N=$(printf '%s\n' "$WIN_HITS" | grep -acE '^triggersU\+F05C(trig_build|trig_crash|trig_idle|rate_probe)\.jkx$')
echo "WIN-NONASCII-HITS=$WIN_HIT_N (legacy U+F05C 패밀리 매치=$FAM_MATCH_N)"
echo "--- Windows 측 히트 목록(U+XXXX 이스케이프 뷰):"
echo "$WIN_HITS"
[ "$WIN_HIT_N" -le 4 ] || FAIL "Windows 측 비ASCII 파일명 히트=$WIN_HIT_N > 4 (U+F05x 재발 또는 신규 비ASCII 이름 — NTFS 저장 바이트 기준) — hits: $(echo "$WIN_HITS" | tr '\n' ' ')"
[ "$WIN_HIT_N" -eq "$FAM_MATCH_N" ] || FAIL "Windows 측 히트 중 legacy 4종(trig_build/trig_crash/trig_idle/rate_probe) 외 이름 존재 — hits: $(echo "$WIN_HITS" | tr '\n' ' ')"
echo "WIN-NONASCII-OK (4=legacy 유산 — 위생 시 이 어설션과 함께 갱신)"

echo "=== 5. jkx-pack negative path (nosuchapp — 정직한 실패 수신) ==="
timeout 30 ./buildwsl/jkdesktop jkx-pack nosuchapp >/tmp/apps_neg.out 2>/tmp/apps_neg.err
NEG_RC=$?
echo "NEGATIVE-PATH-RC=$NEG_RC"
echo "stderr: $(tail -1 /tmp/apps_neg.err)"
[ "$NEG_RC" -ne 0 ] || FAIL "jkx-pack nosuchapp unexpectedly succeeded (rc=0) — negative path broken"
grep -aq 'cannot load' /tmp/apps_neg.err \
    || FAIL "jkx-pack nosuchapp stderr missing 'cannot load' detail (dlerror 상세 소실) — stderr: $(cat /tmp/apps_neg.err | tr '\n' ' ')"
[ -e buildwsl/apps/nosuchapp.jkx ] && FAIL "jkx-pack nosuchapp left buildwsl/apps/nosuchapp.jkx behind"

# 어설션 헬퍼(wsl_chat_boot.sh 템플릿 — 실패=FAIL 경로, 관용 없음). 서버는
# compact JSON으로 답한다(launch_app={"ok":true,...}, list_windows 항목=
# {"id":..,"w":..,"h":..,"title":..,"focused":true|false}).
ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }

echo "=== 6. boot server (setsid nohup detached, WSLg :0) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
rm -f /tmp/JKWindowServerPipe.sock
sleep 1
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >/tmp/apps_srv.log 2>&1 &
sleep 5
SRV_PIDS=$(pgrep -x jkdesktop | tr '\n' ' ')
if [ -z "$SRV_PIDS" ]; then
    echo "--- server log tail:"; tail -20 /tmp/apps_srv.log
    FAIL "no jkdesktop process 5s after setsid boot (server died)"
fi
echo "server pids: $SRV_PIDS"
PING_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING_OUT"
[ -n "$PING_OUT" ] || FAIL "agentctl ping got no reply JSON (server unresponsive)"
ASSERT_FIELD "$PING_OUT" '"ok":true' "ping did not ok"

echo "=== 7. launch_app settings + notes (이전 WSL 미제공 jkx 앱 2종) ==="
L1_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"settings"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch settings: $L1_OUT"
[ -n "$L1_OUT" ] || FAIL "launch_app settings got no reply (server gone?)"
ASSERT_FIELD "$L1_OUT" '"ok":true' "launch_app settings did not ok"
L2_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"notes"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch notes: $L2_OUT"
[ -n "$L2_OUT" ] || FAIL "launch_app notes got no reply (server gone?)"
ASSERT_FIELD "$L2_OUT" '"ok":true' "launch_app notes did not ok"

echo "=== 8. list_windows — receipt: Settings/Notes 창 오브젝트 존재 ==="
sleep 8
WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN_OUT"
[ -n "$WIN_OUT" ] || FAIL "list_windows got no reply"
# 창 오브젝트만 잘라 존재를 잠근다(항목 오브젝트엔 중괄호가 없어 [^}]*
# 절단이 안전하다 — 템플릿 동일). 실패 어설션은 그대로 FAIL.
SETT_WIN=$(printf '%s' "$WIN_OUT" | grep -aoE '\{"id":[^}]*"title":"Settings"[^}]*\}' | head -1)
NOTES_WIN=$(printf '%s' "$WIN_OUT" | grep -aoE '\{"id":[^}]*"title":"Notes"[^}]*\}' | head -1)
[ -n "$SETT_WIN" ] || FAIL "list_windows has no Settings window object (launch_app settings lost) — reply: $WIN_OUT"
[ -n "$NOTES_WIN" ] || FAIL "list_windows has no Notes window object (launch_app notes lost) — reply: $WIN_OUT"
echo "settings window: $SETT_WIN"
echo "notes window: $NOTES_WIN"

echo "=== 8b. launch_app 콘솔 트윈 (Task 3 — sampletodo .sh 스폰 키 수신) ==="
# 스폰된 트윈은 인수 없이 todo.txt 안내 한 줄 인쇄하고 끝난다 → PTY가 닫히고
# 창이 곧 사라진다 — list_windows 단정은 경주형이라 하지 않고, 스폰 응답
# ok:true만 잠근다(terminal: 접두는 jkapp_ 존재 검증 면제 계약 — 실제 실행은
# /bin/sh -c apps/sampletodo/sampletodo.sh로 살아난다).
LT_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"terminal:apps/sampletodo/sampletodo.sh"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch twin: $LT_OUT"
[ -n "$LT_OUT" ] || FAIL "launch_app twin got no reply (server gone?)"
ASSERT_FIELD "$LT_OUT" '"ok":true' "launch_app twin (sampletodo .sh) did not ok"
# 스폰이 실제 서버를 나갔는지 잠근다 — SpawnProcess의 stderr 스폰 1행
# (launch_app ok:true는 접두 면제 계약상 스폰 성공 bool을 전승하지 않는다 —
# 서버 로그 행이 그 진실원).
sleep 2
SPAWNED_LINE=$(grep -aF 'terminal --shell apps/sampletodo/sampletodo.sh' /tmp/apps_srv.log | tail -1)
echo "spawned line: $SPAWNED_LINE"
[ -n "$SPAWNED_LINE" ] || FAIL "server log has no spawn line for the twin (launch_app ok:true was not a spawn)"
# 트윈 본문 수칙 실측(서버리) — 인수 없이는 todo.txt 안내 한 줄로 rc=0.
# (= .cmd 본문 1:1 이식 검증 — 스폰된 터미널 창이 이 출력으로 곧 닫히는 것도
# 같은 이유, list_windows 경주 어설션을 두지 않은 근거)
TWIN_BODY=$(cd buildwsl && timeout 10 ./apps/sampletodo/sampletodo.sh 2>&1)
TWIN_RC=$?
echo "twin body rc=$TWIN_RC first line: $(printf '%s' "$TWIN_BODY" | head -1)"
[ "$TWIN_RC" -eq 0 ] || FAIL "twin body rc=$TWIN_RC (expected 0)"
printf '%s' "$TWIN_BODY" | grep -aq 'todo.txt not found' \
    || FAIL "twin body missing todo.txt hint line — body: $(printf '%s' "$TWIN_BODY" | tr '\n' ' ')"
echo "=== 9. cleanup (bracketed pkill — WSL kill discipline) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"

echo "APPS-COUNT-OK"
exit 0
