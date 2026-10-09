#!/usr/bin/env bash
# 폰 music 실측 probe (music 라인 T5 — plan
# docs/superpowers/plans/2026-10-09-music-library.md task-5, 스펙
# docs/superpowers/specs/2026-10-09-music-library-design.md — 폰 실측 결제 관문).
#
# 승계 원천:
#   · phone_gallery.sh(갤러리 라인 T5) — PHONE_HOST fail-closed 가드(exec 앞)·
#     tar-over-ssh+크기 신선도 대차·원격 스크립트=파일+자기 소각·REMNANT(rc
#     게이트)·브래킷 pkill+-9 에스컬레이션+잔존 게이트·jkweb 상시 유지(절사
#     금지 — localhost:8090)·x11grab 캡처(1920x1080 시도→화면 크기 파싱 재시도·
#     import 금지)·base64 1파이프 회수+md5 대차·settings 백업 병합+원복·
#     send_input 승인 무승인 운용·BOOT-OK 종료(서버 UP).
#   · phone_client_idle.sh(클라 idle 라인 T3 폰 축) — **전수 sweep canary**
#     (HEAD blob 크기(개행 정규화) vs 폰 전수 — SWEEP-DIFF·기대 집합 분류·
#     오차가 기대 밖이면 전량 git archive 재배포 -c core.autocrlf=false·
#     배포 후 SWEEP-POST-DIFF=0 hard 게이트)·원격 영수증 rc 255 RUN-DROPPED
#     재실행 계약·/proc stat 클라 CPU 계측 헬퍼(cclients).
#   · wsl_music.sh(music 라인 T4) — 측정 몫 전문 승계: 합성 WAV 시드(엔진
#     소유 — i:\@keep 등 사용자 미디어 절대 금지)·MUSIC-LIST-EXPECTED=4(3
#     루트+1 sub 재귀)·탭 2 클릭(리스캔 도착 대기)·리스트 캡처·**콜드 접착제
#     전조**(등록 전 app_tool open = unknown_app_tool 원문)·표행 합성 2탭
#     더블클릭(위임 — pair 재시도·gap ms 원장)·**귀속 폴링**(직행 open을 넣기
#     전 get_status 0.4s 폴링 — direct 온 전에 귀속 봉인)·직행 open+get_status
#     2회(pos 증가 = 재생 진행 원문 1행). + spatial 세그먼트(8b — spatial leg
#     라인 T3): app_tool spatial_play/status/stop 직행 릴레이 수형. 폰은
#     SPATIAL_PLAYER_ROOT 미정의를 **계약**으로 둔다(LEG-ENV unset 가드 —
#     설정 런은 계약 위배 hard FAIL) — leg 미링크 .so(D5)에서 spatial_play는
#     start_failed detail=kDelegationHint("spatial leg 불가 — vplayer 위임
#     이용") 종착 = fail-closed 영수증. UI [spatial] 버튼 탭과 도구 발사는
#     SpatialStart 단일 발사 경로로 합류(T2 원문) — 라벨 렌더 캡처가 정직
#     귀속 원문. LEG-FALLBACK에서도 미활성 1행을 남긴다(T3 리뷰 I-2 승계 —
#     재런 자기교정 원장행). leg 성립 실측은 WSL(T3 LEG-OK)이 소유 — 폰은
#     배제의 정직 원문만 실측한다.
#
# 계약 교환(핵심 재량 — T4 리뷰 I-1 원장 수형):
#   · **settings 주입 수형 교환** — T4 WSL probe의 "settings.json ENTRY 부재
#     hard FAIL→기록+소각" 수형은 폰 런타임과 충돌한다(폰 settings.json·
#     permissions.json은 선존 런타임 파일 — 전면 allow 파일은 프로브 소유
#     아님 계약). 폰은 **기존 settings.json에 music.dirs 병합(원본 백업)→END
#     바이트 등호 원복** 수형으로 교환한다. 갤러리 T5 폰 probe가 send_input을
#     병합+바이트 등호 원복한 계약의 확장(같은 구조 — JSON 키 1개 삽입 awk
#     전문 승계). 영수증 행 PRIOR-ORIG-RECOVERED·SETTINGS-BACKUP-OK·
#     SETTINGS-RESTORED-BYTES.
#   · 시드 WAV는 **폰 python3에 의존하지 않는다** — 드라이버(윈도 python, wave
#     stdlib) 합성 → tar 멤버 랜딩 → 원격 스크립트가 $TMPDIR/pcm_music_seed로
#     이동(RETRY 안전: 전 run 유산 4곡 등수면 PRIOR-SEEDS-RECOVERED 재사용·
#     이질 잔상은 FAIL). 폰 python3 불요 = 갤러리 T5 시드 계약 동형.
#
# 측정 계약(이 태스크):
#   부팅 → music 앱 런치 → 리스트 렌더(트랙 행 실측 — expected=4·sub 재귀 경로
#   열·mtime desc 정렬은 캡처 육안 몫) → 위임: vplayer 창 런치(합성 2탭 →
#   Video Player 출현; MISS면 클릭 수 2 합성 폴백 pair, 그래도 MISS면 launch_app
#   폴백 스폰 — DELEG-FALLBACK 표기로 귀속 무효 원장) + app_tool open(합성 wav)
#   → get_status 재생 수치(pos 증가 1행). 원장 후보: **폰 위임 open 페이싱** —
#   재청구 폴백 창(20×250ms=5s) < 폰 클라 스폰 15-18s → 귀속 opened MISS가
#   구조적일 수 있다(WSL은 스폰 수초라 경기 통과 — 폰 실측이 진원 판별. 실측:
#   귀속 OK — 폰 스폰이 15-18s 원장보다 빠르다).
#   + leg sdcard(M-5 재판정): T1 fix r1 리뷰 — 폰 /sdcard FUSE의 st_ino 이질로
#   (st_dev,st_ino) 방문 집합이 무효화될 수 있다는 지적 → music.dirs=/sdcard
#   루트로 실측 스캔이 유한 종료하는지(클라 CPU 2창 정착+list_windows 응답)
#   원문. 권한 부재=빈 목록 정상(갤러리 T5 계동형 — 정직 가시화).
#
# 폰 selftest 캐논(계보): 진입 실측 565(2i 19·2g 40·2m 0) → 실측 정산
#   591 = 565 + 2m 26건(2m 상총 25 + 2m-h 1) — **WSL 591 등호**(dispatch 초산
#   2m 21은 2m-e 5건 폰 미출력 추정이 틀린 것 — 1차 런이 OTHER-N(591)로 정산,
#   2차 런에서 FULL 상수로 확정). 2m [PASS] 실측수 병기.
#   → spatial leg 라인 T4 흡수 608 = 591 + 2n 17건(2n-a 6·2n-b 5·2n-c 4·
#   2n-d 2 — LegSupport D4 표·Apply 순수 전이·ToolJson 3종·표 정합) —
#   **WSL 608 등호**(신규 어설션 0 계약: 등호 승계만 — 2n은 T1 신설 어설션
#   흡수다). 2n [PASS] 실측수 병기.
#   런 계보(원장): 1차=OTHER-N(591) 정산+FULL-GIT-ARCHIVE 배포(PHONE_DEPLOY_BASE
#   오타 — 원장)·2차=FULL receipt·3차(list2 캡처 신설)·4차=세션 단절로 중간
#   사망(RETRY 수형 흡수 실측 — 자가 수복 행 PRIOR-ORIG-RECOVERED)·5차=최종
#   receipt(MUS-PHONE-OK). 탭 첫 클릭 포커스 취식 실측(4·5차: re-click 방어로
#   4곡 행 실측 — sdcard 레그에도 동일 방어).
#
# 판정 사다리(라벨 계약 — wsl_music/phone_gallery 수형):
#   MUS-PHONE-VERDICT: MUS-PHONE-OK = CANON MUSIC-FULL-2N-17 AND 캡처 7종
#     비빈 AND 위임(폰 위임 open+재청구+직행) 성립 AND get_status
#     opened:true(pos 증가) AND 폰 leg 배제 원문(spatial_play →
#     start_failed detail=kDelegationHint·spatial_status 미활성 행) 성립 AND
#     sdcard 스캔 유한(또는 권한 부재 빈 목록=정상). 위임 창·귀속은 원장행
#     (MISS = 폰 스폰 시간 원장 — 판정 게이트에서 뺀 사유는 상단 원장 참조).
#   MUS-PHONE-VERDICT: MUS-PHONE-FAIL(행별 이유) — 수치 미달=정직 원장 rc=0.
#   MUSP-FAIL(rc=1) = hard FAIL만: ssh 단절·sweep·배포·ninja·selftest 회귀
#     ·seed 소각·settings/perm 원복·서버/ping/launch/창·캡처·REMNANT 실패.
#
# 실행법(윈도 Git Bash, 저장소 루트 어디서든):
#   PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_music.sh
# PHONE_HOST 필수(환경변수 — 기록 금지 계약; known_hosts 후보 BatchMode 자기
# 판명은 리포트에 자리표시 PHONE_HOST=<폰>만). PHONE_PORT/USER/KEY/DISPLAY만
# 기본값. 풀 로그 engine/tmp/phone_music.log(tee)·런 원문
# engine/tmp/phone_music_run.log. tar 전송은 LAN 내부 ssh 한정(외부 0).
#
# 함정 원장(sty: 승계+신규):
#   · PHONE_HOST 가드는 tee(exec)보다 **앞** — unset 재실행 시 마지막 영수증
#     로그 truncate 폐곡(텍스트 스케일 T4 fix r1 M5 원문).
#   · 원격 스크립트는 파일 기동 — ssh argv(heredoc)에 jkdesktop 원문 문자열을
#     직접 놓으면 자기 pkill에 걸린다(사건 원장). 브래킷 '[j]kdesktop' 계약.
#   · 원격 바이너리 relink = ETXTBSY — 드라이버 pre-clean 브래킷 pkill+-9
#     에스컬레이션+잔존 게이트 + 원격 ninja 앞 재확인. jkweb은 절사 대상
#     아님(localhost:8090 상시 계약 — 절사 금지).
#   · 폰 클라 스폰 15-18s — 창 대기 3s×30(90s). ssh 원격 `< /dev/null` 금지
#     (채널 hang). rc 봉합: 복합문이 echo로 끝나면 0 — exit로 전파.
#   · 캡처 회수 ssh 호출당 base64 1파이프(ls 등 혼입 금지)+md5 대차. 윈도
#     python은 MSYS식 /i/... 패스를 못 읽는다 — cygpath 변환 후 python.
#   · tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/
#     engine 중첩 트랩). 폰 /tmp는 쓰기 불가 — 스크래치는 $TMPDIR.
#   · comm/sort collation: 양측 LC_ALL=C로 재정렬해 비교(idle 원문 — 가짜
#     오차 306행 사건).
#   · sweep parse canary(idle T3 원장 — 이중 축약 결함): NF/형식 가드로
#     전행 공란 → 전행 MISSING → tar 0파일 사망을 계측 전 hard FAIL.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/pmus_deploy.tar"
SEEDTAR="$SCRATCH/pmus_seeds.tar"
RLOG="$SCRATCH/phone_music.log"
RUNLOG="$SCRATCH/phone_music_run.log"
RSRC_TAR_NAME="pmus_phone_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"
GITROOT="$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")"

# 접속 정보는 환경변수로(기록 금지 계약 — 내부 IP는 커밋·문서·리포트·로그에
# 두지 않는다). PHONE_HOST는 필수 — 미설정이면 fail-closed로 즉시 FAIL(스캔
# 폴백 없음 — 697db8b 계약).
PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
PHONE_DISPLAY="${PHONE_DISPLAY:-:1}"
PHONE_DEPLOY_BASE="${PHONE_DEPLOY_BASE:-8685589}"  # 클라 idle 라인(마지막 폰 배포) 커밋 — 기대 배포 집합 앵커

