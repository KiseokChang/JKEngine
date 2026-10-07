#!/usr/bin/env bash
# WSL 앱 커버리지 probe (앱 커버리지 라인 Task 2 — wsl_chat_boot.sh 원준,
# tmp/wsl_repack_receipt.sh 후속 영수증 대체).
# 영수증 목표: ① WSL ninja 리빌드 ② WSL selftest 편승(374 기준선 무회귀)
# ③ library-list count=29 + 구 jkx-pack 미제공이던 앱 행 source=jkx 실측
# ④ 비ASCII 파일명 바이트 어설션(T1 리뷰 I1 수리 방법론 — tmp 영수증의
# `od -c | grep -F '\ 3 4 5'` 공허 패턴 폐기, 실매치 카운트로 진단)
# ⑤ jkx-pack negative path(nosuchapp → rc≠0 + dlerror 상세 1행)
# ⑥ setsid 서버 부팅 → launch_app settings/notes(ok:true×2) → list_windows에
# 창 2개 오브젝트 → 브래킷 pkill 철수 + 잔존 검사.
# count 기대치=29의 근거(T1 룰링 원장 진술): auto-repack 기준선 — 24개
# launcher .jkx + legacy drvfs 트리거 .jkx 4개 + chat builtin. workshop은
# 어느 플랫폼에서도 auto-repack 산출이 아니므로(수동 pack 유산, count
# 29↔30 소결은 별도 소유 결정) 여기서 기대하지 않는다.
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
#   비ASCII 바이트 어설션은 WSL readdir 관점에서 총 0 히트가 정상이고
#   auto-repack 신규 leg 산출 청결 단정이 된다; legacy 4개는 ASCII 관점
#   파일명(triggers\<name>.jkx)으로 별도 카운트 기록한다.
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "APPS-COUNT-FAIL: $*"; exit 1; }

echo "=== 1. ninja rebuild (buildwsl) ==="
ninja -C buildwsl -j3 2>&1 | tail -6
BUILD_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || FAIL "ninja rebuild rc=$BUILD_RC"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"

echo "=== 2. WSL selftest 편승 (T1 리뷰 carryover — 기준선 374 무회귀 기록) ==="
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
[ "$ST_PASS" -ge 374 ] || FAIL "selftest PASS=$ST_PASS < 기준선 374 (WSL축 회귀)"

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
echo "LIBRARY-COUNT=$COUNT (근거: auto-repack 기준 29 — 헤더 코멘트 원장 진술)"
[ "$COUNT" -eq 29 ] || FAIL "library-list count=$COUNT expected 29 (자동 리팩 기준선 이탈)"

# 신규 jkx-pack이 이전에 미제공이던 앱 행 실측 — source=jkx로 잠근다.
for APP in settings notes terminal; do
    ROW=$(grep -aE "^name=$APP title=.+ source=jkx caps= size=[0-9]+ path=" /tmp/apps_lib_list.out | head -1)
    [ -n "$ROW" ] || FAIL "library-list has no source=jkx row for name=$APP — output: $(tr '\n' ' ' </tmp/apps_lib_list.out)"
    echo "row: $ROW"
done

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

echo "=== 5. jkx-pack negative path (nosuchapp — 정직한 실패 수신) ==="
timeout 30 ./buildwsl/jkdesktop jkx-pack nosuchapp >/tmp/apps_neg.out 2>/tmp/apps_neg.err
NEG_RC=$?
echo "NEGATIVE-PATH-RC=$NEG_RC"
echo "stderr: $(cat /tmp/apps_neg.err | tail -1)"
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

echo "=== 9. cleanup (bracketed pkill — WSL kill discipline) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"

echo "APPS-COUNT-OK"
exit 0
