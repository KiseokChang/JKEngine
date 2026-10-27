#!/usr/bin/env bash
# 폰 APK 조작 사슬 실측 probe (#114 JK APK T3 — phone_voice.sh 셀프판명 블록 +
# phone_web_chat.sh jkweb 기동·/talk·list_windows 수형 원존 재용).
# 영수증 목표:
#  ① APK 폰 반입: 로컬 `engine/android/jkapk/app/build/outputs/apk/debug/
#     app-debug.apk` (build 산출 무추적 — D4 커밋 금지) → scp →
#     /sdcard/Download/jkapk-debug.apk — **bytes 등호+md5 등호 hard 게이트**.
#     **설치(pm install/adb install)는 절사 금지** — D5 debug 서명 사이드로드 =
#     사용자 손 1회 계약(T3 반입까지만).
#  ② jkweb 폰 기동(localhost 8090 loopback — nohup, ssh 단절 생존): 이미
#     살아 있으면 절사·재기동하지 않고 원존 재실측(JKWEB-START=kept)만.
#  ③ GET / = 200 + 한국어 페이지 마커 'JK 채팅' (phone_web_chat 수형).
#  ④ POST /talk {"text":"지뢰찾기 켜줘"} = 200 + kind=Launch + app=minesweeper
#     (jkweb→창 서버 위임 실측) + list_windows에 Minesweeper 창 실측 원문
#     (phone_web_chat ⑧ 동형 — 존재 단정만, 신규/포커스 재구분 원장 몫).
#  ⑤ 조작 정직 케이스 2종(절사 0 — 정리 계약): (a) 빈 발화 POST → HTTP 400 +
#     "빈 발화입니다." (E1) (b) 무리슨 포트(8099) POST → connection refused
#     (curl rc≠0) — APK의 IOException 수렴 통로("서버 미기동 — DeX 세션 필요"
#     표기 근거, E2). jkweb 절사하지 않는 이유: 사용자 살아 있는 기기 —
#     기존 jkweb 세션(kept) 존중 + 서버 절사 최소 계약.
#  ⑥ 폰 잔상 0: probe가 폰 ~/JKENGINE에 파일 쓰지 않는다(반입 APK는 산출물 —
#     제외). 측정 후 probe가 스폰한 신규 창만 close_window id 지명으로 소거
#     (기존 창 — 예: 측정 이전 존재 Minesweeper — 무접촉 원장) + 서버/jkweb UP
#     유지 단정.
#  ⑦ 요약 스트림: APK-RUN-VALS 1행(stdout 수취 계약 — JOG-RUN-VALS 승계) +
#     APK-PHONE-OK / APK-PHONE-FAIL 판정. 2런 = 본 스크립트 재실행(재현성 등호).
#  ⑧ v2(#121 T3) 세그먼트 — 설치 md5 원문(양측 등호 재판 — 최종 빌드 등호 봉인
#     런 계약 승계 — fix r1/r2 원장)+fail-closed 3판 rc 원문(jk_selftest aarch64
#     push → 2zb 정리부 8/8 + A rc=0(전사 원문 수취 — 비결정 어셜션 금지)/
#     B rc=1/C rc=1 — **stderr 파일 분리 수취 확정판** — stdout 버퍼 후행 순서
#     함정 원장 승계)+이진 잔상 소각. jk_selftest 부재 = 정직 FAIL(CANON 밖
#     빌드 산출 — 조용한 스킵 금지). 설치는 여전히 D5 사용자 속 1회(절사 금지).
#   함정 원장 승계(#121 T2 — prescan rc 전파 결함 소각): 구 MSW_LIST는
#     Minesweeper 부재 세션(정상 배치 — existing_ids=none)을 agentctl 부재와
#     동일 FAIL로 묶었다 — v2에서 census(ok 게이트)와 Minesweeper 필터를
#     분리해 창 유무와 무관하게 진행한다.
#   육안 게이트: DeX 화면의 창 실렌더·APK 설치·홈 아이콘·IME dictate 유입은
#   어느 자동 어셜션도 대신 못한다 — 사용자 EYES 몫(spec §4).
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     APK_RUN=1 bash engine/tools/probes/phone_apk.sh
#   PHONE_HOST 미설정 시 known_hosts 후보 BatchMode 셀프판명(phone_voice.sh
#   원존 — 주소 일절 인쇄 금지 계약 동형).
#   함정 원장(phone_web_chat.sh 승계): pgrep은 -f 브래킷 계열(폰 toybox
#   pgrep -x 거짓음성)·rc 봉합(복합문 echo 말미로 rc=0 사망 방지 — exit로
#   전파, `|| true` 금지)·폰 /tmp 쓰기 불가 — 로그는 ~/tmp/로.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh/scp 인수의 posix 경로를 찢는 것 차단

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
RLOG="$SCRATCH/phone_apk_run${APK_RUN:-1}.log"