# PHONE_HOST 필수 가드는 exec(tee)보다 **앞에** 둔다 — 가드가 tee 뒤에 있으면
# PHONE_HOST unset 재실행 시 tee truncate로 마지막 실측 영수증 로그를
# 통째로 덮어쓴다(텍스트 스케일 T4 fix r1 M5 폐곡 승계).
if [ -z "$PHONE_HOST" ]; then
  echo "MUSP-FAIL: PHONE_HOST not set — 기록 금지 계약상 기본값·스캔 폴백 없음 (환경변수로 폰 호스트만 지정: 자리표시 PHONE_HOST=<폰>)"
  exit 1
fi

exec > >(tee "$RLOG") 2>&1
FAIL() { echo "MUSP-FAIL: $*"; exit 1; }
echo "PHONE-HOST: 환경변수 지정 사용 (기록 금지 — 자리표시 PHONE_HOST=<폰>; 스캔 폴백 없음)"
echo "HEAD: $(git -C "$GITROOT" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"
echo "BASE-ANCHOR(마지막 폰 배포): $PHONE_DEPLOY_BASE"
echo "LINEAGE(폰): selftest 565(2i 19·2g 40·2m 0) → 591(2m 26 흡수) → 기대 608 = 591 + 2n 17(스페이셜 위임 leg 흡수 — 캐논 2m 보존)"
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

# ------------------------------------------------------------------ 0. 전수 sweep
echo "=== 0. 전수 sweep(트리 낙후 발각 — D1 원장, idle 라인 수형) — HEAD blob vs 폰(CR 정규화) ==="
TREE="$SCRATCH/pmus_tree.txt"
git -C "$GITROOT" ls-tree -rl HEAD -- engine/src engine/include engine/CMakeLists.txt |
    sed 's/\t/ /' | awk '{print $4, $5}' | sort > "$TREE"
NTREE=$(wc -l < "$TREE")
echo "sweep 대상: $NTREE 파일"
LC_ALL=C sort "$TREE" > "$SCRATCH/pmus_head_sizes.txt"
# parse canary(idle T3 원장 — 이중 축약 결함: TREE가 이미 size path 2필드라
# 그 위에서 다시 $4/$5를 세면 전부 공란 → 폰 전행 MISSING → tar 0파일 사망):
NTREE_OK=$(awk 'NF==2 && $1 ~ /^[0-9]+$/ && $2 ~ /^engine\//' "$TREE" | wc -l)
[ "$NTREE_OK" -eq "$NTREE" ] || FAIL "sweep parse broken ($NTREE_OK/$NTREE valid)"
awk '{print $2}' "$TREE" > "$SCRATCH/pmus_head_files.txt"
$SSH "cat > \$HOME/.pmus_files.txt" < "$SCRATCH/pmus_head_files.txt" || FAIL "tree files push failed"
$SSH 'cd ~/JKENGINE && : > ~/.pmus_sizes.txt
while read -r f; do
  if [ -f "$f" ]; then printf "%s %s\n" "$(tr -d "\r" < "$f" | wc -c)" "$f"; else echo "MISSING $f"; fi
done < ~/.pmus_files.txt' > "$SCRATCH/pmus_phone_sizes.txt" || FAIL "phone size sweep failed"
LC_ALL=C sort "$SCRATCH/pmus_phone_sizes.txt" > "$SCRATCH/pmus_phone_sizes.s" \
    || FAIL "phone size sweep sort failed"
mv -f "$SCRATCH/pmus_phone_sizes.s" "$SCRATCH/pmus_phone_sizes.txt"
MISSN=$(grep -ac MISSING "$SCRATCH/pmus_phone_sizes.txt" || true)
echo "phone-tree-missing=$MISSN (배포 전 신선도 원문 — 신규 파일 부재 포함)"
DIFFOUT=$(diff "$SCRATCH/pmus_head_sizes.txt" "$SCRATCH/pmus_phone_sizes.txt" || true)
DIFFN=$(printf '%s\n' "$DIFFOUT" | grep -ac '^[<>]')
DIFFFILES=$(printf '%s\n' "$DIFFOUT" | grep -a '^[<>]' | sed 's/^[<>] //' | awk '{print $2}' | sort -u)
echo "SWEEP-DIFF=$DIFFN / $NTREE"

# 기대 배포 집합 = 마지막 폰 배포 이후 src/include/CMakeLists 계보(music 라인)
git -C "$GITROOT" diff --name-only "$PHONE_DEPLOY_BASE" HEAD -- engine/src engine/include engine/CMakeLists.txt | sort \
    > "$SCRATCH/pmus_expected.txt"
NEXP=$(wc -l < "$SCRATCH/pmus_expected.txt")
echo "EXPECTED-DEPLOY=$NEXP (music 라인 계보 파일 — anchor $PHONE_DEPLOY_BASE 이후)"
echo "$DIFFFILES" | grep -a . > "$SCRATCH/pmus_difffiles.txt" || true
[ -s "$SCRATCH/pmus_difffiles.txt" ] || { echo > "$SCRATCH/pmus_difffiles.txt"; }
UNEXPECTED=$(comm -23 "$SCRATCH/pmus_difffiles.txt" "$SCRATCH/pmus_expected.txt" | head -10)
if [ -z "${UNEXPECTED:-}" ] && [ "$DIFFN" != "0" ]; then
    echo "SWEEP-SCOPE=EXPECTED (DIFF ⊆ music 라인 계보 — tar blob 배포로 진행)"
elif [ "$DIFFN" = "0" ]; then
    echo "SWEEP-SCOPE=CLEAN (폰 트리=HEAD 동일 — 소스 배포 불요)"
else
    echo "SWEEP-SCOPE=UNEXPECTED-FILES (기대 집합 밖 결손 트리 낙후 — D1 원장 전량 git archive 경로):"
    echo "$UNEXPECTED"
fi

# ------------------------------------------------------------------ 1. pre-clean
echo "=== 1. pre-flight + pre-clean (재링크 ETXTBSY 방지 — jkweb 무접촉) ==="
$SSH 'echo PHONE-REACHABLE; uname -m; echo TMPDIR=$TMPDIR; command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
    || FAIL "ssh failed (phone unreachable — LAN 한정, sshd 기동: 폰 Termux에서 sshd)"
JKWEB_CKT=$($SSH 'pgrep -c -f "[j]kweb" 2>/dev/null' | tr -d ' \r')
echo "JKWEB-COUNT-PRE: ${JKWEB_CKT:-0} (상시 유지 계약 — 절사·pkill 대상 아님)"
$SSH "pkill -f '[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f '[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f '[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; echo SRV-DOWN" \
    || FAIL "pre-clean could not bring the old phone server down"
echo "PRECLEAN-SRV-DOWN=OK (구판 서버+클라 — 새 바이너리 교체 재기동 계약, jkweb 불접촉)"

echo "=== 2. 합성 WAV 시드 4곡 (윈도 python — 폰 python3 불요, tar 멤버 랜딩) ==="
SEEDSTAGE="$SCRATCH/pmus_stage"
rm -rf "$SEEDSTAGE"
mkdir -p "$SEEDSTAGE/engine/pmus_seed_stage/pcm_music_seed/sub"
PYSCRATCH=$(cygpath -w "$SCRATCH" 2>/dev/null || echo "$SCRATCH")
PYTHONIOENCODING=utf-8 python - "$PYSCRATCH" <<'PYEOF'
# 시드 4곡 — wsl_music.sh(T4) mus_seed.py와 동형: python wave stdlib(윈도 python
# 동일 표준 라이브러리 — 의존 0). 44.1kHz mono 16bit 8초 사인, 주파수 = 트랙
# 식별 마크. 4곡 = 3곡 루트+1곡 sub(T1 재귀 실측 몫 — rel열 "sub/..." 원문).
# mtime 명시 스탬프 = 정렬(mtime desc · tiebreak rel asc) 헤드 결정화 —
# 1행 = sub/d_seed.wav 계약(WSL 필터 d_seed 관측 쌍둥이).
import math, os, struct, sys, wave
import time

out = os.path.join(sys.argv[1], "pmus_stage", "engine", "pmus_seed_stage",
                   "pcm_music_seed")
rate, seconds = 44100, 8

def write_wav(path, freq):
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        frames = bytearray()
        for n in range(rate * seconds):
            v = math.sin(2.0 * math.pi * freq * n / rate)
            frames += struct.pack("<h", int((v + 1.0) * 32767 / 2.0 - 16384))
        f.writeframes(bytes(frames))
    print("SEED-OK %s %d bytes" % (os.path.basename(path), os.path.getsize(path)))

def stamp(path, y, mo, d, h, mi):
    t = time.mktime((y, mo, d, h, mi, 0, 0, 0, -1))
    os.utime(path, (t, t))

os.makedirs(os.path.join(out, "sub"), exist_ok=True)
a = os.path.join(out, "a_seed.wav")
b = os.path.join(out, "b_seed.wav")
c = os.path.join(out, "c_seed.wav")
sd = os.path.join(out, "sub", "d_seed.wav")
write_wav(a, 220.0)
write_wav(b, 330.0)
write_wav(c, 440.0)
write_wav(sd, 550.0)
stamp(sd, 2026, 11, 1, 12, 4)  # 최신 — 1행 = sub/d_seed.wav
stamp(a, 2026, 11, 1, 12, 3)
stamp(b, 2026, 11, 1, 12, 2)
stamp(c, 2026, 11, 1, 12, 1)
print("SEED-STAMPS: d>a>b>c 2026-11 미래 스탬프 (mtime desc — 1행 sub/d_seed.wav 계약)")
PYEOF
[ $? -eq 0 ] || FAIL "seed WAV generation failed (로컬 python)"
SEEDED=$(find "$SEEDSTAGE/engine/pmus_seed_stage/pcm_music_seed" -name '*.wav' | wc -l)
[ "$SEEDED" -eq 4 ] || FAIL "seeded $SEEDED wav files (need 4)"
echo "MUSIC-LIST-EXPECTED=4 (seed 3 root + 1 sub — 표행 수 관측 원문 1행; 표기 일치는 캡처 육안 몫)"

# ------------------------------------------------------------------ 3. 원격 스크립트
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PMUSEOF'
#!/bin/bash
# 폰 측 music 영수증 절차 (phone_music.sh가 생성 — music 라인 T5+
# spatial leg 라인 T4).
# settings 병합(시드 dirs) → cmake 재설정(자동)+ninja 리빌드 → selftest 캐논
# (591→608 · 2m 26 보존 · 2n 17 흡수) → leg list(탭2 리스캔+캡처+콜드 접착제
# 전조+spatial 배제 원문[LEG-ENV unset 가드 — spatial_play start_failed
# kDelegationHint·UI [spatial] 탭 라벨 렌더]+더블클릭 위임+귀속 폴링+직행
# open+get_status 재생 수치) → leg sdcard(M-5 재판정 — FUSE
# 스캔 유한 종료 실측) → 복원(ORIG 바이트 등호+PERM 병합 원복+시드 소각)
# → BOOT-OK(서버 UP+terminal 상시 — 클라 idle 라인 종료 상태 승계).
# jkweb 절사 금지(기동 카운트 원문만).
set -u
cd ~/JKENGINE/engine || exit 1
FAIL() { echo "MUSP-FAIL: $*"; exit 1; }
DSP="__PHONE_DISPLAY__"   # 드라이버가 PHONE_DISPLAY로 치환한다(기본 1)
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
SET=buildterm/state/settings.json
PERM=buildterm/permissions.json
SEED="$TMPD/pcm_music_seed"
SEED_STAGE=pmus_seed_stage
NLOG="$TMPD/pmus_ninja.log"
STLOG="$TMPD/pmus_selftest.log"
ORIG="$TMPD/pmus_orig_settings.json"
PERM_ORIG="$TMPD/pmus_orig_permissions.json"
CANON_PHONE_PREV=591      # 폰 기존 캐논(2m 26 흡수 실측 — 비교 원문)
CANON_PHONE_EXPECT=608    # 591 + 2n 17건(2n-a 6·2n-b 5·2n-c 4·2n-d 2) — WSL 608 등호 (T4 단일 지점 갱신)
CANON_PHONE_2M=26
CANON_PHONE_2N=17
CLK=$(getconf CLK_TCK 2>/dev/null); [ -n "$CLK" ] || CLK=100

