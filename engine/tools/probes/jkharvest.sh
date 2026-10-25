#!/usr/bin/env bash
# 수확 probe jkharvest.sh (#113 T2 — 스펙 docs/superpowers/specs/
# 2026-10-26-use-review-cycle-design.md D3 + 브리프
# .superpowers/sdd/2026-10-26-use-review-cycle/task-2-brief.md).
#
# 계약 원문: agentctl로 저널을 소싱하지 않는다 — **state/use_journal.jsonl
# 파일 직독**(T1 리포트 §계약 — events_list 도구에 저널 소싱 불요). 수확 =
# ①서버 부팅(probe 자체 기동) ②agentctl 4콜(ping·list_windows·launch_app+
# close_window — 저널이 쌓이도록; 도착 확인용 list_windows 포함 실측 5콜) ③수확 원문(journal tail·bridge_report
# 목록·에러 행 분류) ④HARVEST-VERDICT 1행+HARV-RUN-VALS 요약 스트림
# (JOG-RUN-VALS 원존 — probe stdout 수취 계약).
#
# 모드(진실원 = 환경 JKHARV_MODE, 기본 OWN):
#   OWN     — probe 자체 기동 원존(wsl_* 선형 승계 — wsl_library_boot.sh 수형):
#             pre-clean(bracket pkill+-9+#98 소켓 가드)→setsid 부팅→4콜→
#             수확→정리(pkill 에스컬레이션+REMNANT 게이트 — probe 소유 잔상 0).
#             라이브 사용자 세션 무접촉 계약 = pre-clean으로 자기 서버와 격리
#             하고 종료 시 자기가 살린 것만 죽인다(wsl_library_boot 원존 접점).
#   OBSERVE — 폰 동형 세그먼트(무접촉): **pre-clean·부팅·launch/close 전량
#             미수행** — 기존 서버(실행 위치의 서버)에 agentctl 읽기 접점만
#             (스펙 D3 "Win/WSL/폰 공통 — agentctl 접점만이라"). 폰 서버는
#             구 플래그 없는 바이너리(저널 leg T1 이전 배포) → 저널 행 0 =
#             **정직 표기 원장행**(저널 leg는 폰 재배포 후 — 배포 드라이버 접촉
#             금지 — 재실측 몫). 창 관리 콜이 잔상을 남기므로 폰에서는
#             생략(원장행 1행 — 동형 축소 선례: phone_music.sh 폰 스킵 수형).
#
# 2런 지속성 원문: 저널은 state dir(실행 위치의 서버 exe dir/state)에
# 살아 있어 서버 재시작 간에 유지된다 — OWN 2런째의 JROWS0(프리 스냅샷)가
# 1런째의 행을 그대로 흡수하면 PERSIST-ROWS-FROM-PREV-RUN 1행으로 봉인.
#
# 실행법(윈도 Git Bash — 저장소 어디서든):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- \
#     bash /mnt/i/progwork/JKENGINE/engine/tools/probes/jkharvest.sh
#   # wsl.exe는 인라인 인용을 찢으니 항상 파일로 실행(wsl_library_boot 원존).
# 폰(드라이버 ssh 원존 절차 — 주소 기록 금지 계약; PHONE_HOST 환경변수):
#   ssh <폰> 'cat > $TMPDIR/jkharvest.sh' < engine/tools/probes/jkharvest.sh
#   ssh <폰> 'JKHARV_MODE=OBSERVE JKHARV_ENGINE=$HOME/JKENGINE/engine \
#             bash $TMPDIR/jkharvest.sh'
#   ssh <폰> 'rm -f $TMPDIR/jkharvest.sh'   # probe 소유 잔상 0
#
# 함정 원장(승계+신규):
#   · 스크립트는 빌드하지 않는다(T2 — 소유/타 세션 ninja 무접촉 계약 —
#     ollama 배치 경합 회피). 바이너리 존재만 검사(mtime 원문 1행).
#   · close_window 기본 게이트 = deny(권한표 기본 — JKWindowServer.cpp
#     {"close_window","server","deny"}). 그래서 4콜의 마지막 콜은 **의도된
#     ok=false err 행**의 원문이 된다(분류 요약의 시료) — ok:false 회신만
#     어설션(원문은 deny든 window_not_found든 정직). 남은 게임 창은 teardown
#     pkill이 클라까지 전량 정리(클라 exe도 jkdesktop — 잔상 0 게이트가 보증).
#   · PHONE_HOST 가드류의 tee/exec 순서 함정은 본 probe에 없다(tee 미사용 —
#     드라이버가 로그를 소유).
#   · journal 행수 산치는 wc -l(writer가 매 행 \n로 닫는다 — T1 규약).
set -u