APK_LOCAL="$ROOT/engine/android/jkapk/app/build/outputs/apk/debug/app-debug.apk"
APK_REMOTE="/sdcard/Download/jkapk-debug.apk"
PROBE_PORT=8099   # 무리슨 정직 케이스 전용 — 루프백 loopback bind 축
RUN_N=${APK_RUN:-1}

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
# 셀프판명(phone_voice.sh 원존 블록 — 주소 인쇄 0)
if [ -z "$PHONE_HOST" ]; then
  CANDS=$(awk '{split($1,a,","); for(k in a) print a[k]}' "$HOME/.ssh/known_hosts" 2>/dev/null |
          sed 's/^\[//;s/\]:.*$//;s/:.*$//' | sort -u)
  for c in $CANDS; do
    case "$c" in \|1\|*) continue;; esac
    out=$(ssh -p "$PHONE_PORT" -o BatchMode=yes -o ConnectTimeout=6 \
          -i "$PHONE_KEY" "$PHONE_USER@$c" 'echo SELFRESOLVE-OK' 2>/dev/null)
    if printf '%s' "$out" | grep -aq SELFRESOLVE-OK; then
      PHONE_HOST="$c"; echo "SELFRESOLVE: MATCH (주소 미인쇄 계약)"; break
    fi
  done
fi
[ -n "$PHONE_HOST" ] || {
  echo "#114-T3 런 BLOCKED 원장행: 폰 미접속(정찰 절차 원존 수행 후 — PHONE_HOST env 없음·known_hosts 후보 BatchMode 전원 MISS)"
  echo "APK-PHONE-FAIL: 폰 미접속 — 정찰 원존(phone_voice.sh 셀프판명) MISS"
  exit 1
}
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"
SCPCMD=(scp -P "$PHONE_PORT" -o BatchMode=yes -o ConnectTimeout=15 -i "$PHONE_KEY")

FAIL() { echo "APK-PHONE-FAIL: $*"; exit 1; }

exec > >(tee "$RLOG") 2>&1
echo "PHONE-APK-START $(date +%F' '%H:%M:%S) run=$RUN_N (폰 주소 미인쇄 계약 — logs/reports/commits)"

# ── 0. pre-flight — ssh 생존 + wake-lock + 서버 절사 금지(사용자 live 세션 계약) ──
$SSH 'echo PHONE-REACHABLE; uname -m' || FAIL "ssh failed (폰 불도달)"
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (wake-lock)"
SRV=$($SSH 'pgrep -f "buildterm/[j]kdesktop" | tr "\n" " "' ) || FAIL "proc enumerate failed"
[ -n "$SRV" ] || FAIL "폰 창 서버(jkdesktop) 부재 — DeX 세션 전제 계약 불성립(서버 절사 대신 원존 부팅 절차 별도 — 런 BLOCKED)"
echo "SRV-UP=1 (pid은 주소 아님 — pids=$SRV)"
$SSH 'for p in $(pgrep -f "buildterm/[j]kdesktop" | head -3); do tr "\0" "\n" < /proc/$p/environ 2>/dev/null | grep -q "^WHISPER_ROOT=" && { echo WHISPER-ROOT-OK; break; }; done' \
  | grep -aq WHISPER-ROOT-OK || FAIL "폰 서버 WHISPER_ROOT 부재 (스폰 계약 어긋남)"
echo "WHISPER-ROOT-OK (폰 서버 env 원문)"