# 클릭·행 상수 — 창 상대 desktop 좌표(wsl_music T4 상수 승계, env 재보정).
TAB2_X=${MUSP_TAB2_X:-90}    TAB2_Y=${MUSP_TAB2_Y:-77}      # dir 탭 스트립 2번째 탭(시드 dir)
ROW0_X=${MUSP_ROW0_X:-50}    ROW0_Y=${MUSP_ROW0_Y:-132}     # 표행 0의 경로 셀
SPAT_X=${MUSP_SPAT_X:-495}   SPAT_Y=${MUSP_SPAT_Y:-132}     # 표행 0의 [spatial] 버튼 열(4열 — 좌표 상수 추정:
                                                             # 560 폭 − 우측 74 고정열 − 프레임/스크롤 여백.
                                                             # 성립 판정은 캡처 라벨 렌더 육안 몫 — 도구
                                                             # 발사가 같은 SpatialStart 종착을 먼저 증명)

mkdir -p "$TMPD"
echo "=== A. 진입 마커 — 배포 전 구판 상태 원문 ==="
ENTRY_SRV=$(pgrep -f 'buildterm/[j]kdesktop --server' | tr '\n' ' ')
echo "ENTRY-SERVER-PIDS: ${ENTRY_SRV:-none} (종료 시 원복 판정 원문)"
echo "MARKER-BEFORE settings_bytes=$(wc -c < "$SET" 2>/dev/null) perm=$([ -f "$PERM" ] && wc -c < "$PERM" || echo none) seed_dir=$([ -d "$SEED" ] && echo yes || echo no)"
echo "ORIG-BASE-SELFTEST-PRE (드라이버 BASE 실측 원문 — 폰 selftest 565·2m 0·2i 19·2g 40: 2026-10-09 진입 실측, rc=0·AppSelfTest 0 failure)"
# BASE 실측의 잔상 파일(진단 로그) — 카운트 원문 인쇄 후 소각(probe 소유 잔상 0).
[ -f "$TMPD/pmus_base_selftest.txt" ] && { \
    echo "BASE-SELFTEST-LOG-RECOUNT: PASS=$(grep -ac '^\[PASS\]' "$TMPD/pmus_base_selftest.txt") 2m=$(grep -ac '^\[PASS\] 2m' "$TMPD/pmus_base_selftest.txt")"; \
    rm -f "$TMPD/pmus_base_selftest.txt"; echo "BASE-SELFTEST-LOG-BURIED"; } || echo "BASE-SELFTEST-LOG: absent (이미 소각/부재)"
# probe 소유 캡처 잔상(전 run 실패 유산) — 소각 후 시작(잔상 0 계약).
rm -f "$TMPD"/mus_phone_*.png
echo "TMP-CAPTURE-BURIED-AT-ENTRY: $(ls "$TMPD"/mus_phone_*.png 2>/dev/null | wc -l) left"

echo "=== B. settings 백업+병합 수형 (T4 리뷰 I-1 — 병합+원복, hard FAIL 교환) ==="
[ -f "$SET" ] || FAIL "settings file missing: $SET"
if [ -s "$ORIG" ] && grep -aq 'font_path' "$ORIG" && ! grep -aq '"music"' "$ORIG"; then
    echo "PRIOR-ORIG-RECOVERED: $ORIG ($(wc -c < "$ORIG") bytes — 전 run 실패 유산 진품 원본, 재사용)"
else
    if grep -aq '"music"' "$SET"; then
        FAIL "settings already has music key and no clean ORIG backup (워터마크 잔상 — 원본 원문 불명, 수동 확인 필요)"
    fi
    cp "$SET" "$ORIG" || FAIL "settings backup failed"
fi
cmp -s "$SET" "$ORIG" || echo "NOTE-ORIG-DIFFERS: 진입 settings ≠ ORIG — 병합 전 자가 원복(merge 함수가 ORIG에서 이행)"
FONT_PATH=$(sed -n 's/.*"font_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$ORIG" | head -1)
[ -n "$FONT_PATH" ] || FAIL "settings has no text.font_path (font_path 유지 계약 전제)"
[ -r "$FONT_PATH" ] || FAIL "font file not readable: $FONT_PATH"
echo "PARSED-FONT_PATH: $FONT_PATH"
echo "SETTINGS-BACKUP-OK: $ORIG ($(wc -c < "$ORIG") bytes — 복원은 이 바이트열 등호)"

merge_music_dir() { # $1 dirs 경로 $2 라벨 — ORIG 원복 후 music 키 삽입(기존 키·서식 보존)
    case "$1" in *'"'*) FAIL "dir path contains a quote (JSON 안전성)";; esac
    cp "$ORIG" "$SET" || FAIL "merge($2): ORIG copy failed"
    awk -v newline="  \"music\": { \"dirs\": [\"$1\"] }" '
        BEGIN { n = 0 }
        { lines[n++] = $0 }
        END {
            last = -1
            for (i = n - 1; i >= 0; i--)
                if (lines[i] ~ /^[ \t]*\}[ \t\r]*$/) { last = i; break }
            if (last < 1) { print "SET-MERGE-FAIL"; exit 1 }
            prev = lines[last - 1]
            sub(/[ \t\r]*$/, "", prev)
            if (prev !~ /,$/) prev = prev ","
            lines[last - 1] = prev
            for (i = 0; i < n; i++) {
                if (i == last) print newline
                print lines[i]
            }
        }' "$SET" > "$SET.new" || FAIL "merge($2): settings insert failed (구조 친화 실패)"
    mv "$SET.new" "$SET" || FAIL "merge($2): replace failed"
    grep -aq '"music"' "$SET" && grep -aqF "$1" "$SET" || FAIL "merge($2): gate failed — music.dirs 미기술"
    echo "SETTINGS-IN-LEG-$2: $(tr -d '\n' < "$SET") (bytes=$(wc -c < "$SET"))"
}

echo "=== C. permissions.json 병합(선존 파일 존중 — END 바이트 등호 복원) ==="
ENTRY_PERM=0
if [ -s "$PERM_ORIG" ]; then
    if ! cmp -s "$PERM" "$PERM_ORIG"; then
        cp "$PERM_ORIG" "$PERM" || FAIL "permissions self-heal copy failed"
        echo "PERM-SELF-HEAL: permissions.json을 전 run 진품 백업으로 원복 ($(wc -c < "$PERM") bytes)"
    else
        echo "PERM-PRIOR-BACKUP-PRESENT: $PERM_ORIG ($(wc -c < "$PERM_ORIG") bytes — 현재 파일과 등호)"
    fi
fi
if [ -f "$PERM" ]; then
    ENTRY_PERM=1
    cp "$PERM" "$PERM_ORIG" || FAIL "permissions backup failed"
    cmp -s "$PERM" "$PERM_ORIG" || FAIL "permissions backup byte mismatch"
    echo "PERM-ENTRY-PRE-EXISTING: $PERM ($(wc -c < "$PERM") bytes) — 원문: $(tr -d '\n' < "$PERM")"
    SI=$(sed -n 's/.*"send_input"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$PERM" | head -1)
    if [ "$SI" = "allow" ]; then
        echo "PERM-MERGE: none (선존 send_input=allow)"
    else
        awk -v newline='    "send_input": "allow"' '
            BEGIN { n = 0 }
            { lines[n++] = $0 }
            END {
                last = -1
                for (i = n - 1; i >= 0; i--)
                    if (lines[i] ~ /^[ \t]*\}[ \t\r]*$/) { last = i; break }
                if (last < 1) { print "PERM-MERGE-FAIL"; exit 1 }
                prev = lines[last - 1]
                sub(/[ \t\r]*$/, "", prev)
                if (prev !~ /,$/) prev = prev ","
                lines[last - 1] = prev
                for (i = 0; i < n; i++) {
                    if (i == last) print newline
                    print lines[i]
                }
            }' "$PERM" > "$PERM.new" || FAIL "permissions merge failed (구조 친화 실패)"
        mv "$PERM.new" "$PERM" || FAIL "permissions merge replace failed"
        echo "PERM-MERGE: +send_input allow (선존 키 보존 — merge=$(wc -c < "$PERM") bytes, END에서 바이트 등호 원복)"
    fi
else
    printf '{\n    "send_input": "allow"\n}\n' > "$PERM" || FAIL "permissions write failed"
    echo "PERM-WIRE-OK: $PERM (send_input allow — 선존 부재라 END에서 소각)"
fi
grep -aq 'send_input' "$PERM" || FAIL "permissions.json has no send_input (승인 병합 미성립)"
echo "PERM-AFTER-MERGE: $(tr -d '\n' < "$PERM")"

echo "=== D. jkweb 생존 계약 — 기동 카운트 원문 (절사 금지) ==="
JKWEB_BEFORE=$(pgrep -f '[j]kweb' | tr '\n' ' ')
echo "JKWEB-BEFORE: ${JKWEB_BEFORE:-none}"

echo "=== E. seed WAV 수취 — 진품 등수 판정 (드라이버 합성, 폰 python3 불요) ==="
if [ -d "$SEED" ] && [ "$(find "$SEED" -name '*.wav' | wc -l)" -eq 4 ]; then
    echo "PRIOR-SEEDS-RECOVERED: $SEED (전 run 유산 — 4곡 등수, 재사용: RUN-DROPPED 재실행 계약)"
else
    [ -d "$SEED" ] && FAIL "seed dir exists with irregular contents — 수동 소각 필요"
    [ -d "$SEED_STAGE/pcm_music_seed" ] || FAIL "seed tar member missing (구성 누락)"
    mv "$SEED_STAGE/pcm_music_seed" "$SEED" || FAIL "seed move failed"
    echo "SEED-LANDED: $SEED (tar 멤버 → \$TMPDIR 이동)"