ROOT="$(cd "$(dirname "$0")"/../.. && pwd)"          # engine/tools/probes → engine
ENGINE_DIR="${JKHARV_ENGINE:-$ROOT}"                 # 폰 $TMPDIR 기동 시 $HOME/JKENGINE/engine 지정
cd "$ENGINE_DIR" || { echo "HARV-FAIL: engine dir missing: $ENGINE_DIR"; exit 1; }

BUILD_DIR="${JKHARV_BUILD:-}"
if [ -z "$BUILD_DIR" ]; then
    if [ -x buildterm/jkdesktop ]; then BUILD_DIR=buildterm
    elif [ -x buildwsl/jkdesktop ]; then BUILD_DIR=buildwsl
    else echo "HARV-FAIL: no buildterm/buildwsl jkdesktop under $ENGINE_DIR"; exit 1; fi
fi
JK="$BUILD_DIR/jkdesktop"
STATE="$BUILD_DIR/state"
[ -x "$JK" ] || { echo "HARV-FAIL: server binary missing: $JK"; exit 1; }
echo "JKHARV-ENGINE: $ENGINE_DIR ($BUILD_DIR)"
echo "JKHARV-BINARY: mtime=$(date -r "$JK" '+%F %T' 2>/dev/null) bytes=$(wc -c < "$JK") (probe는 빌드하지 않는다 — T2 ninja 무접촉 계약)"
echo "JKHARV-STATE: $STATE"
echo "JKHARV-MODE: ${JKHARV_MODE:-OWN}"