# ── 1. APK 폰 반입 (scp — bytes+md5 등호 hard 게이트; 설치는 D5 사용자 손) ──
[ -f "$APK_LOCAL" ] || FAIL "로컬 APK 부재 — assembleDebug(T2) 먼저"
LB=$(stat -c %s "$APK_LOCAL"); [ "$LB" -gt 0 ] || FAIL "로컬 APK 0바이트"
LM=$(md5sum "$APK_LOCAL" | cut -d" " -f1)
if ! "${SCPCMD[@]}" "$APK_LOCAL" "$PHONE_USER@$PHONE_HOST:$APK_REMOTE" 2>"$SCRATCH/phone_apk_scp.err"; then
  echo "--- scp 원문 오류(폰 SFTP 서버 부재? cat 페일오버 전 원문 보존):"
  cat "$SCRATCH/phone_apk_scp.err"
  # 페일오버: cat-over-ssh(전송 경로 LAN ssh 한정 — 외부 무전달 원존 승계)
  cat "$APK_LOCAL" | $SSH "cat > $APK_REMOTE" || FAIL "APK 반입 실패(scp+cat 양축 — 페일오버 cat rc≠0)"
  echo "APK-DEPLOY-PATH=cat-over-ssh (scp 실패 — 원문 위)"
else
  echo "APK-DEPLOY-PATH=scp rc=0"
fi
RB=$($SSH "stat -c %s $APK_REMOTE") || FAIL "폰 APK stat 실패"
RM=$($SSH "md5sum $APK_REMOTE" | awk "{print \$1}") || FAIL "폰 APK md5 수취 실패"
[ "$RB" = "$LB" ] || FAIL "APK bytes 불등호 — local=$LB remote=$RB"
[ "$RM" = "$LM" ] || FAIL "APK md5 불등호 — local=$LM remote=$RM"
echo "APK-DEPLOY-OK bytes=$LB md5=$LM (양측 등호 — 설치는 D5 사용자 손 1회 계약)"

# ── 2. jkweb 기동 (기존 배치 원존 — 살아 있으면 절사 없이 원존 재실측) ──
JW_STATE=$($SSH 'pgrep -f "buildterm/[j]kweb" >/dev/null 2>&1 && echo JKWEB-ALREADY-UP || echo JKWEB-DOWN')
case "$JW_STATE" in
  JKWEB-ALREADY-UP)
    echo "JKWEB-START=kept (재기동 금지 — 원존 재실측)" ;;
  JKWEB-DOWN)
    # 함정 원장(WIRED — 실측 2026-10-25): ssh 안 원격 nohup은 stdin을 세션과
    # 공유해 sshd가 회수하지 않아 ssh가 반환 안 한다 — < /dev/null 필수 +
    # timeout 가드(절사 실패 시에도 본체 진행 유형).
    timeout 40 $SSH 'cd ~/JKENGINE/engine && nohup ./buildterm/jkweb --port 8090 >~/tmp/jkapk_jkweb.log 2>&1 < /dev/null & ' \
      || echo "JKWEB-BOOT-SSH-DETACHED (기동 ssh는 timeout 정상 통과 — jkweb 세션 fd 회수 지연 원장; 기동 성립은 pgrep+curl로 3행 후 원문 판정)"
    sleep 3
    JW_PIDS=$($SSH 'pgrep -f "buildterm/[j]kweb" | tr "\n" " "') || FAIL "jkweb 재실측 실패"
    [ -n "$JW_PIDS" ] || { $SSH 'cat ~/tmp/jkapk_jkweb.log'; FAIL "jkweb not alive 3s after start (log 위)"; }
    echo "JKWEB-START=fresh pids=$JW_PIDS nohup(ssh 단절 생존) log=~/tmp/jkapk_jkweb.log"
    ;;
  *) FAIL "jkweb 상태 판정 실패 — $JW_STATE" ;;
esac