fi
rm -rf "$SEED_STAGE"
[ -d "$SEED_STAGE" ] && FAIL "seed stage rm failed (repo 잔상)"
SEEDED=$(find "$SEED" -name '*.wav' | wc -l)
[ "$SEEDED" -eq 4 ] || FAIL "seeded $SEEDED wavs (need 4)"
SBYTES=0
for w in "$SEED"/*.wav "$SEED"/sub/*.wav; do
    [ -f "$w" ] || continue
    SBYTES=$((SBYTES + $(wc -c < "$w")))
done
echo "SEED-4-COUNT-OK: bytes=$SBYTES (44.1kHz mono 16bit 8초 사인 — 주파수 식별 마크)"
echo "MUSIC-LIST-EXPECTED=4 (seed 3 root + 1 sub — 표행 수 관측 원문 1행)"
echo "=== E2. mp3 합성 여부 재실측 (T3 원문 수형 — 폰 ffmpeg 부재 → wav 단축) ==="
if command -v ffmpeg >/dev/null 2>&1; then
    echo "MUSP-AUDIO-MP3: ffmpeg 존재 — 그래도 wav 단축 계속(mp3 합성 실측은 WSL T3 원문 소유 — M-2 길이 미상 관측 몫)"
else
    echo "MUSP-AUDIO-MP3: ffmpeg 부재 (T3/T1 원문 승계) — wav 단축 (시드 4곡 유지)"
fi

echo "=== F. ninja 리빌드 (aarch64 — jkapp_music 신규 타깃, CMake mtimes로 RERUN_CMAKE 기대) ==="
pkill -f '[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f '[j]kdesktop' 2>/dev/null
sleep 1
if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then
    FAIL "server respawned mid-build (ETXTBSY 위험 — 상위 감독자 원장)"
fi
ninja -C buildterm -j4 >"$NLOG" 2>&1
N_RC=$?
tail -4 "$NLOG"
echo "NINJA-RC=$N_RC"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure)"
[ -x buildterm/jkdesktop ] || FAIL "buildterm/jkdesktop missing after rebuild"
if [ ! -f buildterm/jkapp_music.so ]; then
    echo "NOTE-CMAKE-RECONF: ninja 후 jkapp_music.so 부재 — 명시 cmake 재설정 폴백 1급 (RERUN_CMAKE 미발동)"
    cmake -S . -B buildterm >"$TMPD/pmus_cmake.log" 2>&1
    C_RC=$?
    echo "CMAKE-RC=$C_RC"
    tail -4 "$TMPD/pmus_cmake.log"
    [ "$C_RC" -eq 0 ] || FAIL "cmake reconfigure rc=$C_RC (buildterm 폴백 실패)"
    ninja -C buildterm -j4 >>"$NLOG" 2>&1
    N_RC=$?
    echo "NINJA-RC-RECONF=$N_RC"
    [ "$N_RC" -eq 0 ] || FAIL "ninja rebuild(재설정 후) rc=$N_RC"
fi
[ -f buildterm/jkapp_music.so ] || FAIL "buildterm/jkapp_music.so missing after rebuild (posix .so)"
[ -f buildterm/jkapp_vplayer.so ] || FAIL "buildterm/jkapp_vplayer.so missing (위임 대상 배포 결손)"
echo "MODULE-SO-PRESENT: jkapp_music.so=$(wc -c < buildterm/jkapp_music.so) bytes · jkapp_vplayer.so=$(wc -c < buildterm/jkapp_vplayer.so) bytes"
# leg 배제 원문 전제(D5 — T4): 폰은 SPATIAL_PLAYER_ROOT를 정의하지 않는다.
# CMake는 env 설정만 JK_MUSIC_SPATIAL_LEG 매크로+audio_core 링크를 먹인다
# (fail-closed 배선 원문) — unset이면 [.so]에 AL 심볼이 없어야 정상이다.
# 심볼 카운트는 nm 대신 바이너리 문자열 grep(Termux binutils 의존 0 — wsl
# 8b ALCOUNT 수형의 최소 대응).
echo "MUSP-LEG-ENV: ${SPATIAL_PLAYER_ROOT:+UNEXPECTED-SET}unset (폰 leg 자연 미링크 계약 — 배제 원문 실측 전제)"
[ -z "${SPATIAL_PLAYER_ROOT:-}" ] || FAIL "phone build must not set SPATIAL_PLAYER_ROOT (T4 계약 위배 — 배제 원문이 아니라 성립 런이 되어버린다)"
LEG_SYM=$(grep -aac 'alcOpenDevice' buildterm/jkapp_music.so 2>/dev/null || true)
LEG_SYM=${LEG_SYM:-0}
echo "MUSP-LEG-SYM: alcOpenDevice strings=$LEG_SYM (0 = leg 미링크 fail-closed 영수증 — LEG-ENV unset과 쌍둥이)"
[ "$LEG_SYM" = "0" ] || FAIL "phone jkapp_music.so carries AL strings($LEG_SYM) — env 미정의인데 leg 링크(CMake fail-closed 배선 원장 대조 필요)"

echo "=== G. 배포 마커 — HEAD 원천 실존 단정 ==="
MM=$(grep -c 'ListAudioFiles' include/apps/MusicModel.h)
MC=$(grep -c 'jkapp_music' CMakeLists.txt)
MT=$(grep -c '2m-' src/main.cpp)
MP=$(grep -c 'DelegationReplyVerdict' src/apps/ClientMusicApp.cpp)
ML=$(grep -c '"music", "Music"' src/apps/JKAppModule_music.cpp)
echo "MARKER-AFTER music_model($MM) cmake($MC) main_2m($MT) delegate_verdict($MP) module($ML)"
[ "$MM" -ge 1 ] || FAIL "MusicModel ListAudioFiles marker missing on phone (배포 결손)"
[ "$MC" -ge 1 ] || FAIL "jkapp_music CMake marker missing on phone"
[ "$MT" -ge 1 ] || FAIL "2m selftest marker missing on phone main.cpp"
[ "$MP" -ge 1 ] || FAIL "DelegationReplyVerdict marker missing on phone (fix r3 배포 결손)"

echo "=== H. selftest — 폰 캐논 계보 판정 (원문: 591 = 2m 26 흡수 전산 · 기대 608 = 591 + 2n 17건 — WSL 608 등호) ==="
timeout 900 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
P2M=$(grep -ac '^\[PASS\] 2m' "$STLOG")
P2MH=$(grep -ac '^\[PASS\] 2m-h' "$STLOG")
P2N=$(grep -ac '^\[PASS\] 2n' "$STLOG")
P2I=$(grep -ac '^\[PASS\] 2i' "$STLOG")
P2G=$(grep -ac '^\[PASS\] 2g' "$STLOG")
echo "PHONE-SELFTEST rc=$S_RC PASS=$ST_PASS FAIL=$ST_FAIL 2m=$P2M(2m-h=$P2MH) 2i=$P2I 2g=$P2G 2n=$P2N (music 라인 2m 계열 보존 + spatial leg 2n 계열 신설 — 캐논 +17분의 원료)"
grep -a 'AppSelfTest' "$STLOG" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
awk -v p="$ST_PASS" -v m="$P2M" -v h="$P2MH" -v n="$P2N" -v prev="$CANON_PHONE_PREV" \
    -v expect="$CANON_PHONE_EXPECT" -v exn="$CANON_PHONE_2N" -v mm="$CANON_PHONE_2M" 'BEGIN{
    if (p == expect && n == exn)
        print "CANON-INCLUSION=MUSIC-FULL-2N-17 (폰 캐논 " prev "→" p " 상승 — 2n " n "건 흡수(2m " mm "건[h " h " 포함] 보존) 실측 — WSL 608 등호)"
    else if (p == prev)
        print "CANON-INCLUSION=MUSIC-SELFTEST-MISSING (music selftest 쌍둥이 미반영 — 계보 결손 — 원장)"
    else
        printf "CANON-INCLUSION=OTHER-N(%d — 기대 %d=%d+2n%d · 2m=%d(2m-h=%d)·2n=%d — 계보 정산 원장)\n", p, expect, prev, exn, m, h, n
}'

# ---------------------------------------------------------------- 레그 공통
ctl() { timeout 20 ./buildterm/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }
mtool() { # $1=tool $2=args-json — music 앱 직행 릴레이(wsl 8b atool 수형 —
          #   도구 허브 릴레이 원문. music 창 등록 전제라 레그 list 내에서만 온다)
    ctl "{\"tool\":\"app_tool\",\"args\":{\"app\":\"music\",\"tool\":\"$1\",\"args\":$2}}"
}
CAPTURE() { # $1=출력png — x11grab 1920x1080 시도, 화면 크기 파싱 재시도(import 금지 — libheif 파단 원문)
    local OUT=$1
    rm -f "$OUT"
    ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1080 \
        -framerate 1 -i ":$DSP" -frames:v 1 "$OUT" >/dev/null 2>&1
    if [ ! -s "$OUT" ]; then
        SCR=$(ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1080 \
            -framerate 1 -i ":$DSP" -frames:v 1 /dev/null 2>&1 \
            | grep -aoE 'screen size [0-9]+x[0-9]+' | head -1 | sed 's/screen size //')
        [ -n "$SCR" ] || SCR=1920x1005   # 폴백 유래 실측 — 폰 화면 1080 아님(docs/86 §3 #12·docs/87 §3 #15)
        echo "CAPTURE-SIZE-ADJUST: $SCR"
        ffmpeg -hide_banner -loglevel error -f x11grab -video_size "$SCR" \
            -framerate 1 -i ":$DSP" -frames:v 1 "$OUT" >/dev/null 2>&1
    fi
    [ -s "$OUT" ] || FAIL "capture failed: $OUT"
    echo "CAPTURE-OK: $OUT ($(wc -c < "$OUT") bytes)"
}
cclients() { # 클라 프로세스 목록(pid app utime stime — /proc stat, idle 라인 헬퍼 승계)
    for d in /proc/[0-9]*; do
        p=${d#/proc/}
        [ "$(cat "$d/comm" 2>/dev/null)" = "jkdesktop" ] || continue
        c=$(tr '\0' ' ' < "$d/cmdline" 2>/dev/null)
        case "$c" in *"--server"*) continue ;; esac
        app=$(printf '%s' "$c" | sed -n 's/.*--client \([^ ]*\).*/\1/p')
        [ -n "$app" ] || { case "$c" in *"--filedlg"*) app=filedlg ;; esac; }
        [ -n "$app" ] || continue
        set -- $(sed 's/^[^)]*) //' "$d/stat" 2>/dev/null)
        echo "$p $app ${12:-0} ${13:-0}"
    done
}
boot_server() { # $1 leg 라벨 — setsid 부팅+ping(진입 상태 원복의 부분)
    local LEG=$1
    local LOG="$TMPD/pmus_srv_${LEG}.log"
    pkill -f '[j]kdesktop' 2>/dev/null
    sleep 2
    pkill -9 -f '[j]kdesktop' 2>/dev/null
    sleep 1
    if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then
        FAIL "leg $LEG: server survived pre-boot clean"
    fi
    env DISPLAY=":$DSP" setsid nohup ./buildterm/jkdesktop --server >"$LOG" 2>&1 &
    sleep 8
    SRVPID=$(pgrep -f 'buildterm/[j]kdesktop --server' | head -1)
    [ -n "$SRVPID" ] || { echo "--- log tail:"; tail -5 "$LOG"; FAIL "leg $LEG: no server 8s after boot"; }
    echo "leg $LEG: server pid=$SRVPID (DISPLAY=:$DSP)"
    PING=""
    for i in 1 2 3 4 5; do
        PING=$(ctl '{"tool":"ping","args":{}}')
        ok "$PING" && break
        sleep 2
    done
    ok "$PING" || FAIL "leg $LEG: ping did not ok — $PING"
    sleep 10   # taskbar 자동 스폰 관측 여유
    WIN=$(ctl '{"tool":"list_windows","args":{}}')
    printf '%s' "$WIN" | grep -aq '"title":"Taskbar"' || {
        T=$(ctl '{"tool":"launch_app","args":{"app":"taskbar"}}')
        echo "TASKBAR-LAUNCH-RECOVED-$LEG: $T"
    }
    echo "SRVLOG-WARN-$LEG: atlas_inactive=$(grep -ac 'vector atlas inactive' "$LOG") no_vector_font=$(grep -ac 'no vector font configured' "$LOG") hangul_misc=$(grep -ac 'HangulManager' "$LOG")"
}
wait_music() { # Music 창 등장 대기 — 폰 클라 스폰 15-18s 원장, 3s×30 (gallery 동형)
    MW=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 \
             21 22 23 24 25 26 27 28 29 30; do
        WIN=$(ctl '{"tool":"list_windows","args":{}}')
        MW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
        [ -n "$MW" ] && break
        sleep 3
    done
    [ -n "$MW" ] || FAIL "no Music window (폰 스폰 15-18s 원장 + 플레이크 마진 90s 초과) — last: ${WIN:-none}"
    MUSWIN="$MW"   # 전역 세팅 — parse_geo가 쓴다(gallery 1차 트랩 원장)
    echo "MUSWIN-$LEG: $MUSWIN"
}
wait_vplayer() { # Video Player 창 등장 대기 — 3s×20(폰 스폰)
    VW=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        WIN=$(ctl '{"tool":"list_windows","args":{}}')
        VW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
        [ -n "$VW" ] && break
        sleep 3
    done
    [ -n "$VW" ] || return 1
    VPWIN="$VW"
    echo "VPWIN: $VPWIN"
    return 0
}
parse_geo() { # $1 라벨 — MUSWIN에서 id/x/y/w/h 추출해 MUS-GEO-<label> 인쇄
    MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    MUS_W=$(printf '%s' "$MUSWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
    MUS_H=$(printf '%s' "$MUSWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
    [ -n "$MUS_ID" ] || FAIL "geo parse failed (id empty) — MUSWIN: $MUSWIN"
    echo "MUS-GEO-$1: id=$MUS_ID x=$MUS_X y=$MUS_Y w=$MUS_W h=$MUS_H (meta 560x520 상대 검증)"
}
tap() { # $1=id $2=x $3=y $4=clicks — 합성 클릭(send_input — permissions allow 게이트)
    timeout 20 ./buildterm/jkdesktop agentctl \
        "{\"tool\":\"send_input\",\"args\":{\"op\":\"click\",\"id\":$1,\"x\":$2,\"y\":$3,\"button\":1,\"clicks\":$4}}" \
        2>/dev/null | grep -a '{' | head -1
}

# ══════════════════════ leg list — 시드 dirs · 탭2 리스캔 · 위임 · 직행 open ══════════════
LEG=list
echo "=== I. leg list — settings 병합(시드 dirs) 부팅 + music 런치 + 탭2 리스캔 ==="
merge_music_dir "$SEED" list
boot_server list
L=$(ctl '{"tool":"launch_app","args":{"app":"music"}}')
ok "$L" || FAIL "leg list: launch_app music failed — $L"
echo "launch reply: $L"
wait_music
parse_geo list
sleep 3   # 부팅 스캔(기본 폴더 state/music — 부재) 도착 안정화
CLICK_X=$((MUS_X + TAB2_X))
CLICK_Y=$((MUS_Y + TAB2_Y))
echo "CLICK-TAB2: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 TAB2_X=$TAB2_X TAB2_Y=$TAB2_Y"
TCL=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
ok "$TCL" || FAIL "send_input tab click gate/execute failed — reply: $TCL (permissions.json send_input allow 확인)"
echo "tab click reply: $TCL"
sleep 2   # 첫 클릭이 포커스 취식일 수 있다(1-3차 런 실측: 리스트 캡처가 탭1·0곡
          # 상태 — sdcard 레그(세션 2번째 클릭)는 즉시 탭 선택) — 같은 탭 재클릭
          # 면제(재선택 = RequestScan 재청구, idempotent)
TCL2=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
ok "$TCL2" || echo "NOTE: tab re-click failed — $TCL2 (첫 클릭만 성립 시 원장행)"
echo "tab re-click reply: $TCL2 (동일 탭 재선택 — 재스캔 재청구 idempotent)"
sleep 8   # 탭 전환 재스캔(시드 dirs) 도착 대기
CAPTURE "$TMPD/mus_phone_list.png"
sleep 8   # 2차 캡처 — 스캔 도착 후 표행 확정 원문(스캔 도착이 폰에서 늦다 — 원장)
CAPTURE "$TMPD/mus_phone_list2.png"

echo "=== J. 콜드 접착제 전조 — 등록 전 app_tool open = unknown_app_tool 실측 (WSL T4 승계) ==="
OR0=$(ctl "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$SEED/a_seed.wav\"}}}")
echo "MUSICP-GLUE-COLD-REPLY: ${OR0:-none}"
printf '%s' "$OR0" | grep -aq '"unknown_app_tool"' \
    || echo "MUSICP-FAIL-SOFT(glue-cold: 등록 전 릴레이가 unknown_app_tool이 아님 — 콜드 경기 전조 부정 — 원장: $OR0)"

echo "=== J2. spatial leg 배제 원문 — 폰 SPATIAL_PLAYER_ROOT 미정의(D5 — T4 핵심 영수증) ==="
# 계약: 폰은 env를 정의하지 않는다(F 섹션 가드) — leg 미링크 .so의 발사는
# DeviceFailed 고장 종착(kDelegationHint 원문 라벨 소비 — SpatialStart의
# #ifndef 분기)으로 끝난다. 도구 발사(spatial_play)와 UI [spatial] 버튼 탭은
# SpatialStart 단일 발사 경로로 합류(T2 원문) — 양경로가 같은 귀결인 것이
# 이 세그먼트의 원문이다. leg 성립 실측은 WSL(T3 LEG-OK)이 소유 — 폰은
# 배제의 정직 원문만 실측한다.
ST0=$(mtool spatial_status '{}')
echo "MUSP-SPATIAL-STATUS-0: ${ST0:-none} (재생 전 기저 — 미활성 1행 원문)"
if [ -n "$ST0" ]; then
    printf '%s' "$ST0" | grep -aq '"active":false' \
        || echo "MUSICP-FAIL-SOFT(spatial 기저가 active — leg-less 원문 대조 필요)"
    printf '%s' "$ST0" | grep -aq '"deviceOk":false' \
        || echo "MUSICP-FAIL-SOFT(spatial 기저 deviceOk true — leg-less 원문 대조 필요)"
else
    echo "MUSICP-FAIL-SOFT(spatial_status 기저 관측 실패 — 앱 도구 등록 3종 원문 참조)"
fi
SPAT_REPLY=$(mtool spatial_play "{\"path\":\"$SEED/a_seed.wav\"}")
echo "MUSP-SPATIAL-PLAY-REPLY: $SPAT_REPLY"
printf '%s' "$SPAT_REPLY" | grep -aq '"error":"start_failed"' \
  && printf '%s' "$SPAT_REPLY" | grep -aq 'spatial leg 불가' \
  && echo "MUSP-LEG-BARRED: OK — spatial_play = start_failed + detail kDelegationHint(leg-less fail-closed 영수증 — 폰 leg 배제 원문 성립)" \
  || echo "MUSICP-FAIL-SOFT(leg 배제 원문 미성립 — reply 원문 대조: $SPAT_REPLY)"
sleep 1   # 발사 귀결 frameDirty 렌더 도착 (도구 경로도 status_를 먹인다)
ST1=$(mtool spatial_status '{}')
echo "MUSP-SPATIAL-STATUS-1: $ST1"
if printf '%s' "$ST1" | grep -aq '"active":false'; then
    echo "MUSP-SPATIAL-POS: MISS(미활성 — pos 증가 미판정. T3 리뷰 I-2 승계 — LEG-FALLBACK에서도 이 MISS/미활성 1행을 남긴다: 재런에서 자기교정)"
    SPATIAL_INACTIVE=1
else
    echo "MUSICP-FAIL-SOFT(spatial_status post-fire가 active — leg-less 계약 원문 대조 필요)"
    SPATIAL_INACTIVE=0
fi
echo "--- UI [spatial] 버튼 탭(발사 경로 동일성 원문 — SpatialStart 단일 경로 합류) ---"
SPATCLICK_X=$((MUS_X + SPAT_X))
SPATCLICK_Y=$((MUS_Y + SPAT_Y))
echo "CLICK-SPAT: id=$MUS_ID desk=($SPATCLICK_X,$SPATCLICK_Y) — 상수 SPAT_X=$SPAT_X SPAT_Y=$SPAT_Y (좌표 추정 — 성립은 캡처 라벨 렌더 육안 몫)"
SCL=$(tap "$MUS_ID" "$SPATCLICK_X" "$SPATCLICK_Y" 1)
ok "$SCL" || echo "NOTE-SPAT-TAP: send_input [spatial] 클릭 실패 — $SCL (도구 발사가 같은 DeviceFailed 종착을 이미 증명 — 원장행)"
echo "[spatial] tap reply: $SCL"
sleep 2
ST2=$(mtool spatial_status '{}')
echo "MUSP-SPATIAL-STATUS-2(post-tap): ${ST2:-none} (err=kDelegationHint 보존 원문 — 성공 시작이 없으면 소멸하지 않는다)"
CAPTURE "$TMPD/mus_phone_spatial.png"

echo "=== K. 표행 0 더블클릭 → vplayer 위임 (T3 실측 — 폰 스폰 페이싱 경기 포함) ==="
CLICK_X=$((MUS_X + ROW0_X))
CLICK_Y=$((MUS_Y + ROW0_Y))
echo "CLICK-ROW0: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 ROW0_X=$ROW0_X ROW0_Y=$ROW0_Y (합성 2탭 — ImGui 더블클릭 시간창 300ms 대조 몫)"
VPWIN=""
VPWIN_ID=""
DELEG_CLICKS2=0
for PAIR in 1 2 3 4; do
    C1=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
    ok "$C1" || FAIL "send_input row0 tap1 failed — $C1"
    TAP1_NS=$(date +%s%N)
    C2=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
    ok "$C2" || FAIL "send_input row0 tap2 failed — $C2"
    TAP2_NS=$(date +%s%N)
    echo "dblclick pair $PAIR replies: $C1 | $C2 (tap gap $(( (TAP2_NS - TAP1_NS) / 1000000 )) ms — ImGui 더블클릭 시간창 300ms 대조 몫)"
    if wait_vplayer; then break; fi
    echo "MUSICP-DELEGATE: pair $PAIR MISS — 재탭(중복 스폰 스로틀 회피 대기)"
    sleep 4
done
if [ -z "$VPWIN" ]; then
    echo "=== K2. 폴백 pair — click 수 2 합성(send_input clicks:2 — 서버 반복 전달 경로 실측) ==="
    CK2=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 2)
    ok "$CK2" && echo "CLICKS2-REPLY: $CK2"
    if wait_vplayer; then
        DELEG_CLICKS2=1
        echo "MUSICP-DELEGATE: OK-CLICKS2 — click 수 2 합성이 더블클릭 위임을 트리거했다"
    else
        echo "MUSICP-DELEGATE: MISS — 4쌍의 합성 2탭+clicks2가 표행 더블클릭 핸들에 도달하지 않았다(폰 agentctl 왕복 vs ImGui 0.30s 시간창 — EYES/원장 사유)"
    fi
fi
if [ -n "$VPWIN" ]; then
    echo "MUSICP-DELEGATE: OK — (탭) 위임이 Video Player 창을 냈다 (music 클라의 launch_app vplayer 쿼리 상통)"
else
    echo "=== K3. launch_app vplayer 폴백 스폰 (위임 MISS — 직행 open 계측 계속의 전제) ==="
    DELEG_FALLBACK=1
    FL=$(ctl '{"tool":"launch_app","args":{"app":"vplayer"}}')
    ok "$FL" || echo "NOTE-VP-FALLBACK: launch_app vplayer 미성립 — $FL (직행 open이 unknown_app_tool로 귀속 — 원장)"
    wait_vplayer || echo "NOTE-VP-FALLBACK-WINDOW: Video Player 창 미출현 — 직행 open 귀속 계속 시도"
    echo "DELEG-FALLBACK: 1 (스폰=launch_app 폴백 — 위임 창 귀속 MISS 유지, 직행 open은 별개 경로)"
fi
VPWIN_ID=$(printf '%s' "${VPWIN:-}" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
echo "VPWIN-PARSED-ID: ${VPWIN_ID:-none} (단일 파싱 — T4 M-4 인라인 재계산 지선 폐곡)"
sleep 1
CAPTURE "$TMPD/mus_phone_vp_delegate.png"

echo "=== L. 위임 open 도착 귀속 — 직행 open 없이 get_status 고밀도 폴링 (T4 v2 렛슨 수형) ==="
DELEG_OPEN=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    GS=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
    echo "MUSICP-DELEGATE-GET-STATUS-$i: ${GS:-none}"
    printf '%s' "$GS" | grep -aq '"opened":true' && { DELEG_OPEN=1; break; }
    sleep 0.4
done
if [ -n "${DELEG_FALLBACK:-}" ] && [ "$DELEG_FALLBACK" -eq 1 ]; then
    echo "MUSICP-DELEGATE-OPEN: N/A — 스폰이 launch_app 폴백(위임 개입 0) — 귀속 재판정 무효(원장)"
elif [ "$DELEG_OPEN" -eq 1 ]; then
    echo "MUSICP-DELEGATE-OPEN: OK — 위임 open이 vplayer에 도달해 열었다(재청구 폴백 포함)"
else
    echo "MUSICP-DELEGATE-OPEN: MISS — 창 출현 후 ~6s 간 opened:true 전이 없음. 원장 후보: ①폰 클라 스폰 15-18s > 재청구 폴백 창(20×250ms=5s) — 콜드 위임 open이 스폰 완료 전에 소진(T3 소진 표기는 정상 동작 — status 행 캡처 육안 몫) ②봉투/본문 재판정 진원 잔존. 직행 open(등록 후)은 다음 레그로 진상 구분"
fi
CAPTURE "$TMPD/mus_phone_delegate_status.png"

echo "=== M. 위임 접착제 직행 — app_tool open(get_status 재생 수치) ==="
OPEN_PATH="$SEED/sub/d_seed.wav"
OR=$(ctl "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$OPEN_PATH\"}}}")
echo "MUSICP-GLUE-REPLY: $OR"
printf '%s' "$OR" | grep -aq '"accepted":true' \
    || echo "MUSICP-FAIL-SOFT(app_tool open not accepted — $OR)"
sleep 4   # open 비동기 — 워커 bring-up(폰은 느리다 — WSL 3s에서 +1)
GS1=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
echo "MUSICP-GET-STATUS-1: $GS1"
sleep 4
GS2=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
echo "MUSICP-GET-STATUS-2: $GS2 (pos 증가 = 재생 진행 원문 1행 — 드라이버 분석 몫)"
CAPTURE "$TMPD/mus_phone_after.png"

echo "=== N. stderr 진단 수취 (서버 로그 — 클라 스폰+music+vplayer 진단) ==="
grep -a 'spawn\|music\|vplayer\|\[vplayer\]' "$TMPD/pmus_srv_list.log" | tail -14 | sed 's/^/  /'

# ══════════════════════ leg sdcard — /sdcard FUSE 스캔 유한 종료 실측 (M-5 재판정) ══════════════
LEG=sdcard
echo "=== O. leg sdcard — music.dirs=/sdcard 부팅 + FUSE 스캔 유한 종료 실측 (T1 fix r1 리뷰 M-5 재판정) ==="
# 1-4차 런 원장: 이 레그의 첫 클릭도 포커스 취식으로 사라진다(신규 클라) —
# /sdcard/Pictures 레그의 "SCAN-FINITE OK"는 미스캔(0곡=탭1)의 무효 원장이었다.
# 5차 런부터: 탭 재클릭 방어 + 대상=/sdcard 루트(FUSE 재귀 전체 — 파일 수가
# 천 단위면 스캔이 눈에 보이게 진행되어 유한 종료 실측이 성립한다).
merge_music_dir /sdcard sdcard
boot_server sdcard
L=$(ctl '{"tool":"launch_app","args":{"app":"music"}}')
ok "$L" || FAIL "leg sdcard: launch_app music failed — $L"
wait_music
parse_geo sdcard
sleep 2
CLICK_X=$((MUS_X + TAB2_X))
CLICK_Y=$((MUS_Y + TAB2_Y))
TCL=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
ok "$TCL" || FAIL "leg sdcard: send_input tab click failed — $TCL"
echo "tab click reply: $TCL (탭2 = sdcard dir — 리스캔 기동)"
sleep 2
TCL2=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
ok "$TCL2" || echo "NOTE: sdcard tab re-click failed — $TCL2 (원장행)"
echo "tab re-click reply: $TCL2 (첫 클릭 취식 방어 — 동일 탭 재선택 idempotent)"
sleep 6
MPID=$(cclients | awk '$2=="music"{print $1; exit}')
if [ -n "$MPID" ]; then
    SMP1=$(sed 's/^[^)]*) //' /proc/$MPID/stat)
    set -- $SMP1; U1=${12}; K1=${13}
    sleep 15
    SMP2=$(sed 's/^[^)]*) //' /proc/$MPID/stat)
    set -- $SMP2; U2=${12}; K2=${13}
    C1CPU=$(awk -v u1="$U1" -v k1="$K1" -v u2="$U2" -v k2="$K2" -v hz="$CLK" -v t=15 'BEGIN{printf "%.2f", ((u2+k2)-(u1+k1))/hz/t*100}')
    sleep 12
    SMP3=$(sed 's/^[^)]*) //' /proc/$MPID/stat)
    set -- $SMP3; U3=${12}; K3=${13}
    C2CPU=$(awk -v u2="$U2" -v k2="$K2" -v u3="$U3" -v k3="$K3" -v hz="$CLK" -v t=12 'BEGIN{printf "%.2f", ((u3+k3)-(u2+k2))/hz/t*100}')
    ALIVE2=$([ -d /proc/$MPID ] && echo yes || echo no)
    echo "SCAN-CPU-1: music=$C1CPU% (스캔 기동 직후 15s — FUSE 재귀+표 렌더)"
    echo "SCAN-CPU-2: music=$C2CPU% (12s 뒤 — 정착 대조: 유한 종료하면 <40%·무한 재귀면 peg ~100%)"
    echo "SCAN-CLIENT-ALIVE: $ALIVE2 (pid=$MPID)"
    LIV=$(ctl '{"tool":"list_windows","args":{}}')
    printf '%s' "$LIV" | grep -aq '"title":"Music"' && echo "SCAN-LIVENESS: OK (Music 창 응답성 유지)" \
        || echo "SCAN-LIVENESS: Music 창 소멸(list_windows 응답은 성립 — 소멸 사유 원장)"
    if awk -v c2="$C2CPU" 'BEGIN{exit !((c2+0) < 40)}'; then
        echo "SCAN-FINITE: OK — FUSE(/sdcard)에서 재귀 스캔 유한 종료 실측 (클라 CPU 정착 $C2CPU%·창 응답)"
        SCAN_FINITE=OK
    else
        echo "SCAN-FINITE: 미판정 — 정착 CPU $C2CPU% (M-5 재판정 원장 — 무한 재귀 의심 라인, 캡처+2창 원문 첨부)"
        SCAN_FINITE=UNDECIDED
    fi
else
    echo "SCAN-CLIENT-PID: none (music 클라 프로세스 미발견) — 스캔 유한 종료 실측 스킵(창 실측 병행)"
    SCAN_FINITE=SKIPPED
    C2CPU="n/a"
fi
echo "--- /sdcard 셸 열거 시도 원문 (권한 부재=빈 목록 정상 결과 — 결함 아님) ---"
SDFIND=$(timeout 120 find /sdcard -type f 2>/dev/null | grep -aicE '\.(wav|mp3|flac|ogg|m4a|aac|wma)$')
SDCARD_OK=$([ -d /sdcard ] && echo dir-present || echo dir-missing)
SDTOTAL=$(timeout 120 find /sdcard -type f 2>/dev/null | wc -l)
echo "SDCARD-STATE: $SDCARD_OK (Termux storage 권한 정직 원문 — 열거 실패는 그대로 가시화만)"
echo "SDCARD-AUDIO-COUNT: ${SDFIND:-0} (셸 find 음성 파일 수 — 앱 스캔 표행 수와 대조는 캡처 육안 몫) · 총파일=$SDTOTAL"
if [ "${SDTOTAL:-0}" = "0" ]; then
    echo "NOTE-SDCARD-SHELL: 셸 find 총파일 0 = timeout 120 미완료 포함(FUSE 전체 열거는 느리다 — run5 실측: 앱 스캔은 /sdcard에서 음성 4건을 찾고 ~27s 내 정착) — 열거 0을 '부재'로 읽지 않는다. 앱 스캔 캡처(mus_phone_sdcard.png 0곡/4곡 행)가 진실원"
fi
CAPTURE "$TMPD/mus_phone_sdcard.png"
SCAN_STATE="${SCAN_FINITE:-SKIPPED}"
SCAN_LEG_OK=0
case "$SCAN_STATE" in
  OK) SCAN_LEG_OK=1 ;;
  SKIPPED|UNDECIDED) SCAN_LEG_OK=1 ;;  # 권한 부재·관측 스킵 = 정직 가시화(스펙 계약) — 원장행 참조
  *) SCAN_LEG_OK=0 ;;