JOURNAL="$STATE/use_journal.jsonl"
ctl() { timeout 15 "$JK" agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
assert_ok() { [ -n "${1:-}" ] && printf '%s' "$1" | grep -aq '"ok":true' || { echo "HARVEST-VERDICT: HARVEST-FAIL ($2 — reply: ${1:-none})"; exit 1; }; }
journal_rows() { [ -s "$JOURNAL" ] && wc -l < "$JOURNAL" | tr -d ' ' || echo 0; }
report_count() { ls "$STATE"/bridge_report_*.txt 2>/dev/null | wc -l | tr -d ' '; }
err_count() { # grep -c는 무매치 시 exit 1("0" 인쇄)이라 || echo 폴백이 2행을
              # 내놓는 함정 회피 — awk 산치(부재 시 0)
    [ -s "$JOURNAL" ] || { echo 0; return 0; }
    awk 'BEGIN{c=0} /"ok":false/ {c++} END{print c}' "$JOURNAL"
}
report_names() { # 최근 10건 — mtime 최신순(Termux ls에 -r 지원 — coreutils/busybox 공통)
    ls -t "$STATE"/bridge_report_*.txt 2>/dev/null | head -10
}

# ---------------------------------------------------------------- 0. 프리 스냅샷
MODE="${JKHARV_MODE:-OWN}"
BR_BEFORE=$(report_count)
echo "BRIDGE-REPORTS-BEFORE: $BR_BEFORE"

if [ "$MODE" = "OWN" ]; then
    echo "=== 1. pre-clean (wsl_library_boot 수형 — bracket pkill+-9+#98 소켓 가드) ==="
    pkill -f "$BUILD_DIR/[j]kdesktop" 2>/dev/null
    # #98 pre-clean guard: LIVE 서버 소켓 rm은 리스너를 묻는다 — 존중 원존.
    if pgrep -f '[j]kdesktop.*--server' >/dev/null 2>&1; then
        echo "NOTE #98: surviving server holds the socket — pre-clean rm skipped"
    else
        rm -f /tmp/JKWindowServerPipe.sock
    fi
    sleep 1
    # 프리 스냅샷(정리 훼손 전) — 2런 지속성 원문의 대조 원문.
    JROWS0=$(journal_rows)
    echo "JROWS0: $JROWS0 (프리 스냅샷 — 2런이면 PERSIST-ROWS-FROM-PREV-RUN로 봉인)"
    [ "$JROWS0" -gt 0 ] && echo "PERSIST-ROWS-FROM-PREV-RUN: $JROWS0 (저널 = state dir 서수 — 서버 재시작 간 유지 원문 1런)" || true

    echo "=== 2. 부팅 (setsid nohup detached — DISPLAY :0 WSLg 원존) ==="
    env DISPLAY=:0 setsid nohup "$JK" --server >/tmp/jkh_srv.log 2>&1 &
    sleep 5
    SRV_PIDS=$(pgrep -x jkdesktop | tr '\n' ' ')
    [ -n "$SRV_PIDS" ] || { echo "--- server log tail:"; tail -20 /tmp/jkh_srv.log; echo "HARVEST-VERDICT: HARVEST-FAIL (no server 5s after boot)"; exit 1; }
    echo "server pids: $SRV_PIDS"
    PING=$(ctl '{"tool":"ping","args":{}}')
    echo "ping reply: $PING"
    assert_ok "$PING" "ping did not ok"
    LIST=$(ctl '{"tool":"list_windows","args":{}}')
    echo "list_windows: $LIST"
    assert_ok "$LIST" "list_windows did not ok"

    echo "=== 3. 4콜 중 launch_app (본래 builtin — tetris) ==="
    LAUNCH=$(ctl '{"tool":"launch_app","args":{"app":"tetris"}}')
    echo "launch reply: $LAUNCH"
    assert_ok "$LAUNCH" "launch_app did not ok"
    sleep 4   # 클라 스폰 도착 여유(WSL은 수초 — 폰 15-18s 원장과 구별)
    LIST2=$(ctl '{"tool":"list_windows","args":{}}')
    printf '%s' "$LIST2" | grep -aq 'Tetris' \
        && echo "TETRIS-WINDOW: present (spawn 도착 원문)" \
        || echo "TETRIS-WINDOW: not listed yet (스폰 페이싱 — 저널 행에 무영향 — 원장행)"
    echo "=== 4. 4콜 중 close_window — 기본 deny 게이트(의도된 ok=false err 행 시료) ==="
    CLOSE=$(ctl '{"tool":"close_window","args":{"app":"tetris"}}')
    echo "close reply: $CLOSE (ok:false 기대 — deny/window_not_found 모두 정직 err 원문)"
    printf '%s' "$CLOSE" | grep -aq '"ok":false' \
        || echo "CLOSE-GATE-ERR: ok:true 회신(권한 allow 변경 상태 — err 시료 미생성 — 분류 요약은 실측 행을 그대로 인쇄 — 원장행)"
    CALLS=5   # 실측 콜: ping·list_windows·launch_app·list_windows(도착 확인)·close_window — 1콜 1행이라 journal delta와 등호 산치 원문
else
    echo "=== 1. OBSERVE — 무접촉 원장행: pre-clean·부팅·launch/close 미수행 (기존 서버에 읽기 접점만) ==="
    echo "OBSERVE-SKIP: pre-clean/boot/launch/close 없음 (폰 동형 세그먼트 — 잔상 원리 — phone_music 폰 스킵 수형 승계)"
    SRV_PIDS=$(pgrep -x jkdesktop 2>/dev/null | tr '\n' ' ')
    echo "OBSERVE-SRVPIDS: ${SRV_PIDS:-none} (실행 위치의 서버 — probe가 기동한 것 아님)"
    if [ -n "$SRV_PIDS" ]; then
        PING=$(ctl '{"tool":"ping","args":{}}')
        echo "ping reply: $PING"
        assert_ok "$PING" "ping did not ok (기존 서버 응답 불능)"
        LIST=$(ctl '{"tool":"list_windows","args":{}}')
        echo "list_windows: ${LIST:-none} (읽기 — 창 목록 원문 1행)"
        assert_ok "$LIST" "list_windows did not ok"
        CALLS=2
    else
        echo "OBSERVE-SRVPIDS: 서버 부재 — agentctl 접점 0 (정직 — 접점 가능 축 부재 원장행)"
        PING=""; LIST=""; CALLS=0
    fi
    JROWS0=$(journal_rows)
    echo "JROWS0: $JROWS0 (프리 스냅샷 — OBSERVE는 콜을 더하지 않는다)"
fi

# ---------------------------------------------------------------- 5. 수확 원문
echo "=== 5. 수확 — ① use_journal.jsonl 파일 직독(tail 20) ==="
JROWS1=$(journal_rows)
DELTA=$((JROWS1 - ${JROWS0:-0}))
echo "JOURNAL-ROWS: $JROWS1 (프리 $JROWS0 → 포스트 $JROWS1 · delta=$DELTA · 콜 $CALLS)"
echo "JOURNAL-PATH: $JOURNAL ($(wc -c < "$JOURNAL" 2>/dev/null || echo 0) bytes)"
if [ -s "$JOURNAL" ]; then
    tail -20 "$JOURNAL" | sed 's/^/  JR: /'
else
    echo "JOURNAL-ABSENT: 파일 부재/빈 행 — 행 0 (정직 표기 원장행: $([ "$MODE" = "OBSERVE" ] && echo '폰 = 구 플래그 없는 바이너리 — 저널 leg T1 이전 배포 — 재배포(배포 드라이버 접촉 금지 원장) 후 재실측 몫' || echo 'OWN 모드인데 저널 없음 — T1 leg 결손 의심'))"
fi

echo "=== 6. 수확 — ② bridge_report_*.txt 목록 (부재 = 0 정직) ==="
BR_AFTER=$(report_count)
echo "BRIDGE-REPORTS: $BR_AFTER (프리 $BR_BEFORE)"
if [ "$BR_AFTER" -gt 0 ]; then
    while IFS= read -r r; do
        [ -n "$r" ] || continue
        echo "  BR: $(basename "$r") ($(wc -c < "$r") bytes)"
    done <<< "$(report_names)"
else
    echo "BRIDGE-REPORTS-ABSENT: bridge_report_*.txt 없음 — 0 (정직 — 폰/서버가 /report를 안 썼거나 미촉매)"
fi

echo "=== 7. 수확 — ③ 분류 요약 (ok=false err 행 — 최근 10건 카운트+원문) ==="
ERR_TOTAL=$(err_count | tr -d ' ')
echo "JOURNAL-ERR-ROWS: $ERR_TOTAL (전체 $JROWS1 행 중)"
if [ "$ERR_TOTAL" -gt 0 ]; then
    grep -a '"ok":false' "$JOURNAL" | tail -10 | sed 's/^/  ERR: /'
fi

echo "=== 8. 판정 ==="
if [ "$MODE" = "OWN" ]; then
    if [ "$JROWS1" -eq 0 ] || [ "$DELTA" -lt 4 ]; then
        echo "HARVEST-VERDICT: HARVEST-FAIL (저널 행 미성립 — rows=$JROWS1 delta=$DELTA 콜 $CALLS — 저널 leg 결손/미부팅)"
        exit 1
    fi
    echo "HARV-RUN-VALS: mode=OWN calls=$CALLS journal_rows=$JROWS1 delta=$DELTA err_rows=$ERR_TOTAL bridge_reports=$BR_AFTER state=$STATE"
else
    echo "HARV-RUN-VALS: mode=OBSERVE calls=$CALLS journal_rows=$JROWS1 err_rows=$ERR_TOTAL bridge_reports=$BR_AFTER state=$STATE (저널 행 0 = 구 바이너리 정직 표기 — 배포 후 재실측 몫)"
fi

if [ "$MODE" = "OWN" ]; then
    echo "=== 9. 정리 (pkill 에스컬레이션 — probe 소유 잔상 0 REMNANT 게이트) ==="
    pkill -f "$BUILD_DIR/[j]kdesktop" 2>/dev/null
    sleep 1
    pkill -9 -f "$BUILD_DIR/[j]kdesktop" 2>/dev/null
    sleep 1
    LEFT=$(ps -eo args 2>/dev/null | grep -i "$BUILD_DIR/jkdesktop" | grep -v grep | wc -l)
    [ "$LEFT" -eq 0 ] || { echo "REMNANT: $LEFT process(es) survived teardown"; echo "HARVEST-VERDICT: HARVEST-FAIL (probe 잔상 — REMNANT 게이트)"; exit 1; }
    echo "REMNANT: 0 (잔상 0 계약 성립)"
fi

echo "HARVEST-VERDICT: HARVEST-OK"
exit 0