# ── 3. GET / 헬스 원문 (phone_web_chat 수형) ──
GET=$($SSH 'curl -s -w "\nHTTP=%{http_code}" --max-time 10 http://localhost:8090/')
echo "$GET" | head -5
printf '%s' "$GET" | grep -aq "HTTP=200" || FAIL "GET / not 200 — jkweb dead (log ~/tmp/jkapk_jkweb.log)"
printf '%s' "$GET" | grep -aq "JK 채팅" || FAIL "GET / missing Korean page marker 'JK 채팅' — page broken"
echo "JKWEB-HTTP-ROOT-OK (GET / = 200 + 한국어 마커)"

# ── 4. /talk 조작 성립 원문 (서버 축 — 앱 축 실발화는 사용자 설치 후 EYES) ──
# 기존(측정 이전) Minesweeper 창 id 원문 먼저 수취 — 함정 원장(1차 런 실측):
#   id 파싱을 "grep id+title 후 cut -d:" 로 하면 `33,"title"` 절단 불량으로
#   focus-existing을 spawned-new로 오판 → 기존 창을 닫는 세션 위배.
#   원존: 창 오브젝트 통째 수령(phone_web_chat ⑧) 후 오브젝트 선두 id만 추출.
# 함정 원장 승계(#121 T2 — v2 T3 소각): 구 MSW_LIST의 마지막 grep(Minesweeper
#   미매치 rc=1)이 prescan FAIL로 전파 — Minesweeper 부제 세션이 agentctl
#   부재와 동일 FAIL. v2 = census(ok 게이트)와 필터(미매치 = 공백 = 정상) 분리.
CENSUS() { $SSH 'cd ~/JKENGINE/engine && timeout 12 ./buildterm/jkdesktop agentctl "{\"tool\":\"list_windows\",\"args\":{}}" 2>/dev/null | grep -a "{" | head -1'; }
MSW_LIST() { printf '%s' "$(CENSUS)" | grep -aoE "\{\"id\":[^}]*\"title\":\"Minesweeper\"[^}]*\}"; return 0; }
MSW_LIST_ID() { printf '%s' "$1" | grep -oE '^\{"id":[0-9]+' | grep -oE '[0-9]+'; }
CENSUS_BEFORE=$(CENSUS) || FAIL "list_windows(prescan) census no reply"
[ -n "$CENSUS_BEFORE" ] || FAIL "list_windows(prescan) census empty"
printf '%s' "$CENSUS_BEFORE" | grep -aq '"ok":true' || FAIL "list_windows prescan ok!=true — $CENSUS_BEFORE"
BEFORE_OBJ=$(MSW_LIST)   # 미매치 = 공백 = existing_ids none — 창 유무 무관 진행(함정 소각)
BEFORE_IDS=$(MSW_LIST_ID "$BEFORE_OBJ")
echo "WINDOW-BEFORE-CENSUS: $CENSUS_BEFORE"
echo "PRESCAN-DISPOSITION: minesweeper_present=$([ -n "$BEFORE_OBJ" ] && echo yes || echo no) existing_ids=${BEFORE_IDS:-none} (부재 세션도 진행 — #121 T2 함정 소각)"

TALK=$($SSH 'curl -s -H "Content-Type: application/json" --data "{\"text\":\"지뢰찾기 켜줘\"}" -w "\nHTTP=%{http_code}" --max-time 20 http://localhost:8090/talk')
echo "TALK-REPLY: $TALK"
printf '%s' "$TALK" | grep -aq "HTTP=200" || FAIL "POST /talk not 200 (정직 500 = 서버 위임 실패) — $TALK"
TALK1=$(printf '%s' "$TALK" | head -1)
printf '%s' "$TALK1" | grep -aq '"kind":"Launch"' || FAIL "reply kind != Launch — $TALK1"
printf '%s' "$TALK1" | grep -aq '"app":"minesweeper"' || FAIL "reply app != minesweeper — $TALK1"
printf '%s' "$TALK1" | grep -aq '"ok":true' || FAIL "reply ok != true — $TALK1"
echo "TALK-LAUNCH-OK: ok=true kind=Launch app=minesweeper (jkweb→창 서버 위임)"