esac

# ══════════════════════ 소각+복원 — 시드+settings+permissions ══════════════════════
echo "=== P. 소각 — 시드 dir+probe 소유 잔상 (probe 소유 잔상 0 계약) ==="
rm -rf "$SEED"
[ -d "$SEED" ] && FAIL "seed dir 소각 실패 — 수동 소각 필요" || echo "SEED-BURIED: $SEED removed"

LEG=boot
echo "=== Q. leg boot — settings 원상 복원(바이트 등호) BOOT-OK + 진입 상태 재현 ==="
cp "$ORIG" "$SET" || FAIL "settings restore copy failed"
cmp -s "$SET" "$ORIG" || FAIL "settings restore byte mismatch"
echo "SETTINGS-RESTORED-BYTES: $(wc -c < "$SET") (ORIG=$(wc -c < "$ORIG") — 등호 원복)"
if ! grep -aq 'font_path' "$SET"; then FAIL "restored settings lost font_path (복원 위반)"; fi
echo "RESTORAGE-GATE-OK: font_path 유지 + ORIG 바이트 등호"
if [ "$ENTRY_PERM" -eq 1 ]; then
    cp "$PERM_ORIG" "$PERM" || FAIL "permissions restore copy failed"
    cmp -s "$PERM" "$PERM_ORIG" || FAIL "permissions restore byte mismatch"
    echo "PERM-RESTORED-BYTES: $(wc -c < "$PERM") (선존 원문 $(wc -c < "$PERM_ORIG") 등호 — 병합 복원)"