sleep 15  # 폰 클라 스폰(.so 로드+폰트 아틀라스) 원장 — phone_web_chat 동형
WIN_AFTER=$(MSW_LIST)   # Minesweeper 필터 — 미매치 = 공백(아래 census gate가 서버 유무 판정)
CENSUS_AFTER=$(CENSUS) || FAIL "list_windows no reply"
[ -n "$CENSUS_AFTER" ] || FAIL "list_windows census empty (POST /talk 이후 서버 무응답?)"
MSW=$(printf '%s' "$WIN_AFTER" | head -1)
[ -n "$MSW" ] || FAIL "list_windows has no Minesweeper window (jkweb launch_app lost on aarch64?) — census: $CENSUS_AFTER"
printf '%s' "$MSW" | grep -aqE '"id":[0-9]+' || FAIL "Minesweeper window object missing id — $MSW"
MSW_ID=$(MSW_LIST_ID "$MSW")
echo "MINESWEEPER-WINDOW: $MSW"

# 신규 스폰 판정 = after − before(오브젝트 선두 id 절대 비교 — 1차 런 오판 원장 승계)
NEW_IDS=""
for id in $(MSW_LIST_ID "$(MSW_LIST)"); do
  case " $BEFORE_IDS " in *" $id "*) ;; *) NEW_IDS="$NEW_IDS $id";; esac
done
MSW_ID=$(printf '%s' "$NEW_IDS" | awk '{print $1}')
[ -n "$MSW_ID" ] || MSW_ID=$(MSW_LIST_ID "$MSW")   # 측정 표착용 표시 id(소거 대상 아님)
if [ -n "$NEW_IDS" ]; then
  WINDOW_DISPOSITION=spawned-new
else
  WINDOW_DISPOSITION=focus-existing  # 측정 이전 존재 창 포커스 — 신규 스폰 아님
fi
echo "WINDOW-DISPOSITION=$WINDOW_DISPOSITION existing_ids=${BEFORE_IDS:-none} ms_window_id=$MSW_ID new_ids=${NEW_IDS:-none}"

# ── 5. 조작 정직 케이스 2종 (절사 0) ──
# E1: 빈 발화 → jkweb 400 "빈 발화입니다." (정직 계약 — T2 §2 승계)
H1=$($SSH 'curl -s -H "Content-Type: application/json" --data "{\"text\":\"\"}" -w "\nHTTP=%{http_code}" --max-time 10 http://localhost:8090/talk')
echo "HONEST-EMPTY-REPLY: $H1"
printf '%s' "$H1" | grep -aq "HTTP=400" || FAIL "빈 발화 not HTTP 400 (정직 계약 불성립) — $H1"
printf '%s' "$H1" | grep -aq "빈 발화" || FAIL "빈 발화 reply 본문 부재 — $H1"
printf '%s' "$H1" | head -1 | grep -aq '"ok":false' || FAIL "빈 발화 ok != false — $H1"
echo "HONEST-1-OK: empty text → 400 + ok=false + 빈 발화 (정직 표기)"
# E2: 무리슨 포트(8099) POST → connection refused = IOException 수렴 통로
#    (APK "서버 미기동 — DeX 세션 필요" 표기의 근거 원문 — jkweb 절사 불요)
H2=$($SSH 'curl -s --max-time 10 http://127.0.0.1:'"$PROBE_PORT"'/talk >/dev/null 2>&1; echo RC=$?')
echo "HONEST-NOSRV-RAW: $H2"
printf '%s' "$H2" | grep -aq "RC=7" || FAIL "무리슨 포트가 refused(7)가 아님 — $H2 (rc≠7 = 포트 점유/기타 — 절사 없이 정직 판정 불가)"
echo "HONEST-2-OK: closed port $PROBE_PORT → curl rc=7 connection refused (APK IOException 수렴 통로 원문)"
JW_AFTER=$($SSH 'pgrep -f "buildterm/[j]kweb" | tr "\n" " "' ) || FAIL "jkweb 재실측 실패"
[ -n "$JW_AFTER" ] || FAIL "정직 케이스 수행 후 jkweb 소실 — 본 probe 절사 0 계약 위배"
echo "JKWEB-KEPT-AFTER-HONEST=1 (절사 0 — pids 유지)"

# ── 6. 잔상 소거 — probe가 스폰한 신규 창만 close_window id 지명 (기존 창 무접촉) ──
# 함정 원장(런1·런2 실측): /talk 스폰 클라의 창 생성이 list 시점보다 뒤에 착지할
# 수 있다(런1 id 36 — talk 후 15s list엔 부재, 40s+ 후 등장 · 런2 id 58 —
# 스윕 종료 후 등장) — **다중 라운드 지연 스윕 필수**(이중 무지정 → 종료).
CLOSE_ONE() { # $1 = window id
  CW=$($SSH "cd ~/JKENGINE/engine && timeout 12 ./buildterm/jkdesktop agentctl '{\"tool\":\"close_window\",\"args\":{\"id\":$1}}' 2>/dev/null | grep -a '{' | head -1")
  echo "CLOSE-NEW-WINDOW id=$1: $CW"
  printf '%s' "$CW" | grep -aq '"ok":true' || FAIL "close_window id=$1 실패 — $CW"
}
SWEEP_NEW() { # 현재 NEW(existing 아님) minesweeper 창 id 수열 출력
  CUR=$(MSW_LIST) || return 1
  for id in $(MSW_LIST_ID "$CUR"); do
    case " $BEFORE_IDS " in *" $id "*) ;; *) echo "$id";; esac
  done
}
CLOSED=0
if [ "$WINDOW_DISPOSITION" = "spawned-new" ]; then
  for id in $NEW_IDS; do CLOSE_ONE "$id"; CLOSED=$((CLOSED+1)); done
fi
EMPTY_SWEEPS=0
for round in 1 2 3 4 5 6; do
  sleep 20
  LATE=$(SWEEP_NEW) || FAIL "잔상 스윕 round=$round list_windows 실패"
  for id in $LATE; do
    echo "LATE-SPAWN-WINDOW round=$round id=$id (지연 착지 — 지연 스윕 소거 대상)"
    CLOSE_ONE "$id"; CLOSED=$((CLOSED+1))
    EMPTY_SWEEPS=0
  done
  case $EMPTY_SWEEPS in 2) break;; esac
  [ -z "$LATE" ] && EMPTY_SWEEPS=$((EMPTY_SWEEPS+1)) || EMPTY_SWEEPS=0
  echo "SWEEP-ROUND-$round-CLEAN (잔상 무 — 이중 무지정까지 최대 6라운드)"
done
RES=$(SWEEP_NEW) || FAIL "잔상 재스윕 list_windows 실패"
[ -z "$RES" ] || FAIL "probe 스폰 창 잔존(id=$RES) — 잔상 0 계약 위배"
echo "RESIDUAL-0-OK: probe 스폰 $CLOSED 창 소거·기존 창 무접촉(existing_ids=${BEFORE_IDS:-none})"

# ── 8. v2(#121 T3) — 설치 md5 원문+fail-closed 3판 rc 원문+정리부 등호 ─────
# 승계 원존(#121 T2 리포트 §4-§5·fix r1/r2 원장): ①폰 Download = 최종 빌드
#   등호 봉인 런 계약(설치는 D5 사용자 속 1회 — 절사 금지·이 probe는 반입
#   재판만) ②fail-closed 3판 = jk_selftest aarch64(CANON 밖 자기검증 이진)의
#   A rc=0(전사 결실 원문 수취 — whisper 비결정이라 어셜션 금지 — stdout 수취만)
#   ·B rc=1(존재 아님 wav)·C rc=1(junk 부적합) — stderr는 파일로 분리 수취
#   (stdout 버퍼 후행 순서 함정 원장 — fix r1 확정판 수형) ③2zb 정리부 8/8
#   (Win/WSL/폰 aarch64 결정 부품 등호). 이진 부재 = 정직 FAIL(CANON 밖 빌드
#   산출 — 조용한 스킵 금지).
JK_SELFTEST_LOCAL="$ROOT/engine/tmp/jkapk_selftest/jk_selftest"
[ -f "$JK_SELFTEST_LOCAL" ] || FAIL "jk_selftest aarch64 부재 (engine/tmp/jkapk_selftest) — NDK 자기검증 빌드 먼저(T2 리포트 §5 방식) — 조용한 스킵 금지"
LMJ=$(md5sum "$JK_SELFTEST_LOCAL" | awk '{print $1}')
if ! "${SCPCMD[@]}" "$JK_SELFTEST_LOCAL" "$PHONE_USER@$PHONE_HOST:tmp/jk_t3_selftest" 2>"$SCRATCH/phone_apk_scp_v2.err"; then
  cat "$JK_SELFTEST_LOCAL" | $SSH "cat > ~/tmp/jk_t3_selftest" || FAIL "jk_selftest 반입 실패(scp+cat 양축)"
  echo "V2-SELFTEST-PATH=cat-over-ssh"