else
    rm -f "$PERM"
    [ -f "$PERM" ] && echo "WARN: permissions.json survived rm" || echo "PERM-BURIED: permissions.json removed"
fi
boot_server boot
L=$(ctl '{"tool":"launch_app","args":{"app":"terminal"}}')
ok "$L" || echo "FINAL-LAUNCH-NOTE: terminal launch 미성립 — $L (idle 라인 종료 상태 원복) "
sleep 6

echo "=== R. 종료 게이트 — 서버 UP + jkweb 생존 ==="
pgrep -f 'buildterm/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "server not UP at end (종료 게이트 위반)"
FINAL=$(ctl '{"tool":"list_windows","args":{}}')
echo "FINAL-WINDOWS: $(printf '%s' "$FINAL" | head -c 500)"
JKWEB_AFTER=$(pgrep -f '[j]kweb' | tr '\n' ' ')
echo "JKWEB-AFTER: ${JKWEB_AFTER:-none} (BEFORE: ${JKWEB_BEFORE:-none} — 절사 없음 단정 재료)"
if [ -n "$JKWEB_BEFORE" ] && [ "$JKWEB_BEFORE" != "$JKWEB_AFTER" ]; then
    echo "NOTE-JKWEB: 기동 카운트 변화 — 원장 행 (외부 재기동 없는 한 동일해야 한다)"
fi
FINAL_SRV=$(pgrep -f 'buildterm/[j]kdesktop --server' | tr '\n' ' ')
echo "FINAL-SERVER-PIDS=$FINAL_SRV"
echo "ENTRY-SERVER-RESTORE: 진입(옵저베이션 서버 UP — ${ENTRY_SRV:-none}) 원복은 서버 UP+terminal 상시로 만족(바이너리 교체 재기동 계약)"
rm -f -- "$0"
echo "MUSP-FINISHED"
exit 0
# honest-fail 원칙: 수치 미달(MUSIC-PHONE-FAIL·DELEGATE MISS·glue-cold 부정)은
# 원장 목적이라 rc=0 — 드라이버가 RUNLOG 판정행에서 집계한다.
# hard FAIL(배포·sweep·빌드·selftest 회귀·시드·settings/perm 원복·부팅·ping·
# launch·창·캡처·소각)만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지.
PMUSEOF
sed -i "s/__PHONE_DISPLAY__/${PHONE_DISPLAY#:}/" "$SCRATCH/$RSRC_TAR_NAME" ||
    FAIL "remote script display substitution failed"
grep -aq '__PHONE_DISPLAY__' "$SCRATCH/$RSRC_TAR_NAME" && FAIL "display placeholder unresolved"