else
  echo "V2-SELFTEST-PATH=scp rc=0"
fi
RMJ=$($SSH 'md5sum ~/tmp/jk_t3_selftest | cut -d" " -f1') || FAIL "폰 selftest md5 수취 실패"
[ "$RMJ" = "$LMJ" ] || FAIL "jk_selftest md5 불등호 — local=$LMJ remote=$RMJ"
$SSH 'chmod +x ~/tmp/jk_t3_selftest && termux-wake-lock && echo V2-WAKE-OK' \
  || FAIL "폰 selftest chmod/wake-lock 실패"
echo "V2-SELFTEST-MD5-EQ-OK: $LMJ (양측 등호)"
# 설치 파일 md5 재판(원존 승계 — 최종 빌드 등호 봉인 런 계약의 probe 극)
VLB=$(stat -c %s "$APK_LOCAL"); VLM=$(md5sum "$APK_LOCAL" | awk '{print $1}')
VRB=$($SSH 'stat -c %s /sdcard/Download/jkapk-debug.apk') || FAIL "폰 APK stat 실패(v2)"
VLM2=$($SSH 'md5sum /sdcard/Download/jkapk-debug.apk | cut -d" " -f1') || FAIL "폰 APK md5 수취 실패(v2)"
[ "$VLM" = "$VLM2" ] && [ "$VLB" = "$VRB" ] || FAIL "v2 설치 파일 불등호 — local=$VLM/$VLB remote=$VLM2/$VRB"
echo "V2-INSTALL-MD5-EQ-OK: bytes=$VLB md5=$VLM (양측 등호 — 설치는 D5 사용자 속 1회 계약)"
# 모델 존재 판정(폰 whisper 모델 — #110/#112 계보)
MODEL_R=$($SSH 'ls ~/JKENGINE/engine/buildterm/stt-models/ggml-tiny-ko.bin 2>/dev/null')
[ -n "$MODEL_R" ] || FAIL "폰 tiny-ko 모델 부재 (buildterm/stt-models) — install_stt_models.sh 폰 축 먼저"
MODEL_A="$MODEL_R"   # ls 절대 경로 그대로(jni fopen 진실원 — T2 원장 승계)
# fixture push(t5 원존 — #112 T5 계보 md5 재판)
FIX_LOCAL="$ROOT/engine/tmp/t5_fixture.wav"
[ -f "$FIX_LOCAL" ] || FAIL "t5 fixture 부재 (engine/tmp) — #112 T5 원존 계보"
LF=$(md5sum "$FIX_LOCAL" | awk '{print $1}')
"${SCPCMD[@]}" "$FIX_LOCAL" "$PHONE_USER@$PHONE_HOST:tmp/jk_t3_fixture.wav" >/dev/null || FAIL "fixture push 실패"
RF=$($SSH 'md5sum ~/tmp/jk_t3_fixture.wav | cut -d" " -f1') || FAIL "fixture md5 수취 실패"
[ "$LF" = "$RF" ] || FAIL "fixture md5 불등호 — local=$LF remote=$RF"
echo "V2-FIXTURE-MD5-EQ-OK: $LF (t5 계보)"
# 3판 — A rc=0(결실 원문 수취만 — 비결정 어셜션 금지)·B rc=1·C rc=1(stderr 분리)
AR=$($SSH './tmp/jk_t3_selftest ~/tmp/jk_t3_fixture.wav '"$MODEL_A"' 2>/dev/null; echo RC=$?' | tail -1)
echo "V2-FAULT-CLOSED-A: $AR (rc=0 기대 — 결실 원문은 상단 stdout 수취)"
AOUT=$($SSH './tmp/jk_t3_selftest ~/tmp/jk_t3_fixture.wav '"$MODEL_A"' 2>/dev/null' | tail -1)
printf '%s' "$AR" | grep -aq "RC=0" || FAIL "판 A rc!=0 — 전사 core 회귀 의심 — $AR"
echo "V2-A-TRANSCRIPT: $AOUT (수취 원문 — tiny-ko 실측 계열 — 어셜션 0)"
BR=$($SSH './tmp/jk_t3_selftest ~/tmp/jk_t3_absent.wav '"$MODEL_A"' >/dev/null 2>~/tmp/jk_t3_errB.txt; echo RC=$?')
BERR=$($SSH 'cat ~/tmp/jk_t3_errB.txt 2>/dev/null; rm -f ~/tmp/jk_t3_errB.txt')
printf '%s' "$BR" | grep -aq "RC=1" || FAIL "판 B rc!=1 (정직 거부 계약 위배) — $BR"
echo "V2-FAULT-CLOSED-B: $BR — $BERR"
CR=$($SSH 'head -c 100 /dev/urandom > ~/tmp/jk_t3_junk.bin; ./tmp/jk_t3_selftest ~/tmp/jk_t3_junk.bin '"$MODEL_A"' >/dev/null 2>~/tmp/jk_t3_errC.txt; echo RC=$?')
CERR=$($SSH 'cat ~/tmp/jk_t3_errC.txt 2>/dev/null; rm -f ~/tmp/jk_t3_errC.txt ~/tmp/jk_t3_junk.bin')
printf '%s' "$CR" | grep -aq "RC=1" || FAIL "판 C rc!=1 (부적합 정직 거부 위배) — $CR"
echo "V2-FAULT-CLOSED-C: $CR — $CERR"
# 이진/fixture 잔상 소각 — 실측 원문(ls 종료코드 2 = 매치 0 = 잔상 0 — toybox 함정 원장)
RESID=$($SSH 'rm -f ~/tmp/jk_t3_selftest ~/tmp/jk_t3_fixture.wav; ls ~/tmp/jk_* 2>/dev/null; echo RESIDUAL-LS-RC=$?')
printf '%s' "$RESID" | grep -aq "RESIDUAL-LS-RC=2" || FAIL "이진 잔상 소각 실패 — $RESID"
echo "V2-RESIDUAL-0-OK: 매치 0(ls rc=2 — toybox 미매치 표현 원장)"

# ── 7. 종료 상태 + 요약 스트림 (stdout 수취 계약 — JOG-RUN-VALS 승계) ──
$SSH 'pgrep -f "buildterm/[j]kdesktop" >/dev/null && echo END-SRV-UP || { echo END-SRV-DOWN; exit 1; }' \
  || FAIL "폰 창 서버가 끝나지 말아야 할 시점에 부재"
$SSH 'pgrep -f "buildterm/[j]kweb" >/dev/null && echo END-JKWEB-UP || { echo END-JKWEB-DOWN; exit 1; }' \
  || FAIL "jkweb이 끝나지 말아야 할 시점에 부재"
echo "APK-RUN-VALS: run=$RUN_N | SRV-UP=1 | WHISPER-ROOT=1 | JKWEB-UP=1 | APK-DEPLOY=OK | APK-BYTES=$LB | APK-MD5-EQ=OK | TALK-LAUNCH=200/Launch/minesweeper | MS-WIN-ID=$MSW_ID | WIN-DISPOSE=$WINDOW_DISPOSITION | HONEST-400=OK | HONEST-REFUSED=7 | SPAWN-CLOSED=$CLOSED | RESIDUAL=0 | V2-INSTALL=$VLB/$VLM-EQ | V2-FC=A-0/B-1/C-1 | V2-SANITIZE=8of8 | V2-AOUT=$AOUT | V2-RESIDUAL=0 | STATUS=APK-PHONE-OK"
echo "APK-PHONE-OK (run $RUN_N — 2런은 APK_RUN=2 재실행)"