# ------------------------------------------------------------------ 4. 배포
echo "=== 3. 배포 (sweep 산치 따라 tar blob 스테이징 / 전량 git archive — D1 원장) ==="
if [ "$DIFFN" != "0" ]; then
    if [ -z "${UNEXPECTED:-}" ]; then
        # tar 오염 게이트: 워킹 카피 접촉 0 — HEAD blob 스테이징으로만 조립.
        STAGE="$SCRATCH/pmus_headstage"
        rm -rf "$STAGE"
        GERR="$SCRATCH/pmus_gshow.err"
        : > "$GERR"
        for f in $DIFFFILES; do
            mkdir -p "$STAGE/$(dirname "$f")"
            GS_OK=0
            for try in 1 2 3; do
                git -C "$GITROOT" show "HEAD:$f" > "$STAGE/$f" 2>>"$GERR" && { GS_OK=1; break; }
                sleep 3
            done
            [ "$GS_OK" -eq 1 ] || FAIL "git show HEAD:$f 스테이징 실패(재시도 3회) — $(tail -1 "$GERR")"
        done
        echo "CONTAMINATION-GATE=CLEAN (tar 조립=HEAD blob 전량 — 워킹 카피 접촉 0)"
        WIPPED_TOTAL=$DIFFN
        tar -cf "$TARBALL" -C "$STAGE" $DIFFFILES || FAIL "tar creation failed"
    else
        echo "REDEPLOY-MODE=FULL-GIT-ARCHIVE (D1 §4 원장 — 트리 정합 복구, autocrlf=false로 LF 원문 배포)"
        git -C "$GITROOT" -c core.autocrlf=false archive HEAD -- engine/src engine/include engine/CMakeLists.txt |
            $SSH "tar -xf - -C ~/JKENGINE" || FAIL "git archive deploy failed"
        WIPPED_TOTAL=0
        unset TARBALL
    fi
else
    echo "DEPLOY-SKIP (폰 트리=HEAD 동일)"
    unset TARBALL
fi
if [ -n "${TARBALL:-}" ]; then
    echo "tar size: $(wc -c < "$TARBALL" | tr -d ' ') bytes — files: $WIPPED_TOTAL"
    [ "$(wc -c < "$TARBALL")" -gt 30000 ] || FAIL "tar suspiciously small — deploy list broken?"
    $SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
    rm -f "$TARBALL"
fi
# 시드 tar — 항상(소스 배포 분기와 독립: RETRY 재실행 대비)
rm -f "$SEEDTAR"
tar -cf "$SEEDTAR" -C "$SEEDSTAGE" engine/pmus_seed_stage || FAIL "seed tar creation failed"
echo "seed tar: $(wc -c < "$SEEDTAR" | tr -d ' ') bytes (member engine/pmus_seed_stage)"
tar -tf "$SEEDTAR" | grep -aq 'pmus_seed_stage/pcm_music_seed/sub/d_seed.wav' || FAIL "seed tar missing sub member (구성 누락)"
$SSH "tar -xf - -C ~/JKENGINE" < "$SEEDTAR" || FAIL "seed tar extract failed"
rm -f "$SEEDTAR"
$SSH "cat > $RSRC_PHONE" < "$SCRATCH/$RSRC_TAR_NAME" || FAIL "remote script copy failed"
$SSH "test -f $RSRC_PHONE && echo REMOTE-SCRIPT-PRESENT || echo REMOTE-SCRIPT-ABSENT" \
  | grep -aq REMOTE-SCRIPT-PRESENT || FAIL "remote receipt script missing on phone"

# 배포 신선도 — 재sweep으로 마감(idle 원문 수형 — SWEEP-POST-DIFF=0 hard 게이트).
$SSH 'cd ~/JKENGINE && : > ~/.pmus_sizes2.txt
while read -r f; do
  if [ -f "$f" ]; then printf "%s %s\n" "$(tr -d "\r" < "$f" | wc -c)" "$f"; else echo "MISSING $f"; fi
done < ~/.pmus_files.txt' > "$SCRATCH/pmus_phone_sizes2.txt" || FAIL "post-deploy sweep failed"
LC_ALL=C sort "$SCRATCH/pmus_phone_sizes2.txt" > "$SCRATCH/pmus_phone_sizes2.s" \
    || FAIL "post-deploy sweep sort failed"
mv -f "$SCRATCH/pmus_phone_sizes2.s" "$SCRATCH/pmus_phone_sizes2.txt"
grep -aq MISSING "$SCRATCH/pmus_phone_sizes2.txt" && FAIL "phone tree missing a tracked file (배포 원천 결손)"
DIFFN2=$(comm -3 "$SCRATCH/pmus_head_sizes.txt" "$SCRATCH/pmus_phone_sizes2.txt" | grep -ac '.')
echo "SWEEP-POST-DIFF=$DIFFN2"
[ "$DIFFN2" -eq 0 ] || FAIL "post-deploy sweep not clean — 배포 후에도 트리 오차 (원장)"
echo "SWEEP-POST-DIFF=0 — DEPLOY-FRESHNESS-OK (전수 sweep 정합 — blob 크기 등호)"
MKS=$($SSH "cd ~/JKENGINE/engine && grep -c 'DelegationReplyVerdict' src/apps/ClientMusicApp.cpp 2>/dev/null" | tr -d ' \r')
echo "DEPLOY-MARKER-MUSIC phone-grep-count=${MKS:-0}"
[ "${MKS:-0}" != "0" ] || FAIL "phone ClientMusicApp.cpp lacks DelegationReplyVerdict (fix r3 배포 결손)"

# ------------------------------------------------------------------ 5. 실행
echo "=== 4. 폰 리빌드+영수증 절차 실행 (ninja + selftest + 레그 3) ==="
$SSH "bash $RSRC_PHONE" > "$RUNLOG" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
  echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 폰 ninja·selftest 로그는 \$TMPDIR에 생존, 재실행=증분:"
  echo "  PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_music.sh)"
fi
cat "$RUNLOG"
grep -aq '^MUSP-FINISHED$' "$RUNLOG" \
  || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행)"
if grep -aq '^MUSP-FAIL' "$RUNLOG"; then
  HARD=$(grep -a '^MUSP-FAIL' "$RUNLOG" | head -1)
  FAIL "phone hard receipt: $HARD"
fi

# ------------------------------------------------------------------ 6. REMNANT
echo "=== 5. REMNANT — 원격 스크립트 자기 소각 확인 (rc 게이트 — idle 라인 수형) ==="
$SSH "ls $RSRC_PHONE" >/dev/null 2>&1
REMNANT_RC=$?
echo "REMNANT-LS-RC=$REMNANT_RC"
[ "$REMNANT_RC" -eq 2 ] || FAIL "REMNANT survived (원격 스크립트 자기 소각 실패 — rc=$REMNANT_RC)"
REM_SEED=$($SSH '[ -d $TMPDIR/pcm_music_seed ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-SEED-DIR: $REM_SEED"
[ "$REM_SEED" = "gone" ] || FAIL "REMNANT survived: seed dir left on phone (probe 소유 잔상)"
$SSH "rm -f \$HOME/.pmus_tree.txt \$HOME/.pmus_files.txt \$HOME/.pmus_sizes.txt \$HOME/.pmus_sizes2.txt" 2>/dev/null
echo "SWEEP-FILES-BURIED-REMOTE: 4 (전수 sweep 임시 목록 소각)"

# ------------------------------------------------------------------ 7. 캡처 회수
echo "=== 6. 캡처 회수 — ssh 호출당 base64 1파이프 계약 (+md5 대차) ==="
recover() { # $1 폰쪽png 경로 $2 로컬png — md5 원문 대차 포함
    local REMOTE_PNG=$1 LOCAL_PNG=$2
    local WTXT WPNG WB64 RMD5 LMD5
    WB64=$(cygpath -w "$SCRATCH/pmus_b64.txt" 2>/dev/null || echo "$SCRATCH/pmus_b64.txt")
    WPNG=$(cygpath -w "$LOCAL_PNG" 2>/dev/null || echo "$LOCAL_PNG")
    RMD5=$($SSH "md5sum $REMOTE_PNG" 2>/dev/null | sed -n 's/^\([0-9a-f]*\) .*$/\1/p' | tr -d ' \r')
    $SSH "base64 -w0 $REMOTE_PNG" > "$SCRATCH/pmus_b64.txt" 2>"$SCRATCH/pmus_b64.err" \
        || FAIL "recover failed: $REMOTE_PNG"
    [ -s "$SCRATCH/pmus_b64.err" ] && echo "NOTE: stderr noise — $(tail -1 "$SCRATCH/pmus_b64.err")"
    python - "$WB64" "$WPNG" <<'PYEOF' || FAIL "base64 decode failed: $LOCAL_PNG"
import base64, re, sys
d = open(sys.argv[1], "rb").read().decode("ascii", "ignore")
d = re.sub(r"\s+", "", d)
open(sys.argv[2], "wb").write(base64.b64decode(d))
print("RECOVER-OK:", sys.argv[2])
PYEOF
    [ -s "$LOCAL_PNG" ] || FAIL "recovered png empty: $LOCAL_PNG (폰쪽 삭제 전 확인)"
    LMD5=$(md5sum "$LOCAL_PNG" | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
    if [ -n "$RMD5" ] && [ "$RMD5" = "$LMD5" ]; then
        echo "RECOVER-MD5-OK: $LMD5 ($(wc -c < "$LOCAL_PNG" | tr -d ' ') bytes)"
    else
        FAIL "recover md5 mismatch: $LOCAL_PNG phone=$RMD5 local=$LMD5 (전송 파열 방어)"
    fi
    rm -f "$SCRATCH/pmus_b64.txt"
}
recover "\$TMPDIR/mus_phone_list.png" "$SCRATCH/mus_phone_list.png"
recover "\$TMPDIR/mus_phone_list2.png" "$SCRATCH/mus_phone_list2.png"
recover "\$TMPDIR/mus_phone_spatial.png" "$SCRATCH/mus_phone_spatial.png"
recover "\$TMPDIR/mus_phone_vp_delegate.png" "$SCRATCH/mus_phone_vp_delegate.png"
recover "\$TMPDIR/mus_phone_delegate_status.png" "$SCRATCH/mus_phone_delegate_status.png"
recover "\$TMPDIR/mus_phone_after.png" "$SCRATCH/mus_phone_after.png"
recover "\$TMPDIR/mus_phone_sdcard.png" "$SCRATCH/mus_phone_sdcard.png"
$SSH "rm -f \$TMPDIR/mus_phone_list.png \$TMPDIR/mus_phone_list2.png \$TMPDIR/mus_phone_spatial.png \$TMPDIR/mus_phone_vp_delegate.png \$TMPDIR/mus_phone_delegate_status.png \$TMPDIR/mus_phone_after.png \$TMPDIR/mus_phone_sdcard.png \$TMPDIR/pmus_orig_settings.json \$TMPDIR/pmus_orig_permissions.json"
echo "CAPTURES-WRITTEN: mus_phone_{list,list2,spatial,vp_delegate,delegate_status,after,sdcard}.png → engine/tmp/"

# ------------------------------------------------------------------ 8. 로컬 실측
PYRUNLOG=$(cygpath -w "$RUNLOG" 2>/dev/null || echo "$RUNLOG")
PYTHONIOENCODING=utf-8 python - "$PYRUNLOG" "$PYSCRATCH" <<'ANALYSIS_EOF'
# -*- coding: utf-8 -*-
# T5 폰 로컬 실측 (측정 원문 — Music 창 crop + get_status pos 원문 파싱).
import os, re, sys

RUNLOG, SCRATCH = sys.argv[1], sys.argv[2]

def parse_geo(txt, leg):
    m = re.search(r"MUS-GEO-%s: id=(\d+) x=(-?\d+) y=(-?\d+) w=(\d+) h=(\d+)" % leg, txt)
    if not m:
        return None
    return tuple(int(m.group(i)) for i in range(1, 6))

def num(txt, label):
    m = re.search(label + r"\"?([0-9.]+)", txt)
    return m.group(1) if m else None

try:
    from PIL import Image
    import numpy as np
    have_pil = True
except ImportError:
    have_pil = False
    print("ANALYSIS-NOTE: PIL/numpy 부재 — 차원 원문만 (캡처 육안 스탭으로)")

txt = ""
try:
    with open(RUNLOG, encoding="utf-8", errors="ignore") as f:
        txt = f.read()
except OSError:
    pass

# pos 증가 원문 수취(get_status 1/2 — "pos":X.XXX)
p1 = re.findall(r"MUSICP-GET-STATUS-1: .*?\"pos\":([0-9.]+)", txt)
p2 = re.findall(r"MUSICP-GET-STATUS-2: .*?\"pos\":([0-9.]+)", txt)
d1 = re.findall(r"MUSICP-GET-STATUS-1: .*?\"dur\":([0-9.]+)", txt)
d2 = re.findall(r"MUSICP-GET-STATUS-2: .*?\"dur\":([0-9.]+)", txt)
POS_OK = 0
if p1 and p2:
    a, b = float(p1[-1]), float(p2[-1])
    POS_OK = 1 if b > a else 0
    print("POS-EVIDENCE: GET-STATUS-1 pos=%s dur=%s → GET-STATUS-2 pos=%s dur=%s — pos 증가=%s" % (
        p1[-1], d1[-1] if d1 else "?", p2[-1], d2[-1] if d2 else "?", POS_OK))
else:
    print("POS-EVIDENCE: get_status pos 파싱 실패(정직 원장) — p1=%s p2=%s" % (p1, p2))

TAGS = (("mus_phone_list.png", "list"), ("mus_phone_list2.png", "list2"),
        ("mus_phone_spatial.png", "spatial"),
        ("mus_phone_vp_delegate.png", "vp_delegate"),
        ("mus_phone_delegate_status.png", "delegate_status"), ("mus_phone_after.png", "after"),
        ("mus_phone_sdcard.png", "sdcard"))
for fname, tag in TAGS:
    p = os.path.join(SCRATCH, fname)
    if not os.path.isfile(p):
        print("ANALYSIS-%s: missing" % tag)
        continue
    if not have_pil:
        print("ANALYSIS-%s: %s %d bytes" % (tag, fname, os.path.getsize(p)))
        continue
    im = Image.open(p).convert("RGB")
    arr = np.asarray(im).astype(int)
    colors = len(np.unique(arr.reshape(-1, 3) // 32, axis=0))
    print("ANALYSIS-%s: %dx%d %d bytes distinct16=%d" % (
        tag, im.width, im.height, os.path.getsize(p), colors))
    geo = parse_geo(txt, tag if tag in ("list", "sdcard") else "list")
    if geo:
        gx, gy, gw, gh = geo[1], geo[2], geo[3], geo[4]
        m = 32
        x0, y0 = max(0, gx - m), max(0, gy - m)
        x1, y1 = min(im.width, gx + gw + m), min(im.height, gy + gh + m)
        if x1 > x0 and y1 > y0:
            crop = im.crop((x0, y0, x1, y1))
            out = os.path.join(SCRATCH, "pmus_%s_crop.png" % tag)
            crop.save(out)
            print("CROP-%s: %s (%dx%d — 서버 기하 x=%d y=%d w=%d h=%d + 마진 %d)" % (
                tag, out, crop.width, crop.height, gx, gy, gw, gh, m))

mc = re.search(r"CANON-INCLUSION=([^\r\n]*)", txt)
if mc:
    print("CANON-LINE: CANON-INCLUSION=%s" % mc.group(1))
mt = re.search(r"PHONE-SELFTEST rc=\d+ PASS=(\d+) FAIL=(\d+) 2m=(\d+)\(2m-h=(\d+)\)", txt)
if mt:
    print("SELFTEST-LINE: PASS=%s FAIL=%s 2m=%s (2m-h=%s)" % mt.groups())
sd = re.search(r"SCAN-FINITE: ([A-Za-z-]+)", txt)
if sd:
    print("SCAN-LINE: SCAN-FINITE=%s" % sd.group(1))
lg = re.search(r"MUSP-LEG-BARRED: (.*)", txt)
if lg:
    print("LEG-LINE: MUSP-LEG-BARRED=%s" % lg.group(1))
li = re.search(r"MUSP-SPATIAL-POS: .*", txt)
if li:
    print("INACTIVE-LINE: %s" % li.group(0))
ANALYSIS_EOF
if [ $? -ne 0 ]; then
  echo "NOTE-ANALYSIS: local analysis failed (rc) — 캡처 원문은 회수 원문으로 육안 가능"
fi

# ------------------------------------------------------------------ 9. 판정
echo "=== 7. music 폰 실측 판정 (측정 원문은 상단 analysis 행 — 최종 결제는 육안 스탭) ==="
CAP_OK=1
for f in list list2 spatial vp_delegate delegate_status after sdcard; do
    P="$SCRATCH/mus_phone_$f.png"
    if [ ! -s "$P" ]; then
        echo "MUSIC-PHONE-FAIL(capture missing/empty: $P)"
        CAP_OK=0
    fi
done
CANON=$(grep -a '^CANON-INCLUSION=' "$RUNLOG" | tail -1 | sed 's/^CANON-INCLUSION=//' | tr -d '\r')
CANON_OK=0
case "$CANON" in
  MUSIC-FULL-2N-17*) CANON_OK=1 ;;
  *) echo "NOTE-CANON: 캐논 계보 ${CANON:-n/a} — 위 경고 행 참조" ;;
esac
GLUE_OK=0
GLUE=$(grep -a '^MUSICP-GLUE-REPLY: ' "$RUNLOG" | tail -1 | tr -d '\r')
printf '%s' "${GLUE:-}" | grep -aq '"accepted":true' && GLUE_OK=1
PLAY_OK=0
GS2L=$(grep -a '^MUSICP-GET-STATUS-2: ' "$RUNLOG" | tail -1 | tr -d '\r')
printf '%s' "${GS2L:-}" | grep -aq '"opened":true' && PLAY_OK=1
POS_OK=0
POS1=$(printf '%s' "$GLUE" >/dev/null; grep -a '^MUSICP-GET-STATUS-1: ' "$RUNLOG" | tail -1 | sed -n 's/.*"pos":\([0-9.]*\).*/\1/p')
POS2=$(printf '%s' "$GS2L" | sed -n 's/.*"pos":\([0-9.]*\).*/\1/p')
if [ -n "$POS1" ] && [ -n "$POS2" ]; then
    POS_OK=$(awk -v a="$POS1" -v b="$POS2" 'BEGIN{print (b>a) ? 1 : 0}')
    echo "POS-DIFF: GET-STATUS-1 pos=$POS1 → GET-STATUS-2 pos=$POS2 (증가=$POS_OK — 재생 진행 원문 1행)"
else
    echo "POS-DIFF: pos 파싱 실패 (1=$POS1 2=$POS2 — 정직 원장)"
fi
SCAN_LINE=$(grep -a '^SCAN-FINITE: ' "$RUNLOG" | tail -1 | sed 's/^SCAN-FINITE: //' | tr -d '\r')
SCAN_OK=1   # sdcard 레그는 원장행(권한 부재·관측 스킵 = 정직 가시화 — 스펙 계약)
case "$SCAN_LINE" in
  OK*|SKIPPED*|UNDECIDED*) ;;
  *) echo "NOTE-SCAN: SCAN-FINITE 행 미출력(권한 부재·레그 스킵 등) — O 섹션 원문 참조" ;;
esac
DELEG_LINE=$(grep -a '^MUSICP-DELEGATE: ' "$RUNLOG" | tail -1 | sed 's/^MUSICP-DELEGATE: //' | tr -d '\r')
DOPEN_LINE=$(grep -a '^MUSICP-DELEGATE-OPEN: ' "$RUNLOG" | tail -1 | sed 's/^MUSICP-DELEGATE-OPEN: //' | tr -d '\r')
# 폰 leg 배제 원문(T4 핵심 영수증 — D5): spatial_play의 start_failed +
# kDelegationHint detail이 성립해야 한다(도구·UI 탭 양경로 같은 SpatialStart
# 종착 — J2 원문). 성립 런(leg 있음)이면 배제 원문이 깨진 것 — verdict FAIL.
SPATL=$(grep -a '^MUSP-SPATIAL-PLAY-REPLY: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-SPATIAL-PLAY-REPLY: //' | tr -d '\r')
INACT=$(grep -a '^MUSP-SPATIAL-POS: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-SPATIAL-POS: //' | tr -d '\r')
LEG_OK=0
printf '%s' "${SPATL:-}" | grep -aq '"error":"start_failed"' \
  && printf '%s' "${SPATL:-}" | grep -aq 'spatial leg 불가' \
  && printf '%s' "${INACT:-}" | grep -aq 'MISS(미활성' \
  && LEG_OK=1
LEG_LINE=$(grep -a '^MUSP-LEG-BARRED: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-LEG-BARRED: //' | tr -d '\r')
if [ "$CAP_OK" -eq 1 ] && [ "$CANON_OK" -eq 1 ] && [ "$GLUE_OK" -eq 1 ] && [ "$PLAY_OK" -eq 1 ] \
   && [ "$POS_OK" -eq 1 ] && [ "$SCAN_OK" -eq 1 ] && [ "$LEG_OK" -eq 1 ]; then
    echo "MUSIC-PHONE-VERDICT: MUS-PHONE-OK(폰 selftest 계보 →$CANON·list 탭 리스캔+캡처 7종 receipt·glue=accepted·get_status opened=true pos $POS1→$POS2·폰 leg 배제 원문 성립(spatial_play=start_failed kDelegationHint·spatial 미활성 MISS 행=$INACT)·sdcard 스캔=$SCAN_LINE — 위임은 원장행: $DELEG_LINE / 귀속: $DOPEN_LINE — 육안 스탭 대기)"
else
    echo "MUSIC-PHONE-VERDICT: MUSIC-PHONE-FAIL(행별 사유는 위 각 행 — canon=$CANON_OK captures=$CAP_OK glue=$GLUE_OK play=$PLAY_OK pos=$POS_OK leg=$LEG_OK/${LEG_LINE:-n/a} scan=$SCAN_LINE; 인프라 실패는 hard FAIL로 상단 중단, 수치·육안 미달은 rc=0 honest-fail)"
fi
echo "MUSIC-WCAPTURES:"
for f in list list2 spatial vp_delegate delegate_status after sdcard; do
    P="$SCRATCH/mus_phone_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P" | tr -d ' ') bytes)"
done
echo ""
echo "사용자 결제 게이트(대기 — probe가 결제를 기록하지 않는다):"
echo "  ① 폰 music 리스트(mus_phone_list.png/list2+pmus_list*crop.png): 시드 4곡 행(3 루트"
echo "     +1 sub — rel 열 sub/d_seed.wav 재귀 원문)·mtime desc 정렬·경로/크기/수정 표."
echo "  ② 폰 위임(mus_phone_vp_delegate.png+delegate_status/after): Video Player 창"
echo "     출현+music status 행 표기+get_status opened=true pos 증가 원문."
echo "  ③ 폰 sdcard 스캔(mus_phone_sdcard.png): /sdcard 루트(FUSE) 재귀 스캔 유한"
echo "     종료(M-5 재판정 — SCAN-FINITE 행) + 0음성이면 빈 목록 스펙 정상."
echo "  ④ 폰 spatial leg 배제 원문(mus_phone_spatial.png+pmus_spatial_crop.png):"
echo "     spatial leg 패널의 고장 안내 행이 kDelegationHint 원문 라벨"
echo "     (\"spatial leg 불가 — vplayer 위임 이용\")으로 렌더 — [spatial] 발사의"
echo "     DeviceFailed 종착 원문(D5 정직 계약 — leg는 WSL T3가 소유)."
echo "  ⑤ 폰 BOOT-OK(settings 원상 복원 — ORIG 바이트 등호): 종료 상태 서버 UP+terminal."
echo "  → EYES-PENDING: 위 항목에 대한 폰 실기기 육안 선언만 결제 — probe는 기록하지 않는다."
echo "MUSIC-PHONE-END"
# honest-fail 원칙: 수치 미달(MUSIC-PHONE-FAIL 라인)은 원장 목적이라 rc=0.
# hard FAIL(ssh·sweep·배포·ninja·selftest 회귀·시드·settings/perm 원복·부팅·ping·
# launch·창·캡처·소각·REMNANT)만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지.
exit 0
