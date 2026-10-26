#!/usr/bin/env bash
# T4 WSL music 실측 probe (스펙 2026-10-09-music-library — plan task-4).
#
# T3 확장 (plan 2026-10-10-music-dirs-ui, 2026-10-10):
#   * 16b dirs 도구 세그먼트 — music_dir_add/remove/list(등록 6종 — spatial
#     3종 병합 유지 원존)의 settings.json 실측 쓰기: 합성 폴더($TMPDIR) 등록
#     →list 원문→settings.json 원문(music.dirs 실기록+기존 audio/retention 키
#     보존 — C1 보존의 라이브 수취)→제거→원복 바이트 등호(④ T5 수형 — seed를
#     ComposeKeyed 출하형 compact으로 심어 등호 판정 가능).
#   * 16c I-1 라이브 실측(T2 리뷰 M-1r) — CP949 혼입(quickjs 부적합) 원문
#     시딩 → 부팅 stderr 경고 1행 → 쓰기 정문(settings_set) 거부 detail 원문
#     → 저장소 스캐너 수용(remove 1콜 — CP949 문서에서 dirs 소각) →
#     [패널 제거 치유 루트](UI [폴더 관리] 토글→[제거] 클릭 — 도구 인자는
#     quickjs JSON 문자열 = UTF-8 검사라 CP949 바이트 수송 불가인 원문 근거
#     동봉) → 치유 후 music_dir_add 재시도+settings_set 성공(부팅 재기동
#     불요) → 백업 원복 바이트 등호. 서버 재기동 1회(부팅 경고 캡처).
#
# 목적: T1-T3 music 모듈(디렉토리 탭 스트립+비동기 스캔 워커+경로/크기/수정
#   표+이름 필터+더블클릭 vplayer 위임 T3 폴백)을 WSL 실측한다 —
#   부팅 → launch_app music → 탭 클릭(리스캔 도착 대기) → 리스트 캡처 →
#   필터 타이핑 캡처 → 표행 더블클릭(위임 — vplayer 콜드 부팅 경기 포함) →
#   Video Player 창 출현 + get_status 재생 수치 → 접착제 직행(app_tool open) →
#   get_status 재실측 → 복원.
#
# 계약 원문(승계 — wsl_gallery.sh 전체 수형):
#   * 테스트 미디어 — **합성 WAV**(python3 wave stdlib, 자가 생성 — 엔진
#     소유): i:\@keep 등 사용자 미디어 사용 금지(19금 계약 — vplayer 원문).
#     ffmpeg 부재 실측(WSL Ubuntu-24.04) — wave 모듈이 의존 0 경로.
#   * music.dirs 주입 — buildwsl/state/settings.json 1행(music.dirs 배열,
#     갤러리가 gallery.dirs를 주입하는 수형은 아니고 settings 부재 ENTRY를
#     기록+원복한다 — 전면 allow 런타임 파일은 프로브 소유 아님 계약).
#     스캔 폴더는 **$TMPDIR(/tmp) 아래 임시 폴더** — WSL 클라가 직접 스캔.
#   * idle 측정 불요 — 이 probe는 계약·모양·위임만(idle 영수증은 이 라인
#     스코프 밖 — client_idle 계열 probe가 소유).
#   * spatial leg 변형(T3 신설 — 스펙 2026-10-10-music-spatial-leg): leg 런타임
#     실측의 최초 동작 실측이 되는 세그먼트. SPATIAL_PLAYER_ROOT env는
#     **호출자 전달 축**으로만 쓴다(커밋 본문에 경로 리터럴을 쓰지 않는
#     계약 — wsl.exe -- env SPATIAL_PLAYER_ROOT=<spatial-player 루트> bash ...).
#       LEG_MODE=1(설정) — scratch(/tmp/mus_scratch) cmake+build로
#         jkapp_music.so를 leg 있는 빌드로 만들어 buildwsl에 배포한다
#         (시차 배치 — buildwsl 빌드/셀프테스트가 끝난 뒤 scratch 빌드.
#         동시 -j는 공유 트리 I/O 경합 fail 실측 원장). END에서 relink+
#         scratch 소각으로 원복(relink 후 AL 심볼 0 — 원복 강도 영수증).
#       LEG_MODE=0(미설정) — leg-less .so 그대로: spatial_play는
#         DeviceFailed 종착(kDelegationHint 원문 — fail-closed 영수증).
#     어느 변형에서든 ALC 디바이스가 실패하면(WSLg 환경 가능성) start_failed
#     detail=kDelegationHint의 라벨 표기가 **정상 영수증**이다 —
#     LEG-FALLBACK-OK(폰 leg는 T4 몫).
#   * 초장 경로 트랙(M-1 승계 — T2 review: spatial_status의 절단 정직 가드는
#     path 이스케이프 768B 버퍼 초과 축에서만 도달): ~1KB 경로의 트랙 1건을
#     시드하고 spatial_status가 truncated:true(정직 축소 전문)를 내는 원문을
#     실측한다. 단 truncation은 legState_.path가 채워진 뒤(디코더 open 성공)
#     봉합이므로, ALC 실패 런(path 미채움)에서는 MISS 원장이 정상이다.
#   * 위임 접착제 — app_tool open/get_status는 **직행 2콜**로 검증한다(music
#     앱을 경유하지 않는다 — app_tool은 남의 앱에 보내는 도구 허브 릴레이).
#     music 창의 실제 더블클릭 위임은 send_input 2탭(합성 마우스 — 선행
#     MouseMove 필수 원문 계약)으로 시도하고, ImGui 더블클릭 시간창
#     (MouseClickedTime 0.30s 기본)에 agentctl 왕복이 못 미치면 그 사유를
#     원장에 남긴다(MUSIC-DELEGATE 라인 — honest-fail). 위임 open의 도착은
#     **직행 open을 넣기 전** get_status 폴링으로 귀속한다(v2 실측 렛슨 —
#     직행 open이 먼저 가면 귀속이 사라진다).
#   * T3 콜드 경기 관측 (T4 재실측 — 라인 신설 결함 원장): 콜드 위임의 open은
#     vplayer에 도달하지 않았다 — 진원 = 서버 즉답 경로(:6747)의 봉투
#     ok=고정 1 × PollReplies의 reply.ok 성공 분류(ClientMusicApp :393) —
#     sync unknown_app_tool 거부가 "vplayer 재생 요청됨"으로 읽혀 재청구
#     크레딧이 즉시 소멸한다(폴백 자기 소멸 — MUSIC-DELEGATE-OPEN MISS
#     라인+리포트 원장). 접착제 직행 open은 등록 후라 정상(accepted+opened).
#   * 스캔 취소 경계 실측 (T3 신설 — 플랜 2026-10-10-music-scan-cancel
#     task-3): 스캔 진행 중 창 닫기(소멸자 취소 join)·폴더 제거(취소 1발+
#     도착 멤버십 필터)의 **런타임 영수증** 세그먼트(16d). 대형 합성 트리
#     (빈 .wav 셸 $BIG_SUBS 하위 × $BIG_FILES 파일 × 2 트리 — 스캔은 내용을
#     읽지 않는다 — MusicModel.h 원문)로 스캔을 수 초 지속시킨다. ① 탭 전환
#     → 스캔 중 캡처 → 창 close(서버 close_window 도구 — wsl_chat_close.sh
#     선형 수형 — permissions 부재 Deny 규약이라 프로브 소유 파일에 allow
#     스테이지) → close→소멸 시차(취소 join ms급의 정직 영수증 — 취소가
#     없으면 워커 join이 완주 스캔 잔량을 견딘다)·로그 델타 crash 마커 0 →
#     재스폰 → 도구 응답 원문 ② 스캔 중 music_dir_remove → stderr 폐기 진단
#     1행(도착 시차가 취소 성립의 판정축 — 취소 없으면 460k 완주 후 도착)
#     → 클라 생존(list/list_windows) 원문 → settings 원문 ③ 취소 후 재스캔
#     정착 캡처(pub2 채택 — 곡 카운터 EYES)+캐논 등호(코드 무변경 — 원장).
#   * 16b2 [T3 — #97 브라우저 실측 세그먼트 (플랜 2026-10-11-music-dir-browser
#     task-3)]: 도구 경유 불가(UI 탭 실측) — send_input 클릭 브래킷(16c 수형)+
#     PX 차분 착탄 판정(16d 수형 — tapq·landtap·mus_px를 tap() 뒤로 위상 이동,
#     본문 무변경)으로 [폴더 관리]→[찾아보기] 브라우저를 실측한다(합성 트리
#     sub_a·sub_b 2건 등장→JoinDir 진입→Parent 복귀→[이 폴더 추가] 원문 →
#     music_dir_list/settings 원문+종료·재스캔 캡처 → 복귀 바이트 등호). 좌표 =
#     env 상수+차분 판정(16c 정찰 원칙)·MUS_BROW_RECON=1 정찰 런 수형. vplayer
#     재생 창은 세그먼트 입구에서 close_window로 회수(차분 오염 소각 원장).
#   * 16e 태그 표기 세그먼트[T3 신설 — 플랜 2026-10-11-music-tags task-3]:
#     합성 태그 트랙(최소 mp3 + own ID3v2.3 블롭 — T1 2s 수형 재용: 10B 헤더+
#     TIT2/TPE1/TALB 10B 프레임(4BE 크기 — v2.3은 syncsafe 아님)+enc 0 Latin-1
#     ASCII(2s-② 등가 계보)+말미 0x00 패딩, **유효 mp3 프레임 0**(ffmpeg 부재
#     원문 승계 — 태그 파싱만 성립, vplayer open은 openFailed (정직 원문)) +
#     태그 없는 raw mp1건(폴백 행 — 파일명 스템+앨범 "-"). 스캔 포함(mtime
#     desc 최신 — 상단 행) → 목록 표기 캡처+필터(곡명/아티스트 매치 — D5)
#     캡처. 위상 = 16d-5 정착 뒤(restart 이후 세그먼트들이 필터/레이아웃 상태
#     계약을 갖는다 — step 10 필터·16b2 좌표 무오염), 16d-6 뒤 본 원문 유지.
#   * settings.json 상존 수형[T3 — 진입 가드 확장]: WSL 런타임 settings.json이
#     ENTRY에 존재하면(사용자 실세션 잔상 — /mnt/j 등) hard FAIL 대신 바이트
#     스냅샷(SNAP_USER)→END 바이트 등호 원복(16b/16c의 SNAP 수형 — 무손상
#     수취 원문). 라이브 서버 있음 = #98 러너 가드가 앞서 [SKIP] — 경합 0.
#   * 캡처 회수 — 단열 파이프: /tmp 캡처 → engine/tmp 영수증 단일 cp
#     (ls·혼입 금지). mus_wsl_{list,filter,after}.png. 캡처기=wsl_gallery.sh
#     원문의 X11 창 단위 shot(python3 ctypes — XImage 오프셋 원장 포함 전문).
#   * 복원 — 시드 wav+settings.json+permissions.json 소각(ENTRY 기록+원복)·
#     서버 DOWN(ENTRY에 UP이면 재부팅)·REMNANT(시드 dir 잔존=hard FAIL —
#     probe 소유 잔상 0).
#
# 판정(스크립트 규약 — gallery 원문 수형): 인프라 실패(빌드·부팅·ping·launch·
# 창 부재·캡처 실패·시드·소각)만 hard FAIL rc=1. 수치·육안 이벤트는
# MUSIC-OK/MUSIC-FAIL 원장 라인으로 남기고 rc=0(honest-fail) — 최종 판정은
# 캡처 육안 스탭.
#
# 표준: WSLg :0, setsid 부팅, 브래킷 pkill+(-9 에스컬레이션), stale 소켓 rm,
# selftest 출력 WSL 내부 리다이렉트, ninja 풀 로그 리다이렉트.
#
# 실행 표준(WSL 밖 Git Bash에서):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_music.sh \
#     > engine/tmp/wsl_music_run.log 2>&1
# MUS_RECON=1 로 실행하면 리스트 캡처 직후 복원·종료(클릭 좌표 정찰 런 —
# 상수 재보정 후 풀 런).
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "MUS-FAIL: $*"; exit 1; }
LOGDIR=/tmp/mus
mkdir -p "$LOGDIR"
CANON_WSL_SELFTEST=833   # 캔논 WSL 축 [#115 라인 갱신(T4 — opus/m4a 2x 계보 라인 재확인): T1(20e3abf 2x ①②③ +3)·T2(95cb8c6 2x ④-⑧ +5)·T3(840ea41 2x ⑨[round-trip+폴백 2어설션]⑩⑪⑫ +5)·T3 fix r1(ce3f539 — stco/co64 Δ 패치 C1 수형 — 2x-⑫b 2어설션 +2) = 818→833 — T4 조정자 원문행 Win 856/WSL 833 등호·posix 304 등호 유지(하네스 TU에 MusicModel.h 비링크 — T1 M2 원장). 이전 계보: 2w 4(#113 T1 저널 leg — 814→818·t3legs 배포 실측)·814 = 795 + #111 최종 fix 5(f33d41a) + 2v 14(#112 구두 — WSL 814 등호) → 그 이전: 668→795(#111 태그 쓰기 2u ①-⑮ 127건·T4 커밋 본문 원문 Win 818/WSL 795) → 648→668(#99 2s 20·갱신 59 원장) → 646(8e1d84c). opus/m4a 신설 어설션 = 2x 15건(라벨 ①-⑫+⑫b — ⑨ 2어설션·⑫b 2어설션) 전부)
RECEIVE=/mnt/i/progwork/JKENGINE/engine/tmp
SEED_DIR=/tmp/mus_seed
SHOT_PY="$LOGDIR/mus_shot.py"

# 합성 입력(클릭·타이핑) 좌표 — 클라 좌표(desktop point = 창 xy + 오프셋,
# wsl_gallery 셀 0 클릭 원문 수형). 정찰 런의 캡처로 재보정한다:
#   MUS_TAB2  = dir 탭 스트립 2번째 탭(mus_seed) — 라벨행 center
#   MUS_FILTER= 이름 필터 InputText 상단행 center
#   MUS_ROW0  = 표행 0의 경로 셀 필터 1행 상태의 d_seed 행
MUS_TAB2_X=${MUS_TAB2_X:-90}    MUS_TAB2_Y=${MUS_TAB2_Y:-77}
MUS_FILTER_X=${MUS_FILTER_X:-250} MUS_FILTER_Y=${MUS_FILTER_Y:-42}
MUS_ROW0_X=${MUS_ROW0_X:-50}    MUS_ROW0_Y=${MUS_ROW0_Y:-132}
FILTER_TEXT="d_seed"

# [T3(#97)] 브라우저 실측 세그먼트 상수(env 오버라이드 — 16c/16d의 정찰-재보정
# 원문 수형: 클라 상대 좌표 후보 그리드+PX 차분 착탄 판정). 기본 상수는
# 레이아웃 산식의 중앙값(16c 캘리브레이션 원장 — [폴더 관리] 토글 (130,70)·
# 패널 첫 행 [제거] (86,118)·dirmg child 고정 128px → 행 정점 산식) — 런 1
# 캡처(mus_wsl_browse_*.png)로 재보정하는 계약.
#   MUS_BTAB_X  = 브라우즈 탭(mus_browse) — 3탭 스트립의 3번째 슬롯(16d
#     MUS_TAB3 산식 동형 — [music|mus_seed|<대상탭>|폴더 관리▸]).
#   MUS_BTOG_*  = [폴더 관리] 토글 — 탭 수가 늘면 토글 x가 오른쪽으로 밀린다
#     (3탭 ≈ x 220-290 — 후보 그리드가 정찰. 16c의 (130,70)은 2탭 산식).
#   MUS_BBROW_* = [찾아보기] 토글 — 패널 고정 레이아웃(dirmg 128px → InputText
#     행 → 찾아보기 행)이라 탭 수와 무관한 행 산식(≈ 클라 y 250-272).
#   MUS_BSUB_*  = browselist child의 첫 하위 행(sub_a — ≈ 클라 y 340-360 —
#     T2 fix r1 뒤: [이 폴더 추가]/[닫기] 행이 목록 위로 이동해 +24px).
#   MUS_BUP_*   = ".." 행(≈ 클라 y 300-318 — fix r1 뒤에도 무이동).
#   MUS_BADD_*  = [이 폴더 추가](≈ 클라 (~, 306-326) — fix r1: 목록 위 행,
#     상위 ".." 행 바로 아래 — 원존 y 442-457은 T3 런 E/F 합성 클릭 무전달
#     사건의 좌표(흐름 최하단 = tracks child 압착 경계 원장)라 클릭 도달
#     가능 영역 밖 — [닫기]는 SameLine 우측 ≈ x 124 이후).
MUS_BROWSE_TREE=${MUS_BROWSE_TREE:-/tmp/mus_browse}
MUS_BTAB_X=${MUS_BTAB_X:-158}
MUS_BTAB_YS=${MUS_BTAB_YS:-"82 76 88 70"}   # 스트립행(상태행 +17px 시프트로 67-93 — 16c 원장)
MUS_BTOG_XS=${MUS_BTOG_XS:-"228 236 220 244 260 252"}
MUS_BTOG_YS=${MUS_BTOG_YS:-"70 64 76"}
MUS_BBROW_XS=${MUS_BBROW_XS:-"58 44 30 70 20"}
MUS_BBROW_YS=${MUS_BBROW_YS:-"262 256 268 250 274"}
MUS_BSUB_XS=${MUS_BSUB_XS:-"64 86 42 106 30"}
MUS_BSUB_YS=${MUS_BSUB_YS:-"348 342 354 356 360"}   # sub_a·sub_b 행 — T2 fix r1(압착 가드) 뒤의 산식(+24px: 버튼행이 목록 위로)
MUS_BUP_XS=${MUS_BUP_XS:-"24 44 64 14"}
MUS_BUP_YS=${MUS_BUP_YS:-"294 288 300 306"}   # ".." 행 = 클라 289-299(런 D 실측 — fix r1 뒤에도 무이동)
MUS_BADD_XS=${MUS_BADD_XS:-"46 32 62 20 76"}
MUS_BADD_YS=${MUS_BADD_YS:-"321 315 327 333 318"}   # [이 폴더 추가] 행 전용(fix r1: 버튼 행 클라 ~316-336 — ".."행(289-310)과 겹치지 않게 303/309 배제 — 런 2 원장: (46,309) 탭이 ".." 행을 쳐 cwd를 루트로 되돌려 no-op 추가로 강도 축을 상실한 사건)
MUS_BROW_TH=${MUS_BROW_TH:-15}    # 개봉/종료(레이아웃 전이 ~190px) 판정 행 임계
MUS_BROW_TH_TEXT=${MUS_BROW_TH_TEXT:-2}   # 행 클릭(텍스트 교체) 판정 행 임계
MUS_BROW_TH_ADD=${MUS_BROW_TH_ADD:-10}    # [이 폴더 추가] 종료 회귀 판정(텍스트 전이 ~4행과의 겹침 회피 — 중간 임계)

# [T3] I-1 패널 치유 정찰 상수(env 오버라이드 — 정찰 캡처 재보정 선례).
# 라이브 캘리브레이션(2026-10-10 — 클릭 차분×툴팁 증거 캡처 engine/tmp/cal_*)
# : 합성 캡처 = 클라 + (417,173); [폴더 관리] 토글 = 클라 (130,70) — 패널
# 열림+토글 툴팁이 그 마우스 자리에서 뜬 캡처(cal_510_170.png)로 증명; 상태행
# 감김(+17px 시프트) 상태의 (160,77) 클릭은 토글 x존(~91-133) 오른쪽 밖(
# 클릭-트랩 원장). 패널 첫 행 [제거] SmallButton = 클라 (86,118) 실증 치유 콜
# — 이웃 격자 폴백. 클릭 판정: 토글 = 캡처 차분, [제거] = settings.json
# 바이트(무음 실패 금지 원장).
MUS_TOGGLE_TWEAK_X=${MUS_TOGGLE_TWEAK_X:-0}
MUS_TOGGLE_X=${MUS_TOGGLE_X:-130}
MUS_TOGGLE_Y=${MUS_TOGGLE_Y:-70}
MUS_REMOVE_XS=${MUS_REMOVE_XS:-"86 90 94 98"}
MUS_REMOVE_YS=${MUS_REMOVE_YS:-"118 123 113 128"}

# [T3] 스캔 취소 경계 세그먼트 상수. 4탭 상태의 dir 스트립
# [music|mus_seed|mus_big_a|mus_big_b|폴더 관리▸]에서 mus_big_a 탭 = 클라
# 대략 (158,64) — 런 1 캡처 실측(2026-10-10): 스트립 버튼 rect = 클라 y≈56-75
# (y=77 = 하단 2px 밖 — 런 1의 두 탭 모두 미적중/경계 클램프 1회만 성공),
# mus_big_a = 클라 x 123-193 — 중앙값 = env 오버라이드·캡처 재보정 원장.
MUS_TAB3_X=${MUS_TAB3_X:-158}   MUS_TAB3_Y=${MUS_TAB3_Y:-64}
BIGA=/tmp/mus_big_a             # ① close 경계의 스캔 루트(활성 탭)
BIGB=/tmp/mus_big_b             # ② 제거 뒤 dirIndex 승계 클램프가 가리킬 루트 — pub2 지연 도착 지평
BIG_SUBS=${MUS_BIG_SUBS:-40}    # 하위 디렉터 개수 (트리 1개당)
BIG_FILES=${MUS_BIG_PER:-11520} # 하위 1개당 파일 수 — 런 외 캘리브레이션(실제 헤더
                                #   ListAudioFiles 링크 실측: 82k 트랙=0.78-0.83s,
                                #   ~10µs/트랙)의 선형 외삽 ~4.7s/트리 = 수 초 경계
BIG_TRACKS=$((BIG_SUBS * BIG_FILES))   # 트리 1개당 총 트랙 수 (정직 수수 가드 축)

# spatial leg 변형 디스패치(SPATIAL_PLAYER_ROOT env — 위 원문). 경로 원문은
# 로그에도 무표기(엔진 tmp 로그 세척 원장 — 값이 아니라 상태만 기록한다).
LEG_MODE=0
case "${SPATIAL_PLAYER_ROOT:-}" in
    "" )
        echo "MUS-LEG-ENV: unset — fail-closed 변형 런(leg-less .so — spatial_play는 DeviceFailed 종착 전이 원문)" ;;
    /* )
        [ -f "$SPATIAL_PLAYER_ROOT/CMakeLists.txt" ] \
            || FAIL "SPATIAL_PLAYER_ROOT set but no CMakeLists.txt at its root(경로 무표기 원칙 — 호출자 확인 몫)"
        LEG_MODE=1
        echo "MUS-LEG-ENV: set — scratch 빌드 변형 런(env 설정 빌드를 배포한다)" ;;
    * )
        FAIL "SPATIAL_PLAYER_ROOT는 WSL 절대경로(/mnt/... 축)로 전달하지 않은 것 같다(값 무표기 — 호출자 재확인)" ;;
esac
SCRATCH=${MUS_SCRATCH:-/tmp/mus_scratch}   # scratch 빌드 dir — END에서 소각
LONG_SEG=$(printf 'x%.0s' $(seq 1 125))   # 125자 세그먼트(컴포넌트 255 상한 안전)

# ---------------------------------------------------------------- fail-closed 가드
if [ -z "${WSL_DISTRO_NAME:-}" ]; then
  echo "MUS-WFAIL: not inside WSL — 실행 축 계약 위반 (wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_music.sh)"
  exit 1
fi
if [ -n "${PHONE_HOST:-}" ]; then
  echo "MUS-NOTE: PHONE_HOST set — WSL 축은 PHONE_HOST 불요(무시하고 진행)"
fi

# #98 runner guard (park 4 — ENTRY 확장, taskmgr 수형): this runner's soft+
# hard bracketed pkill (step 1, restore paths, 16c-1 재부팅 — 러너 소유) and
# rm은 ENTRY 이후 사슬 전부를 커버한다 — LIVE 서버가 probe-외부(사용자 실세션)
# 이면 kill/엄폐/외부 서버로의 어설션 부착 없이 러너 전체를 [SKIP]+rc=0로
# 정직 종료한다(ENTRY 상태 기록도 하지 않는다).
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "[SKIP] music: a live jkdesktop server is running — #98 runner guard (nothing pkilled, nothing rm'd)"
    exit 0
fi

echo "=== 0. ENTRY 상태 기록 (종료 시 원복 판정용) ==="
ENTRY_UP=0
pgrep -f 'buildwsl/[j]kdesktop' >/dev/null 2>&1 && ENTRY_UP=1
ENTRY_PERM=0
[ -f buildwsl/permissions.json ] && ENTRY_PERM=1
ENTRY_SET=0
[ -f buildwsl/state/settings.json ] && ENTRY_SET=1
ENTRY_SEED=0
[ -d "$SEED_DIR" ] && ENTRY_SEED=1
ENTRY_BIGA=0
[ -d "$BIGA" ] && ENTRY_BIGA=1
ENTRY_BIGB=0
[ -d "$BIGB" ] && ENTRY_BIGB=1
echo "ENTRY-STATE: server-up=$ENTRY_UP permissions.json=$ENTRY_PERM settings.json=$ENTRY_SET seed-dir=$ENTRY_SEED big_a=$ENTRY_BIGA big_b=$ENTRY_BIGB"

[ "$ENTRY_SEED" -eq 0 ] || FAIL "$SEED_DIR already exists — 이전 probe 잔상, 수동 소각 필요"
[ "$ENTRY_BIGA" -eq 0 ] || FAIL "$BIGA already exists — 이전 probe 잔상(16d), 수동 소각 필요"
[ "$ENTRY_BIGB" -eq 0 ] || FAIL "$BIGB already exists — 이전 probe 잔상(16d), 수동 소각 필요"
# [T3 — settings.json 상존 수형] 런타임 settings.json이 ENTRY에 존재하면
# (사용자 실세션/WSL 데스크톱 잔상 — 예: music.dirs=["/mnt/j"]) hard FAIL
# 대신 바이트 스냅샷을 찍고 END에서 바이트 등호 원복한다(16b/16c의 SNAP
# 수형 — T5 수형). 원문 계약("probe는 런타임 settings 파일을 건드릴 수
# 없다")의 보호 본질 = 사용자 파일의 무손상 — 스냅샷+바이트 등호 원복은
# 같은 보호의 등가 성립이고, 라이브 서버 있음은 앞의 #98 러너 가드가
# [SKIP]하므로 파일 경합 0. 원복 불일치 = hard FAIL(무손상 계약의 정직
# 위반 보고).
SNAP_USER="$LOGDIR/mus_settings_user.json"
if [ "$ENTRY_SET" -eq 1 ]; then
    cp buildwsl/state/settings.json "$SNAP_USER" \
        || FAIL "user settings.json snapshot failed"
    cmp -s buildwsl/state/settings.json "$SNAP_USER" \
        || FAIL "user settings.json snapshot mismatch"
    echo "SET-SNAPSHOT: user runtime settings.json snapshotted($(wc -c < "$SNAP_USER") bytes) — END 바이트 등호 원복 계약"
fi

restore_and_exit() { # 정찰 런 등 중간 탈출 — 복원 다형(스텝 12와 동일 본문)
    echo "=== 복원 (early exit) ==="
    pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    rm -rf "$SEED_DIR"
    [ -d "$SEED_DIR" ] && FAIL "seed dir 소각 실패 — 수동 소각 필요" || echo "SEED-BURIED: $SEED_DIR removed"
    if [ "$LEG_MODE" -eq 1 ]; then
        # spatial leg 배포 원복(T3): leg-less .so로 relink — 재링크 뒤 AL 심볼
        # 0이 원복 강도의 영수증이다(0이 아니면 leg 잔상 = hard FAIL).
        rm -f buildwsl/jkapp_music.so
        ninja -C buildwsl jkapp_music.so >"$LOGDIR/mus_relink.log" 2>&1 \
            || FAIL "buildwsl jkapp_music.so relink failed — tail: $(tail -3 "$LOGDIR/mus_relink.log" | tr '\n' ' ')"
        RELINK_SYM=$(ALCOUNT buildwsl/jkapp_music.so)
        echo "MUS-LEG-RESTORE-SYM: alcOpenDevice refs=$RELINK_SYM (0 = leg-less 원복 성립)"
        [ "$RELINK_SYM" -eq 0 ] || FAIL "relinked jkapp_music.so still carries AL symbols($RELINK_SYM) — 원복 불충분"
        rm -rf "$SCRATCH"
        [ -d "$SCRATCH" ] && FAIL "scratch dir 소각 실패 — 수동 소각 필요" \
            || echo "SCRATCH-BURIED: scratch dir removed (트리 잔산 0)"
    fi
    if [ "$ENTRY_SET" -eq 0 ]; then
        rm -f buildwsl/state/settings.json
        echo "SET-BURIED: settings.json removed (ENTRY 부재 원복)"
    else
        # [T3] ENTRY에 사용자 settings.json이 있었다 — 스냅샷 바이트 등호 원복
        # (rm 금지 — 파일은 사용자 소유 잔상).
        cp "$SNAP_USER" buildwsl/state/settings.json \
            || FAIL "user settings.json restore copy failed"
        cmp -s buildwsl/state/settings.json "$SNAP_USER" \
            || echo "MUSIC-FAIL(set-restore: user settings.json 원복 바이트 불일치 — 무손상 계약 위반 원장)"
        echo "SET-RESTORED: user settings.json restored byte-exact ($(wc -c < buildwsl/state/settings.json) bytes)"
    fi
    if [ "$ENTRY_PERM" -eq 0 ]; then
        rm -f buildwsl/permissions.json
        echo "PERM-BURIED: permissions.json removed"
    fi
    if [ "$ENTRY_UP" = "1" ]; then
        env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOGDIR/mus_srv_restore.log" 2>&1 &
        sleep 6
        pgrep -f 'buildwsl/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "restore boot failed"
        echo "END-STATE: WSL 서버 UP 재부팅 — 진입 상태 원복 완료"
    else
        echo "END-STATE: WSL 서버 DOWN — 진입 상태 원복 완료"
    fi
}

echo "=== 1. bracketed pre-clean (살아있는 jkdesktop은 drvfs 재링크를 막는다) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"

echo "=== 2. ninja rebuild (buildwsl — 풀 로그 리다이렉트; 코드 무변경 no-op 확인 몫) ==="
ninja -C buildwsl -j4 >"$LOGDIR/mus_build.log" 2>&1
B_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$B_RC"
[ "$B_RC" -eq 0 ] || FAIL "ninja rebuild rc=$B_RC — tail: $(tail -3 "$LOGDIR/mus_build.log" | tr '\n' ' ')"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"
[ -f buildwsl/jkapp_music.so ] || FAIL "buildwsl/jkapp_music.so missing (T1-T3 배포 확인 실패)"
[ -f buildwsl/jkapp_vplayer.so ] || FAIL "buildwsl/jkapp_vplayer.so missing (위임 대상 배포 확인 실패)"

echo "=== 3. WSL selftest (캐논 $CANON_WSL_SELFTEST — 출력 WSL 내부 리다이렉트) ==="
timeout 420 ./buildwsl/jkdesktop test >"$LOGDIR/mus_st.log" 2>&1
ST_RC=$?
ST_FAIL=$(grep -ac '^\[FAIL\]' "$LOGDIR/mus_st.log")
ST_PASS=$(grep -ac '^\[PASS\]' "$LOGDIR/mus_st.log")
echo "selftest rc=$ST_RC PASS=$ST_PASS FAIL=$ST_FAIL"
grep -aq 'AppSelfTest: 0 failure(s)' "$LOGDIR/mus_st.log" \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_PASS" -ge "$CANON_WSL_SELFTEST" ] \
    || echo "WARN: selftest PASS=$ST_PASS < 캐논 $CANON_WSL_SELFTEST (계보 이탈 — 리포트 원장 기록 필요)"
# music 라인 신설 케이스 (2m 계열): T1-T3가 +27 (progress 원장 등호 근거).
ST2M=$(grep -ac '^\[PASS\] 2m' "$LOGDIR/mus_st.log")
echo "SELFTEST-2M-CASES=$ST2M (music 라인 신설 2m 계열 — 캐논 +27분의 원료)"
# 태그 라인 신설 케이스 (2s 계열): T1이 +20 (캐논 648→668의 원료 — #99).
ST2S=$(grep -ac '^\[PASS\] 2s' "$LOGDIR/mus_st.log")
echo "SELFTEST-2S-CASES=$ST2S (태그 라인 신설 2s 계열 — 캐논 +20분의 원료)"
# [T5(#111) — 태그 쓰기 라인 신설 케이스 (2u 계열)] T1/T2/T4가 ①-⑮ 증분 —
# 캔논 668→795의 원료(T4 커밋 본문 캔논행 원문: Win 795→818·WSL 772→795).
# 수형 = 위 ST2M/ST2S 수취 수형 통합(2u 계보 라인 재확인 — 본 세그먼트 계약).
ST2U=$(grep -ac '^\[PASS\] 2u' "$LOGDIR/mus_st.log")
echo "SELFTEST-2U-CASES=$ST2U (태그 쓰기 라인 신설 2u 계열 ①-⑮ = 127 어설션 1기수 — #111 T4 원문 캔논행 대조 몫)"
# [#115 T4] opus/m4a 라인+구두/저널 승계 케이스 수취(2v/2w/2x — 캔논 상승분
# 분해의 원료 — 833 = 818 + 2x 15(①-⑫ 12라벨·⑨ 2어설션·⑫b 2어설션[fix r1
# ce3f539]) 원장 대조 몫).
ST2V=$(grep -ac '^\[PASS\] 2v' "$LOGDIR/mus_st.log")
ST2W=$(grep -ac '^\[PASS\] 2w' "$LOGDIR/mus_st.log")
ST2X=$(grep -ac '^\[PASS\] 2x' "$LOGDIR/mus_st.log")
echo "SELFTEST-2V-CASES=$ST2V SELFTEST-2W-CASES=$ST2W SELFTEST-2X-CASES=$ST2X (2v 14(#112)·2w 4(#113)·2x 15(#115 ①-⑫+⑫b — ⑨ 2·⑫b 2 어설션[fix r1 ce3f539]) — #115 T4 캔논 원료)"

# === 3b. spatial 변형 빌드+배포 (T3 — 시차 배치 원장 준수) ===
# 2n selftest는 main.cpp 하네스 TU 소비(빌드wsl 설정은 env 미설정으로 고착)
# 이라 셀프테스트는 배포와 무관 — 배포 뒤 재실측 불요(T1 원문 계보 유지).
echo "=== 3b. spatial leg 빌드+배포 (LEG_MODE=$LEG_MODE — buildwsl 빌드 완료 뒤 scratch — 시차 배치 원장) ==="
COUNTER() { sed 's|/mnt/[^ ]*|<WSLPATH>|g'; }   # 로그 tail 열람 경로 위생 세척 원장
ALCOUNT() { # $1=.so — AL 심볼 개수(nm symtab 우선 .symtab 부재 시 dynsym 폴백)
    { nm "$1" 2>/dev/null || nm -D "$1" 2>/dev/null; } | grep -ac 'alcOpenDevice'
}
if [ "$LEG_MODE" -eq 0 ]; then
    UNSET_SYM=$(ALCOUNT buildwsl/jkapp_music.so)
    echo "MUS-LEG-UNSET-SYM: alcOpenDevice refs=$UNSET_SYM (0 = leg 부재 fail-closed 영수증)"
    [ "$UNSET_SYM" -eq 0 ] || FAIL "env 미설정 buildwsl jkapp_music.so에 AL 심볼($UNSET_SYM) — fail-closed 배선 위반"
else
    rm -rf "$SCRATCH"
    echo "SPATIAL-SCRATCH: /tmp/mus_scratch (소스=engine, 경로 env 전달 무표기)"
    env SPATIAL_PLAYER_ROOT="$SPATIAL_PLAYER_ROOT" \
        GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*' \
        cmake -S . -B "$SCRATCH" -G Ninja >"$LOGDIR/mus_spatial_config.log" 2>&1
    C_RC=${PIPESTATUS[0]}
    echo "SPATIAL-CONFIG-RC=$C_RC"
    [ "$C_RC" -eq 0 ] || FAIL "scratch configure rc=$C_RC — tail: $(COUNTER < <(tail -3 "$LOGDIR/mus_spatial_config.log") | tr '\n' ' ')"
    grep -aq 'audio_core\|openal-soft' "$LOGDIR/mus_spatial_config.log" \
        && echo "SPATIAL-CONFIG-SIGNAL: audio_core/openal 흡수 확인(전문은 /tmp 로그만)" \
        || echo "WARN: spatial config log에 audio_core/openal 표식 0건 — 배선 재확인 몫"
    env SPATIAL_PLAYER_ROOT="$SPATIAL_PLAYER_ROOT" \
        GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*' \
        ninja -C "$SCRATCH" jkapp_music.so -j4 >"$LOGDIR/mus_spatial_build.log" 2>&1
    B2_RC=${PIPESTATUS[0]}
    echo "SPATIAL-BUILD-RC=$B2_RC"
    [ "$B2_RC" -eq 0 ] || FAIL "scratch jkapp_music.so build rc=$B2_RC — tail: $(COUNTER < <(tail -3 "$LOGDIR/mus_spatial_build.log") | tr '\n' ' ')"
    [ -f "$SCRATCH/jkapp_music.so" ] || FAIL "scratch jkapp_music.so missing after build"
    SET_SYM=$(ALCOUNT "$SCRATCH/jkapp_music.so")
    echo "MUS-LEG-SYM-SET: alcOpenDevice refs=$SET_SYM (>=1 = leg 설정 축 영수증)"
    [ "$SET_SYM" -ge 1 ] || FAIL "env 설정 jkapp_music.so에 AL 심볼 0건 — leg 배선 빠짐"
    cp "$SCRATCH/jkapp_music.so" buildwsl/jkapp_music.so \
        || FAIL "deploy scratch jkapp_music.so -> buildwsl failed"
    DEP_SYM=$(ALCOUNT buildwsl/jkapp_music.so)
    echo "MUS-LEG-DEPLOY: buildwsl/jkapp_music.so <- scratch (배포 뒤 AL 심볼 refs=$DEP_SYM — END relink 원복)"
fi

echo "=== 4. 합성 WAV 시드 + music.dirs 주입 (엔진 소유 — i:\\@keep 불접촉) ==="
cat > "$LOGDIR/mus_seed.py" <<'PYEOF'
# 시드 5곡(+mp3 가변) — python3 wave stdlib(의존 0): 44.1kHz mono 16bit 8초
# 사인. 주파수 = 트랙 식별 마크(스펙트럼 구별 가능), 4곡 = 3곡 루트+1곡 sub(D3
# 재귀 실측 몫 — rel열에 "sub/..." 원문 수형 확인)+1곡 초장 경로(M-1 승계 —
# ~1KB 경로: 7단 × 132자 세그먼트, 컴포넌트 255·PATH_MAX 상한 안전). 생성
# 순서 = 트랙 정렬 계약(mtime desc)에서 l_seed가 최신이 되게 한다.
import math, os, struct, sys, wave
rate = 44100
seconds = 8
out = sys.argv[1]
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
    print("SEED-OK %s %d bytes" % (path, os.path.getsize(path)))
os.makedirs(out + "/sub", exist_ok=True)
write_wav(out + "/a_seed.wav", 220.0)
write_wav(out + "/b_seed.wav", 330.0)
write_wav(out + "/c_seed.wav", 440.0)
write_wav(out + "/sub/d_seed.wav", 550.0)
deep_file = sys.argv[2]
os.makedirs(os.path.dirname(deep_file), exist_ok=True)
write_wav(deep_file, 660.0)
PYEOF
LONG_PATH="$SEED_DIR"
for i in 1 2 3 4 5 6 7; do LONG_PATH="$LONG_PATH/deep_${i}_${LONG_SEG}"; done
LONG_TRACK="$LONG_PATH/l_seed.wav"
LONG_LEN=$(printf '%s' "$LONG_TRACK" | wc -c)
echo "MUS-LONGTRACK-LEN=$LONG_LEN (M-1 승계 — >=700B 요구)"
[ "$LONG_LEN" -ge 700 ] || FAIL "long track path $LONG_LEN B < 700 (M-1 truncation 축 미달)"
python3 "$LOGDIR/mus_seed.py" "$SEED_DIR" "$LONG_TRACK" \
    || FAIL "seed WAV generation failed (python3)"
echo "=== 4b. mp3 합성 여부 실측 (ffmpeg+libmp3lame — 플랜 원문; 부재면 wav 단축) ==="
if command -v ffmpeg >/dev/null 2>&1; then
    if ffmpeg -loglevel error -f lavfi -i "sine=frequency=880:sample_rate=44100" \
        -t 6 -codec:a libmp3lame -b:a 128k "$SEED_DIR/m_seed.mp3" \
        >"$LOGDIR/mus_mp3.log" 2>&1 && [ -s "$SEED_DIR/m_seed.mp3" ]; then
        echo "MUS-AUDIO-MP3: ffmpeg+libmp3lame 합성 OK ($(wc -c < "$SEED_DIR/m_seed.mp3") bytes — durSec 미상(M-2) 원문 관측 몫)"
    else
        echo "MUS-AUDIO-MP3: ffmpeg 있음·libmp3lame 인코딩 실패 — wav 단축 계속 ($(tail -1 "$LOGDIR/mus_mp3.log"))"
    fi
else
    echo "MUS-AUDIO-MP3: ffmpeg 부재(T1/T4 원문 승계) — wav 단축 계속(플랜 원문 수형)"
fi
echo "=== 4c. 합성 태그 트랙 시드 (T3 #99 — own ID3v2.3 블롭 최소 mp3 — 태그 목록 실측) ==="
# T1 2s 수형 재용: ID3v2.3 = 10B 헤더("ID3" + 03 00 + flag 1B + **syncsafe
# 아닌 4BE 크기**) + TIT2/TPE1/TALB 프레임(10B 헤더: 4BE 크기+flag 2B) + enc 0
# Latin-1 바이트(ASCII — 2s-② 등가 계보: <0x80은 UTF-8과 동일) + 말미 0x00
# 패딩(2s-⑥ 스킵 수형). **유효 mp3 프레임 0** — ffmpeg 부재 원문 승계(T1/T4
# 실측)라 인코더 없음: 태그 파싱은 성립(스캔의 표기 leg가 소비), 재생은
# vplayer open 실패(정직 원문 — 16e-5 영수증). 태그 없는 raw mp3 1건 = 폴백
# 행(파일명 스템+앨범 "-"). 생성 순서 = mtime desc 상단행 지배(raw → beta →
# alpha = alpha가 최상단).
cat > "$LOGDIR/mus_tagseed.py" <<'PYEOF'
# 합성 태그 트랙 시더 — python3 stdlib만(의존 0 — T1 2s 블롭 수형 재용).
# ① t_tag_alpha.mp3  v2.3 TIT2/TPE1/TALB(enc 0) ② t_tag_beta.mp3  ③
# t_raw_noid.mp3 (ID3 없음 — 폴백 행). 생성 후 자기 블롭을 순수 파이썬으로
# 재파싱해 TAG-PARSED-PREVIEW(앱 파서와 독립 — 양식 증명)를 낸다.
import os, struct, sys

out = sys.argv[1]

def frame(fid, text):
    body = b"\x00" + text.encode("latin-1")   # enc 0 (Latin-1 — ASCII 등가)
    return fid.encode("ascii") + struct.pack(">I", len(body)) + b"\x00\x00" + body

def write_tagged(path, title, artist, album):
    frames = frame("TIT2", title) + frame("TPE1", artist) + frame("TALB", album)
    data = b"ID3" + b"\x03\x00" + b"\x00" + struct.pack(">I", len(frames)) + frames
    with open(path, "wb") as f:
        f.write(data + b"\x00" * 64)   # 말미 패딩(2s-⑥ 스킵 수형) + 프레임 0
    return path

def write_raw(path):
    # ID3 매직 0 + 말미 "TAG" 0 — magic 분기·v1 폴백 둘 다 부재 = 빈 TagSet
    # (파일명 폴백 계보 — 2s-⑩ 수형).
    with open(path, "wb") as f:
        f.write(b"\x00" * 256)
    return path

def parse_v23(path):   # 순수 파이썬 재파싱(앱 파서와 독립 — 블롭 양식 증명)
    with open(path, "rb") as f:
        d = f.read()
    if d[:3] != b"ID3":
        return {}
    i = 10
    out = {}
    end = 10 + int.from_bytes(d[6:10], "big")
    while i + 10 <= min(end, len(d)):
        fid = d[i:i+4].decode("latin-1")
        sz = int.from_bytes(d[i+4:i+8], "big")
        if sz == 0 or not ('A' <= fid[0] <= 'Z'):
            break   # 패딩(전부 0x00) — 프레임 사슬 종료(2s-⑥ 수형)
        body = d[i+10:i+10+sz]
        if fid in ("TIT2", "TPE1", "TALB") and body:
            out[fid] = body[1:].decode("latin-1", "replace")
        i += 10 + sz
    return out

p1 = write_tagged(os.path.join(out, "t_tag_alpha.mp3"),
                  "Wsl Tag Alpha", "Probe Artist Alpha", "Tag Album One")
p2 = write_tagged(os.path.join(out, "t_tag_beta.mp3"),
                  "Wsl Tag Beta", "Probe Artist Beta", "Tag Album Two")
p3 = write_raw(os.path.join(out, "t_raw_noid.mp3"))
for p in (p3, p2, p1):
    m = parse_v23(p)
    print("TAG-PARSED-PREVIEW %s bytes=%d title=%r artist=%r album=%r"
          % (os.path.basename(p), os.path.getsize(p),
             m.get("TIT2", ""), m.get("TPE1", ""), m.get("TALB", "")))
PYEOF
python3 "$LOGDIR/mus_tagseed.py" "$SEED_DIR" \
    || FAIL "tag seed mp3 generation failed (python3)"

SEEDED=$(find "$SEED_DIR" \( -name '*.wav' -o -name '*.mp3' \) | wc -l)
EXPECT_SEEDS=8
[ -f "$SEED_DIR/m_seed.mp3" ] && EXPECT_SEEDS=9
[ "$SEEDED" -eq "$EXPECT_SEEDS" ] || FAIL "seeded $SEEDED media files (need $EXPECT_SEEDS)"
SEED_BYTES=$(find "$SEED_DIR" -name '*.wav' -printf '%s\n' | awk '{s+=$1} END{print s}')
echo "MUSIC-LIST-EXPECTED=$EXPECT_SEEDS (seed 3 root + 1 sub + 1 초장 경로[+1 mp3 if any] + 태그 트랙 3건[alpha·beta·raw 폴백] — 표행 수 관측 원문 1행; 표기 일치는 캡처 육안 몫)"

# [T3] 태그 세그먼트 상수 — 시더의 문자열 원존(16e가 필터 타이핑에 재용 —
# 두 값의 일치가 "태그가 실제 파싱됐다"의 판정 축: 필터는 Track 매치(title+
# artist OR — D5)라 파일명과 무관한 문자열로 골랐다).
TAG_MP3_A=/tmp/mus_seed/t_tag_alpha.mp3
TAG_FILTER_TEXT_TITLE="Wsl Tag Alpha"      # t_tag_alpha의 TIT2
TAG_FILTER_TEXT_ARTIST="Probe Artist Beta" # t_tag_beta의 TPE1

# [T3] seed를 ComposeKeyed 출하형(compact)으로 — 관리 3키의 audio/retention 원문
# 슬라이스를 심어 C1 보존의 라이브 수취 대상으로 쓴다(16b) + 말미 개행 없음
# (원자적 쓰기가 데이터 크기만큼만 기록 — ④ 등호 판정 축). 스냅샷 = 16b/16c의
# 백업 원문(T5 수형 — 백업+원복 바이트 등호; END에서 소각).
SNAP="$LOGDIR/mus_settings_snap.json"
printf '{"audio": {"mute": 0, "volume": 50},"retention": {"days": 14},"music":{"dirs":["/tmp/mus_seed"]}}' \
    > buildwsl/state/settings.json
[ -f buildwsl/state/settings.json ] || FAIL "settings.json write failed"
cp buildwsl/state/settings.json "$SNAP" || FAIL "settings.json snapshot failed"
echo "DIRS-WIRE-OK: buildwsl/state/settings.json music.dirs=[/tmp/mus_seed]+audio/retention (백업 스냅샷=$SNAP — END에서 소각)"

echo "=== 5. send_input·close_window 승인 무승인 운용 — permissions.json 1행 (ENTRY 원복) ==="
# [T3] close_window 스테이지 — 16d ①(스캔 중 창 close). 부재 Deny 규약
# (M1 — wsl_chat_close.sh 원장)이라 프로브 소유 스테이지 파일에 allow 1키
# 추가(END에서 소각 — 원복). 승인 운용 동형(send_input 선형).
[ "$ENTRY_PERM" -eq 0 ] || FAIL "buildwsl/permissions.json already exists — probe는 런타임 permissions 파일을 건드릴 수 없다(전면 allow 파일은 프로브 소유 아님)"
printf '{\n    "send_input": "allow",\n    "close_window": "allow"\n}\n' > buildwsl/permissions.json
[ -f buildwsl/permissions.json ] || FAIL "permissions.json write failed"
grep -aq '"send_input"' buildwsl/permissions.json && grep -aq '"close_window"' buildwsl/permissions.json \
    || FAIL "permissions.json staging malformed (send_input+close_window)"
echo "PERM-WIRE-OK: buildwsl/permissions.json (send_input+close_window allow — END에서 소각)"

echo "=== 6. 서버 부팅 (setsid — taskbar 자동 스폰 선례) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
# #98 pre-clean guard: rm on a LIVE server's socket entombs its parked
# listener (client connects land ENOENT) — skip and note.
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "NOTE #98: live server holds /tmp/JKWindowServerPipe.sock — pre-clean rm skipped"
else
    rm -f /tmp/JKWindowServerPipe.sock
fi
LOG="$LOGDIR/mus_srv.log"
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOG" 2>&1 &
sleep 6
SRV_PID=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
[ -n "$SRV_PID" ] || FAIL "no server 6s after boot — log tail: $(tail -3 "$LOG" | tr '\n' ' ')"
echo "server pid=$SRV_PID"
PING=""
# [T3(#97) 부팅 지연 원장] 런 1-2 실측(2026-10-11): 콜드 부팅의 agentctl 응답
# 지점이 ~33s(T3 정찰 실측 — engine/tmp/t3_bootprobe.sh — 3s 간격 33s 응답)로
# 늘었다(전전 런 계보 5시도 ~12s 창에서는 전부 무응답 hard FAIL — boot 늦응답
# 인프라 견적). 부팅 원문 수형의 시도 수만 인프라 견적으로 확장(판정 본질
# 무변경 — 30 시도 × 2s ≈ 72s+ 창).
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
    PING=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$PING" | grep -aq '"ok":true' && break
    sleep 2
done
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "ping failed — $PING"

echo "=== 7. launch_app music (윈도우 목록 실측) ==="
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L" | grep -aq '"ok":true' || FAIL "launch_app music failed — $L"
echo "launch reply: $L"
MUSWIN=""
WIN=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window in list_windows — last: ${WIN:-none}"
echo "MUSWIN: $MUSWIN"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
MUS_W=$(printf '%s' "$MUSWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
MUS_H=$(printf '%s' "$MUSWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
echo "MUS-GEO: id=$MUS_ID x=$MUS_X y=$MUS_Y w=$MUS_W h=$MUS_H (meta 560x520 상대 검증)"
sleep 3   # 부팅 스캔(기본 폴더 state/music — 부재) 도착 안정화

# === 캡처기 자가 생성 (wsl_gallery.sh gal_shot.py 원문 — XImage 오프셋 원장 포함) ===
cat > "$SHOT_PY" <<'PYEOF'
# wsl_text_scale_diag.sh의 tsd_shot.py 축본 — X11 창 단위 캡처+최소 PNG 기록기.
import ctypes, struct, sys, zlib
c_void_p = ctypes.c_void_p
X11 = ctypes.CDLL("libX11.so.6")
X11.XOpenDisplay.restype = c_void_p
X11.XOpenDisplay.argtypes = [ctypes.c_char_p]
X11.XCloseDisplay.argtypes = [c_void_p]
X11.XDefaultRootWindow.restype = ctypes.c_ulong
X11.XGetGeometry.restype = ctypes.c_int
X11.XFetchName.argtypes = [c_void_p, ctypes.c_ulong, c_void_p]
X11.XFree.argtypes = [c_void_p]
X11.XSetErrorHandler.restype = c_void_p
X11.XSetErrorHandler.argtypes = [c_void_p]
X11.XQueryTree.restype = ctypes.c_int
X11.XGetImage.restype = c_void_p
X11.XGetImage.argtypes = [c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                          ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong,
                          ctypes.c_int]
X11.XDestroyImage.argtypes = [c_void_p]
X11.XSync.argtypes = [c_void_p, ctypes.c_int]
ZPIXMAP = 2
ALL_PLANES = 0xFFFFFFFF
GOT_ERROR = {"n": 0}
# XImage 구조체 오프셋(x86_64 — 1차 실측 트랩 원장: 48/56/64는
# bits_per_pixel을 red 마스크로 읽는 3틀림 — 캡처 색이 자유로이 거짓됐다.
# 40=depth·44=bytes_per_line·48=bits_per_pixel·56/64/72=R/G/B 마스크.)
OFF_DATA = 16
OFF_DEPTH, OFF_BPL, OFF_BPP = 40, 44, 48
OFF_RED, OFF_GREEN, OFF_BLUE = 56, 64, 72

def err_handler(dpy, ev):
    GOT_ERROR["n"] += 1
    return 0

def write_png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    comp = zlib.compress(raw, 6)
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data)))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", comp)
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)

def main():
    out = sys.argv[1]
    d = X11.XOpenDisplay(None)
    if not d:
        print("XSHOT-FAIL: cannot open display")
        return 1
    try:
        H = ctypes.CFUNCTYPE(ctypes.c_int, c_void_p, c_void_p)(err_handler)
        X11.XSetErrorHandler(H)
        root = X11.XDefaultRootWindow(d)
        rr = ctypes.c_ulong(); pw = ctypes.c_ulong()
        kids = c_void_p(); n = ctypes.c_uint()
        if not X11.XQueryTree(d, root, ctypes.byref(rr), ctypes.byref(pw),
                              ctypes.byref(kids), ctypes.byref(n)) or not kids:
            print("XSHOT-FAIL: XQueryTree failed")
            return 1
        arr = ctypes.cast(kids, ctypes.POINTER(ctypes.c_ulong))
        cands = []
        for i in range(n.value):
            w = arr[i]
            x, y, gw, gh, bw, bd = [ctypes.c_int() for _ in range(6)]
            rw = ctypes.c_ulong()
            if not X11.XGetGeometry(d, w, ctypes.byref(rw), ctypes.byref(x),
                                    ctypes.byref(y), ctypes.byref(gw),
                                    ctypes.byref(gh), ctypes.byref(bw),
                                    ctypes.byref(bd)):
                continue
            name = ctypes.c_char_p(None)
            nm = X11.XFetchName(d, w, ctypes.byref(name))
            title = name.value.decode("utf-8", "replace") if nm and name.value else ""
            if nm:
                X11.XFree(name)
            print("XSHOT cand: win=0x%x %dx%d title=%r" % (w, gw.value, gh.value, title))
            cands.append((w, title, gw.value, gh.value))
        X11.XFree(kids)
        pick = None
        # WSL 서버 데스크톱 합성 창(전체 뷰). XWayland 배경 창 8192x8192 배제
        # 상한 선례 원문(wsl_gallery 동형).
        big = [(w, gw, gh) for w, t, gw, gh in cands
               if 500 <= gw <= 4096 and 400 <= gh <= 4096]
        if big:
            big.sort(key=lambda t: -(t[1] * t[2]))
            pick = big[0]
        if pick is None:
            print("XSHOT-FAIL: no candidate window")
            return 1
        w, gw, gh = pick
        X11.XSync(d, False)
        got0 = GOT_ERROR["n"]
        img = X11.XGetImage(d, w, 0, 0, gw, gh, ALL_PLANES, ZPIXMAP)
        X11.XSync(d, False)
        if not img or GOT_ERROR["n"] > got0:
            print("XSHOT-FAIL: XGetImage failed (BadMatch?)")
            return 1
        depth = ctypes.c_int32.from_address(img + OFF_DEPTH).value
        bpl = ctypes.c_int32.from_address(img + OFF_BPL).value
        bpp = ctypes.c_int32.from_address(img + OFF_BPP).value
        data_ptr = c_void_p.from_address(img + OFF_DATA).value
        red = ctypes.c_uint64.from_address(img + OFF_RED).value
        green = ctypes.c_uint64.from_address(img + OFF_GREEN).value
        blue = ctypes.c_uint64.from_address(img + OFF_BLUE).value
        print("XSHOT hit: win=0x%x depth=%d %dx%d bpp=%d masks r=%#x g=%#x b=%#x"
              % (w, depth, gw, gh, bpp, red, green, blue))
        bpp_bytes = (bpp + 7) // 8
        if bpp_bytes not in (3, 4):
            print("XSHOT-FAIL: unsupported bpp=%d" % bpp)
            return 1
        if not (red and green and blue):
            print("XSHOT-FAIL: empty channel mask(s) r=%#x g=%#x b=%#x"
                  % (red, green, blue))
            return 1

        def chan(v, mask):  # 마스크 비트폭 무관 확장(2차 트랩 원장: 폭=popcount).
            lsb = (mask & -mask).bit_length() - 1
            span = bin(mask).count("1")
            val = (v >> lsb) & ((1 << span) - 1)
            return (val * 255) // ((1 << span) - 1) & 0xFF
        raw = ctypes.string_at(data_ptr, bpl * gh)
        rgb = bytearray(gw * gh * 3)
        for yy in range(gh):
            row = yy * bpl
            for xx in range(gw):
                off = row + xx * bpp_bytes
                v = int.from_bytes(raw[off:off + bpp_bytes], "little")
                o = (yy * gw + xx) * 3
                rgb[o] = chan(v, red)
                rgb[o + 1] = chan(v, green)
                rgb[o + 2] = chan(v, blue)
        X11.XDestroyImage(img)
        write_png(out, gw, gh, bytes(rgb))
        print("XSHOT wrote %s" % out)
        return 0
    finally:
        X11.XCloseDisplay(d)

if __name__ == "__main__":
    sys.exit(main())
PYEOF
[ -f "$SHOT_PY" ] || FAIL "capture script generation failed"

RECEIVE="$RECEIVE"
mkdir -p "$RECEIVE"
shot() { # $1 = 영수증 상대명 — /tmp 단열 → engine/tmp 단일 cp
    local NAME="$1" TMP="/tmp/mus_shot_cur.png" SLOG="/tmp/mus_shot_cur.log"
    rm -f "$TMP"
    python3 "$SHOT_PY" "$TMP" >"$SLOG" 2>&1 \
        || FAIL "capture $NAME failed — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -aq 'XSHOT wrote' "$SLOG" \
        || FAIL "capture $NAME: no XSHOT wrote — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -a 'XSHOT hit\|XSHOT cand' "$SLOG" | sed 's/^/  /'
    cp "$TMP" "$RECEIVE/$NAME" || FAIL "capture copy $NAME failed"
    rm -f "$TMP"
    echo "CAPTURE: $RECEIVE/$NAME ($(wc -c < "$RECEIVE/$NAME") bytes)"
}
tap() { # $1=id $2=x $3=y — 합성 클릭 1탭(send_input — permissions allow 게이트)
    # timeout 35 (T2 fix r1 — T3 런의 봉합 원장 승계): WSL agentctl 응답 지연은
    # 원장 실측 ~33s(런 1-2의 ping 30×2s 창) — timeout 12는 지연 창의 텅빈
    # 회수(무전달 오인의 2차 원인 — 본 라인 16b2 런의 btog 루프 18 전멸 원문)
    # 라 원장 수치에 맞춘 상향. 무착탄 판정(차분 0+무음)은 tapsettle이 소유 —
    # 이 타임아웃은 "회수 대기 상한"만 늘린다(기존 세그먼트 행동 동형 상위호환
    # — 응답이 12s 내 오면 원존과 같은 시각).
    timeout 35 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"send_input\",\"args\":{\"op\":\"click\",\"id\":$1,\"x\":$2,\"y\":$3,\"button\":1,\"clicks\":1}}" \
        2>/dev/null | grep -a '{' | head -1
}

# [T3(#97) — 위상 이동 원장] PX(캡처 차분기)와 landtap(탭 착탄 판정기)은
# 본래 16c-5/16d 지점에 정의돼 있었다 — 16b2 브라우저 세그먼트(step 8 전에
# 쓸 수 없는 하단 정의)가 함께 소비하기에 tap() 뒤로 정의 위치만 이동한다
# (**본문 무변경** — 정의 원문 수형 그대로; 16c/16d의 소비 행도 만지지 않는다).
PX="$LOGDIR/mus_px.py"
cat > "$PX" <<'PYEOF'
# [T3] 캡처 차분기 — 두 PNG(필터 0·RGB8, 자가 캡처 전제)의 유의 행 수(픽셀
# 차합 > 75 합, 행별 ≥8 픽셀, y 60-700 2픽셀 스텝)를 센다. 패널 열림 =
# 레이아웃 전이(수십 행), 무효 클릭 = ≤ 몇 행 — 임계 15행 = 토글 판정.
import struct, sys, zlib

def load(p):
    d = open(p, "rb").read()
    pos, idat, w, h = 8, b"", 0, 0
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos+4])[0]
        tag = d[pos+4:pos+8]
        body = d[pos+8:pos+8+ln]
        pos += 12 + ln
        if tag == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    raw = zlib.decompress(idat)
    return w, h, raw

w, h, a = load(sys.argv[1])
_, _, b = load(sys.argv[2])
n = 0
for y in range(60, min(h, 700), 2):
    c = 0
    for x in range(400, min(w, 1030)):
        va = a[y*(w*3+1)+1+x*3]
        vb = b[y*(w*3+1)+1+x*3]
        if abs(va - vb) > 75:
            c += 1
            if c >= 8:
                break
    if c >= 8:
        n += 1
print("diff_rows=%d" % n)
PYEOF

landtap() { # $1=id $2=baseshot $3=tag $4=winX $5=winY $6=tap 클라 x — 출력:
            #   착탄 reply 원문 행들 | 사이드: LANDING=1/0·MUS_TAB3_Y_EFF
    local rid="$1" base="$2" tag="$3" wx="$4" wy="$5" tx="${6:-158}" r yy rows df
    LANDING=0
    for yy in ${LT_YS:-64 70 76 82 58}; do
        r=$(tap "$rid" "$((wx + tx))" "$((wy + yy))")
        printf '%s' "$r" | grep -aq '"ok":true' \
            || echo "  tap reply 비정상(전송 실패?): ${r:-empty} — 착탄 판정에선 계속"
        echo "LTAP try($tag): 클라(x=$tx,y=$yy) — reply: ${r:-none}"
        sleep 5
        python3 "$SHOT_PY" "$LOGDIR/mus_land_$tag.png" >"$LOGDIR/mus_land_$tag.log" 2>&1 \
            || FAIL "landing capture $tag failed — $(tail -2 "$LOGDIR/mus_land_$tag.log" | tr '\n' ' ')"
        df=$(python3 "$PX" "$base" "$LOGDIR/mus_land_$tag.png")
        rows=$(printf '%s' "$df" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p')
        echo "LTAP-DIFF($tag): $df (착탄 판정 — 임계 3행)"
        if [ -n "$rows" ] && [ "$rows" -ge 3 ]; then
            LANDING=1
            MUS_TAB3_Y_EFF=$yy
            LAST_TAP_Y=$yy
            LAST_DIFF=$rows
            return 0
        fi
    done
    return 1
}

tapsettle() { # [T3(#97) 신설 — 정찰-판정기(런 원장: 1-behind 프레젠트 지연)]
           # $1=기저 캡처 $2=tag $3=클라 x $4=클라 y $5=차분 행 임계
           # 클릭 1탭 → 2.5s 간격 캡처·차분(최대 6회): 첫 도달(diff≥임계) =
           # 착탄 성공(정착 프레임 = $LOGDIR/mus_bc_$tag.png — 사이드
           # SETTLE_DIFF·SETTLE_T=도달 시차 s)·연속 0차분+8s 경과 = 무음 소멸
           # 판정(상태 무변화 보증 — **재시도 안전**의 근거: 토글형 버튼의
           # 재시도가 역전을 만드는 함정을 무음 판정이 해소한다 — 상태가 바뀐
           # 채 놓친 전이(지연 도착)는 8s 내에 잡힌다).
    local base="$1" tag="$2" tx="$3" ty="$4" th="$5" i rows zeros r df t0
    SETTLE_DIFF=0
    SETTLE_T=""
    r=$(tap "$MUS_ID" "$((MUS_X + tx))" "$((MUS_Y + ty))")
    printf '%s' "$r" | grep -aq '"ok":true' \
        || echo "  tap reply 비정상(전송 실패?): ${r:-empty} — 정찰 판정에선 계속"
    echo "TAP-SETTLE($tag): 클라($tx,$ty) — reply: ${r:-none}"
    t0=$(date +%s)
    zeros=0
    for i in 1 2 3 4 5 6; do
        sleep 2.5
        python3 "$SHOT_PY" "$LOGDIR/mus_bc_$tag.png" \
            >"$LOGDIR/mus_bc_$tag.log" 2>&1 \
            || FAIL "settle capture $tag failed — $(tail -2 "$LOGDIR/mus_bc_$tag.log" | tr '\n' ' ')"
        df=$(python3 "$PX" "$base" "$LOGDIR/mus_bc_$tag.png")
        rows=$(printf '%s' "$df" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p')
        echo "SETTLE($tag#$i): diff_rows=${rows:-0} (기저 대비 — 임계 $th)"
        if [ -n "$rows" ] && [ "$rows" -ge "$th" ]; then
            SETTLE_DIFF=$rows
            SETTLE_T=$(( $(date +%s) - t0 ))
            return 0
        fi
        if [ "${rows:-0}" = "0" ]; then zeros=$((zeros + 1)); else zeros=0; fi
        if [ "$zeros" -ge 2 ] && [ $(( $(date +%s) - t0 )) -ge 8 ]; then
            return 1   # 무음 — 상태 무변화(재시도 안전 — 원장)
        fi
    done
    return 1
}

echo "=== 8. 탭 전환(mus_seed) → 리스캔 도착 대기 → 리스트 캡처 ==="
CLICK_X=$((MUS_X + MUS_TAB2_X))
CLICK_Y=$((MUS_Y + MUS_TAB2_Y))
echo "CLICK-TAB2: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 MUS_TAB2_X=$MUS_TAB2_X MUS_TAB2_Y=$MUS_TAB2_Y"
TCL=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
printf '%s' "$TCL" | grep -aq '"ok":true' \
    || FAIL "send_input click gate/execute failed — reply: $TCL (permissions.json send_input allow 확인)"
echo "tab click reply: $TCL"
sleep 3   # 탭 전환 재스캔 도착(유일 더티 원 #89) 안정화
shot mus_wsl_list.png

if [ "${MUS_RECON:-0}" = "1" ]; then
    echo "MUS-RECON-EXIT: 리스트 캡처 확보 — 상수 재보정 후 풀 런(MUS_RECON=0)"
    restore_and_exit
    echo "MUSIC-RECON-END"
    exit 0
fi

# ==================================================================
# 8b. spatial leg 실측 (T3 신설 — 스펙 2026-10-10 §2, 도구 3종 원문 계약)
# ==================================================================
atool() { # $1=tool $2=args-json — music 앱 직행(도구 허브 릴레이 원문 — music 창
          #   등록 전제이므로 이 세그먼트는 launch_app music 뒤에만 온다)
    timeout 15 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"app_tool\",\"args\":{\"app\":\"music\",\"tool\":\"$1\",\"args\":$2}}" \
        2>/dev/null | grep -a '{' | head -1
}
spstatus() { sleep 1.5; atool spatial_status '{}'; }
jpos() { # status JSON에서 pos 수치만 뽑는다
    printf '%s' "$1" | sed -n 's/.*"pos":\([0-9][0-9.eE+\-]*\).*/\1/p'
}

echo "=== 8b. spatial leg 실측 (LEG_MODE=$LEG_MODE — app_tool 직행 릴레이) ==="
ST0=$(atool spatial_status '{}')
echo "MUS-SPATIAL-STATUS-0: ${ST0:-none} (재생 전 기저 — active:false·path 빈 원문)"
PLAY1=$(atool spatial_play "{\"path\":\"$SEED_DIR/a_seed.wav\"}")
echo "MUS-SPATIAL-PLAY-REPLY: ${PLAY1:-none}"
sleep 1   # open(디코더+최초 ALC lazy — 프레임 스레드 수십 ms급 I-3 정직 원장)
S1=$(atool spatial_status '{}')
echo "MUS-SPATIAL-STATUS-1: ${S1:-none}"
S2=$(spstatus)
echo "MUS-SPATIAL-STATUS-2: ${S2:-none} (pos 증가 = 재생 진행 원문 1행)"
POS1=$(jpos "$S1"); POS2=$(jpos "$S2")
POS_DELTA=""
if [ -n "$POS1" ] && [ -n "$POS2" ]; then
    POS_DELTA=$(awk -v a="$POS1" -v b="$POS2" 'BEGIN{printf "%.3f", b - a}')
fi
echo "MUS-SPATIAL-POS: $POS1 -> $POS2 (delta=$POS_DELTA — 원문 등호는 리포트 몫)"
shot mus_wsl_spatial.png   # spatial 패널(진행 bar·모드·디바이스 표기) — EYES 캡처

STP=$(atool spatial_stop '{}')
echo "MUS-SPATIAL-STOP-REPLY: ${STP:-none} (idempotent — idle leg도 {\"active\":false,\"idle\":true} 원문)"
ST3=$(atool spatial_status '{}')
echo "MUS-SPATIAL-STATUS-3(post-stop): ${ST3:-none} (active:false·직전 pos/path·deviceOk 보존 원문)"

M1_TRUNC=0
if [ "$LEG_MODE" -eq 1 ]; then
    echo "--- 8b-2. 초장 경로 트랙 — M-1 절단 정직 가드 원문 실측 ---"
    echo "MUS-LONGTRACK: len=$LONG_LEN (경로 원문 무단열 — /tmp/mus_seed 아래 deep_*)"
    PLAY2=$(atool spatial_play "{\"path\":\"$LONG_TRACK\"}")
    echo "MUS-SPATIAL-PLAY-LONG-REPLY: ${PLAY2:-none}"
    sleep 1.5
    SL1=$(atool spatial_status '{}')
    echo "MUS-SPATIAL-STATUS-LONG: ${SL1:-none}"
    printf '%s' "$SL1" | grep -aq '"truncated":true' \
        && { M1_TRUNC=1; echo "MUS-M1-TRUNCATED: OK — 절단 시 path 생략+truncated:true 정직 축소 전문 원문 성립"; } \
        || echo "MUS-M1-TRUNCATED: MISS(need>=768 미도달 또는 재생 성립 전 고장 — 사유는 위 PLAY-LONG·STATUS-LONG 원문)"
    STP2=$(atool spatial_stop '{}')
    echo "MUS-SPATIAL-STOP-LONG: ${STP2:-none}"
    if [ -f "$SEED_DIR/m_seed.mp3" ]; then
        echo "--- 8b-3. mp3 트랙 — durSec 미상(M-2) 원문 관측 ---"
        PLAY3=$(atool spatial_play "{\"path\":\"$SEED_DIR/m_seed.mp3\"}")
        echo "MUS-SPATIAL-PLAY-MP3-REPLY: ${PLAY3:-none}"
        sleep 1.5
        SM=$(atool spatial_status '{}')
        echo "MUS-SPATIAL-STATUS-MP3: ${SM:-none} (dur:0.000 = 길이 미상 원문 — 진행 표기는 캡처 육안 몫)"
        STP3=$(atool spatial_stop '{}')
        echo "MUS-SPATIAL-STOP-MP3: ${STP3:-none}"
    fi
fi

echo "=== 9. 접착제 콜드 전조 — 등록 전 app_tool open = unknown_app_tool 실측 ==="
OR0=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"app_tool","args":{"app":"vplayer","tool":"open","args":{"path":"/tmp/mus_seed/a_seed.wav"}}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-GLUE-COLD-REPLY: ${OR0:-none}"
printf '%s' "$OR0" | grep -aq '"unknown_app_tool"' \
    || echo "MUSIC-FAIL(glue-cold: 등록 전 릴레이가 unknown_app_tool이 아님 — T3 콜드 부팅 경기 전조 부정: $OR0)"

echo "=== 10. 이름 필터 관측 (필터 박스 클릭 → 타이핑) → 필터 캡처 ==="
CLICK_X=$((MUS_X + MUS_FILTER_X))
CLICK_Y=$((MUS_Y + MUS_FILTER_Y))
echo "CLICK-FILTER: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 MUS_FILTER_X=$MUS_FILTER_X MUS_FILTER_Y=$MUS_FILTER_Y"
FCL=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
printf '%s' "$FCL" | grep -aq '"ok":true' || FAIL "send_input filter click failed — $FCL"
echo "filter click reply: $FCL"
sleep 1
TYP=$(timeout 12 ./buildwsl/jkdesktop agentctl \
    "{\"tool\":\"send_input\",\"args\":{\"op\":\"type\",\"id\":$MUS_ID,\"text\":\"$FILTER_TEXT\"}}" \
    2>/dev/null | grep -a '{' | head -1)
printf '%s' "$TYP" | grep -aq '"ok":true' || FAIL "send_input type failed — $TYP"
echo "type reply: $TYP"
sleep 2   # 타이핑 = 입력 활동 더티(자연 귀결) — 표 정산(1행) 렌더 도달
shot mus_wsl_filter.png
echo "MUS-FILTER-OBSERVATION: text=$FILTER_TEXT — 표 1행(sub/d_seed.wav)·카운터 1곡 일치는 캡처 육안 몫"

echo "=== 11. 표행 0 더블클릭 → vplayer 위임 (T3 실측 — 콜드 부팅 경기 포함) ==="
CLICK_X=$((MUS_X + MUS_ROW0_X))
CLICK_Y=$((MUS_Y + MUS_ROW0_Y))
echo "CLICK-ROW0: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 MUS_ROW0_X=$MUS_ROW0_X MUS_ROW0_Y=$MUS_ROW0_Y (합성 2탭 — ImGui 더블클릭 시간창 0.30s 내 착탄 몫)"
VPWIN=""
OR=""
GS1=""
GS2=""
for PAIR in 1 2 3; do
    C1=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
    printf '%s' "$C1" | grep -aq '"ok":true' || FAIL "send_input row0 tap1 failed — $C1"
    TAP1_NS=$(date +%s%N)
    C2=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
    printf '%s' "$C2" | grep -aq '"ok":true' || FAIL "send_input row0 tap2 failed — $C2"
    TAP2_NS=$(date +%s%N)
    echo "dblclick pair $PAIR replies: $C1 | $C2 (tap gap $(( (TAP2_NS - TAP1_NS) / 1000000 )) ms — ImGui 더블클릭 시간창 300ms 대조 몫)"
    for i in 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24; do
        sleep 1
        VP=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        VPWIN=$(printf '%s' "$VP" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
        [ -n "$VPWIN" ] && break
    done
    [ -n "$VPWIN" ] && break
    echo "MUSIC-DELEGATE: pair $PAIR 재탭 — 2탭 사이 2s 대기(중복 스폰 스로틀 500ms 회피)"
    sleep 2
done
if [ -n "$VPWIN" ]; then
    echo "MUSIC-DELEGATE: OK — 더블클릭 위임이 Video Player 창을 냈다 (music 클라의 launch_app 쿼리 상통)"
    echo "VPWIN: $VPWIN"
else
    echo "MUSIC-DELEGATE: MISS — 3쌍의 합성 2탭이 표행 더블클릭 핸들에 도달하지 않았다(ImGui 더블클릭 시간창 0.30s vs agentctl 왕복 — EYES 승계 사유 원장)"
fi

echo "=== 12. 위임 open 도착 귀속 — 직행 open 없이 get_status 고밀도 폴링 ==="
# attribution: 이 폴링 창엔 probe의 직행 open이 없다 — opened:true 전이는
# **music 위임의 open**(재청구 폴백 포함)만이 만들 수 있다. 창 출현 직후
# 캡처 1장(실패 분류 문구는 vplayer UI 상태행 — statusText 빨강) + 0.4s 폴링
# (1s 간격은 흡수 1프레임(16ms) 전이를 놓친다 — v2 실측 렛슨).
VPWIN_ID=$(printf '%s' "$VPWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
sleep 1
shot mus_wsl_vp_delegate.png
DELEG_OPEN=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    GS=$(timeout 15 ./buildwsl/jkdesktop agentctl \
        '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
        2>/dev/null | grep -a '{' | head -1)
    echo "MUSIC-DELEGATE-GET-STATUS-$i: ${GS:-none}"
    printf '%s' "$GS" | grep -aq '"opened":true' && { DELEG_OPEN=1; break; }
    sleep 0.4
done
if [ "$DELEG_OPEN" -eq 1 ]; then
    echo "MUSIC-DELEGATE-OPEN: OK — 위임 open이 vplayer에 도달해 열었다(재청구 폴백 포함)"
else
    echo "MUSIC-DELEGATE-OPEN: MISS — 창 출현 후 ~7s 간 opened:true 전이 없음. 원인 진원(서버 즉답 경로 JKWindowServer :6747-6749 — sync 거부 답신의 봉투 ok=고정 1): 콜드 open의 sync unknown_app_tool 거부가 봉투 ok=1로 회송되고 PollReplies의 reply.ok 분류(ClientMusicApp :393-398)가 그것을 성공(" '"vplayer 재생 요청됨"' ")으로 읽어 재청구 크레딧을 0으로 소멸 — 폴백이 자기 판정과 모순되지 않고 자기 소멸. 리포트 원장(진원·고침 후보) 참조"
fi

echo "=== 13. music status 표기 관측 — vplayer 창을 치우고 캡처 ==="
MV=$(timeout 12 ./buildwsl/jkdesktop agentctl \
    '{"tool":"window_move","args":{"id":'"$(printf '%s' "$VPWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')"', "x":820,"y":100}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "window_move (vplayer 치우기 — music status 행 노출): ${MV:-none}"
sleep 2
shot mus_wsl_delegate_status.png

echo "=== 14. 위임 접착제 직행 — app_tool open(get_status 재생 수치) ==="
OPEN_PATH=/tmp/mus_seed/sub/d_seed.wav
OR=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$OPEN_PATH\"}}}" \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-APP-OPEN-REPLY: $OR"
printf '%s' "$OR" | grep -aq '"accepted":true' \
    || echo "MUSIC-FAIL(app_tool open not accepted — $OR)"
sleep 3   # open 비동기 — 워커 bring-up(10s 상한보다 짧아야 정상)
GS2=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-GET-STATUS-1: $GS2"
sleep 3
GS3=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-GET-STATUS-2: $GS3 (pos 증가 = 재생 진행 원문 1행)"

echo "=== 15. stderr 진단 수취 (서버 로그 — 클라 스폰+vplayer 진단) ==="
grep -a 'spawn\|music\|vplayer\|\[vplayer\]' "$LOG" | tail -14 | sed 's/^/  /'

echo "=== 16. 위임 후 모양 캡처 → mus_wsl_after.png ==="
shot mus_wsl_after.png

# ==================================================================
# 16b. [T3 — dirs 도구 세그먼트] music_dir_add/remove/list — music.dirs의
#      settings.json 실측 쓰기(등록 6종 — spatial 3종 병합 유지 원존)+기존
#      audio/retention 키 보존 원문+add/remove 왕복 후 원복 바이트 등호(④ —
#      T5 수형). 도구 경로만(클릭 없음 — UI 패널은 16c 치유 루트에서 1회).
# ==================================================================
SETTINGS=buildwsl/state/settings.json
echo "=== 16b. dirs 도구 세그먼트 (music_dir_add/remove/list — settings 쓰기) ==="
DL0=$(atool music_dir_list '{}')
echo "MUSIC-DIR-LIST-0: ${DL0:-none} (시딩 기저 원문 — /tmp/mus_seed 단일)"
[ -d /tmp/mus_dirs_x ] || mkdir -p /tmp/mus_dirs_x || FAIL "synthetic dir create failed"
DADD=$(atool music_dir_add '{"path":"/tmp/mus_dirs_x"}')
echo "MUSIC-DIR-ADD-REPLY: ${DADD:-none}"
DL1=$(atool music_dir_list '{}')
echo "MUSIC-DIR-LIST-1: ${DL1:-none} (등록 인증 원문 1행)"
printf '%s' "$DADD" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(dir-add: ok 수취 실패 — $DADD)"
printf '%s' "$DL1" | grep -aq 'mus_dirs_x' \
    || echo "MUSIC-FAIL(dir-list-1: 합성 폴더 등록 미반영 — $DL1)"
echo "MUSIC-DIR-SETTINGS-1: $(cat "$SETTINGS") (settings.json 원문 실측 — 등록 후 파일)"
SETCHK=$(python3 - "$SETTINGS" <<'PYEOF'
import json, sys
with open(sys.argv[1], "rb") as f:
    obj = json.loads(f.read().decode("utf-8"))
dirs = obj.get("music", {}).get("dirs", [])
ok = ("audio" in obj and "retention" in obj
      and "/tmp/mus_dirs_x" in dirs and "/tmp/mus_seed" in dirs)
print("SET-OK" if ok else "SET-MISS audio=%s retention=%s dirs=%s"
      % ("audio" in obj, "retention" in obj, dirs))
PYEOF
) || SETCHK="SET-MISS(python3 파서 실패)"
echo "MUSIC-DIR-SETTINGS-CHECK: $SETCHK (독립 파서 수취 — music.dirs 실제 기록+기존 audio/retention 키 보존)"
printf '%s' "$SETCHK" | grep -aq 'SET-OK' \
    || echo "MUSIC-FAIL(dir-settings: 기록/보존 원문 미달 — $SETCHK)"
DREM=$(atool music_dir_remove '{"path":"/tmp/mus_dirs_x"}')
echo "MUSIC-DIR-REMOVE-REPLY: ${DREM:-none}"
DL2=$(atool music_dir_list '{}')
echo "MUSIC-DIR-LIST-2: ${DL2:-none} (제거 인증 원문 1행)"
printf '%s' "$DREM" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(dir-remove: ok 수취 실패 — $DREM)"
if printf '%s' "$DL2" | grep -aq 'mus_dirs_x'; then
    echo "MUSIC-FAIL(dir-list-2: 제거 미반영 — $DL2)"
else
    echo "MUSIC-DIR-GONE: mus_dirs_x 소각 확인 (제거 원문 성립)"
fi
if cmp -s "$SETTINGS" "$SNAP"; then
    echo "MUSIC-DIR-BYTE-RESTORE: OK — add/remove 왕복 == seed 원문 바이트 등호(④ T5 수형)"
else
    echo "MUSIC-FAIL(dir-byte-restore: 왕복 후 원문 불일치)"
    echo "  now : $(cat "$SETTINGS")"
    echo "  snap: $(cat "$SNAP")"
fi
rmdir /tmp/mus_dirs_x || echo "MUSIC-FAIL(dir-bury: /tmp/mus_dirs_x 소각 실패 — 수동 소각)"

# ==================================================================
# 16b2. [T3(#97) — 브라우저 실측 세그먼트 (플랜 2026-10-11-music-dir-browser
#       task-3)] 도구 경유 불가(브라우징 = UI 탭 실측 — 브리프 계약): send_input
#       클릭 브래킷(16c 수형)+PX 차분 착탄 판정(16d 수형 — tapq·landtap·mus_px
#       를 상단 tap() 뒤로 위상 이동해 공용 소비 — 본문 무변경 이동).
#       * 라인 규약: 런 도구·캡처·차분만 수급(**합성 어설션 0 — 등호 원문만**
#         — 판정은 하단 18e MUS-BROWSE-CANON). rc는 0 유지(honest-fail 원칙).
#       * 좌표 = 정찰 원칙(16c 원문 수형): env 상수+차분 판정 — 기본 상수는
#         레이아웃 산식의 중앙값이고 런 1 캡처(mus_wsl_browse_*.png·mus_bc_*)
#         가 재보정의 원문(런 2 = 재보정 후 첫 후보 단락). MUS_BROW_RECON=1로
#         ② 캡처 직후 복원·종료(정찰 전용 런 — MUS_RECON 수형).
#       * ⑤의 판정 축의 정직 한계 원장(런 1 갱신): 개봉 cwd = dirs_[dirIndex_]
#         = 등록 루트 자체(①)라 [이 폴더 추가]는 Store 중복 원문("중복(정규화
#         후) — no-op ok" — MusicDirStore.h 계보)으로 **다시 쓰지 않는다** —
#         등록 원문의 강도 leg = ④2(③ 재수형 — cwd = sub_a에서의 신규 추가)가
#         소유하고, ⑤ 이후의 종료 원문 = 브라우즈 회귀(성공 = 개봉 종료 계약
#         ③)+list/settings 원문.
#       흐름(디스패치 원문 5항 — ①부팅·music 런치는 step 6-7 공용 축 소유):
#         ① 브라우즈 탭 루트 등록(music_dir_add BTREE — 하위 2건 mkdir) — 탭
#           존재가 개봉 cwd(계약 ⑤ 현재 탭 루트)의 전제.
#         ② 탭 클릭 브래킷(dirIndex_ = BTREE — landtap) → [폴더 관리] 토글 →
#           [찾아보기] 토글 → cwd 표기 캡처+하위 폴더 목록 캡처 2종.
#         ③ sub_a 행 클릭 = JoinDir 진입(T1 2r-② 소비) → cwd 표기 갱신 캡처.
#         ④ ".." 행 클릭 = Parent 복귀(T1 2r-③④ 소비) → cwd 회귀 캡처.
#         ⑤ [이 폴더 추가] 클릭 → 성공 = 브라우즈 종료(계약 ③)+music_dir_list
#           원문(등록 확인)+settings 원문.
#         ④2(⑤의 등록 강도 leg — 런 1 원장) — ".." 복귀 뒤의 cwd = 등록 루트
#           자체라 추가가 Store 중복 no-op ok(MusicDirStore 원문)으로 등록
#           증명이 비지한다 → ③ 수형을 재수해 cwd = sub_a에서 추가 = **신규
#           등록** — list/settings 원문이 실반영을 절단판정. ⑤ 이후의 종료
#           계약은 동일.
#         6. 브라우즈 종료 원문(트랙 목록 재스캔 정착 캡처 — EYES).
#         (복귀) remove 2콜+settings 바이트 등호(SNAP 대조 — ④ T5 수형)+트리
#           소각(잔상은 17b 가드).
#       * 선행 조건 원장: 이 세그먼트 지점의 music 클라는 step 7-16 경유 —
#         웜업 무음 소멸(첫 합성 클릭 소멸 — 16c 원장)은 소진 상태(landtap 재시도
#         가 흡수)·필터 "d_seed" 잔존(step 10 — 표 1행의 정적 배경 원문).
#         vplayer 창(step 14 재생)은 **여기서 close_window로 회수**한다 — 재생
#         파편이 PX 차분을 흉내 내는 오염의 사전 소각(착탄 판정의 순도 원장).
# ==================================================================
echo "=== 16b2. 브라우저 실측 세그먼트 (#97 T3 — UI 탭 계약) ==="
BTREE=${MUS_BROWSE_TREE:-/tmp/mus_browse}
BT_SUBA="$BTREE/sub_a"
BT_SUBB="$BTREE/sub_b"
[ -d "$BTREE" ] && FAIL "$BTREE already exists — 이전 probe 잔상, 수동 소각 필요"

echo "--- 16b2-0. vplayer 회수 (차분 오염 소각 — step 14 재생 잔재) ---"
VPCLOSE=0
VPQ=""
for i in 1 2 3 4 5 6; do
    VPQ=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    VPW=$(printf '%s' "$VPQ" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
    [ -z "$VPW" ] && break
    VPID=$(printf '%s' "$VPW" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    VPR=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$VPID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "  vplayer close#$i: id=$VPID repl=${VPR:-none}"
    VPCLOSE=$((VPCLOSE + 1))
    sleep 1
done
echo "MUS-BROWSE-VP-RECLAIM: closed=$VPCLOSE (0 = 세그먼트 입구에 Video Player 창이 없었다는 원문 — step 8-14의 위임 경계 후 계보)"
sleep 1

echo "--- 16b2-1. ① 브라우즈 탭 루트 등록 (music_dir_add — 하위 2건 sub_a·sub_b) ---"
mkdir -p "$BT_SUBA" "$BT_SUBB" || FAIL "browse tree mkdir failed"
BATREE=$(atool music_dir_add "{\"path\":\"$BTREE\"}")
echo "MUS-BROWSE-ADD-REPLY: ${BATREE:-none}"
printf '%s' "$BATREE" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(browse-add: ok 수취 실패 — $BATREE)"
BBL0=$(atool music_dir_list '{}')
echo "MUS-BROWSE-LIST-0: ${BBL0:-none} (① 등록 인증 원문 1행 — 탭 스트립의 라벨원)"
printf '%s' "$BBL0" | grep -aq 'mus_browse' \
    || echo "MUSIC-FAIL(browse-list-0: 합성 트리 등록 미반영 — $BBL0)"
echo "MUSIC-BROWSE-SETTINGS-1: $(cat "$SETTINGS") (settings.json 원문 — ① 등록 후)"

echo "--- 16b2-2. 창 재정찰 (16d-0 수형 — 현행 Music 창 id/geo 원문) ---"
MUSWIN=""
WINB=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WINB=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WINB" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window at browse segment — last: ${WINB:-none}"
echo "MUSWIN-16B2: $MUSWIN"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')

BR_STAGE=0
BR_ADD_FAIL_HARD=0   # [T2 fix r1] ⑤ 무전달 hard 마커(18e 종결 rc=1)
echo "--- 16b2-3. ② 탭 클릭 브래킷 (dirIndex_ = BTREE — 정찰-판정기) ---"
python3 "$SHOT_PY" "$LOGDIR/mus_base16b2.png" >"$LOGDIR/mus_base16b2.log" 2>&1 \
    || FAIL "16b2 baseline capture failed — $(tail -2 "$LOGDIR/mus_base16b2.log" | tr '\n' ' ')"
BR_TAB=0
for TY in $MUS_BTAB_YS; do
    if tapsettle "$LOGDIR/mus_base16b2.png" "bt0" "$MUS_BTAB_X" "$TY" 3; then
        BR_TAB=1
        BR_STAGE=2
        echo "MUS-BROWSE-TAB-OK: 탭 착탄(클라 x=$MUS_BTAB_X·y=$TY — dirIndex_=2 = BTREE 전환·active 강조+표 전이) — 도달 시차 ${SETTLE_T}s — 개봉 cwd(계약 ⑤ 현재 탭 루트)의 전제 성립"
        break
    fi
    echo "  탭 무착탄(y=$TY) — 무음 소멸 판정(상태 무변화 = 재시도 안전)"
done
[ "$BR_TAB" -eq 1 ] || echo "MUSIC-FAIL(browse-tab-land: 무착탄 — MUS_BTAB_X/YS 재보정 몫(mus_bc_bt0 원문) — 브라우저 계약 단계 생략, 원장)"

if [ "$BR_STAGE" -eq 2 ]; then
    echo "--- 16b2-4. [폴더 관리] 토글 → [찾아보기] 토글 → 개봉 ---"
    BR_TOG=0
    for TY in $MUS_BTOG_YS; do
        for TX in $MUS_BTOG_XS; do
            if tapsettle "$LOGDIR/mus_bc_bt0.png" "btog" "$TX" "$TY" "$MUS_BROW_TH"; then
                BR_TOG=1
                echo "MUS-BROWSE-PANEL-OK: [폴더 관리] 패널 열림(클라 ${TX},${TY} — 도달 시차 ${SETTLE_T}s) — 탭 수에 동기한 토글 x(3탭 산식 — 16c 상수 재보정 원장 — 런 좌표는 원장행)"
                break 2
            fi
            echo "  토글 무착탄(${TX},${TY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    [ "$BR_TOG" -eq 1 ] \
        || echo "MUSIC-FAIL(browse-panel: 패널 무열림 — MUS_BTOG_XS/YS 재보정 몫(mus_bc_btog 원문) — 브라우저 계약 단계 생략, 원장)"
    if [ "$BR_TOG" -eq 1 ]; then
        BR_OPEN=0
        for TY in $MUS_BBROW_YS; do
            for TX in $MUS_BBROW_XS; do
                if tapsettle "$LOGDIR/mus_bc_btog.png" "bropen" "$TX" "$TY" "$MUS_BROW_TH"; then
                    BR_OPEN=1
                    BR_STAGE=4
                    echo "MUS-BROWSE-OPEN-OK: [찾아보기] 개봉(클라 ${TX},${TY} — 도달 시차 ${SETTLE_T}s) — cwd 표기 1행+".."행+하위 폴더 목록의 레이아웃 전이 성립(diff=$SETTLE_DIFF)"
            cp "$LOGDIR/mus_bc_bropen.png" "$RECEIVE/mus_wsl_browse_open.png" \
                || FAIL "browse_open capture copy failed"
            cp "$LOGDIR/mus_bc_bropen.png" "$RECEIVE/mus_wsl_browse_subs.png" \
                || FAIL "browse_subs capture copy failed"
            echo "MUS-BROWSE-CAPTURE-2: engine/tmp/mus_wsl_browse_open.png+mus_wsl_browse_subs.png (② cwd 표기+하위 폴더 목록 2종 — sub_a/sub_b 2건 = 2r byte asc 결정론의 라이브 원문 — 2건 등장은 캡처 육안 몫 EYES)"
            if [ "${MUS_BROW_RECON:-0}" = "1" ]; then
                echo "MUS-BROW-RECON-EXIT: ② 캡처 확보 — 상수 재보정 후 풀 런(MUS_BROW_RECON=0)"
                RCR=$(atool music_dir_remove "{\"path\":\"$BTREE\"}")
                echo "  recon cleanup remove: ${RCR:-none}"
                cmp -s "$SETTINGS" "$SNAP" \
                    && echo "  recon 원복: settings 바이트 등호" \
                    || echo "  MUSIC-FAIL(brow-recon-restore: settings 원문 불일치)"
                rm -rf "$BTREE"
                restore_and_exit
                echo "MUS-BROW-RECON-END"
                exit 0
            fi
            break 2
        fi
        echo "  [찾아보기] 무착탄(${TX},${TY}) — 무음 소멸 판정(재시도 안전 — 원장)"
    done
    done
    [ "$BR_OPEN" -eq 1 ] \
        || echo "MUSIC-FAIL(browse-open: [찾아보기] 무개봉 — MUS_BBROW_XS/YS 재보정 몫(mus_bc_bropen 원문) — 브라우저 계약 단계 생략, 원장)"
    fi
fi

if [ "$BR_STAGE" -eq 4 ]; then
    echo "--- 16b2-5. ③ 하위 진입 (sub_a 행 클릭 = JoinDir) ---"
    BR_SUB=0
    for SY in $MUS_BSUB_YS; do
        for SX in $MUS_BSUB_XS; do
            if tapsettle "$LOGDIR/mus_bc_bropen.png" "bsub" "$SX" "$SY" "$MUS_BROW_TH_TEXT"; then
                BR_SUB=1
                BR_STAGE=5
                echo "MUS-BROWSE-SUB-ENTER-OK: 하위 진입(클라 ${SX},${SY} — 도달 시차 ${SETTLE_T}s) — cwd 표기 갱신 = JoinDir 소비·목록 교체(diff=$SETTLE_DIFF) — cwd 갱신 원문은 캡처 육안 몫(EYES)"
                break 2
            fi
            echo "  하위 행 무착탄(${SX},${SY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    if [ "$BR_SUB" -eq 1 ]; then
        cp "$LOGDIR/mus_bc_bsub.png" "$RECEIVE/mus_wsl_browse_sub.png" \
            || FAIL "browse_sub capture copy failed"
    else
        echo "MUSIC-FAIL(browse-sub-enter: 무진입 — MUS_BSUB_XS/YS 재보정 몫(mus_bc_bsub 원문) — 개봉 상태 유지 추정, ④⑤ 계속(원장))"
    fi
    echo "--- 16b2-6. ④ ".." 상위 복귀 (Parent — T1 2r-③④ 소비) ---"
    BR_UP=0
    # 차분 기저 = ③ 단계의 정착 프레임(bsub — 무착탄이어도 마지막 정찰 프레임 =
    # 현 상태의 차분 원문 축).
    for UY in $MUS_BUP_YS; do
        for UX in $MUS_BUP_XS; do
            if tapsettle "$LOGDIR/mus_bc_bsub.png" "bup" "$UX" "$UY" "$MUS_BROW_TH_TEXT"; then
                BR_UP=1
                BR_STAGE=6
                echo "MUS-BROWSE-UP-OK: ".." 행 착탄(클라 ${UX},${UY} — 도달 시차 ${SETTLE_T}s) — cwd 표기 회귀 = Parent 소비(diff=$SETTLE_DIFF) — 회귀 원문은 캡처 육안 몫(EYES)"
                break 2
            fi
            echo "  ".." 행 무착탄(${UX},${UY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    if [ "$BR_UP" -eq 1 ]; then
        cp "$LOGDIR/mus_bc_bup.png" "$RECEIVE/mus_wsl_browse_up.png" \
            || FAIL "browse_up capture copy failed"
    else
        echo "MUSIC-FAIL(browse-up: 무복귀 — MUS_BUP_XS/YS 재보정 몫(mus_bc_bup 원문 — cwd 잔류는 ⑤의 등록 원문이 대변, 원장))"
    fi
    echo "--- 16b2-7. ④2 하위 재진입 (③의 재수형 — ⑤의 등록 원문을 신규로) ---"
    # [정직 한계 원장 — 등록 강도 leg]: ".." 복귀 뒤의 cwd = 등록 루트 자체라
    # [이 폴더 추가]는 Store 중복 no-op ok(MusicDirStore 원문 — 다시 쓰지
    # 않는다)로 클릭의 등록 증명이 비지한다. 런 1(칼리브레이션) 원장도 ⑤-2의
    # [찾아보기] 재개봉 leg(25 후보 무성립)로 이 leg가 유일한 강도 축이었다.
    # 그래서 ③ 수형(JoinDir 2수)을 여기서 재수한다: cwd = sub_a 상태의 추가 =
    # **신규 등록** — music_dir_list·settings 원문이 실반영을 절단판정한다.
    # ⑤의 종료·재스캔(계약 ③·기존 재스캔 계약)은 동일 — 디스패치 원문의 ④⑤
    # 의 원문 종류는 전부 남는다(순서 원장: ④2 = ③의 재수형 — 등록 강도 leg).
    BR_SUB2=0
    for SY in $MUS_BSUB_YS; do
        for SX in $MUS_BSUB_XS; do
            if tapsettle "$LOGDIR/mus_bc_bup.png" "bsub2" "$SX" "$SY" "$MUS_BROW_TH_TEXT"; then
                BR_SUB2=1
                echo "MUS-BROWSE-SUB2-ENTER-OK: 하위 재진입(${SX},${SY} — 도달 시차 ${SETTLE_T}s) — cwd = 하위 폴더 원문(구체 행은 y 격자 = sub_a 317-331·sub_b 332-346 — ⑤의 추가 대상 — 캡처 육안 몫 EYES)"
                break 2
            fi
            echo "  하위 행 무착탄(${SX},${SY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    if [ "$BR_SUB2" -eq 1 ]; then
        cp "$LOGDIR/mus_bc_bsub2.png" "$RECEIVE/mus_wsl_browse_sub2.png" \
            || FAIL "browse_sub2 capture copy failed"
    else
        echo "MUSIC-FAIL(browse-sub2: 무재진입 — MUS_BSUB 재보정 몫(mus_bc_bsub2 원문) — ⑤는 cwd 잔류 상태 계속(원장))"
    fi
    echo "--- 16b2-8. ⑤ [이 폴더 추가] 원문 클릭 → tapsettle 착탄+도구 진실원 판정 (T2 fix r1 — 재검) ---"
    # [T2 fix r1 재검 원장 — ⑤ 무전달 재발 방지]: 원존 후보 지대(클라 y
    # 442-457)의 클릭 = 런 5회 × 25 후보 전부 무전달(서버 relay sent=ok·클라
    # 수취 frames=0의 단절 — task-3-report concern 1) — tracks child 음수
    # 높이 압착 경계 원장. T2 fix r1(압착 가드+③④ 버튼행의 목록 위 이동)으로
    # 클릭 지대가 상위 행 성립 축(런 E/F에서 전달 성립된 y≈294-324 원문)으로
    # 이동했다 — MUS_BADD_* = fix r1 뒤의 산식. 판정 = **이중**: 착탄(tapsettle
    # — 종료 전이 임계 MUS_BROW_TH_ADD)+도구 진실원(music_dir_list에서
    # sub_[ab] 등장 = DirAddManaged 신규 등록의 절단판정 — cwd = 하위 폴더가
    # 전제, 그 이름은 ①·도구 어느 경로로도 등록하지 않은 이름). 후보마다 폴
    # 응답 원문을 남긴다(무음 실패 금지 원장). **클릭 실패(전 후보 미성립) =
    # hard 마커(BR_ADD_FAIL_HARD — 18f 끝에서 rc=1 종결, 복귀 16b2-10 이후)**
    # — honest-fail(rc 0 유지)에서 격상: 무전달 재발이 fix r1의 수리 실패
    # 원장이 된다.
    BR_ADD=0
    BRADDEX=""
    BRADDY=""
    BRADDFULL=0
    for ry in $MUS_BADD_YS; do
        for rx in $MUS_BADD_XS; do
            tapsettle "$LOGDIR/mus_bc_bsub2.png" "badd" "$rx" "$ry" "$MUS_BROW_TH_ADD" \
                && { echo "  착탄(종료 전이 — 클라 ${rx},${ry} — 도달 시차 ${SETTLE_T}s diff=$SETTLE_DIFF)"; }
            sleep 1
            BLP=$(atool music_dir_list '{}')
            echo "  BADD poll list: ${BLP:0:200}"
            if printf '%s' "$BLP" | grep -aqE 'sub_[ab]' 2>/dev/null; then
                BRADDEX=$rx; BRADDY=$ry; BRADDFULL=1; break
            fi
            if [ "${SETTLE_DIFF:-0}" -ge "$MUS_BROW_TH_ADD" ]; then
                # 전달 원문(브라우즈 종료 전이) 성립 — 등록 미출현 = no-op 추가
                # 계보(런 2 원장: cwd = 루트의 중복 no-op ok — Store 원문)라
                # **전달 축 성립으로 종결**(등록 강도 축 = MUSIC-FAIL 원장행 —
                # 무음 실패 금지·계속 후보 탐색 없음 — 종료 전이는 역전 불가)
                BRADDEX=$rx; BRADDY=$ry; break
            fi
        done
        [ -n "$BRADDEX" ] && break
    done
    if [ -n "$BRADDEX" ]; then
        BR_ADD=1
        BR_STAGE=8
        if [ "$BRADDFULL" -eq 1 ]; then
            echo "MUS-BROWSE-ADD-CLICK-OK(FULL): [이 폴더 추가] 착탄(클라 ${BRADDEX},${BRADDY}) — cwd = 하위 폴더 = **신규 등록**(DirAddManaged 단일 경로 — [추가] 버튼·music_dir_add 도구와 같은 경로 원문) — 성공 = 브라우즈 종료(계약 ③)"
        else
            echo "MUS-BROWSE-ADD-CLICK-OK(DELIVERED): [이 폴더 추가] 착탄(클라 ${BRADDEX},${BRADDY}) — 종료 전이(diff=$SETTLE_DIFF)+브라우즈 종료 — 단 등록 원문(sub_[ab]) 미출현 = no-op 추가 계보(cwd = 루트 — ④2·행 격자 계보 원장) — **전달 복원 = fix r1 계약 1항의 축**(무전달 소각)"
            echo "MUSIC-FAIL(browse-add-reg: 등록 강도 축 미달 — no-op 원장 — 전달 축과 별개)"
        fi
        sleep 3   # 추가 성공 = 재스캔+종료 렌더의 컴포지트 도착 여유(전윈도 원장)
        python3 "$SHOT_PY" "$LOGDIR/mus_bc_badd.png" >"$LOGDIR/mus_bc_badd.log" 2>&1 \
            || FAIL "badd capture failed — $(tail -2 "$LOGDIR/mus_bc_badd.log" | tr '\n' ' ')"
        cp "$LOGDIR/mus_bc_badd.png" "$RECEIVE/mus_wsl_browse_added.png" \
            || FAIL "browse_added capture copy failed"
        BBL2=$(atool music_dir_list '{}')
        echo "MUS-BROWSE-LIST-2: ${BBL2:-none} (⑤ 등록 확인 원문 1행)"
        echo "MUSIC-BROWSE-SETTINGS-2: $(cat "$SETTINGS") (settings.json 원문 — ⑤ 클릭 뒤)"
        SUBCHK=$(python3 - "$SETTINGS" <<'PYEOF'
import json, sys
with open(sys.argv[1], "rb") as f:
    obj = json.loads(f.read().decode("utf-8"))
dirs = obj.get("music", {}).get("dirs", [])
if any(d.endswith("/sub_a") or d.endswith("/sub_b") for d in dirs):
    print("SUB-OK(하위 등록) dirs=%s" % dirs)
else:
    print("SUB-MISS(하위 미등록) dirs=%s" % dirs)
PYEOF
) || SUBCHK="SUB-MISS(python3 파서 실패)"
        echo "MUSIC-BROWSE-SUB-REG: $SUBCHK (독립 파서 — [이 폴더 추가] 클릭의 등록 원문)"
        if [ "$BRADDFULL" -eq 1 ]; then
            printf '%s' "$SUBCHK" | grep -aq 'SUB-OK' \
                || echo "MUSIC-FAIL(browse-sub-reg: sub_a 실기록 원문 미달 — $SUBCHK)"
        fi
    else
        BR_ADD_FAIL_HARD=1
        echo "MUSIC-FAIL(browse-add: [이 폴더 추가] 무착탄(종료 전이 무성립 · 전 후보) — fix r1 좌표 재보정 몫(16b2-5~8 캡처), 원장)"
        echo "MUS-BROWSE-HARD: ⑤ 무전달 재발 — [이 폴더 추가] 클릭의 종료 전이 둘 다 미성립(하드 마커 — 18e 뒤 rc=1 종결, 복귀 16b2-10 선행)"
    fi
fi

if [ "$BR_STAGE" -eq 8 ] && [ "$BR_ADD" -ge 1 ]; then
    echo "--- 16b2-9. 6. 브라우즈 종료 원문 — 재스캔 정착 캡처 ---"
    sleep 3   # 추가 성공 = 재스캔(기존 계약 — 도착 더티 1프레임) + 컴포지트
              # 프레젠트 지연(런 정찰 ~1-4s — T3 클릭 전윈도 원장) 정착 렌더
    shot mus_wsl_browse_rescan.png 2>/dev/null || true
    echo "MUS-BROWSE-EXIT-LEDGER: 추가 성공 = 브라우즈 종료(계약 ③)+목록 재스캔 — 종료 원문 = mus_wsl_browse_added.png(회귀)+mus_wsl_browse_rescan.png(표 정착) 2캡처(EYES)"
else
    echo "MUS-BROWSE-EXIT-SKIP: 브라우즈 종료 캡처 생략(단계 ${BR_STAGE}/8 — 원장)"
fi

echo "--- 16b2-10. 복귀 — remove 2콜 + settings 바이트 등호 + 트리 소각 ---"
CRA=$(atool music_dir_remove "{\"path\":\"$BT_SUBA\"}")
CRB=$(atool music_dir_remove "{\"path\":\"$BT_SUBB\"}")
CRR=$(atool music_dir_remove "{\"path\":\"$BTREE\"}")
echo "MUS-BROWSE-CLEANUP: sub_a=${CRA:-none} sub_b=${CRB:-none} root=${CRR:-none}"
BR_RESTORE=0
if cmp -s "$SETTINGS" "$SNAP"; then
    BR_RESTORE=1
    echo "MUS-BROWSE-RESTORE-BYTE: OK — 등록/탐색/추가/해제 왕복 == seed 원문 바이트 등호(④ T5 수형)"
else
    echo "MUSIC-FAIL(browse-restore: 원문 불일치)"
    echo "  now : $(cat "$SETTINGS")"
    echo "  snap: $(cat "$SNAP")"
fi
rm -rf "$BTREE" 2>/dev/null
[ -d "$BTREE" ] && echo "MUSIC-FAIL(browse-bury: $BTREE 소각 실패 — 수동 소각)"

# ==================================================================
# 16c. [T3 — I-1 라이브 실측 (T2 리뷰 M-1r)] 부적합(CP949 혼입) 문서 시딩 →
#      부팅 stderr 경고 1행 → 쓰기 정문(settings_set) 거부 detail 원문 →
#      저장소 스캐너 수용(remove 1콜 — CP949 문서에서 dirs 소각) →
#      [패널 제거 치유 루트] — CP949 항목의 in-엔진 치유는 UI [제거] 원촉이
#      유일(도구 인자는 quickjs JSON 문자열 = UTF-8 검사라 CP949 바이트 수송
#      불가 — 16c-4에서 bad_args/파서 거부 원문 실측) — T2 리뷰 I-1 §(1)
#      "music 패널에서 CP949 항목 제거 → 문서 순수 회귀" 원문) → 치유 후
#      music_dir_add 재시도 성공+settings_set 성공(부팅 재기동 불요 — 이후
#      쓰기 정상) → 백업 원복 바이트 등호.
#      * 전각 치유(CP949 변환)·Win 축 pid tmp 출하는 라인 별도(원장).
# ==================================================================
echo "=== 16c. I-1 라이브 (CP949 혼입 시딩 → 거부 detail 원문 → 패널 치유) ==="
CP949_FIX=$(printf '\xB0\xA1\xBF\xE4')   # 2o-f corpus 동형 — CP949 '가요'
PAT="$LOGDIR/mus_cp949.pat"
printf '%s' "$CP949_FIX" > "$PAT"
CPENTRY="$CP949_FIX"
CP_HERE() { grep -aq -F -f "$PAT" "$SETTINGS"; }
printf '{"audio": {"mute": 0, "volume": 50},"retention": {"days": 14},"music":{"dirs":["%s"]}}' \
    "$CP949_FIX" > "$SETTINGS"
CP_HERE || FAIL "CP949 seed write failed"
echo "MUSIC-I1-SEEDED: OK — settings.json을 부적합(CP949 혼입) 원문으로 교체(백업 원문=$SNAP — 원복은 바이트 등호로 증명)"

echo "--- 16c-1. 서버 재기동 1회 (부팅 stderr 경고 캡처용 — 브래킷 pkill 재용) ---"
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
# #98 pre-clean guard: rm on a LIVE server's socket entombs its parked
# listener (client connects land ENOENT) — skip and note.
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "NOTE #98: live server holds /tmp/JKWindowServerPipe.sock — pre-clean rm skipped"
else
    rm -f /tmp/JKWindowServerPipe.sock
fi
I1LOG="$LOGDIR/mus_srv_i1.log"
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$I1LOG" 2>&1 &
sleep 6
SRV2=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
[ -n "$SRV2" ] || FAIL "no server after I-1 restart — log tail: $(tail -3 "$I1LOG" | tr '\n' ' ')"
echo "server pid=$SRV2"
I1_BOOT=0
I1BOOT_WARN=$(grep -a 'settings.json 파싱 실패 — 기본값으로 기동' "$I1LOG" | head -1)
echo "MUSIC-I1-BOOT-WARN: ${I1BOOT_WARN:-MISS}"
[ -n "$I1BOOT_WARN" ] && { I1_BOOT=1; echo "MUS-I1-BOOTW: OK — 부팅 stderr 경고 원문 1행 실측(파일 무접촉 · 기본값 기동 표기 포함)"; } \
    || echo "MUSIC-FAIL(i1-boot-warn: 부팅 경고 미검출 — $I1LOG)"
P2=""
# [T3(#97) 부팅 지연 원장 — step 6과 동일 견적 확장(콜드 재기동의 agentctl
# 응답 ~33s 실측 — 30 시도 × 2s 창)].
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
    P2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$P2" | grep -aq '"ok":true' && break
    sleep 2
done
printf '%s' "$P2" | grep -aq '"ok":true' || FAIL "ping after I-1 restart failed — $P2"
L2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L2" | grep -aq '"ok":true' || FAIL "relaunch music after I-1 restart failed — $L2"
MUSWIN=""
WIN2=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WIN2" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window after I-1 restart — last: ${WIN2:-none}"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
echo "MUSWIN(restart, step-7 수형 재사용): $MUSWIN"
sleep 2   # 부팅 스캔(부적합 문서 — 스캐너 수용 파) 도착 안정화

echo "--- 16c-2. music_dir_add 1콜 (부적합 문서 위 — 수용/거부 원문 실측) ---"
A1=$(atool music_dir_add '{"path":"/tmp/mus_i1_dir"}')
echo "MUSIC-I1-ADD-REPLY: ${A1:-none}"
if printf '%s' "$A1" | grep -aq '"ok":true'; then
    echo "MUS-I1-ADD-OK: 수용 — AddDir의 자기 스캐너는 CP949 문서를 수용·보존한다(2o-f 계열 라이브 원문 — 거부 detail 'settings.json 파싱 실패 — 수기 치유 필요'는 서버 정문(settings_set) 소유 — 16c-3)"
else
    echo "MUSIC-I1-ADD-REJECT: 거부 — 위 reply 원문(브리프 예상 경로가 이쪽이면 리포트 원장)"
fi
I1_KEEP=0
if CP_HERE; then
    I1_KEEP=1
    echo "MUSIC-I1-ADD-KEEP: OK — add 쓰기 뒤 CP949 바이트 보존 원문(무음 소각 봉합의 라이브 영수증)"
else
    echo "MUSIC-FAIL(i1-add-keep: add 쓰기가 CP949 항목을 소각 — T1 fix r1 봉합 부정)"
fi
IL1=$(atool music_dir_list '{}')
echo "MUSIC-I1-LIST-1: ${IL1:-none} (보존-가시 read leg 원문 — CP949 항목도 수취)"

echo "--- 16c-3. 쓰기 정문(settings_set) 거부 detail 원문 (M-1r 본안) ---"
SS1=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"settings_set","args":{"key":"audio_master_volume","value":50}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-I1-SS-REPLY: ${SS1:-none}"
I1_SS=0
if printf '%s' "$SS1" | grep -aq '수기 치유 필요'; then
    I1_SS=1
    echo "MUS-I1-SS-OK: OK — 정문 거부 detail 'settings.json 파싱 실패 — 수기 치유 필요' 라이브 실측"
else
    echo "MUSIC-FAIL(i1-ss: 정문 거부 detail 미검출 — $SS1)"
fi
SS_STDERR=$(grep -a 'settings_set 거부' "$I1LOG" | head -1)
echo "MUSIC-I1-SS-STDERR: ${SS_STDERR:-MISS} (서버 stderr 거부 1행)"
[ -n "$SS_STDERR" ] || echo "MUSIC-FAIL(i1-ss-stderr: 거부 stderr 1행 미검출)"

echo "--- 16c-4. remove 1콜 — 스캐너 수용으로 CP949 문서에서 dirs 소각 ---"
DREM2=$(atool music_dir_remove '{"path":"/tmp/mus_i1_dir"}')
echo "MUSIC-I1-REMOVE-REPLY: ${DREM2:-none}"
I1_RSC=0
printf '%s' "$DREM2" | grep -aq '"ok":true' \
    && { I1_RSC=1; echo "MUS-I1-RSC-OK: OK — remove가 CP949 문서(quickjs 거부 원문)를 자기 스캐너로 수용해 dirs를 소각(원본 보존 — 2o-f 계열)"; } \
    || echo "MUSIC-FAIL(i1-rsc: remove의 스캐너 수용 미관측 — $DREM2)"
IL2=$(atool music_dir_list '{}')
echo "MUSIC-I1-LIST-2: ${IL2:-none}"
if [ "$I1_RSC" -eq 1 ]; then
    CP_HERE && I1_KEEP=1 && echo "MUSIC-I1-RM-KEEP: OK — 제거 뒤에도 CP949 항목 보존(대상 아님 소각 금지 원문)" \
        || echo "MUSIC-FAIL(i1-rm-keep: 다른 항목 제거가 CP949 항목을 소각)"
fi
RA=$(atool music_dir_remove "{\"path\":\"$CPENTRY\"}")
echo "MUSIC-I1-REMOVE-CP949-REPLY: ${RA:-none} (도구 인자 경계 — AgentJson JSON 문자열은 UTF-8 검사라 CP949 바이트 수송 불가 — 패널 [제거] 원촉이 유일 인-엔진 치유 루트의 원문 근거)"

echo "--- 16c-5. [패널 제거 치유 루트] — [폴더 관리] 토글 → [제거] 클릭 ---"
# 정찰 원칙(T4 수형 — env 재보정): 클라 상대 좌표는 라이브 캘리브레이션 세션
# (2026-10-10)의 클릭-차분+툴팁 검증으로 찍었다. 합성 캡처 좌표계 = 클라 좌표
# + (417,173) — 캡처 원점이 데스크톱 (0,0)이 아니어서 절대 좌표 직용 불가(레슨
# — 캡처↔클라 변환 필수). [폴더 관리] 토글 = 클라 (130,70) — 툴팁("settings.
# music.dirs 폴더 추가/제거…")이 그 마우스 밑에서 뜬 채 패널이 열린 캡처가
# 증거. 상태행 감김(+17px 시프트)이 토글 x존 오른쪽 밖을 가리키게 한 (160,77)
# 클릭은 MISS였다(트랩 원장 — 상태행 1행 보장 단계에서 재측정). 패널 첫 행
# [제거] SmallButton = 클라 x=86,y=118(실증 치유 콜) — 이웃 격자 폴백. 클릭
# 판정: 토글 = 캡처 차분(열림 = 레이아웃 전이 — 임계 15행), [제거] =
# settings.json 바이트(무음 실패 금지 원장).
# PX(캡처 차분기) 정의는 상단 tap() 뒤로 위상 이동했다(16b2 브라우저
# 세그먼트가 공용 소비 — step 8 이전 쓰기 원장 — 본문 무변경 이동).
I1_OPEN=0
I1_CURE=0
if [ "$I1_BOOT" -eq 1 ] && [ "$I1_SS" -eq 1 ]; then
    # 웜업 클릭(무해 지점 — 표 남는칸의 빈 행역): 재기동 뒤 앱의 **첫 합성
    # 클릭은 무음 소멸한다**(2026-10-10 실측 렛슨 — cal/exp/repro/run-4/run-5
    # 5세션 전부 첫 클릭 0-diff, 2nd부터 착탄) — 착탄 판정 전 소거.
    WK=$(tap "$MUS_ID" "$((MUS_X + 300))" "$((MUS_Y + 300))")
    printf '%s' "$WK" | grep -aq '"ok":true' || echo "  warmup tap reply: $WK (계속 — 무해 지점)"
    sleep 1
    rm -f "$LOGDIR/mus_px_pre.png"
    python3 "$SHOT_PY" "$LOGDIR/mus_px_pre.png" >"$LOGDIR/mus_px_pre.log" 2>&1 \
        || FAIL "I-1 pre-state capture failed — $(tail -2 "$LOGDIR/mus_px_pre.log" | tr '\n' ' ')"
    TOG_TRIES=0
    while [ "$TOG_TRIES" -lt 3 ]; do
        TOG_TRIES=$((TOG_TRIES + 1))
        TCK=$(tap "$MUS_ID" "$((MUS_X + MUS_TOGGLE_X + MUS_TOGGLE_TWEAK_X))" \
            "$((MUS_Y + MUS_TOGGLE_Y))")
        printf '%s' "$TCK" | grep -aq '"ok":true' \
            || FAIL "I-1 toggle tap failed — $TCK"
        echo "CLICK-TOGGLE#$TOG_TRIES: id=$MUS_ID 클라($MUS_TOGGLE_X,$MUS_TOGGLE_Y) — repl: $TCK"
        sleep 1.3
        rm -f "$LOGDIR/mus_px_tog.png"
        python3 "$SHOT_PY" "$LOGDIR/mus_px_tog.png" >"$LOGDIR/mus_px_tog.log" 2>&1 \
            || FAIL "I-1 toggle-open capture failed — $(tail -2 "$LOGDIR/mus_px_tog.log" | tr '\n' ' ')"
        OP=$(python3 "$PX" "$LOGDIR/mus_px_pre.png" "$LOGDIR/mus_px_tog.png")
        echo "MUSIC-I1-PX-OPEN-CHECK#$TOG_TRIES: $OP (토글 판정 — 임계 15행)"
        if printf '%s' "$OP" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p' | \
            awk '{exit ($1 >= 15) ? 0 : 1}'; then
            I1_OPEN=1
            echo "MUS-I1-TOGGLE-OK: [폴더 관리] 패널 열림 ($TOG_TRIES 시도)"
            break
        fi
        echo "  무변화(0-diff) — 첫 클릭 소멸 렛슨(웜업 후에도 간헐) — 재시도"
        cp "$LOGDIR/mus_px_tog.png" "$LOGDIR/mus_px_pre.png"
    done
    [ "$I1_OPEN" -eq 1 ] \
        || echo "MUS-I1-TOGGLE-MISS: 3시도 무변화 — MUS_TOGGLE_X/Y 재보정 몫"
else
    echo "MUS-I1-PANEL-SKIP: 부팅 경고/정문 거부 전항 미달 — 치유 루트 단계 생략(원장)"
fi
if [ "$I1_OPEN" -eq 1 ]; then
    shot mus_wsl_dirs_panel.png   # [폴더 관리] 패널 모양 1캡처(T3 브리프 — 치유 직전 상태)
    for RY in $MUS_REMOVE_YS; do
        for RX in $MUS_REMOVE_XS; do
            RK=$(tap "$MUS_ID" "$((MUS_X + RX))" "$((MUS_Y + RY))")
            printf '%s' "$RK" | grep -aq '"ok":true' \
                || FAIL "I-1 cure grid tap failed — $RK"
            sleep 1
            if ! CP_HERE; then
                I1_CURE=1
                echo "MUS-I1-CURE-OK: OK — 패널 [제거] 클릭(클라 x=$RX,y=$RY)이 CP949 항목을 소각했다 (tap reply: ${RK:-none} — 도구 인자 경계 원문은 16c-4)"
                break 2
            fi
            echo "  cure try (클라 x=$RX,y=$RY) — CP949 잔존(클릭 미적중)"
        done
    done
fi
if [ "$I1_CURE" -eq 1 ]; then
    echo "MUS-I1-PANEL-OK: 패널 제거 치유 루트 성립 — [폴더 관리] 토글 → [제거] 1콜로 문서 순수 회귀(T2 리뷰 I-1 §(1) 원문)"
else
    echo "MUS-I1-PANEL-MISS: 치유 루트 미검출 — 재정찰 런 필요(MUS_TOGGLE_X/Y·MUS_REMOVE_XS/YS — 위 mus_wsl_dirs_panel·mus_px_tog 원문)"
fi
IL3=$(atool music_dir_list '{}')
echo "MUSIC-I1-LIST-3: ${IL3:-none} (치유 후 list 원문)"
if [ "$I1_CURE" -eq 1 ]; then
    PURE=$(python3 - "$SETTINGS" <<'PYEOF'
import json, sys
try:
    with open(sys.argv[1], "rb") as f:
        obj = json.loads(f.read().decode("utf-8"))
except Exception as e:
    print("PURE-MISS %r" % e)
    raise SystemExit
print("PURE-OK audio=%s retention=%s dirs=%s"
      % ("audio" in obj, "retention" in obj,
         obj.get("music", {}).get("dirs", [])))
PYEOF
) || PURE="PURE-MISS(python3 런처 실패)"
    echo "MUSIC-I1-PURE-JSON: $PURE (독립 파서 — 문서 순수 회귀 영수증: 부팅 리더 회복 대리)"
    if printf '%s' "$PURE" | grep -aq 'PURE-OK'; then
        echo "MUS-I1-PURE-OK: OK — CP949 바이트 0(문서 순수)"
    else
        echo "MUSIC-FAIL(i1-pure: 치유 후 문서 비순수 — $PURE)"
        I1_CURE=0
    fi
    echo "--- 16c-6. 치유 후 music_dir_add 재시도 + settings_set 성공 (이후 쓰기 정상) ---"
    # 관측 원장(2026-10-10 런6): 패널 [제거]의 저장소 쓰기(원자적 — .bak 잔산)
    # 직후 music 클라가 세그폴트 사망한다(cal 세션과 2전 2 — tool_gone/
    # unknown_app_tool 답신). 서버는 생존(settings_set 계속 성립) — 신규 결함
    # 원장은 리포트(치유 경로의 P1 — T2 UI/스캔 leg 어느 축 — gdb 원문 몫).
    A2=$(atool music_dir_add '{"path":"/tmp/mus_i1_retry"}')
    echo "MUSIC-I1-ADD-RETRY-REPLY: ${A2:-none}"
    if printf '%s' "$A2" | grep -aq '"ok":true'; then
        I1_RETRY=1
        echo "MUS-I1-RETRY-OK: OK — 치유 후 music_dir_add 재시도 성공(부팅 재기동 불요)"
    elif printf '%s' "$A2" | grep -aq 'unknown_app_tool\|tool_gone'; then
        echo "MUSIC-FAIL(i1-add-retry: music 클라 [제거] 쓰기 뒤 세그폴트 사망 — 치유 후 도구 경로 소멸 — 재기동 후 재실측(앱 재스폰 — 서버 부팅 불요))"
        RL=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$RL" | grep -aq '"ok":true' || FAIL "music relaunch after client crash failed — $RL"
        echo "MUSIC-I1-APP-RELAUNCH: OK"
        sleep 3
        A2=$(atool music_dir_add '{"path":"/tmp/mus_i1_retry"}')
        echo "MUSIC-I1-ADD-RETRY-REPLY-2: ${A2:-none}"
        printf '%s' "$A2" | grep -aq '"ok":true' \
            && { I1_RETRY=1; echo "MUS-I1-RETRY-OK: OK — 재스폰 뒤 add 재시도 성공(이후 쓰기 정상)"; } \
            || echo "MUSIC-FAIL(i1-add-retry-2: 재스폰 뒤 등록 실패 — $A2)"
    else
        echo "MUSIC-FAIL(i1-add-retry: 치유 후 등록 실패(기타) — $A2)"
    fi
    IL4=$(atool music_dir_list '{}')
    echo "MUSIC-I1-LIST-4: ${IL4:-none} (재시도 등록 목록 원문)"
    SS2=$(timeout 15 ./buildwsl/jkdesktop agentctl \
        '{"tool":"settings_set","args":{"key":"audio_master_volume","value":50}}' \
        2>/dev/null | grep -a '{' | head -1)
    echo "MUSIC-I1-SS-POST-REPLY: ${SS2:-none}"
    I1_SS2=0
    if printf '%s' "$SS2" | grep -aq '"ok":true'; then
        I1_SS2=1
        echo "MUS-I1-SS2-OK: 치유 후 정문 쓰기 성공(부팅 재기동 불요 — 이후 쓰기 정상 원문)"
    else
        echo "MUSIC-FAIL(i1-ss2: 치유 후 정문 쓰기 실패 — $SS2)"
    fi
    # C1 라이브 등가는 **파일 수준** 판정으로 바꾼다(app 사망 시 tool_gone이
    # C1 부정으로 오독하는 함정 — 정직 부기). settings_set(정문)이 미관리
    # music 키를 원문 그대로 보존했는지 = 독립 파서 수취.
    C1CHK=$(python3 - "$SETTINGS" <<'PYEOF'
import json, sys
with open(sys.argv[1], "rb") as f:
    obj = json.loads(f.read().decode("utf-8"))
print("C1-OK music=%s audio=%s retention=%s" %
      (obj.get("music"), "audio" in obj, "retention" in obj))
PYEOF
) || C1CHK="C1-MISS(python3 파서 실패)"
    echo "MUSIC-I1-C1-FILE: $C1CHK (정문 쓰기 뒤 파일 원문 — music 키 생존 = C1 라이브 등가)"
    printf '%s' "$C1CHK" | grep -aq 'C1-OK' \
        || echo "MUSIC-FAIL(i1-c1: settings_set이 미관리 music 키를 소각 — C1 부정: $C1CHK)"
else
    I1_RETRY=0
    I1_SS2=0
    echo "MUS-I1-POST-CURE-SKIP: 치유 MISS — 이후 단계는 정찰 재보정 런에서(settings는 백업 원복으로 정리)"
fi
cp "$SNAP" "$SETTINGS" || echo "MUSIC-FAIL(i1-restore: 백업 복사 실패)"
I1_RESTORE=0
cmp -s "$SETTINGS" "$SNAP" && I1_RESTORE=1 \
    && echo "MUSIC-I1-RESTORE-BYTE: OK — 백업 원복 바이트 등호(T5 수형)" \
    || echo "MUSIC-FAIL(i1-restore: 원복 바이트 불일치)"

# ==================================================================
# 16d. [T3 — 스캔 취소 경계 세그먼트 (플랜 2026-10-10-music-scan-cancel
#      task-3)] 스캔 진행 중 창 닫기(①)·폴더 제거(②)·취소 후 재스캔 정착(③)
#      실측 — **코드 무변경 라인**(원문 수급 = 도구 창 close·서버 로그·캡처).
#      * ①관측 대상(T2 #93 결정 ②): ~ClientMusicApp의 join 전 취소 1발 —
#        스캔 진행 중 창 close는 워커를 T1 경계 콜백에서 절단해 join이 ms급
#        이어야 한다. **취소가 없으면(레거시) join은 완주 스캔 잔량을 견딘다**
#        (ListAudioFiles 유한 — 2m-e) — 그래서 본 세그먼트의 판정축은
#        close→소멸 시차 + crash 마커 0(로그 델타) 2종.
#      * ②관측 원리(T2 결정 ③④ — 도착 멤버십 필터의 세대 게이트 앞 단 배치
#        원장): [제거]의 취소 1발이 진행 스캔(big_a)을 절단 → 도착 pub1
#        (root=big_a — dirs_ 비멤버)은 OnIdle에서 폐기+stderr 1행. 이어지는
#        재청구의 루트는 ResolveDirs의 **부분 범위 승계**(dirs_=[default,
#        seed, big_b]·dirIndex_=2 클램프 회피)가 big_b로 흘려주고(big_b 재스
#        캔 4.9-5.4s 실측) pub1↔pub2 사이에 클라 펌프 틱(SDL_Delay(1) — 원문)이
#        확실히 끼어든다 → 폐기 1행의 관측이 경합이 아니라 구도다. pub1/pub2
#        가 µs급으로 인접하는 구성(재청구 루트가 소형)은 관측 불가 구역 —
#        2 트리 시드의 이유이고 T2 리포트 concern 1 원장.
#      * ③의 정착 영수증: 스캔→도착 채택의 더티 1프레임(도착 더티 원문 계약)
#        — 표기 곡 카운터(460800) 캡처 = EYES 몫. 캐논은 코드 무변경으로
#        등호 승계(하단 16d-6·step 3의 런 PASS 원문 병기).
# ==================================================================
echo "=== 16d-0. 창 재정찰 (16c 이후의 현행 music 창 — id/geo 원문) ==="
MUSWIN=""
WIN16=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN16=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WIN16" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window at cancel segment — last: ${WIN16:-none}"
echo "MUSWIN-16D: $MUSWIN"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')

tapq() { # $1=id $2=x $3=y — 탭 1회 재시도(런 1 원장: 취소 경계의 시차 압박
         #   아래 답신 공백 — 재시도 1회로 성격 보존; 3회 정찰 반복이 필요한
         #   상수 캘리브레이션은 env 몫)
    local r i
    r=$(tap "$1" "$2" "$3")
    case "$r" in *'"ok":true'*) printf '%s' "$r"; return 0;; esac
    for i in 1 2; do
        echo "  tapq miss(i=$i) — 3s 후 재시도: ${r:-empty}"
        sleep 3
        r=$(tap "$1" "$2" "$3")
        case "$r" in *'"ok":true'*) printf '%s' "$r"; return 0;; esac
    done
    printf '%s' "$r"
}

# [T3 런-2 원장] 탭의 ok/sent 답신은 **전송 성립**이지 착탄이 아니다: 런 1-2
# 에서 탭의 UI 전이(dirIndex 전환+스캔 중 행)가 수 초 뒤에야 화면에 도달했다
# (런-1 ②: 제거 뒤가 된 클램프 상태에서 최종 관측 — 선행 MouseMove/이벤트
# 경계 원장). 그래서 탭은 **착탄 판정과 함께** 낸다: 기저 캡처와의 차분(16c
# 의 mus_px 도구 재용 — 스캔 중 행 교체+active-tab 강조 = 레이아웃 전이)이
# 임계 행을 넘을 때까지 y 스윕 × 시도 상한. 검증 없는 탭 = 레거시(완주 스캔)
# 경계와 구별 불가 — T3의 판정축 원문이 이 판정기다.
# (landtap 정의는 상단 tap() 뒤로 위상 이동했다 — 16b2 공용 소비 — 본문
#   무변경 이동.)

echo "=== 16d-1. 대형 합성 트리 시드 (2 트리 — 트랙 $BIG_TRACKS/트리, 빈 .wav 셸) ==="
cat > "$LOGDIR/mus_bigseed.py" <<'PYEOF'
# [T3] 대형 합성 트리 시더 — 빈 .wav 셸 파일(스캔은 내용을 읽지 않는다 —
# MusicModel.h 원문: 확장자 필터+stat만 소비). 파일 수수 정직.
import os, sys, time
root, subs, per = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
t = time.time()
for d in range(subs):
    p = os.path.join(root, "sub_%02d" % d)
    os.makedirs(p, exist_ok=True)
    for f in range(per):
        fd = os.open(os.path.join(p, "m_%05d.wav" % f),
                     os.O_CREAT | os.O_WRONLY, 0o644)
        os.close(fd)
n = 0
for _d, _dirs, fs in os.walk(root):
    n += sum(1 for x in fs if x.endswith(".wav"))
print("BIGSEED-OK %s tracks=%d seed_sec=%.1f" % (root, n, time.time() - t))
PYEOF
for TREE in "$BIGA" "$BIGB"; do
    [ -d "$TREE" ] && FAIL "$TREE 존재 — 16d ENTRY 가드 대조 불일치, 수동 소각 필요"
done
python3 "$LOGDIR/mus_bigseed.py" "$BIGA" "$BIG_SUBS" "$BIG_FILES" \
    >"$LOGDIR/mus_bigseed_a.log" 2>&1 \
    || FAIL "big seed A failed — $(tail -2 "$LOGDIR/mus_bigseed_a.log" | tr '\n' ' ')"
sed 's/^/  /' "$LOGDIR/mus_bigseed_a.log"
python3 "$LOGDIR/mus_bigseed.py" "$BIGB" "$BIG_SUBS" "$BIG_FILES" \
    >"$LOGDIR/mus_bigseed_b.log" 2>&1 \
    || FAIL "big seed B failed — $(tail -2 "$LOGDIR/mus_bigseed_b.log" | tr '\n' ' ')"
sed 's/^/  /' "$LOGDIR/mus_bigseed_b.log"
BIGA_COUNT=$(find "$BIGA" -name '*.wav' | wc -l)
[ "$BIGA_COUNT" -eq "$BIG_TRACKS" ] \
    || FAIL "big seed A count=$BIGA_COUNT want=$BIG_TRACKS (파일 수수 정직 가드)"
# 시드 writeback 배수(런 1 원장 — 2×460k 생성 뒤 클라의 drvfs(settings.json
# 원자적 쓰기)가 VHD writeback 경합으로 분 단위 막혀 도구·입력 응답이 다 시간
# 초과했다) — sync로 메모리 dirty 방출을 선행 배수한다(런 2 — 수리 몫).
SYNC_T0=$(date +%s.%N)
sync
echo "MUS-BIGSEED-SYNC: drain=$(awk -v a="$SYNC_T0" -v b="$(date +%s.%N)" 'BEGIN{printf "%.1f", b-a}')s (시드 writeback 선행 배수 — drvfs 경합 방지 원장)"
echo "MUSIC-SIZE-EVIDENCE: tracks=$BIG_TRACKS/트리(subs=$BIG_SUBS × files=$BIG_FILES) — 스캔은 stat만 소비(원문 주석), 수 초 요구의 성립은 16d-4 도착 시차로 별도 실측"

echo "=== 16d-2. 등록 (music_dir_add × 2) → list 원문(등록) → settings 원문 ==="
BLIST0=$(atool music_dir_list '{}')
echo "MUSIC-BIG-LIST-0: ${BLIST0:-none} (등록 전 기저 원문)"
BADD_A=$(atool music_dir_add "{\"path\":\"$BIGA\"}")
echo "MUSIC-BIG-ADD-A-REPLY: ${BADD_A:-none} (추가의 재스캔은 활성 탭 0 — 합성 트리는 탭 전환 뒤부터 스캔 대상)"
printf '%s' "$BADD_A" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(big-add-a: ok 수취 실패 — $BADD_A)"
BADD_B=$(atool music_dir_add "{\"path\":\"$BIGB\"}")
echo "MUSIC-BIG-ADD-B-REPLY: ${BADD_B:-none}"
printf '%s' "$BADD_B" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(big-add-b: ok 수취 실패 — $BADD_B)"
BLIST1=$(atool music_dir_list '{}')
echo "MUSIC-BIG-LIST-1: ${BLIST1:-none} (①등록 인증 원문 1행)"
printf '%s' "$BLIST1" | grep -aq 'mus_big_a' && printf '%s' "$BLIST1" | grep -aq 'mus_big_b' \
    || echo "MUSIC-FAIL(big-list-1: big 트리 등록 미반영 — $BLIST1)"
echo "MUSIC-BIG-SETTINGS-1: $(cat "$SETTINGS") (settings.json 원문 — big 2종 등록)"

# [T3 런-3 원장 — 세그먼트 순서] 브리프 표기는 ①close/②remove/③정착이나 실행
# 은 **②→①→③**이다. 사유(순서 등가 — 원장): 탭 착탄(ok/sent ≠ 착탄 — 런 1-3
# 원장)이 폐기 관측과 close 경계의 시작점 판정을 흐렸는데, ②의 제거가 자체
# 재청구(원존 계약 — 4.9-5.4s 실측 재스캔)를 **자동으로** 세우므로 그 창이
# ①의 in-flight close 경계로 재사용되고, ③의 정착은 재스폰 클라의 스캔-채택
# 전이로 독립 실측한다. 세 경계의 원문 종류는 브리프 각 조와 동일하다.
echo "=== 16d-3. [②] 스캔 진행 중 music_dir_remove (취소 1발+도착 멤버십 필터) ==="
TAP2_S=$(date +%s.%N)
# 현행(16c 클라) 1차 시도 — 기저 캡처와의 차분으로 착탄 판정, 실패면 고착 클라
# 를 close로 회수하고 재스폰 경로(런 3 원장: 신생 클라+웜업이 성립 경로).
python3 "$SHOT_PY" "$LOGDIR/mus_base16d.png" >"$LOGDIR/mus_base16d.log" 2>&1 \
    || FAIL "16d baseline capture failed — $(tail -2 "$LOGDIR/mus_base16d.log" | tr '\n' ' ')"
echo "16d-BASE: 기저 캡처($LOGDIR/mus_base16d.png) — 탭 착탄 판정의 전 프레임"
BIG_TAP_LAND2=0
if landtap "$MUS_ID" "$LOGDIR/mus_base16d.png" "c0" "$MUS_X" "$MUS_Y" "$MUS_TAB3_X"; then
    BIG_TAP_LAND2=1
    echo "BIG-LAND(current): OK — 탭 착탄(y=${MUS_TAB3_Y_EFF}) — dirIndex_=2 = mus_big_a 전환의 착탄 캡처가 '스캔 중...' 행을 대변(EYES)"
else
    echo "MUS-CURRENT-NO-LAND: 현행 클라 5시도 무착탄 — 고착 회수(close)+재스폰 경로로 승계(런 3 원장 수형)"
    FC0=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$MUS_ID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "MUSIC-FROZEN-CLOSE-REPLY: ${FC0:-none} (고착 클라 회수 — 아래 16d-4의 in-flight close 영수증과 별개)"
    for i in $(seq 1 14); do
        sleep 1
        W16=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$W16" | grep -aq '"title":"Music"' || break
    done
    RL16B=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$RL16B" | grep -aq '"ok":true' || FAIL "music respawn (land fallback) failed — $RL16B"
    echo "MUSIC-RESPAWN-REPLY-FB: $RL16B"
    MUSWIN=""
    for i in 1 2 3 4 5 6 7 8; do
        sleep 2
        W18=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        MUSWIN=$(printf '%s' "$W18" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
        [ -n "$MUSWIN" ] && break
    done
    [ -n "$MUSWIN" ] || FAIL "no Music window (land fallback) — last: ${W18:-none}"
    MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    echo "MUSWIN-FB: $MUSWIN"
    sleep 2
    WK16=$(tap "$MUS_ID" "$((MUS_X + 300))" "$((MUS_Y + 300))")   # 첫 합성 클릭 무음 소멸 렛슨(16c 원장)
    printf '%s' "$WK16" | grep -aq '"ok":true' || echo "  warmup tap reply: $WK16 (계속 — 무해 지점)"
    sleep 0.5
    python3 "$SHOT_PY" "$LOGDIR/mus_base16d.png" >"$LOGDIR/mus_base16d.log" 2>&1 \
        || FAIL "16d(fallback) baseline capture failed — $(tail -2 "$LOGDIR/mus_base16d.log" | tr '\n' ' ')"
    if landtap "$MUS_ID" "$LOGDIR/mus_base16d.png" "c0b" "$MUS_X" "$MUS_Y" "$MUS_TAB3_X"; then
        BIG_TAP_LAND2=1
        echo "BIG-LAND(fallback): OK — 재스폰 클라에서 탭 착탄(y=${MUS_TAB3_Y_EFF})"
    else
        echo "MUSIC-FAIL(big-land-fb: 재스폰 뒤에도 무착탄 — 폐기 관측은 원장 MISS(MUS_TAB3 재부정찰 몫))"
    fi
fi
cp "$LOGDIR/mus_land_c0.png" "$RECEIVE/mus_wsl_scan_busy.png" 2>/dev/null \
    || cp "$LOGDIR/mus_land_c0b.png" "$RECEIVE/mus_wsl_scan_busy.png" 2>/dev/null \
    || shot mus_wsl_scan_busy.png
echo "MUSIC-SCAN-BUSY-CAPTURE: engine/tmp/mus_wsl_scan_busy.png (제거 앞 탭 캡처 — '스캔 중...' 표기·탭 강조는 EYES 몫; 캡처 내용은 프레젠트 관할 — 런 1-3 스테일 프레임 원장)"
sleep 0.4
REM1_SEC=$(date +%s.%N)
LLOG0=$(wc -c < "$I1LOG")
L16D0=$LLOG0
BDREM=$(atool music_dir_remove "{\"path\":\"$BIGA\"}")
echo "MUSIC-SCAN-REMOVE-REPLY: ${BDREM:-none} (스캔 진행 중 제거 — 취소 1발+재청구 원문 계약)"
printf '%s' "$BDREM" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(scan-remove: ok 수취 실패 — $BDREM)"
BIG_DISC_AT=""
DISC_LINE=""
for i in $(seq 1 20); do
    sleep 0.4
    L16D=$(wc -c < "$I1LOG")
    if [ "$L16D" -gt "$L16D0" ]; then
        DISC_LINE=$(tail -c $((L16D - L16D0)) "$I1LOG" | grep -a '도착 폐기' | head -1)
        [ -n "$DISC_LINE" ] && { BIG_DISC_AT=$(date +%s.%N); break; }
    fi
done
echo "MUSIC-ARRIVAL-DISCARD: ${DISC_LINE:-MISS} (stderr 폐기 진단 1행 원문 — T2 수형 ·pub1=published 취소 절단분)"
if [ -n "$DISC_LINE" ] && [ -n "$BIG_DISC_AT" ]; then
    DISC_ELAPSED=$(awk -v a="$REM1_SEC" -v b="$BIG_DISC_AT" 'BEGIN{printf "%.2f", b-a}')
    echo "MUS-DISCARD-AT: 제거→폐기 도착=${DISC_ELAPSED}s (취소 성립 판정축 — 취소 없으면 460k 완주 후 = ~5s+)"
    awk -v d="$DISC_ELAPSED" 'BEGIN{exit (d <= 3.0) ? 0 : 1}' \
        && echo "MUS-CANCEL-EFFECTIVE: OK — 절단이 완주 잔량보다 앞섬(취소 join/절단 라이브 영수증)" \
        || echo "MUSIC-FAIL(scan-cancel-slow: 폐기 도착이 완주 스캔급 — 취소 절단 불성실(원장))"
    printf '%s' "$DISC_LINE" | grep -aq 'mus_big_a' \
        || echo "MUSIC-FAIL(discard-root: 폐기 진단의 루트가 제거 대상 아님 — $DISC_LINE)"
else
    DDELTA=$(tail -c $(( $(wc -c < "$I1LOG") - L16D0 )) "$I1LOG" | tail -12 | sed 's/^/    /')
    echo "MUSIC-FAIL(scan-remove-discard: 폐기 진단 1행 미검출 — 폴링 창의 로그 델타 원문 [${DDELTA:-0바이트}] — landing=$BIG_TAP_LAND2·pub1/pub2 구도 재판정 몫(원장))"
fi
BLIVE=$(atool music_dir_list '{}')
echo "MUSIC-ALIVE-LIST: ${BLIVE:-none} (클라 생존 원문 — 제거 경계 뒤 도구 응답 ok = 도구 경로 정상)"
printf '%s' "$BLIVE" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(remove-alive: 제거 경계 뒤 클라 생존 원문 부정 — $BLIVE)"
WALIVE=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$WALIVE" | grep -aq '"title":"Music"' \
    && echo "MUSIC-ALIVE-WINDOW: OK — Music 창 생존 원문 1행" \
    || echo "MUSIC-FAIL(remove-alive-window: Music 창 소멸(사망 원장) — $WALIVE)"
BLIST2=$(atool music_dir_list '{}')
echo "MUSIC-BIG-LIST-2: ${BLIST2:-none} (②제거 인증 원문 1행 — mus_big_a 부재·mus_big_b 잔존)"
if printf '%s' "$BLIST2" | grep -aq 'mus_big_a'; then
    echo "MUSIC-FAIL(big-list-2: 제거 대상 잔존 — $BLIST2)"
else
    printf '%s' "$BLIST2" | grep -aq 'mus_big_b' \
        && echo "MUSIC-BIG-REMOVED: OK — mus_big_a 제거 + mus_big_b 원존(제거 원문 성립)" \
        || echo "MUSIC-FAIL(big-post-remove: big_b도 부재한 목록 — $BLIST2)"
fi
echo "MUSIC-BIG-SETTINGS-2: $(cat "$SETTINGS") (settings.json 원문 — 제거 후 파일)"

echo "=== 16d-4. [①] 스캔 진행 중 창 close (소멸자 취소 join — T2 결정 ②) ==="
# ②의 제거가 남긴 재청구 = dirs_[dirIndex_] = mus_big_b(클램프 승계 — pub2)의
# 재스캔(~4.9-5.4s 실측)이 진행 중인 창에 근접 close한다(제거 시점(REM1_SEC)
# 대비 ~2-3s = 스캔 중반). in-flight 근거 = 시차 영수증+고속 소멸+crash 마커 0
# (런 3 원장: 탭의 착탄 지연이 close 시작점 판정을 흐려 PUB2 진입점을 택했다 —
# 사전 '스캔 중...' 캡처는 2.3s 캡처 비용이 5.2s 스캔 전량이라 시차에서 불가).
sleep 2
CLOSE1_SEC=$(date +%s.%N)
BCLS=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$MUS_ID}}" 2>/dev/null | grep -a '{' | head -1)
echo "MUS-CLOSE-REPLY: ${BCLS:-none} (서버 close_window id 직접호출 — wsl_chat_close.sh 원형 승계)"
printf '%s' "$BCLS" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(close-reply: ok 수취 실패 — $BCLS)"
BIG_GONE=0
GONE_SEC=""
for i in $(seq 1 24); do
    sleep 0.5
    W19=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    if ! printf '%s' "$W19" | grep -aq '"title":"Music"'; then
        BIG_GONE=1
        GONE_SEC=$(date +%s.%N)
        break
    fi
done
CLOSE_DONE_SEC=$(date +%s.%N)
if [ "$BIG_GONE" -eq 1 ] && [ -n "$GONE_SEC" ]; then
    echo "MUS-CLOSE-GONE: OK — Music 창 소멸(list_windows — chat_close 소멸 단정 선형) close→소멸=$(awk -v a="$CLOSE1_SEC" -v b="$GONE_SEC" 'BEGIN{printf "%.2f", b-a}')s"
    echo "MUS-CLOSE-JOIN-LEDGER: 소멸 시차 <=3.5s면 취소 join ms급 성립(수 초 = 레거시 완주 join 잔량 — 원장)"
    awk -v a="$CLOSE1_SEC" -v b="$GONE_SEC" 'BEGIN{exit (b-a <= 3.5) ? 0 : 1}' \
        || echo "MUSIC-FAIL(close-gone-slow: 소멸 시차 > 3.5s — 취소 절단이 스캔 완주 잔량 뒤에 도달(원장))"
    echo "MUS-REMOVE-CLOSE-OFFSET: ②제거→①close=$(awk -v a="$REM1_SEC" -v b="$CLOSE1_SEC" 'BEGIN{printf "%.2f", b-a}')s (pub2 재스캔 ~4.9-5.4s 실측 중의 착탄 — in-flight 시차 영수증)"
else
    echo "MUSIC-FAIL(close-gone: Music 창 미소멸 — $GONE_SEC)"
fi
BIG_CRASH=0
LLOG1=$(wc -c < "$I1LOG")
if [ "$LLOG1" -gt "$LLOG0" ]; then
    CDELTA=$(tail -c $((LLOG1 - LLOG0)) "$I1LOG" | sed 's/^/  /')
    echo "MUS-CLOSE-LOG-DELTA (소멸 경계 서버 로그 원문):"
    printf '%s\n' "$CDELTA" | tail -8
    BIG_CRASH=$(printf '%s\n' "$CDELTA" | grep -aicE 'segfault|SIGSEGV|세그폴트|core dumped|terminate called|Aborted')
    echo "MUSIC-CLOSE-CRASH-MARKERS: $BIG_CRASH (클라 사망 원문 — 0 = 정상 소멸)"
    [ "$BIG_CRASH" -eq 0 ] || echo "MUSIC-FAIL(close-crash: 스캔 중 close에서 클라 사망 원문 검출 — 결함 원장)"
else
    echo "MUS-CLOSE-LOG-DELTA: 0 바이트 (소멸 경계 로그 무변 — 정상)"
fi
P3=""
for i in 1 2 3 4 5; do
    P3=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$P3" | grep -aq '"ok":true' && break
    sleep 2
done
printf '%s' "$P3" | grep -aq '"ok":true' || FAIL "ping after scan-window close failed — $P3 (서버 생존 원문)"
echo "MUS-SERVER-ALIVE: $P3 (close 경계 뒤 서버 생존 원문 1행)"
RL16=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$RL16" | grep -aq '"ok":true' || FAIL "music respawn after window close failed — $RL16"
echo "MUSIC-RESPAWN-REPLY: $RL16"
MUSWIN=""
W17=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    W17=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$W17" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window after respawn — last: ${W17:-none}"
echo "MUSWIN-RESPAWNED: $MUSWIN (재스폰 원문)"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
sleep 3
BLRES=$(atool music_dir_list '{}')
echo "MUSIC-RESPAWN-LIST: ${BLRES:-none} (재스폰 후 도구 응답 원문 — 응답 정상 = 도구 경로 정상)"
printf '%s' "$BLRES" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(respawn-list: 재스폰 뒤 도구 응답 비정상 — $BLRES)"

echo "=== 16d-5. [③] 취소 후 재스캔 정착 (재스폰 클라의 스캔→채택 전이 settle — 캡처) ==="
# ①의 close가 pub2를 함께 소멸시킨다(파괴 취소 — 원장) — 정착 실측은 재스폰
# 클라에서 **동일 스캔-채택 경계**(탭 스캔($EXPECT_SEEDS — T3 태그 트랙 3건
# 포함) → 도착 더티 1프레임 → 표 "N곡" 승계)로 대리한다. 이 전이가 없으면
# 폐기 뒤의 재스캔도 정착하지 않는다.
python3 "$SHOT_PY" "$LOGDIR/mus_base16s.png" >"$LOGDIR/mus_base16s.log" 2>&1 \
    || FAIL "16d(③) baseline capture failed — $(tail -2 "$LOGDIR/mus_base16s.log" | tr '\n' ' ')"
if landtap "$MUS_ID" "$LOGDIR/mus_base16s.png" "s1" "$MUS_X" "$MUS_Y" 84; then
    echo "MUSIC-RESCAN-SETTLED: engine/tmp/mus_wsl_rescan_settled.png (정착 캡처 — 탭 스캔 ${EXPECT_SEEDS}곡 채택 전이 = 곡 카운터는 캡처 육안 몫)"
    cp "$LOGDIR/mus_land_s1.png" "$RECEIVE/mus_wsl_rescan_settled.png" \
        || FAIL "settle shot copy failed"
    BIG_SETTLED=1
else
    BIG_SETTLED=0
    shot mus_wsl_rescan_settled.png 2>/dev/null || true
    echo "MUSIC-FAIL(rescan-settled: 정착 전이 무착탄 — 원장 MISS(상수 재보정 몫))"
fi

echo "=== 16d-6. [③] 캐논 등호 — 코드 무변경 라인 원문 ==="
echo "MUS-CANCEL-CANON: T2(f409856) 3축 등호 원문 승계 — Win 661 PASS/0 FAIL·WSL 638 PASS/0 FAIL·posix 296 PASS/0 FAILURE(신설 어설션 0건 — 코드 무변경). 본 런 WSL selftest: PASS=$ST_PASS FAIL=$ST_FAIL(캐논 $CANON_WSL_SELFTEST — step 3 계보)"
if [ "$ST_PASS" -eq "$CANON_WSL_SELFTEST" ]; then
    echo "MUS-CANCEL-CANON-EQ: OK — 런 selftest PASS == 캐논(승계 등호 — 스캔 취소 라인 산수 변동 0)"
else
    echo "MUSIC-FAIL(cancel-canon: 런 PASS=$ST_PASS != 캐논 $CANON_WSL_SELFTEST — 계보 재확인 몫)"
fi
echo "BIG-TAP-ELAPSED-LEDGER: ②제거→①close=$(awk -v a="$REM1_SEC" -v b="$CLOSE1_SEC" 'BEGIN{printf "%.2f", b-a}')s (pub2 재스캔 창의 시차 영수증 — 실측 스캔 4.9-5.4s/460,800트랙 대비 원장 부기)·②탭→remove는 16d-3의 finding 행"

# ==================================================================
# 16e. [T3(#99) — 태그 표기 세그먼트 (플랜 2026-10-11-music-tags task-3)]
#      합성 태그 트랙(4c — 최소 mp3+own ID3v2.3 블롭)의 **목록 표기 실측**:
#      곡명 열 = TrackRowLabel(태그 = "곡명 (아티스트)"), 앨범 열 =
#      TrackAlbumLabel(= "-", 폴백 행), 폴백 행 = 파일명 스템. + 필터 타이핑
#      (D5 — title 매치·artist 매치 각각 원문).
#      * 위상 = **16d-6 뒤**(세그먼트 체인의 말단): step 10·16b2가 필터
#        "d_seed"/레이아웃 상태 계약을 갖는다 — 앞 단 삽입은 그 상태를
#        오염한다(원장 — 필터는 ctrl+a/삭제 클리어 경로로만 되돌릴 수
#        있어 위험). 여기서는 16d-4의 재스폰 클라 = 신선한 빈 필터.
#      * 재생 leg는 접촉하지 않는다(태그 트랙 더블클릭·spatial_play
#        그대로) — vplayer open 실패 영수증은 app_tool 직행 2콜+창 회수만
#        (16b2-0 수형).
# ==================================================================
echo "=== 16e-0. 창 재정찰 (16d-4 재스폰 클라 — 응답 가용성 원문) ==="
TAG_STAGE=0
MUSWIN=""
WIN16E=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN16E=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WIN16E" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
if [ -n "$MUSWIN" ]; then
    TAG_STAGE=1
    MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    echo "MUSWIN-16E: $MUSWIN (재스폰 클라 승계 — 태그 세그먼트 진입)"
else
    echo "MUSIC-FAIL(tag-recon: Music 창 부재 — 16d-4 재스폰 계보 재대조 몫 — 태그 세그먼트 생략, 원장)"
fi

if [ "$TAG_STAGE" -eq 1 ]; then
    echo "--- 16e-1. 활성 탭 = mus_seed (16d-5 정착 승계; 미확보 시 landtap 폴백) ---"
    if [ "${BIG_SETTLED:-0}" -eq 1 ]; then
        echo "MUS-TAG-TAB-CARRY: 16d-5의 landtap(s1)이 mus_seed 탭 착탄을 settle했다 — 탭 클릭 생략(상태 승계 — 원장; 불요 클릭 = 폐기 위험 회피)"
    else
        python3 "$SHOT_PY" "$LOGDIR/mus_base16e.png" >"$LOGDIR/mus_base16e.log" 2>&1 \
            || FAIL "16e baseline capture failed — $(tail -2 "$LOGDIR/mus_base16e.log" | tr '\n' ' ')"
        if landtap "$MUS_ID" "$LOGDIR/mus_base16e.png" "e0" "$MUS_X" "$MUS_Y" "$MUS_TAB2_X"; then
            echo "MUS-TAG-TAB-OK: mus_seed 탭 착탄(클라 x=$MUS_TAB2_X·y=$MUS_TAB3_Y_EFF — dirIndex_=mus_seed 전이 diff=$LAST_DIFF) — 도달 시차는 LTAP 원문 행"
        else
            echo "MUSIC-FAIL(tag-tab: mus_seed 탭 무착탄 — MUS_TAB2 재보정 몫(mus_bc/mus_land_e0 원문) — 현 탭 계속, 원장)"
        fi
    fi
    sleep 3   # 탭 스캔(합성 8곡 — ffmpeg 부재 원문 승계라 m_seed.mp3 없음) 도착 안정화

    echo "--- 16e-2. 목록 표기 캡처 (곡명(아티스트) 열·앨범 열·폴백 행) ---"
    shot mus_wsl_taglist.png
    echo "MUS-TAG-LIST-LEDGER: 예상 행(mtime desc — 4c 생성 순서): ① t_tag_alpha → 'Wsl Tag Alpha (Probe Artist Alpha)'+앨범 'Tag Album One' ② t_tag_beta → 'Wsl Tag Beta (Probe Artist Beta)'+앨범 'Tag Album Two' ③ t_raw_noid → 폴백 행 't_raw_noid'+앨범 '-' ④ 이하 wav 5건 폴백 행 — 표기 일치는 캡처 육안 몫(EYES)"

    echo "--- 16e-3. 필터 타이핑 — 곡명(TIT2) 매치 → 캡처 ---"
    # [런 1-3 원장 — 정착 논리 정정] 클릭(caret) 프레임은 ~3s에 프레젠트되지만
    # **타이핑(Char) 프레임의 프레젠트가 수초~수십 초 늦다**(런 1 ~8s·런 2
    # ~25s·런 3 = 35s 창 내 미도달 — 이후의 툴콜/포커스 등 활동의 재렌더에서만
    # 관측 — 활동 게이트 원문 #89). 연속 동일(0차분) 정착은 스테일 2장 오답,
    # 전역 차분의 조기 화력은 툴팁/캐럿/윈도 흔들림 오판. 정착 = **상태행 밴드
    # (필터 박스+곡 카운터의 좌표 밴드)의 변화 픽셀합** 판정 + **영수증 = 창
    # 말미 프레임**(도착을 기다린 30s 창의 마지막 프레임 — 프레젠트 지연 원장).
    PX_BAND="$LOGDIR/mus_bandpx.py"
    cat > "$PX_BAND" <<'PYEOF'
# [T3] 상태행 밴드 차분기 — 두 PNG(필터 0·RGB8)에서 밴드(x0,x1,y0,y1)의
# 변화 픽셀합만 센다(캡처 좌표계 원장: 클라 (104-279,26-48) ↔ 캡처
# (524-700,197-219) — 본 밴드 = 필터 박스 전폭+곡 카운터 1행).
import struct, sys, zlib

def load(p):
    d = open(p, "rb").read()
    pos, idat, w, h = 8, b"", 0, 0
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos+4])[0]
        tag = d[pos+4:pos+8]
        body = d[pos+8:pos+8+ln]
        pos += 12 + ln
        if tag == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    return w, h, zlib.decompress(idat)

w, h, a = load(sys.argv[1])
_, _, b = load(sys.argv[2])
x0, x1, y0, y1 = (int(v) for v in sys.argv[3:])
total, maxrow = 0, 0
for y in range(y0, min(y1, h)):
    c = 0
    for x in range(x0, min(x1, w)):
        va = a[y*(w*3+1)+1+x*3]
        vb = b[y*(w*3+1)+1+x*3]
        if abs(va - vb) > 75:
            c += 1
    total += c
    maxrow = max(maxrow, c)
print("band_px=%d maxrow=%d" % (total, maxrow))
PYEOF
    # 밴드 = 캡처 좌표(위 원장 — MUS_FILTER 클라 (250,42) 소비 축+박스 전폭+곡
    # 카운터). 창 배치 변화 시 이 밴드도 정찰 재보정 몫(원장).
    TAG_BAND_X0=${TAG_BAND_X0:-524}  TAG_BAND_X1=${TAG_BAND_X1:-960}
    TAG_BAND_Y0=${TAG_BAND_Y0:-196}  TAG_BAND_Y1=${TAG_BAND_Y1:-222}
    TAG_BAND_PX=${TAG_BAND_PX:-60}   # 텍스트 글리프+카운터 변화 ≥60px — 캐럿(~30px) 배제
    FFILTER() { # $1=텍스트 $2=태그 $3=영수증명 — 클릭+키 프라이밍+타이핑+밴드 판정
        local text="$1" tag="$2" name="$3" t i d last total pk
        t=$(tap "$MUS_ID" "$((MUS_X + MUS_FILTER_X))" "$((MUS_Y + MUS_FILTER_Y))")
        printf '%s' "$t" | grep -aq '"ok":true' || FAIL "send_input filter click failed — $t"
        sleep 1
        # [런 1-4 원장 — 착륙 레시피] 타이핑의 적용이 **키 선행 라운드에서만
        # 실측됐다**(런 1 = ctrl+a/bs/del 선행 → 착륙 ~8s·런 2 = backspace
        # 16탭 선행 → 착륙 ~25s·런 3/4 = 키 없음 → 35s 창 내 미적용 2/2 —
        # 진단 v2/v3 재현). 신선 빈 박스의 백스페이스 1탭 = 무해 프라이밍 —
        # 유일 실측 착륙 레시피를 소비한다(적용되지 않으면 정직 MISS — 판정은
        # 밴드+캡처 육안).
        pk=""
        for i in 1 2; do
            pk=$(timeout 12 ./buildwsl/jkdesktop agentctl \
                "{\"tool\":\"send_input\",\"args\":{\"op\":\"key\",\"id\":$MUS_ID,\"key\":8,\"mods\":0}}" \
                2>/dev/null | grep -a '{' | head -1)
            printf '%s' "$pk" | grep -aq '"ok":true' \
                || echo "  프라이밍 key#$i reply 비정상(전송 실패?): ${pk:-empty} — 계속"
        done
        sleep 1
        t=$(timeout 12 ./buildwsl/jkdesktop agentctl \
            "{\"tool\":\"send_input\",\"args\":{\"op\":\"type\",\"id\":$MUS_ID,\"text\":\"$text\"}}" \
            2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$t" | grep -aq '"ok":true' || FAIL "send_input type failed — $t"
        echo "MUS-TAG-FILTER-TYPE($tag): text='$text' — reply ok (프라이밍 후 타이핑 — 신선 빈 박스)"
        TAG_SETTLE_RC=0
        last=""
        for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
            sleep 2
            python3 "$SHOT_PY" "$LOGDIR/mus_bc_$tag.png" >"$LOGDIR/mus_bc_$tag.log" 2>&1 \
                || FAIL "settle capture $tag failed — $(tail -2 "$LOGDIR/mus_bc_$tag.log" | tr '\n' ' ')"
            last="$LOGDIR/mus_bc_$tag.png"
        done
        # 영수증 = 창 말미 프레임(30s — 프레젠트 지연 원장 상관)+판정 = 기저와의
        # 밴드 차분(텍스트 글리프+카운터 변화).
        cp "$last" "$RECEIVE/$name" || FAIL "capture copy $name failed"
        echo "CAPTURE: $RECEIVE/$name ($(wc -c < "$RECEIVE/$name") bytes — 30s 창 말미 프레임)"
        d=$(python3 "$PX_BAND" "$LOGDIR/mus_base16f_$tag.png" "$last" \
            "$TAG_BAND_X0" "$TAG_BAND_X1" "$TAG_BAND_Y0" "$TAG_BAND_Y1")
        total=$(printf '%s' "$d" | sed -n 's/.*band_px=\([0-9]*\).*/\1/p')
        echo "MUS-TAG-FILTER-SETTLE($tag): $d (기저 대비 상태행 밴드 — 임계 $TAG_BAND_PX)"
        if [ -n "$total" ] && [ "$total" -ge "$TAG_BAND_PX" ]; then
            TAG_SETTLE_RC=1
            echo "MUS-TAG-FILTER-ARRIVED($tag): OK — 밴드 변화 $total ≥ $TAG_BAND_PX (필터 상태 도착 원문)"
        else
            echo "MUSIC-FAIL(tag-settle($tag): 밴드 도착 미판정 — 스테일/미적용 원장행(마지막 프레임 캡처 = EYES 몫))"
        fi
    }

    python3 "$SHOT_PY" "$LOGDIR/mus_base16f_tftitle.png" >"$LOGDIR/mus_base16f_tftitle.log" 2>&1 \
        || FAIL "16e-3 base capture failed — $(tail -2 "$LOGDIR/mus_base16f_tftitle.log" | tr '\n' ' ')"
    FFILTER "$TAG_FILTER_TEXT_TITLE" "tftitle" mus_wsl_tag_filter.png
    MUS_TF_TITLE=${TAG_SETTLE_RC:-0}
    echo "MUS-TAG-FILTER-OBSERVATION: text='$TAG_FILTER_TEXT_TITLE' — 예상 표 1행(파일명 매치 0 — 'Wsl Tag Alpha'는 TIT2 원존 — D5 title 매치의 라이브 원문), 카운터 1곡은 캡처 육안 몫"

    echo "--- 16e-4. 필터 타이핑 — 아티스트(TPE1) 매치 → 캡처 (신선 클라 — 재스폰) ---"
    # [런 1-2 원장 — 두번째 타이핑 라운드(기존 텍스트의 클리어+대체)가 미성립]
    # : run 1 = ctrl+a/bs/del 3키 뒤의 타이핑, run 2 = 백스페이스 16탭 뒤의
    # 타이핑 — 어느 쪽도 기존 텍스트를 치우지 않았다(run 2 tag_after 실측:
    # 타이틀 텍스트 잔존). key op(합성 keydown)의 InputText 반영이 미실증이라
    # **클리어 의존을 소각**한다: close_window+launch_app으로 신선 클라(빈
    # 필터)를 다시 세우고 같은 수형의 클릭+단일 타이핑으로 두 번째 매치를
    # 실측한다(step 10/16e-3의 실증 경로만 소비 — run 1/2의 실측 렛슨).
    MCLOSE=$(timeout 12 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"close_window\",\"args\":{\"id\":$MUS_ID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "MUS-TAG-APP-CLOSE: ${MCLOSE:-none}"
    for i in 1 2 3 4 5 6; do
        sleep 1
        WD=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$WD" | grep -aq '"title":"Music"' || break
    done
    MREL=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$MREL" | grep -aq '"ok":true' || FAIL "music relaunch (tag artist round) failed — $MREL"
    echo "MUS-TAG-APP-RELAUNCH: $MREL"
    MUSWIN=""
    WIN16E2=""
    for i in 1 2 3 4 5 6 7 8; do
        sleep 2
        WIN16E2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        MUSWIN=$(printf '%s' "$WIN16E2" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
        [ -n "$MUSWIN" ] && break
    done
    [ -n "$MUSWIN" ] || FAIL "no Music window for tag artist round — last: ${WIN16E2:-none}"
    MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    echo "MUSWIN-16E2: $MUSWIN (신선 클라 — 빈 필터 보증)"
    sleep 2
    WK16E=$(tap "$MUS_ID" "$((MUS_X + 300))" "$((MUS_Y + 300))")   # 첫 합성 클릭 무음 소멸 렛슨(16c 원장)
    printf '%s' "$WK16E" | grep -aq '"ok":true' || echo "  warmup tap reply: ${WK16E:-none} (계속 — 무해 지점)"
    sleep 1
    # 신선 클라의 활성 탭 = dirs_[0](default) — mus_seed 탭 클릭(landtap 정찰 —
    # 16e-1의 폴백 수형) 뒤 스캔 도착 → 기저 캡처.
    python3 "$SHOT_PY" "$LOGDIR/mus_base16e2.png" >"$LOGDIR/mus_base16e2.log" 2>&1 \
        || FAIL "16e-4 base (pre-tab) capture failed — $(tail -2 "$LOGDIR/mus_base16e2.log" | tr '\n' ' ')"
    TAG_TAB2=0
    if landtap "$MUS_ID" "$LOGDIR/mus_base16e2.png" "e2" "$MUS_X" "$MUS_Y" "$MUS_TAB2_X"; then
        TAG_TAB2=1
        echo "MUS-TAG-TAB2-OK: mus_seed 탭 착탄(클라 x=$MUS_TAB2_X·y=$MUS_TAB3_Y_EFF diff=$LAST_DIFF) — 신선 클라에서도 상수 유효(원장)"
    else
        echo "MUSIC-FAIL(tag-tab2: 신선 클라의 mus_seed 탭 무착탄 — 현 탭 계속(기저가 대변) — 아티스트 매치 캡처의 배경 원장)"
    fi
    sleep 3   # 탭 스캔(합성 8곡) 도착 안정화
    python3 "$SHOT_PY" "$LOGDIR/mus_base16f_tfartist.png" >"$LOGDIR/mus_base16f_tfartist.log" 2>&1 \
        || FAIL "16e-4 base capture failed — $(tail -2 "$LOGDIR/mus_base16f_tfartist.log" | tr '\n' ' ')"
    FFILTER "$TAG_FILTER_TEXT_ARTIST" "tfartist" mus_wsl_tag_filter_artist.png
    MUS_TF_ARTIST=${TAG_SETTLE_RC:-0}
    echo "MUS-TAG-FILTER-ARTIST-OBSERVATION: text='$TAG_FILTER_TEXT_ARTIST' — 예상 표 1행(t_tag_beta — TPE1 매치 — D5 artist 매치 원문), 캡처 육안 몫"

    echo "--- 16e-5. 재생 open 실패 영수증 (태그 성립·프레임 0 — 정직 원문) ---"
    # vplayer 재등록 — 16b2-0의 회수 이후 미등록(런 1 원장: app_tool 릴레이의
    # 후보 0 → unknown_app_tool — step 9의 콜드 경기 원문과 동일 거부). step 14
    # 수형 = launch_app으로 인스턴스를 스폰한 뒤 직행한다.
    LP=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$LP" | grep -aq '"ok":true' || FAIL "launch_app vplayer failed — $LP"
    echo "MUS-TAG-VP-LAUNCH: $LP"
    VPREG=0
    for i in 1 2 3 4 5 6; do
        sleep 2
        VPQ=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$VPQ" | grep -aq '"title":"Video Player"' && { VPREG=1; break; }
    done
    [ "$VPREG" -eq 1 ] || echo "MUSIC-FAIL(tag-vp-reg: vplayer 윈도우 미등록 — 릴레이 거부 예상, 원장)"
    OR_TAG=$(timeout 15 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$TAG_MP3_A\"}}}" \
        2>/dev/null | grep -a '{' | head -1)
    echo "MUS-TAG-OPEN-REPLY: ${OR_TAG:-none} (accepted = 비동기 도달 — 판정은 get_status의 openFailed)"
    GS_TAG=""
    TAG_OPENFAIL=0
    for i in 1 2 3 4 5 6; do
        sleep 1.5
        GS_TAG=$(timeout 15 ./buildwsl/jkdesktop agentctl \
            '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
            2>/dev/null | grep -a '{' | head -1)
        echo "MUS-TAG-OPEN-STATUS-$i: ${GS_TAG:-none}"
        printf '%s' "$GS_TAG" | grep -aq '"openFailed":true' && { TAG_OPENFAIL=1; break; }
        printf '%s' "$GS_TAG" | grep -aq '"opened":true' && break
    done
    echo "MUS-TAG-OPEN-STATUS: ${GS_TAG:-none} (opened:false+openFailed:true = 유효 mp3 프레임 0 파일의 정직 개방 실패 원문 — 태그 표기 leg는 이것과 무관 성립)"
    printf '%s' "$OR_TAG" | grep -aq '"accepted":true' \
        && [ "$TAG_OPENFAIL" -eq 1 ] \
        && echo "MUS-TAG-OPENFAIL: OK — open은 수취되고 개방은 실패(정직 원문 — 합성 최소 mp3 계약)" \
        || echo "MUSIC-FAIL(tag-openfail: open/get_status 원문 미달 — $OR_TAG|$GS_TAG)"
    # 창 회수(차분·후속 캡처 오염 소각 — 16b2-0 수형)
    TAG_VPCLOSE=0
    for i in 1 2 3 4 5 6; do
        VPT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        VPW=$(printf '%s' "$VPT" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
        [ -z "$VPW" ] && break
        VPID=$(printf '%s' "$VPW" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
        TVC=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$VPID}}" 2>/dev/null | grep -a '{' | head -1)
        echo "  tag vplayer close#$i: id=$VPID repl=${TVC:-none}"
        TAG_VPCLOSE=$((TAG_VPCLOSE + 1))
        sleep 1
    done
    echo "MUS-TAG-VP-RECLAIM: closed=$TAG_VPCLOSE (0 = 실패 UI가 즉닫힘·1 = 실패 메시지를 남긴 뒤 회수 — 어느 쪽이나 회수 완료 부기, 판정 아님)"
    sleep 1
    shot mus_wsl_tag_after.png
fi

# ==================================================================
# 16f-TW. [#111 T5 — 태그 쓰기(도구) 세그먼트 (플랜 2026-10-25-music-tagwrite
#       task-5)] music_tag_write(app_tool 릴레이 — atool 수형 재용)의 라이브
#       원문 실측:
#       ① 합성 무태그 mp3+FLAC(4c/2s 합성 블롭 수형 승계 — probe 소유, $SEED_DIR
#         경유 — 활성 탭 mus_seed의 목록에 반영)을 시드 → **도구 relay 쓰기**
#         (title/artist/album+합성 cover jpg — 마법수 FFD8·LoadCoverByMagic
#         계약) → **재독 표기 원문**: 파일 수준(순수 파이썬 재파싱 — 16e
#         TAG-PARSED-PREVIEW 수형 — 앱 파서와 독립)+목록 행 표기 캡처 1종
#         (태그 반영 행 = 곡명(아티스트)·앨범 열+커버아트 썸네일 32px — EYES)
#         +필터 라운드 1회(16e FFILTER 수형 재용 — 쓰기로 바뀐 TIT2가 필터
#         매치 = 앱이 쓴 태그를 재읽었다는 인-엔진 원문 — 프레젠트 지연 원장
#         승계라 MISS면 honest-fail).
#       ② cover=null 2차 쓰기(TIT2 교체+cover 인자 null) → **기존 APIC 유지
#         원문** — WriteTag cover=null 계약의 도구 경계 라이브 수취(2u-⑮
#         APIC 생존 어설션의 실기기 대응 — md5 등호로 절단판정).
#       ③ FLAC 도구 쓰기(cover 생략 = 유지 leg 계열 — PICTURE 부재 원존) →
#         VORBIS_COMMENT 신설(T2 leg) → 블록 재조립 재독 원문.
#       * 위상 = 세그먼트 체인 말단(16e 뒤·복원 전 — 기존 VERDICT 산치 규약의
#         계약 상태(step 10 필터·16b2 좌표·16e 기저)가 이미 소진된 뒤라 신규
#         상태 오염 0): 16d-4 재스폰·16e-4 신선 클라의 **빈 필터·mus_seed
#         활성 탭** 상태 승계. 클릭 0 — 도구 경로만(필터 라운드 1회 제외).
#       * scan_busy 대응 원장(T4 도구 규약): 쓰기 성공 → RequestScan의 재스캔
#         진행 중 도착하는 뒤 도구 쓰기는 {"error":"scan_busy","retry":true}
#         정직 거부 — tw_write 재시도 수형(대기+재발사 — 재스캔이 소형 시드
#         dir이라 ~s급).
#       * REMNANT = TW 시드 3건 소각(16f-6·17d 가드 — probe 소유 잔상 0).
# ==================================================================
MUS_TW_RUN=${MUS_TW_RUN:-1}   # 런 라벨(TW-RUN-VALS 요약 스트림의 run 축 — JOG-RUN-VALS 수형)
echo "=== 16f-TW. 태그 쓰기(도구) 세그먼트 (#111 T5 — run=$MUS_TW_RUN) ==="
TW_STAGE=0
TW1_OK=0
TW2_OK=0
TWF_OK=0
MUS_TW_SETTLE=0
MUSWIN=""
WIN16F=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN16F=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WIN16F" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
if [ -n "$MUSWIN" ]; then
    TW_STAGE=1
    MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    echo "MUSWIN-16F: $MUSWIN (16e 신선 클라 승계 — 빈 필터·mus_seed 활성 탭)"
else
    echo "MUSIC-FAIL(tw-recon: Music 창 부재 — 16e 계보 재대조 몫 — 태그 쓰기 세그먼트 생략, 원장)"
fi

if [ "$TW_STAGE" -eq 1 ]; then
    echo "--- 16f-0a. 신선 클라 재건 (16e-4 수형 — 16e 필터 잔존 소각·빈 필터 보증) ---"
    # [런 1 원장(T5, run1_parsebug): 16e-4의 마지막 행동 = 아티스트 필터 타이핑
    # ("Probe Artist Beta") — 그 필터가 잔존한 채 16f가 도는 사건:
    # mus_wsl_tagwrite_list.png가 t_tag_beta 행(필터 매치)만 보여 태그 반영
    # 행 캡처가 비졌다. 상태 계약의 정정 원장행 — 필터는 16e의 소비 후 잔존,
    # 도구 경로로 못 지운다(InputText 클리어 의존 소각 원장 — 16e-4 수형 그대로
    # close_window+launch_app 신선 클라 = 유일 실측 빈 필터 경로).]
    MC16F=$(timeout 12 ./buildwsl/jkdesktop agentctl         "{\"tool\":\"close_window\",\"args\":{\"id\":$MUS_ID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "MUS-TW-APP-CLOSE: ${MC16F:-none}"
    for i in 1 2 3 4 5 6; do
        sleep 1
        WD16F=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$WD16F" | grep -aq '"title":"Music"' || break
    done
    MREL16F=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$MREL16F" | grep -aq '"ok":true'         || FAIL "music relaunch (16f fresh client) failed — $MREL16F"
    echo "MUS-TW-APP-RELAUNCH: $MREL16F"
    MUSWIN=""
    WINF16F=""
    for i in 1 2 3 4 5 6 7 8; do
        sleep 2
        WINF16F=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        MUSWIN=$(printf '%s' "$WINF16F" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
        [ -n "$MUSWIN" ] && break
    done
    [ -n "$MUSWIN" ] || FAIL "no Music window (16f fresh client) — last: ${WINF16F:-none}"
    MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    echo "MUSWIN-16F-FRESH: $MUSWIN (신선 클라 — 빈 필터·dirs_[0] 활성 — 16e-4 수형)"
    sleep 2
    WK16F=$(tap "$MUS_ID" "$((MUS_X + 300))" "$((MUS_Y + 300))")   # 첫 합성 클릭 무음 소멸 렛슨(16c 원장)
    printf '%s' "$WK16F" | grep -aq '"ok":true' || echo "  warmup tap reply: ${WK16F:-none} (계속 — 무해 지점)"
    sleep 1
    # mus_seed 탭 착탄(landtap 수형 — 16e-1/16e-4 폴백 수형: 탭의 ok/sent 답신은
    # 전송 성립뿐 — 착탄 = 기저 대비 차분 3행, 재시도 y 스윕).
    python3 "$SHOT_PY" "$LOGDIR/mus_base16f0.png" >"$LOGDIR/mus_base16f0.log" 2>&1 || FAIL "16f fresh-client baseline capture failed — $(tail -2 "$LOGDIR/mus_base16f0.log" | tr '\n' ' ')"
    TW_TAB=0
    if landtap "$MUS_ID" "$LOGDIR/mus_base16f0.png" "f0" "$MUS_X" "$MUS_Y" "$MUS_TAB2_X"; then
        TW_TAB=1
        echo "MUS-TW-TAB-OK: mus_seed 탭 착탄(클라 x=$MUS_TAB2_X·y=$MUS_TAB3_Y_EFF diff=$LAST_DIFF) — mus_seed 재스캔 도착 전제 성립"
    else
        echo "MUS-TW-TAB-MISS: mus_seed 탭 무착탄 — 현 탭 계속(원장 — MUS_TAB2 재보정 몫(mus_bc_f0 원문))"
    fi
    sleep 3   # 탭 스캔 도착 안정화(쓰기 1의 재스캔 축 — 활성 탭이 곧 반영 축)

    echo "--- 16f-1. 합성 무태그 mp3+FLAC+cover jpg 시드 (4c 블롭 수형 승계 — 의존 0) ---"
    cat > "$LOGDIR/mus_twseed.py" <<'PYEOF'
# [#111 T5] 태그 쓰기 세그먼트 시더 — ①무태그 mp3(ID3 매직 0 junk 768B —
# 2u-① 구성 계약 수형: v2 부재 = WriteTag mp3 leg가 v2.3 신설) ②무태그 FLAC
# ("fLaC"+STREAMINFO 첫 블록 34B is-last — MusicModel.h FlacScanRegion의 구성
# 계약 그대로: 첫 블록 = type 0·34B — 내용은 writer가 원문 보존) ③합성 커버
# jpg(드라이버 PIL 산출 base64 — FFD8 마법수·4색 밴드 = 32px 썸네일에서
# 구별 가능 — 대각 마크). stdlib만(의존 0 — 4c 원문 계약).
import base64, os, sys
out, b64 = sys.argv[1], sys.argv[2]
mp3 = os.path.join(out, "t_tw_write.mp3")
with open(mp3, "wb") as f:
    f.write(bytes((i * 7 + 13) & 0xFF for i in range(768)))
flac = os.path.join(out, "t_tw_write.flac")
with open(flac, "wb") as f:
    f.write(b"fLaC")
    f.write(bytes([0x80, 0, 0, 34]))   # is-last·type 0(STREAMINFO)·34B
    f.write(bytes(34))                 # 34B 본체(원문 보존 — 파싱 계약 밖)
cover = os.path.join(out, "t_tw_cover.jpg")
cb = base64.b64decode(b64)
with open(cover, "wb") as f:
    f.write(cb)
print("TW-SEED-OK mp3=%d flac=%d cover=%d jpeg_magic=%d"
      % (os.path.getsize(mp3), os.path.getsize(flac), len(cb),
         1 if cb[:2] == b"\xff\xd8" else 0))
PYEOF
    # 합성 cover jpg(941B) = 드라이버 PIL 산출 base64 원문(64×64 RGB — 4색 밴드
    # +중앙 마크 — FFD8 마법수·stb_image 디코드 실측)의 **인라인 수형** — 외부
    # 파일 의존 0(경로 축 함정 회피 — wsl /mnt/i 축; engine/tmp 작업물은 폐기).
    TW_COVER_B64='/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAQDAwMDAgQDAwMEBAQFBgoGBgUFBgwICQcKDgwPDg4MDQ0PERYTDxAVEQ0NExoTFRcYGRkZDxIbHRsYHRYYGRj/2wBDAQQEBAYFBgsGBgsYEA0QGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBj/wAARCABAAEADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDzuiiivgz+tAooooAKKKKACiiigDvKKKK4z/NMKKK8K+IfxD8YaH8SNS0rStX+z2kPleXH9nifbmJGPLKT1J711YTCTxU3CDV7X1PRyzLKuY1XSpNJpX1v3S6J9z3WivCvh58Q/GGufEjTdK1XV/tFpN5vmR/Z4k3Yidhyqg9QO9e60YvCTws1CbV7X0DM8sq5dVVKq021fS/drql2CiiiuU848vooor+4j7oK838U+Fte1LxZd3tlY+bBJs2v5qLnCKDwTnqDXpFFeTnGT0c2oqhXbSTvpa97NdU+504XFTw03OC121PN/C3hbXtN8WWl7e2PlQR79z+ajYyjAcA56kV6RRRRk+T0cpouhQbabvra97JdEuwYrFTxM1Oa120CiiivWOY+8qKKK/yrP3AKKKKACiiigAooooA//9k='
    [ -n "$TW_COVER_B64" ] || FAIL "cover b64 missing (인라인 원문 상실)"
    TW_SEED=$(python3 "$LOGDIR/mus_twseed.py" "$SEED_DIR" "$TW_COVER_B64") \
        || FAIL "TW seed generation failed (python3)"
    echo "MUS-TW-SEED: $TW_SEED"
    TW_COVER="$SEED_DIR/t_tw_cover.jpg"
    TW_MP3="$SEED_DIR/t_tw_write.mp3"
    TW_FLAC="$SEED_DIR/t_tw_write.flac"
    grep -aq 'jpeg_magic=1' <<<"$TW_SEED" || FAIL "cover jpg magic broken (FFD8 선단 필수 — LoadCoverByMagic 계약)"
    TW_COVER_MD5=$(md5sum "$TW_COVER" | awk '{print $1}')
    echo "MUS-TW-COVER-MD5: $TW_COVER_MD5 (합성 cover jpg 원문 — APIC round-trip 절단판정 축)"
    cat > "$LOGDIR/mus_twparse.py" <<'PYEOF'
# [#111 T5] 재독 파서 — 시더와 독립(앱 파서와도 독립 — 16e 수형): mp3 = ID3
# 프레임 사슬(v2.3 BE32·v2.4 syncsafe 폴백 — 4c 수형)에서 TIT2/TPE1/TALB+APIC
# (enc0 mime\0·type·desc\0·data)를 다시 읽고, FLAC = 메타 블록 사슬을 다시
# 읽어 VORBIS_COMMENT(4·LE32 길이 규약 — ParseVorbisCommentsRaw 동형)의 관리
# 키를 수취한다. 진리원 = 파일 바이트.
import hashlib, struct, sys

def txt_of(enc_body):
    enc, body = enc_body[0], enc_body[1:]
    if enc == 1:
        try:
            return body.decode("utf-16").lstrip("\ufeff").rstrip("\x00")
        except Exception:
            pass
    return body.decode("latin-1", "replace").rstrip("\x00")

def parse_mp3(path, out):
    d = open(path, "rb").read()
    if d[:3] != b"ID3":
        out.append("TW-TAG none")
        return out
    out.append("TW-ID3-VER %d" % d[3])
    sz_be = int.from_bytes(d[6:10], "big")
    sz_ss = ((d[6] & 0x7F) << 21) | ((d[7] & 0x7F) << 14) | \
            ((d[8] & 0x7F) << 7) | (d[9] & 0x7F)
    end = 10 + (sz_be if d[3] == 0x03 else sz_ss)
    tags, apic = {}, None
    i = 10
    while i + 10 <= min(end, len(d)):
        fid = d[i:i + 4].decode("latin-1")
        fsz = int.from_bytes(d[i + 4:i + 8], "big")
        if fsz == 0 or not ("A" <= fid[0] <= "Z"):
            break
        body = d[i + 10:i + 10 + fsz]
        if not body:
            break
        if fid in ("TIT2", "TPE1", "TALB"):
            tags[fid] = txt_of(body)
        elif fid == "APIC":
            z = body.find(b"\x00", 1)
            if z != -1:
                mime = body[1:z].decode("latin-1", "replace")
                # 엔진 원문 레이아웃 수형(AppendId3ApicFrame 1425-1436 — 브리프의 그림
                # 타입 바이트는 불내장): [enc][mime][NUL][desc=빈+종결]
                # [picture] — desc 검색 = mime 종결 다음 바이트
                # (ApicFromFrames 2056-2076 보행 동형). [런 1 원장:
                # z+2 스킵이 그림 선단 5B(0x00 포함)를 잠식해 md5 불일치]
                de = body.find(b"\x00", z + 1)
                if de != -1:
                    apic = (mime, body[de + 1:])
        i += 10 + fsz
    out.append("TW-TAG title=%r artist=%r album=%r"
               % (tags.get("TIT2", ""), tags.get("TPE1", ""), tags.get("TALB", "")))
    if apic:
        out.append("TW-APIC mime=%s md5=%s bytes=%d jpeg_magic=%d"
                   % (apic[0], hashlib.md5(apic[1]).hexdigest(), len(apic[1]),
                      1 if apic[1][:2] == b"\xff\xd8" else 0))
    else:
        out.append("TW-APIC none")
    return out

def parse_flac(path, out):
    d = open(path, "rb").read()
    if d[:4] != b"fLaC":
        out.append("TW-FLAC magic-miss")
        return out
    out.append("TW-FLAC-VER ok")
    i, first, keys, pic = 4, True, [], 0
    while i + 4 <= len(d):
        hdr = d[i]
        btype = hdr & 0x7F
        blen = int.from_bytes(d[i + 1:i + 4], "big")
        body = d[i + 4:i + 4 + blen]
        if first and (btype != 0 or blen != 34):
            out.append("TW-FLAC streaminfo-broken btype=%d blen=%d" % (btype, blen))
            return out
        first = False
        if btype == 4:
            off = 4 + int.from_bytes(body[:4], "little")
            cnt = int.from_bytes(body[off:off + 4], "little")
            off += 4
            for _ in range(cnt):
                elen = int.from_bytes(body[off:off + 4], "little")
                off += 4
                ent = body[off:off + elen].decode("utf-8", "replace")
                off += elen
                if ent.startswith(("TITLE=", "ARTIST=", "ALBUM=")):
                    keys.append(ent)
        elif btype == 6:
            pic += 1
        i += 4 + blen
        if hdr & 0x80:
            break
    t = {k.split("=", 1)[0]: k.split("=", 1)[1] for k in keys}
    out.append("TW-TAG title=%r artist=%r album=%r"
               % (t.get("TITLE", ""), t.get("ARTIST", ""), t.get("ALBUM", "")))
    out.append("TW-FLAC-BLOCKS keys=%d picture=%d" % (len(keys), pic))
    return out

path = sys.argv[1]
d = open(path, "rb").read(4)
print("TW-PARSE-FILE %s %d bytes" % (path, len(open(path, "rb").read())))
if d[:3] == b"ID3":
    for ln in parse_mp3(path, []):
        print(ln)
elif d[:4] == b"fLaC":
    for ln in parse_flac(path, []):
        print(ln)
else:
    print("TW-PARSE magic-unknown (v2 부재 전제 위반 — 시더 원문 재확인)")
PYEOF
    TW_PRE=$(python3 "$LOGDIR/mus_twparse.py" "$TW_MP3")
    echo "MUS-TW-PREVIEW-MP3: $TW_PRE (쓰기 전 — 무태그 원문)"
    TW_PRE_FLAC=$(python3 "$LOGDIR/mus_twparse.py" "$TW_FLAC")
    echo "MUS-TW-PREVIEW-FLAC: $TW_PRE_FLAC (쓰기 전 — fLaC만)"

    echo "--- 16f-2. 도구 쓰기 1 — title/artist/album+cover jpg (마법수 수취) ---"
    tw_write() { # $1=args-json — scan_busy 정직 거부 재시도 수형(T4 도구 규약:
                 # 쓰기 성공의 RequestScan 진행 중에 도착하는 뒤 쓰기 = 대기+
                 # 재발사 — reply 원문은 시도마다 행으로 남긴다 — 무음 실패 금지
                 # 원장). reply는 TW_REPLY로 돌려준다.
        TW_REPLY=""
        local a="$1" r i ok=1
        for i in 1 2 3; do
            r=$(atool music_tag_write "$a")
            TW_REPLY=$r
            echo "  TW-WRITE#$i: ${r:-none}"
            if printf '%s' "$r" | grep -aq '"ok":true'; then ok=0; break; fi
            printf '%s' "$r" | grep -aq '"error":"scan_busy"' || break
            sleep 4
        done
        return $ok
    }
    TW_TITLE_A="Wsl Write Alpha"
    TW_ARTIST="Probe Tw Artist"
    TW_ALBUM="Tw Album One"
    TW_TITLE_B="Wsl Write Reup"
    if tw_write "{\"path\":\"$TW_MP3\",\"title\":\"$TW_TITLE_A\",\"artist\":\"$TW_ARTIST\",\"album\":\"$TW_ALBUM\",\"cover\":\"$TW_COVER\"}"; then
        TW1_OK=1
        echo "MUS-TW-WRITE1-OK: ok 수취 — title/artist/album+cover(jpg 마법수)relay 쓰기 성립(성공 = RequestScan 재스캔 재청구 배선 — T4 도구 규약)"
        sleep 5   # 재스캔 도착(도착 더티 1프레임 — 캡처 전정)
        shot mus_wsl_tagwrite_list.png
        echo "MUS-TW-LIST-LEDGER: 예상 상단 행(쓰기 직후 = mtime desc 최신): 'Wsl Write Alpha (Probe Tw Artist)'+'Tw Album One'+좌측 커버아트 썸네일 32px(4색 밴드 jpg — 회색 자리표시 대조) — 표기·썸네일 일치는 캡처 육안 몫(EYES)"
    else
        echo "MUSIC-FAIL(tw-write1: 도구 쓰기 1 거부 — $TW_REPLY)"
    fi
    AP1=$(python3 "$LOGDIR/mus_twparse.py" "$TW_MP3")
    echo "MUS-TW-PARSED-A: $AP1 (파일 수준 재독 — 앱 파서와 독립 round-trip 원문)"
    TW_MIME=$(printf '%s\n' "$AP1" | sed -n 's/.*TW-APIC mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\1/p')
    TW_MD5_1=$(printf '%s\n' "$AP1" | sed -n 's/.*TW-APIC mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    echo "MUS-TW-APIC-1: mime=${TW_MIME:-none} md5=${TW_MD5_1:-none} (합성 cover md5=$TW_COVER_MD5 대조 몫)"
    if [ -n "$TW_MD5_1" ] && [ "$TW_MD5_1" = "$TW_COVER_MD5" ]; then
        echo "MUSIC-TW-COVER-RT: OK — APIC 그림 == 합성 cover jpg 원문(md5 등호 — ReadCover 계약의 파일 수준 라이브 대응)"
    else
        echo "MUSIC-FAIL(tw-cover-rt: APIC round-trip 불일치 — md5=${TW_MD5_1:-none} vs cover=$TW_COVER_MD5)"
    fi

    echo "--- 16f-3. 도구 쓰기 2 — cover=null(기존 APIC 유지 계약)+TIT2 교체 ---"
    if tw_write "{\"path\":\"$TW_MP3\",\"title\":\"$TW_TITLE_B\",\"artist\":\"$TW_ARTIST\",\"album\":\"$TW_ALBUM\",\"cover\":null}"; then
        TW2_OK=1
        echo "MUS-TW-WRITE2-OK: ok 수취 — cover=null = 기존 유지 leg(WriteTag 계약 — 2u-⑮ APIC 생존 어설션의 도구 경계 라이브 대응)·TIT2 교체 쓰기"
        sleep 5
        shot mus_wsl_tagwrite_list2.png   # 재상단 행(쓰기 2 label) — EYES 보조 캡처
    else
        echo "MUSIC-FAIL(tw-write2: 도구 쓰기 2 거부 — $TW_REPLY)"
    fi
    AP2=$(python3 "$LOGDIR/mus_twparse.py" "$TW_MP3")
    echo "MUS-TW-PARSED-B: $AP2 (쓰기 2 재독 — TIT2 교체+APIC 유지 원문)"
    TW_MD5_2=$(printf '%s\n' "$AP2" | sed -n 's/.*TW-APIC mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    if [ -n "$TW_MD5_2" ] && [ "$TW_MD5_2" = "$TW_COVER_MD5" ]; then
        echo "MUS-TW-APIC-KEEP: OK — cover=null 뒤 APIC 원문 유지(md5 등호 — $TW_MD5_2)"
    else
        echo "MUSIC-FAIL(tw-apic-keep: cover=null 뒤 APIC 소각/변조 — md5=${TW_MD5_2:-none} vs cover=$TW_COVER_MD5)"
    fi

    echo "--- 16f-4. FLAC 도구 쓰기 — cover 생략(유지 leg 계열 — PICTURE 부재 원존)+코멘트 신설(T2 leg) ---"
    if tw_write "{\"path\":\"$TW_FLAC\",\"title\":\"$TW_TITLE_A\",\"artist\":\"$TW_ARTIST\",\"album\":\"$TW_ALBUM\"}"; then
        TWF_OK=1
        echo "MUS-TW-FLAC-OK: ok 수취 — fLaC 스켈레톤(STREAMINFO 첫 블록 34B)에서 VORBIS_COMMENT 신설+블록 재조립(STREAMINFO 보존 — D9·PICTURE 부재 = 교체 대상 없음)"
        sleep 3
        shot mus_wsl_tagwrite_flac.png   # FLAC 행 표기(곡명(아티스트) 반영 — EYES 보조)
    else
        echo "MUSIC-FAIL(tw-flac: FLAC 도구 쓰기 거부 — $TW_REPLY)"
    fi
    FP=$(python3 "$LOGDIR/mus_twparse.py" "$TW_FLAC")
    echo "MUS-TW-FLAC-PARSED: $FP (FLAC 재독 — 메타 블록 사슬 재파싱 원문)"
    TW_FLAC_KEYS=$(printf '%s\n' "$FP" | sed -n 's/.*TW-FLAC-BLOCKS keys=\([0-9]*\) picture=\([0-9]*\).*/keys=\1 picture=\2/p')
    echo "MUS-TW-FLAC-BLOCKS: ${TW_FLAC_KEYS:-none} (관리 3키 TITLE/ARTIST/ALBUM = keys=3·cover 생략 = picture 0 — D9 재조립의 재독 원문)"

    echo "--- 16f-5. 필터 라운드 1회 — 쓰기로 바뀐 TIT2 매치 (16e FFILTER 수형 재용) ---"
    MUS_TW_SETTLE=-1
    if [ -n "${PX_BAND:-}" ] && [ -f "${PX_BAND:-}" ] && type FFILTER >/dev/null 2>&1; then
        python3 "$SHOT_PY" "$LOGDIR/mus_base16f_tw.png" >"$LOGDIR/mus_base16f_tw.log" 2>&1 \
            || FAIL "16f filter base capture failed — $(tail -2 "$LOGDIR/mus_base16f_tw.log" | tr '\n' ' ')"
        FFILTER "$TW_TITLE_B" "tw" mus_wsl_tagwrite_filter.png
        MUS_TW_SETTLE=${TAG_SETTLE_RC:-0}
        echo "MUS-TW-FILTER-OBSERVATION: text='$TW_TITLE_B' — 예상 표 1행(t_tw_write.mp3 — 파일명 매치 0 — TIT2='Wsl Write Reup' 원존 = 쓰기 반영 재독의 인-엔진 원문·D5 title 매치), 카운터 1곡은 캡처 육안 몫"
    else
        echo "MUS-TW-FILTER-SKIP: 16e 미성립(TAG_STAGE=0) — FFILTER/PX_BAND 부재 — 생략(원장; MUS_TW_SETTLE=-1=스킵)"
    fi

    echo "--- 16f-6. TW 시드 소각 (probe 소유 잔상 0 — REMNANT 계약) ---"
    rm -f "$TW_MP3" "$TW_FLAC" "$TW_COVER"
    TW_LEFT=$(ls "$SEED_DIR"/t_tw_* 2>/dev/null | wc -l)
    [ "$TW_LEFT" -eq 0 ] \
        && echo "TW-BURIED: t_tw_* 3건 소각 (17d 가드)" \
        || echo "MUSIC-FAIL(tw-bury: TW 시드 잔존 $TW_LEFT건 — 수동 소각)"
fi

# ==================================================================
# 16f-OM. [#115 T4 — opus/m4a 태그 쓰기(도구) 세그먼트 (플랜
# 2026-10-26-opus-m4a task-4)] music_tag_write relay의 opus/m4a 원문 실측:
#   ① 합성 m4a+opus(2x 픽스처 승계 — mkBox/mkData 4B data atom+PushPacketLacing
#     ·BuildOggPage CRC 규약)를 시더(python3 — $SEED_DIR)로 시드: m4a = ftyp+
#     moov(udta/meta/hdlr('mdir')+ilst[©nam/©ART/©alb UTF-8+covr(png블롭 — 코드
#     14)])+mdat 64B·opus = [BOS OpusHead 19B][OpusTags(vendor+3필드+covr
#     METADATA_BLOCK_PICTURE base64)]. **cover jpg = 16f-TW의 인라인 B64 상수
#     재용**(동일 바이트 — md5 등호 축 공유 — 세그먼트 순서 원존 뒤 신설이라
#     TW 소각 이후 자체 수취).
#   ② 도구 relay 쓰기 4콜 — m4a: ①cover 지정(2x-⑩(b) covr 교체 — 재독 md5 등호)
#     ②cover=null+title 교체(2x-⑩(a) covr 유지)·opus: ③cover 지정(MBP 교체 —
#     D4 병합) ④cover 생략(MBP 유지+벤더 유지 — 2x-⑪ 원존 leg) —
#     **재독 표기 원문**: 파일 수준(순수 파이썬 재파싱 — box/lacing 규약 동형
#     독립 해석 — 앱 파서와 독립)+목록 행 캡처 2종(m4a 행·opus 행 — D5 UI
#     무변경 = "-" 표기가 실값 전환하는 것의 EYES 원문).
#   ③ covr/MBP round-trip 원문 = md5/magic 등호(합성 cover jpg 대차).
#   * 위상 = 16f-TW 말단 뒤·복원 전(TW 소각 이후 — 같은 도구 경계·같은 스캔
#     dir)·클릭 0 — 도구 경로만. scan_busy 재시도 = tw_write 수형 재용.
#   * REMNANT = OM 시드 3건 소각(16f-OM 말단·17e 가드 — probe 소유 잔상 0).
# ==================================================================
MUS_OM_RUN=${MUS_OM_RUN:-1}   # 런 라벨(#115-RUN-VALS 요약 스트림의 run 축 — TW-RUN-VALS 수형)
echo "=== 16f-OM. opus/m4a 태그 쓰기(도구) 세그먼트 (#115 T4 — run=$MUS_OM_RUN) ==="
OM_STAGE=0
OM1_OK=0
OM2_OK=0
OM3_OK=0
OM4_OK=0
OM_CAP_OK=0
OM_M4A_MD5_1=""
OM_M4A_MD5_2=""
OM_MBP_3=""
OM_MBP_4=""
OM_VENDOR=""
OM_COVER_B64='/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAQDAwMDAgQDAwMEBAQFBgoGBgUFBgwICQcKDgwPDg4MDQ0PERYTDxAVEQ0NExoTFRcYGRkZDxIbHRsYHRYYGRj/2wBDAQQEBAYFBgsGBgsYEA0QGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBj/wAARCABAAEADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDzuiiivgz+tAooooAKKKKACiiigDvKKKK4z/NMKKK8K+IfxD8YaH8SNS0rStX+z2kPleXH9nifbmJGPLKT1J711YTCTxU3CDV7X1PRyzLKuY1XSpNJpX1v3S6J9z3WivCvh58Q/GGufEjTdK1XV/tFpN5vmR/Z4k3Yidhyqg9QO9e60YvCTws1CbV7X0DM8sq5dVVKq021fS/drql2CiiiuU848vooor+4j7oK838U+Fte1LxZd3tlY+bBJs2v5qLnCKDwTnqDXpFFeTnGT0c2oqhXbSTvpa97NdU+504XFTw03OC121PN/C3hbXtN8WWl7e2PlQR79z+ajYyjAcA56kV6RRRRk+T0cpouhQbabvra97JdEuwYrFTxM1Oa120CiiivWOY+8qKKK/yrP3AKKKKACiiigAooooA//9k='
                               # 합성 cover jpg(드라이버 PIL 산출 base64 — 16f-TW
                               # 인라인 상수와 동일 바이트 — md5 등호 축 공유.
                               # 자기 완결 — TW 세그먼트 계보와 무관 성립)

cat > "$LOGDIR/mus_omseed.py" <<'PYEOF'
# [#115 T4] opus/m4a 세그먼트 시더 — 16f-TW 수형(2x 픽스처 승계 — 합성이 파스
# 계약을 앞서 "성공"하지 않는 T1 mkData 함정 승계): m4a = ftyp+
# moov(udta/meta(ver/flags 4B+hdlr('mdir')+ilst[©nam UTF-8·©ART·©alb+covr(코드
# 14 — png 시그니처 블롭)])+mdat 64B·opus = [BOS OpusHead 19B][OpusTags
# (vendor+TITLE/ARTIST/ALBUM+METADATA_BLOCK_PICTURE base64)] — lacing(255-런+
# 정크 종결)+BuildOggPage CRC(poly 0x04C11DB7·init 0) 규약 동형. stdlib만(
# 의존 0 — 4c 원문 계약).
import base64, hashlib, os, struct, sys
out, b64 = sys.argv[1], sys.argv[2]
be32 = lambda v: struct.pack(">I", v)
le32 = lambda v: struct.pack("<I", v)

def box(typ, payload):
    return be32(8 + len(payload)) + typ + payload

def data_atom(code, value):
    return box(b"data", b"\x00\x00\x00" + bytes([code])
               + b"\x00\x00\x00\x00" + value)

def item(typ, code, value):
    return box(typ, data_atom(code, value))

png = bytes([0x89]) + b"PNG\r\n\x1a\n" + bytes([1, 2, 3, 4])
jpg = base64.b64decode(b64)

def m4a_blob():
    """2x-①/⑩ 시더 계약 수형: ftyp+moov(udta/meta/hdlr/ilst)+mdat."""
    hdlr = box(b"hdlr", be32(0) + be32(0) + b"mdir" + bytes(17))
    ilst = box(b"ilst", b"".join([
        item(b"\xa9nam", 1, b"Om Seed Title"),
        item(b"\xa9ART", 1, b"Om Seed Artist"),
        item(b"\xa9alb", 1, b"Om Seed Album"),
        item(b"covr", 14, png),
    ]))
    meta = box(b"meta", be32(0) + hdlr + ilst)
    udta = box(b"udta", meta)
    moov = box(b"moov", udta)
    ftyp = box(b"ftyp", b"M4A " + be32(0) + b"isom")
    mdat = box(b"mdat", b"\xab" * 64)
    return ftyp + moov + mdat

TBL = []
for i in range(256):
    r = i << 24
    for _ in range(8):
        r = ((r << 1) ^ 0x04C11DB7) if (r & 0x80000000) else (r << 1)
    r &= 0xFFFFFFFF
    TBL.append(r)

def oggcrc(buf):
    crc = 0
    for b in buf:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ TBL[((crc >> 24) ^ b) & 0xFF]
    return crc & 0xFFFFFFFF

def page(htype, packets, seq, granule=0):
    body = b"".join(packets)
    lace = bytearray()
    n = len(body)
    segs = []
    for p in packets:
        k = len(p) // 255
        segs.extend([255] * k)
        segs.append(len(p) % 255 if (k == 0 or len(p) % 255) else 0)
    for v in segs:
        lace.append(v)
    pg = bytearray(b"OggS" + bytes([0, htype]) + struct.pack("<Q", granule)
                   + struct.pack("<III", 77, seq, 0) + bytes([len(lace)]))
    pg += lace + body
    crc = oggcrc(pg)
    pg[22:26] = struct.pack("<I", crc)
    return bytes(pg)

def opus_blob(vendor, entries, mbp_b64=None):
    head = (b"OpusHead" + bytes([1, 2]) + struct.pack("<H", 312)
            + le32(48000) + struct.pack("<H", 0) + bytes([0]))
    es = [(b"TITLE", b"Om Seed Opus"), (b"ARTIST", b"Om Seed Op Artist"),
          (b"ALBUM", b"Om Seed Op Album"),
          (b"GENRE", b"Wave")]
    if mbp_b64:
        es.append((b"METADATA_BLOCK_PICTURE", mbp_b64.encode("ascii")))
    tags = b"OpusTags" + le32(len(vendor)) + vendor + le32(len(es))
    for k, v in es:
        tags += le32(len(k) + 1 + len(v)) + k + b"=" + v
    return (page(0x02, [head], 0)
            + page(0x04, [tags], 1))  # 코멘트 페이지 EOS 표지 0x04(2x-⑪ 원문)

def build_pic(mime, blob):
    """FLAC 그림 구조(BuildFlacPictureStruct 규약 수형): type 3(front cover)·
    mime·desc 빈·w/h/depth/colors 0·data."""
    return (be32(3) + be32(len(mime)) + mime + be32(0) + be32(0) * 4
            + be32(len(blob)) + blob)

pic = build_pic(b"image/png", png)
mbp = base64.b64encode(pic).decode()
m4a = os.path.join(out, "t_om_write.m4a")
with open(m4a, "wb") as f:
    f.write(m4a_blob())
opus = os.path.join(out, "t_om_write.opus")
with open(opus, "wb") as f:
    f.write(opus_blob(b"om-vendor", [], mbp))
cover = os.path.join(out, "t_om_cover.jpg")
with open(cover, "wb") as f:
    f.write(jpg)
print("OM-SEED-OK m4a=%d opus=%d cover=%d png_magic=%d mbp_base64=%d "
      "mbp_md5=%s cover_md5=%s"
      % (os.path.getsize(m4a), os.path.getsize(opus), len(jpg),
         1 if png[:4] == b"\x89PNG" else 0, 1,
         hashlib.md5(pic).hexdigest(), hashlib.md5(jpg).hexdigest()))
PYEOF
OM_SEED=$(python3 "$LOGDIR/mus_omseed.py" "$SEED_DIR" "$OM_COVER_B64" 2>"$LOGDIR/mus_omseed.err") \
    || FAIL "OM seed generation failed — $(tail -2 "$LOGDIR/mus_omseed.err" | tr '\n' ' ')"
echo "MUS-OM-SEED: $OM_SEED"
grep -aq 'OM-SEED-OK' <<<"$OM_SEED" || FAIL "OM seed receipt broken (합성 m4a/opus 미성립)"
OM_M4A="$SEED_DIR/t_om_write.m4a"
OM_OPUS="$SEED_DIR/t_om_write.opus"
OM_COVER="$SEED_DIR/t_om_cover.jpg"
OM_COVER_MD5=$(md5sum "$OM_COVER" | awk '{print $1}')
grep -aq 'png_magic=1' <<<"$OM_SEED" || FAIL "OM m4a seed covr png magic broken (89504E47 — 2x-⑥ pngBlob 규약)"
grep -aq 'mbp_base64=1' <<<"$OM_SEED" || FAIL "OM opus seed MBP broken (base64 그림 구조 규약 — ParseFlacPictureStruct 접점)"
echo "MUS-OM-COVER-MD5: $OM_COVER_MD5 (16f-TW 합성 cover와 동일 바이트 — TW-MD5=${TW_COVER_MD5:-n/a} 대조 원장)"

# tw_write 없음 폴백(TW 세그먼트 미성립 계보의 OM 독립성 — 16f-2 수형 재작성):
if ! type tw_write >/dev/null 2>&1; then
    tw_write() {
        TW_REPLY=""
        local a="$1" r i ok=1
        for i in 1 2 3; do
            r=$(atool music_tag_write "$a")
            TW_REPLY=$r
            echo "  TW-WRITE#$i: ${r:-none}"
            if printf '%s' "$r" | grep -aq '"ok":true'; then ok=0; break; fi
            printf '%s' "$r" | grep -aq '"error":"scan_busy"' || break
            sleep 4
        done
        return $ok
    }
fi

cat > "$LOGDIR/mus_omparse.py" <<'PYEOF'
# [#115 T4] opus/m4a 재독 파서 — 시더와 독립·앱 파서와 독립(트리 보행을 다시
# 쓴다): m4a = 최상위 box 보행 → moov → udta → meta(ver/flags 4B 스킵) →
# hdlr('mdir' 확인) → ilst 항목 ©nam/©ART/©alb+\xa9nam 1바이트 접두어(파스
# 계약 원문)·covr data(code 13=jpeg·14=png·0=binary 스니프) 재독 —
# size=1(largesize)/잘린 박스 = 정직 절단(앱 계약과 동일한 보행 규약의 독립
# 재작성). opus = OggS 페이지 보행(lacing 패킷 재조립 — 255-연속/잔여) →
# "OpusTags" 패킷에서 vendor+엔트리(관리 3키)+METADATA_BLOCK_PICTURE(base64 →
# FLAC 그림 구조: type·mime·desc·w/h/depth/colors·data) 재독. 진실원 = 파일
# 바이트.
import base64, hashlib, struct, sys

def rdb(buf, off):
    return struct.unpack(">I", buf[off:off + 4])[0]

def walk(buf, off, end):
    # 최상위 box 보행 원존(앱 Mp4BoxWalk 규약의 독립 재작성): size<8·잘림 =
    # 종결(정직 절단)·size==0 = 박스 = 말미 영역(마지막 박스 규약).
    while off + 8 <= end:
        size = rdb(buf, off)
        typ = buf[off + 4:off + 8]
        if size == 0:
            size = end - off
        if size < 8 or off + size > end:
            return
        yield typ, off + 8, off + size
        off += size

def first_data(item, buf):
    # ilst 항목의 첫 data atom(파스 계약 — [ver/flags 4B][4B locale][값]).
    for typ, s, e in walk(buf, item[1], item[2]):
        if typ == b"data":
            code = buf[s + 3]
            return code, buf[s + 8:e]
    return None, None

def parse_m4a(path, out):
    d = open(path, "rb").read()
    out.append("OM-M4A-BYTES %d" % len(d))
    moov = None
    mdat_ab = -1
    for typ, s, e in walk(d, 0, len(d)):
        if typ == b"moov":
            moov = (s, e)
        elif typ == b"mdat":
            mdat_ab = d[s:e].count(0xAB)   # D2 — mdat 원문 무이동 검증 원료
    if moov is None:
        out.append("OM-M4A no-moov")
        return out
    out.append("OM-M4A-MDAT 0xab=%d" % mdat_ab)
    udta = None
    for typ, s, e in walk(d, moov[0], moov[1]):
        if typ == b"udta":
            udta = (s, e)
    if udta is None:
        out.append("OM-M4A no-udta")
        return out
    meta = None
    for typ, s, e in walk(d, udta[0], udta[1]):
        if typ == b"meta":
            meta = (s + 4, e)   # full box — ver/flags 4B 스킵
    if meta is None:
        out.append("OM-M4A no-meta")
        return out
    ok_hdlr = False
    tags, covr = {}, None
    ilst = None
    for typ, s, e in walk(d, meta[0], meta[1]):
        if typ == b"hdlr" and d[s + 8:s + 12] == b"mdir":
            ok_hdlr = True
        elif typ == b"ilst":
            ilst = (s, e)
    if ilst is None:
        out.append("OM-M4A no-ilst")
        return out
    out.append("OM-M4A hdlr=%d" % (1 if ok_hdlr else 0))
    for typ, s, e in walk(d, ilst[0], ilst[1]):
        if typ in (b"\xa9nam", b"\xa9ART", b"\xa9alb"):
            code, val = first_data((typ, s, e), d)
            if code == 1:
                try:
                    tags[typ.decode("latin-1")] = val.decode("utf-8")
                except UnicodeDecodeError:
                    pass
        elif typ == b"covr":
            code, val = first_data((typ, s, e), d)
            if code in (13, 14) and val is not None:
                covr = (code, val)
    out.append("OM-M4A-TAG title=%r artist=%r album=%r"
               % (tags.get("\xa9nam", ""), tags.get("\xa9ART", ""),
                  tags.get("\xa9alb", "")))
    if covr:
        out.append("OM-COVR mime=%s md5=%s bytes=%d code=%d magic=%s"
                   % ("image/jpeg" if covr[0] == 13 else "image/png",
                      hashlib.md5(covr[1]).hexdigest(), len(covr[1]), covr[0],
                      covr[1][:2].hex()))
    else:
        out.append("OM-COVR none")
    return out

def parse_opus(path, out):
    d = open(path, "rb").read()
    out.append("OM-OPUS-BYTES %d" % len(d))
    # Ogg 페이지 보행 → 패킷 재조립(lacing 255-연속+잔여 — OpusTagsPacket 규약
    # 의 독립 재작성).
    pkt, off = b"", 0
    vendor, tags, mbp = None, {}, None
    cmt_htype = None
    while off + 27 <= len(d):
        if d[off:off + 4] != b"OggS":
            break
        segs = d[off + 26]
        tables = d[off + 27:off + 27 + segs]
        pay_off = off + 27 + segs
        plen = sum(tables)
        if pay_off + plen > len(d):
            break                       # 잘린 페이지 = 정직 절단
        for t in tables:
            seg = d[pay_off:pay_off + t]
            pay_off += t
            pkt += seg
            if t < 255:                 # 패킷 완성 지점에서만 매김
                if pkt[:8] == b"OpusTags" and vendor is None:
                    cmt_htype = d[off + 5]   # 코멘트 페이지 htype([0..4)=OggS·[4)=version·[5)=htype — D10 원문 유지 검증 원료 — fix r1: off+4 원문 오독 = version 0 오판)
                    vlen = struct.unpack("<I", pkt[8:12])[0]
                    vendor = pkt[12:12 + vlen].decode("utf-8", "replace")
                    p, cnt = 12 + vlen, struct.unpack(
                        "<I", pkt[12 + vlen:16 + vlen])[0]
                    p = 16 + vlen
                    for _ in range(cnt):
                        elen = struct.unpack("<I", pkt[p:p + 4])[0]
                        ent = pkt[p + 4:p + 4 + elen].decode(
                            "utf-8", "replace")
                        p += 4 + elen
                        k, _, v = ent.partition("=")
                        if k == "METADATA_BLOCK_PICTURE":
                            try:
                                pic = base64.b64decode(v)
                                mlen = rdb(pic, 4)
                                mime = pic[8:8 + mlen].decode("latin-1")
                                desc_len = rdb(pic, 8 + mlen)
                                p2 = 8 + mlen + 4 + desc_len
                                skip = 16
                                dlen = rdb(pic, p2 + skip)
                                data = pic[p2 + skip + 4:p2 + skip + 4 + dlen]
                                mbp = (mime, data)
                            except Exception:
                                mbp = None
                        else:
                            tags[k] = v
                pkt = b""
        off = off + 27 + segs + plen
    out.append("OM-OPUS vendor=%r title=%r artist=%r album=%r cmt_htype=%s"
               % (vendor, tags.get("TITLE", ""), tags.get("ARTIST", ""),
                  tags.get("ALBUM", ""),
                  str(cmt_htype) if cmt_htype is not None else "none"))
    if mbp:
        out.append("OM-MBP mime=%s md5=%s bytes=%d magic=%s"
                   % (mbp[0], hashlib.md5(mbp[1]).hexdigest(), len(mbp[1]),
                      mbp[1][:2].hex()))
    else:
        out.append("OM-MBP none")
    return out

path = sys.argv[1]
head = open(path, "rb").read(12)
if b"ftyp" in head:
    for ln in parse_m4a(path, []):
        print(ln)
elif head[:4] == b"OggS":
    for ln in parse_opus(path, []):
        print(ln)
else:
    print("OM-PARSE magic-unknown (합성 전제 위반 — 시더 원문 재확인)")
PYEOF

if [ ! -n "$MUSWIN" ]; then
    for i in 1 2 3 4 5 6 7 8; do
        sleep 2
        WINOM=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        MUSWIN=$(printf '%s' "$WINOM" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
        [ -n "$MUSWIN" ] && break
    done
fi
if [ -n "$MUSWIN" ]; then
    OM_STAGE=1
    echo "MUS-OM-RECON: Music 창 성립 (16f-TW 승계 — 활성 탭 mus_seed — MUSWIN: $MUSWIN)"
else
    echo "MUSIC-FAIL(om-recon: Music 창 부재 — 16f-TW 계보 재대조 몫 — opus/m4a 세그먼트 생략, 원장)"
fi

if [ "$OM_STAGE" -eq 1 ]; then
    echo "--- 16f-0a. OM 재독 파서 — 프리뷰 (쓰기 전 — 합성 원문) ---"
    OM_PRE_M4A=$(python3 "$LOGDIR/mus_omparse.py" "$OM_M4A")
    echo "MUS-OM-PREVIEW-M4A: $OM_PRE_M4A (쓰기 전 — 시더 값+png covr 원문)"
    OM_PRE_OPUS=$(python3 "$LOGDIR/mus_omparse.py" "$OM_OPUS")
    echo "MUS-OM-PREVIEW-OPUS: $OM_PRE_OPUS (쓰기 전 — 시더 값+png MBP 원문)"
    grep -aq 'hdlr=1' <<<"$OM_PRE_M4A" || FAIL "OM m4a seed hdlr('mdir') missing — 2x-① 구성 계약 위배"

    echo "--- 16f-1. 도구 쓰기 1 — m4a cover 지정(2x-⑩(b) covr 교체 leg) ---"
    # tw_write 수형 재용(16f-2 — scan_busy 재시도 3콜 — TW 라인 원존).
    OM_TITLE_A="Wsl Omus Alpha"
    OM_ARTIST="Probe Om Artist"
    OM_ALBUM="Om Album One"
    OM_TITLE_B="Wsl Omus Reup"
    if tw_write "{\"path\":\"$OM_M4A\",\"title\":\"$OM_TITLE_A\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\",\"cover\":\"$OM_COVER\"}"; then
        OM1_OK=1
        echo "MUS-OM-WRITE1-OK: ok 수취 — m4a relay 쓰기 성립(WriteM4aTag — ilst 재조립+mdat 원문 무이동 — D2)"
        sleep 5
        shot mus_wsl_om_list.png   # m4a 행 표기(D5 — "-" 대신 실값 — EYES)
        echo "MUS-OM-LIST-LEDGER: 예상 상단 행(쓰기 mtime desc): 'Wsl Omus Alpha (Probe Om Artist)'+'Om Album One'+좌측 커버아트 썸네일(4색 밴드 jpg — 교체된 covr) — 표기·썸네일은 캡처 육안 몫(EYES)"
    else
        echo "MUSIC-FAIL(om-write1: m4a 도구 쓰기 거부 — $TW_REPLY)"
    fi
    AP1=$(python3 "$LOGDIR/mus_omparse.py" "$OM_M4A")
    echo "MUS-OM-PARSED-A: $AP1 (m4a 파일 수준 재독 — 앱 파서와 독립 round-trip 원문)"
    OM_M4A_MD5_1=$(printf '%s\n' "$AP1" | sed -n 's/.*OM-COVR mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    OM_M4A_MIME_1=$(printf '%s\n' "$AP1" | sed -n 's/.*OM-COVR mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\1/p')
    OM_MDAT_1=$(printf '%s\n' "$AP1" | sed -n 's/.*OM-M4A-MDAT 0xab=\([0-9]*\).*/\1/p')
    echo "MUS-OM-COVR-1: mime=${OM_M4A_MIME_1:-none} md5=${OM_M4A_MD5_1:-none} mdat_0xab=${OM_MDAT_1:-none} (합성 cover md5=$OM_COVER_MD5 대조 — 교체 성립 축·mdat 무이동 검증 = 64 기대)"
    if [ -n "$OM_M4A_MD5_1" ] && [ "$OM_M4A_MD5_1" = "$OM_COVER_MD5" ] \
        && [ "$OM_M4A_MIME_1" = "image/jpeg" ] \
        && [ "$OM_MDAT_1" = "64" ]; then
        echo "MUSIC-OM-COVR-RT: OK — m4a covr == 합성 cover jpg+mdat 원문 무이동(교체 — D2/D4·2x-⑩(b)의 라이브 대응)"
    else
        echo "MUSIC-FAIL(om-covr-rt: covr 교체/mdat 불일치 — md5=${OM_M4A_MD5_1:-none} mime=${OM_M4A_MIME_1:-none} mdat=${OM_MDAT_1:-none})"
    fi

    echo "--- 16f-2. 도구 쓰기 2 — m4a cover=null(기존 covr 유지 계약)+title 교체 ---"
    if tw_write "{\"path\":\"$OM_M4A\",\"title\":\"$OM_TITLE_B\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\",\"cover\":null}"; then
        OM2_OK=1
        echo "MUS-OM-WRITE2-OK: ok 수취 — cover=null = 기존 covr 유지 leg(WriteM4aTag covr null 계약 — 2x-⑩(a)의 도구 경계 라이브 대응)"
        sleep 5
        shot mus_wsl_om_list2.png   # 재상단 행(쓰기 2 label) — EYES 보조 캡처
    else
        echo "MUSIC-FAIL(om-write2: m4a 도구 쓰기 2 거부 — $TW_REPLY)"
    fi
    AP2=$(python3 "$LOGDIR/mus_omparse.py" "$OM_M4A")
    echo "MUS-OM-PARSED-B: $AP2 (쓰기 2 재독 — title 교체+covr 유지 원문)"
    OM_M4A_MD5_2=$(printf '%s\n' "$AP2" | sed -n 's/.*OM-COVR mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    if [ -n "$OM_M4A_MD5_2" ] && [ "$OM_M4A_MD5_2" = "$OM_COVER_MD5" ]; then
        echo "MUS-OM-COVR-KEEP: OK — cover=null 뒤 covr 원문 유지(md5 등호 — $OM_M4A_MD5_2)"
    else
        echo "MUSIC-FAIL(om-covr-keep: cover=null 뒤 covr 소각/변조 — md5=${OM_M4A_MD5_2:-none} vs cover=$OM_COVER_MD5)"
    fi

    echo "--- 16f-3. 도구 쓰기 3 — opus cover 지정(MBP 교체 — 2x-⑩(c) leg) ---"
    if tw_write "{\"path\":\"$OM_OPUS\",\"title\":\"$OM_TITLE_A\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\",\"cover\":\"$OM_COVER\"}"; then
        OM3_OK=1
        echo "MUS-OM-WRITE3-OK: ok 수취 — opus relay 쓰기 성립(WriteOpusTag — OpusTags 패킷 교체+페이지 재조립 — D3)"
        sleep 5
        shot mus_wsl_om_opus.png   # opus 행 표기(D5 — EYES)
        echo "MUS-OM-OPUS-LEDGER: 예상 상단 행: 'Wsl Omus Alpha (Probe Om Artist)'+'Om Album One'(.opus 행 — 커버 썸네일 = MBP 교체 jpg) — 캡처 육안 몫(EYES)"
    else
        echo "MUSIC-FAIL(om-write3: opus 도구 쓰기 거부 — $TW_REPLY)"
    fi
    AP3=$(python3 "$LOGDIR/mus_omparse.py" "$OM_OPUS")
    echo "MUS-OM-PARSED-C: $AP3 (opus 파일 수준 재독 — OpusTags/MBP 재조립 round-trip 원문)"
    OM_MBP_3=$(printf '%s\n' "$AP3" | sed -n 's/.*OM-MBP mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    OM_MBP_MIME_3=$(printf '%s\n' "$AP3" | sed -n 's/.*OM-MBP mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\1/p')
    OM_VENDOR=$(printf '%s\n' "$AP3" | sed -n "s/.*vendor='\([^']*\)'.*/\1/p")
    echo "MUS-OM-MBP-3: mime=${OM_MBP_MIME_3:-none} md5=${OM_MBP_3:-none} vendor=${OM_VENDOR:-none}"
    OM_HT_3=$(printf '%s\n' "$AP3" | sed -n 's/.*cmt_htype=\([0-9a-z]*\).*/\1/p')
    if [ -n "$OM_MBP_3" ] && [ "$OM_MBP_3" = "$OM_COVER_MD5" ] \
        && [ "$OM_MBP_MIME_3" = "image/jpeg" ] \
        && [ "$OM_VENDOR" = "om-vendor" ] && [ "$OM_HT_3" = "4" ]; then
        echo "MUSIC-OM-MBP-RT: OK — opus MBP == 합성 cover jpg+벤더 유지+코멘트 페이지 EOS 0x04 유지(D4/D9/D10 라이브 대응 — 2x-⑩(c)+⑪)"
    else
        echo "MUSIC-FAIL(om-mbp-rt: MBP 교체/벤더/EOS 불일치 — md5=${OM_MBP_3:-none} vendor=${OM_VENDOR:-none} htype=${OM_HT_3:-none})"
    fi

    echo "--- 16f-4. 도구 쓰기 4 — opus cover 생략(MBP 유지 leg)+title 교체 ---"
    if tw_write "{\"path\":\"$OM_OPUS\",\"title\":\"$OM_TITLE_B\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\"}"; then
        OM4_OK=1
        echo "MUS-OM-WRITE4-OK: ok 수취 — opus cover 생략 = 기존 MBP 유지 leg(WriteOpusTag 유지 계약 — FLAC PICTURE 부재 leg의 opus 대응)"
        sleep 5
    else
        echo "MUSIC-FAIL(om-write4: opus 도구 쓰기 2 거부 — $TW_REPLY)"
    fi
    AP4=$(python3 "$LOGDIR/mus_omparse.py" "$OM_OPUS")
    echo "MUS-OM-PARSED-D: $AP4 (쓰기 4 재독 — title 교체+MBP 유지 원문)"
    OM_MBP_4=$(printf '%s\n' "$AP4" | sed -n 's/.*OM-MBP mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    if [ -n "$OM_MBP_4" ] && [ "$OM_MBP_4" = "$OM_COVER_MD5" ]; then
        echo "MUS-OM-MBP-KEEP: OK — opus cover 생략 뒤 MBP 원문 유지(md5 등호 — $OM_MBP_4)"
    else
        echo "MUSIC-FAIL(om-mbp-keep: opus cover 생략 뒤 MBP 소각/변조 — md5=${OM_MBP_4:-none} vs cover=$OM_COVER_MD5)"
    fi

    echo "--- 16f-5. OM 시드 소각 (probe 소유 잔상 0 — 17e 가드) ---"
    rm -f "$OM_M4A" "$OM_OPUS" "$OM_COVER"
    rm -f "$LOGDIR/mus_omseed.py" "$LOGDIR/mus_omparse.py" "$LOGDIR/mus_omseed.err"
    OM_LEFT=$(ls "$SEED_DIR"/t_om_* 2>/dev/null | wc -l)
    [ "$OM_LEFT" -eq 0 ] \
        && echo "OM-BURIED: t_om_* 3건 소각 (17e 가드)" \
        || echo "MUSIC-FAIL(om-bury: OM 시드 잔존 $OM_LEFT건 — 수동 소각)"
else
    echo "MUS-OM-SUMMARY: stage=0 w1=0 w2=0 w3=0 w4=0 covr_rt=0 covr_keep=0 mbp_rt=0 mbp_keep=0 (Music 창 부재 — 세그먼트 생략 — 원장)"
fi

restore_and_exit

echo "=== 17. REMNANT — /tmp 자가 스크립트·중간파일 소각 ==="
rm -f "$SNAP" "$PAT"
LEFTS=$(ls "$LOGDIR" 2>/dev/null | wc -l)
REMNANT_LIST=$(ls "$LOGDIR")
echo "REMNANT-COUNT=$LEFTS (서버·빌드·셀프테스트·shot 로그만 잔존 — 원장 수형)"
echo "$REMNANT_LIST" | sed 's/^/  /'
if [ -d "$SCRATCH" ]; then
    echo "MUS-FAIL: scratch dir remnant — 수동 소각 필요"
else
    echo "SCRATCH-REMNANT: 0 (트리 잔산 0)"
fi
[ -d "$SEED_DIR" ] && { echo "MUS-FAIL: seed dir remnant — 수동 소각 필요"; exit 1; }
SETL=$(ls buildwsl/state/ 2>/dev/null | grep -a 'settings.json' \
    | grep -av '^settings.json$' | tr '\n' ' ')
echo "SETTINGS-REMNANT: ${SETL:-0} (bak/<pid>.tmp — 원자적 쓰기의 성공세대 이주/크래시 잔산 정직 부기 — T2 fix r1 M-1 영수증)"
[ -z "$SETL" ] || echo "MUSIC-FAIL(settings remnant: $SETL — 원장)"
rm -f buildwsl/state/settings.json.bak buildwsl/state/settings.json.*.tmp
[ -d /tmp/mus_dirs_x ] && echo "MUSIC-FAIL(dir remnant: /tmp/mus_dirs_x — 수동 소각)"
rm -rf /tmp/mus_dirs_x
echo "=== 17b. [T3] 대형 합성 트리 소각 (16d — probe 소유 잔상 0) ==="
rm -rf "$BIGA" "$BIGB" 2>/dev/null
if [ -d "$BIGA" ] || [ -d "$BIGB" ]; then
    echo "MUSIC-FAIL(big-tree remnant: $BIGA $BIGB — 수동 소각 필요)"
else
    echo "BIG-BURIED-OK: ${BIGA}·${BIGB} 0 존재 (16d 트리 2종 소각 — 잔상 0)"
fi
# big 트리 등록 잔상 청산은 settings.json 원복 계약(ENTRY 대조)의 소관 —
# 여기는 등록 값의 잔상만 정직 부기한다(파일이 END 원복 전이면 원문 그대로).
[ -f "$SETTINGS" ] \
    && echo "MUSIC-BIG-SETTINGS-END: $(cat "$SETTINGS") (settings 원본 잔존 — 원복 경계의 소관 분석 몫)" \
    || echo "MUSIC-BIG-SETTINGS-END: (settings.json 원복 완료 — ENTRY 대조 경계가 소각)"

echo "=== 17c. [T3(#97)] 브라우저 트리 잔상 가드 (16b2 — probe 소유 잔상 0) ==="
if [ -d "$MUS_BROWSE_TREE" ]; then
    echo "MUS-FAIL: browse tree remnant ($MUS_BROWSE_TREE) — 수동 소각 필요(원장)"
else
    echo "BROWSE-TREE-REMNANT: 0 (16b2 트리 소각 — 잔상 0)"
fi

echo "=== 17d. [#111 T5] 태그 쓰기 TW 시드 잔상 가드 (16f-TW — probe 소유 잔상 0) ==="
TW_LEFT_ALL=$(ls "$SEED_DIR"/t_tw_* 2>/dev/null | wc -l)
if [ "$TW_LEFT_ALL" -eq 0 ]; then
    echo "TW-REMNANT: 0 (16f-TW 합성 mp3+FLAC+cover jpg 소각 — 잔상 0)"
else
    echo "MUS-FAIL: TW seed remnant ($SEED_DIR/t_tw_* $TW_LEFT_ALL건) — 수동 소각 필요(원장)"
fi

echo "=== 17e. [#115 T4] opus/m4a OM 시드 잔상 가드 (16f-OM — probe 소유 잔상 0) ==="
OM_LEFT_ALL=$(ls "$SEED_DIR"/t_om_* 2>/dev/null | wc -l)
if [ "$OM_LEFT_ALL" -eq 0 ]; then
    echo "OM-REMNANT: 0 (16f-OM 합성 m4a+opus+cover jpg 소각 — 잔상 0)"
else
    echo "MUS-FAIL: OM seed remnant ($SEED_DIR/t_om_* $OM_LEFT_ALL건) — 수동 소각 필요(원장)"
fi

echo "=== 18. music 실측 판정 ==="
CAP_OK=1
for f in spatial list filter vp_delegate delegate_status after; do
    P="$RECEIVE/mus_wsl_$f.png"
    if [ ! -s "$P" ]; then
        echo "MUSIC-FAIL(capture missing/empty: $P)"
        CAP_OK=0
    fi
done
DEL_OK=0
[ -n "${VPWIN:-}" ] && DEL_OK=1
GLUE_OK=0
printf '%s' "${OR:-}" | grep -aq '"accepted":true' && GLUE_OK=1
PLAY_OK=0
printf '%s' "${GS2:-}${GS3:-}" | grep -aq '"opened":true' && PLAY_OK=1
if [ "$CAP_OK" -eq 1 ] && [ "$DEL_OK" -eq 1 ] && [ "$GLUE_OK" -eq 1 ] && [ "$PLAY_OK" -eq 1 ]; then
    echo "MUSIC-VERDICT: MUSIC-OK(WSL 빌드 rc=$B_RC·selftest FAIL=$ST_FAIL 계보 PASS=$ST_PASS(캐논 $CANON_WSL_SELFTEST) 2m=$ST2M·delegate=창 출현(deleg-open=$DELEG_OPEN)·glue=accepted·get_status opened=true — 육안 스탭 대기)"
else
    echo "MUSIC-VERDICT: MUSIC-FAIL(행별 사유는 위 각 행 — delegate=$DEL_OK deleg-open=$DELEG_OPEN glue=$GLUE_OK play=$PLAY_OK captures=$CAP_OK; 인프라 실패는 hard FAIL로 상단 중단, 수치·육안 미달은 rc=0 honest-fail)"
fi
echo "MUSIC-WCAPTURES:"
for f in spatial list filter vp_delegate delegate_status after dirs_panel taglist tag_filter tag_filter_artist tag_after; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
echo "=== 18c. [T3] dirs 도구 세그먼트/I-1 판정 (rc는 0 유지 — 원문은 위 각 행) ==="
DIRS_OK=0
if printf '%s' "${DADD:-}" | grep -aq '"ok":true' \
    && printf '%s' "${DL1:-}" | grep -aq 'mus_dirs_x' \
    && printf '%s' "${DREM:-}" | grep -aq '"ok":true' \
    && ! printf '%s' "${DL2:-}" | grep -aq 'mus_dirs_x' ; then
    DIRS_OK=1
fi
[ "$DIRS_OK" -eq 1 ] \
    && echo "MUSIC-DIRS-VERDICT: DIRS-OK(add=ok·list 등록/제거 원문·settings 기록+audio/retention 보존·원복 바이트 등호) — 도구 6종 원존(spatial 3종 병합 유지), 육안 스탭 대기" \
    || echo "MUSIC-DIRS-VERDICT: DIRS-FAIL(add=${DADD:-none} list1=${DL1:-none} remove=${DREM:-none} list2=${DL2:-none} — 행별 사유는 16b)"
I1_SUM="boot=$I1_BOOT ss=$I1_SS keep=$I1_KEEP rsc=${I1_RSC:-0} cure=$I1_CURE retry=${I1_RETRY:-0} ss2=${I1_SS2:-0} restore=${I1_RESTORE:-0}"
if [ "$I1_BOOT" -eq 1 ] && [ "$I1_SS" -eq 1 ] && [ "$I1_KEEP" -eq 1 ] \
    && [ "${I1_RSC:-0}" -eq 1 ] && [ "${I1_RESTORE:-0}" -eq 1 ]; then
    echo "MUSIC-I1-VERDICT: I1-OK(부팅 stderr 경고 1행·정문 거부 detail 원문·CP949 보존·스캐너 수용 remove 1콜·백업 원복 바이트 등호) — $I1_SUM cure=$I1_CURE(패널 치유 — MISS면 재정찰/EYES 승계)"
else
    echo "MUSIC-I1-VERDICT: I1-FAIL(honest-fail — 구성항목 원문은 16c 행별; $I1_SUM)"
fi
echo "=== 18b. spatial leg 판정 (T3 — rc는 0 유지, 원문은 위 각 행) ==="
LEG_FALLBACK=0
if printf '%s' "${PLAY1:-}" | grep -aq '"error":"start_failed"' \
    && printf '%s' "${PLAY1:-}" | grep -aq 'vplayer 위임'; then
    LEG_FALLBACK=1   # kDelegationHint 원문 라벨 — fail-closed 정상 영수증
fi
LEG_OK=0
printf '%s' "${PLAY1:-}" | grep -aq '"accepted":true' \
    && printf '%s' "${S2:-}" | grep -aq '"active":true' \
    && printf '%s' "${S2:-}" | grep -aq '"deviceOk":true' \
    && { [ -n "$POS_DELTA" ] && awk -v d="$POS_DELTA" 'BEGIN{exit (d > 0.05) ? 0 : 1}' \
        && LEG_OK=1; }
if [ "$LEG_MODE" -eq 1 ]; then
    if [ "$LEG_OK" -eq 1 ]; then
        echo "MUS-LEG-VERDICT: LEG-OK(env 설정 배포 런 — accepted+active:true+deviceOk:true+pos 진행 delta=$POS_DELTA·stop 원문 — 청안 게이트는 EYES 별도)"
    elif [ "$LEG_FALLBACK" -eq 1 ]; then
        echo "MUS-LEG-VERDICT: LEG-FALLBACK-OK(WSLg에서 ALC 디바이스 실패 — kDelegationHint 라벨 표기 = fail-closed 정상 영수증(rc=0) — 폰 leg는 T4 몫)"
    else
        echo "MUSIC-FAIL(leg verdict 미달 — LEG_MODE=1에서도 시작/진행/폴백 어느 성질도 관측 못함: play=${PLAY1:-none} status=${S2:-none})"
    fi
else
    if [ "$LEG_FALLBACK" -eq 1 ]; then
        echo "MUS-LEG-UNSET-VERDICT: LEG-UNSET-OK(leg-less .so의 spatial_play = DeviceFailed 종착 — kDelegationHint 원문 라벨 — fail-closed 영수증 성립)"
    else
        echo "MUSIC-FAIL(env 미설정 런에서 spatial_play의 DeviceFailed 종착이 관측되지 않음: ${PLAY1:-none} — fail-closed 배선 원장 대조 몫)"
    fi
fi
if [ "$LEG_MODE" -eq 1 ] && [ "$LEG_OK" -eq 1 ]; then
    [ "$M1_TRUNC" -eq 1 ] \
        && echo "MUS-LEG-M1: OK — 초장 경로(~${LONG_LEN}B) spatial_status가 truncated:true를 냈다(절단 정직 가드 원문 실측)" \
        || echo "MUS-LEG-M1: MISS — truncated 도달 실패(SL1 원문) — M-1 가드 런타임 실측 미충족(리포트 원장)"
fi
echo "=== 18d. [T3] 스캔 취소 경계 판정 (rc는 0 유지 — 원문은 16d 행별) ==="
CLOSE_OK=0
if [ "$BIG_GONE" -eq 1 ] && [ "$BIG_CRASH" -eq 0 ] \
    && printf '%s' "${BLRES:-}" | grep -aq '"ok":true'; then
    CLOSE_OK=1
fi
DISC_OK=0
if [ "${BIG_TAP_LAND2:-0}" -eq 1 ] && [ -n "${DISC_LINE:-}" ] \
    && printf '%s' "$DISC_LINE" | grep -aq 'mus_big_a' \
    && printf '%s' "${BLIVE:-}" | grep -aq '"ok":true' \
    && ! printf '%s' "${BLIST2:-}" | grep -aq 'mus_big_a'; then
    DISC_OK=1
fi
CAP_CANCEL_OK=1
for f in scan_busy rescan_settled; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] || { echo "MUSIC-FAIL(cancel capture missing: $P)"; CAP_CANCEL_OK=0; }
done
echo "  구성: close=$CLOSE_OK(gone=$BIG_GONE crash=$BIG_CRASH respawn-list=${BLRES:-none}) discard=$DISC_OK(land=$BIG_TAP_LAND2·settle=${BIG_SETTLED:-0} 1행=${DISC_LINE:-MISS}) captures=$CAP_CANCEL_OK"
if [ "$CLOSE_OK" -eq 1 ] && [ "$DISC_OK" -eq 1 ] && [ "$CAP_CANCEL_OK" -eq 1 ]; then
    echo "MUSIC-CANCEL-VERDICT: CANCEL-OK(①스캔 중 창 close — 고속 소멸 원문+crash 마커 0+재스폰 도구 응답 원문·②스캔 중 music_dir_remove — 착탄 캡처+폐기 stderr 1행 원문+클라 생존+제거/원복 원문·③재스캔 정착 전이 캡처+캐논 등호(코드 무변경 — 661/638/296 원문 승계)) — 육안 스탭 대기"
else
    echo "MUSIC-CANCEL-VERDICT: CANCEL-FAIL(honest-fail — 행별 사유는 16d; close=$CLOSE_OK discard=$DISC_OK captures=$CAP_CANCEL_OK)"
fi
echo "MUSIC-CAPTURES-16D:"
for f in scan_busy rescan_settled; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
echo "=== 18e. [T3(#97)] 브라우저 세그먼트 판정 (rc는 0 유지 — 원문은 16b2 행별) ==="
BR_ALL=0
if [ "${BR_STAGE:-0}" -eq 8 ] && [ "${BR_ADD:-0}" -eq 1 ] && [ "${BR_SUB2:-0}" -eq 1 ] \
    && [ "${BR_RESTORE:-0}" -eq 1 ]; then
    BR_ALL=1
fi
BR_CAP_OK=1
for f in browse_open browse_subs browse_sub browse_up browse_sub2 browse_added browse_rescan; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] || { echo "MUSIC-FAIL(browse capture missing: $P)"; BR_CAP_OK=0; }
done
echo "  구성: stage=${BR_STAGE:-0}/8(tab=${BR_TAB:-0}·tog=${BR_TOG:-0}·open=${BR_OPEN:-0}·sub=${BR_SUB:-0}·up=${BR_UP:-0}·sub2=${BR_SUB2:-0}·add=${BR_ADD:-0}) restore=${BR_RESTORE:-0} captures=${BR_CAP_OK}"
if [ "$BR_ALL" -eq 1 ] && [ "$BR_CAP_OK" -eq 1 ]; then
    echo "MUSIC-BROWSE-VERDICT: BROWSE-OK(②탭 클릭 브래킷+패널+[찾아보기] 개봉 캡처 2종·③JoinDir 진입·④Parent 복귀·④2 재진입·⑤[이 폴더 추가] $( [ "${BRADDFULL:-0}" -eq 1 ] && echo "신규 등록 원문(list/settings) FULL" || echo "전달 성립(종료 전이+브라우즈 종료 — 등록 강도 = no-op 원장")+종료·6. 재스캔 정착 캡처) — 육안 스탭 대기"
else
    echo "MUSIC-BROWSE-VERDICT: BROWSE-FAIL(honest-fail — 행별 사유는 16b2·단계 ${BR_STAGE:-0}, 원장)"
fi
echo "MUSIC-CAPTURES-16B2:"
for f in browse_open browse_subs browse_sub browse_up browse_sub2 browse_added browse_rescan; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
echo "=== 18g. [T3(#99)] 태그 표기 판정 (rc는 0 유지 — 원문은 16e 행별) ==="
TAG_CAP_OK=1
for f in taglist tag_filter tag_filter_artist; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] || { echo "MUSIC-FAIL(tag capture missing: $P)"; TAG_CAP_OK=0; }
done
TAG_OK=0
if [ "${TAG_STAGE:-0}" -eq 1 ] && [ "$TAG_CAP_OK" -eq 1 ] \
    && [ "${MUS_TF_TITLE:-0}" -eq 1 ] && [ "${MUS_TF_ARTIST:-0}" -eq 1 ] \
    && printf '%s' "${OR_TAG:-}" | grep -aq '"accepted":true'; then
    TAG_OK=1
fi
echo "  구성: recon=${TAG_STAGE:-0} captions=$TAG_CAP_OK settle-title=${MUS_TF_TITLE:-0} settle-artist=${MUS_TF_ARTIST:-0} open-fail=${TAG_OPENFAIL:-0}(${GS_TAG:-none} — vp-reclaim=${TAG_VPCLOSE:-0})"
if [ "$TAG_OK" -eq 1 ]; then
    echo "MUSIC-TAG-VERDICT: TAG-OK(16e — 합성 태그 트랙 3건 스캔 포함·목록 표기 캡처 3종[표기+곡명 매치+아티스트 매치]·재생 open 실패 정직 영수증) — **표기 문자열 자체(곡명(아티스트)·앨범·폴백 행)는 캡처 육안 몫(EYES — 캡처 육안 스탭)**"
else
    echo "MUSIC-TAG-VERDICT: TAG-FAIL(honest-fail — 행별 사유는 16e; recon=${TAG_STAGE:-0} captures=$TAG_CAP_OK settle-t=${MUS_TF_TITLE:-0} settle-a=${MUS_TF_ARTIST:-0} open=${OR_TAG:-none})"
fi
echo "MUSIC-CAPTURES-16E:"
for f in taglist tag_filter tag_filter_artist tag_after; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
echo "=== 18h. [#111 T5] 태그 쓰기(도구) 세그먼트 판정 (rc는 0 유지 — 원문은 16f-TW 행별) ==="
TW_CAP_OK=1
for f in tagwrite_list; do   # 필수 1종 — 태그 반영 행+썸네일(list2/flac/filter = 보조)
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] || { echo "MUSIC-FAIL(tw capture missing: $P)"; TW_CAP_OK=0; }
done
TW_OK=0
if [ "${TW_STAGE:-0}" -eq 1 ] && [ "${TW1_OK:-0}" -eq 1 ] && [ "${TW2_OK:-0}" -eq 1 ] \
    && [ "${TWF_OK:-0}" -eq 1 ] && [ "$TW_CAP_OK" -eq 1 ] \
    && printf '%s' "${AP1:-}" | grep -aq "title='${TW_TITLE_A:-<unset>}'" \
    && printf '%s' "${AP2:-}" | grep -aq "title='${TW_TITLE_B:-<unset>}'" \
    && printf '%s' "${FP:-}" | grep -aq 'TW-FLAC-BLOCKS keys=3 picture=0' \
    && [ -n "${TW_MD5_1:-}" ] && [ "${TW_MD5_1}" = "${TW_COVER_MD5:-}" ] \
    && [ -n "${TW_MD5_2:-}" ] && [ "${TW_MD5_2}" = "${TW_COVER_MD5:-}" ]; then
    TW_OK=1
fi
APIC_KEEP=0
[ "${TW_MD5_1:-}" = "${TW_MD5_2:-}" ] && [ -n "${TW_MD5_2:-}" ] && APIC_KEEP=1
echo "  구성: recon=${TW_STAGE:-0} write1=${TW1_OK:-0} write2=${TW2_OK:-0} flac=${TWF_OK:-0} captures=$TW_CAP_OK cover_rt=$([ "${TW_MD5_1:-}" = "${TW_COVER_MD5:-}" ] && [ -n "${TW_MD5_1:-}" ] && echo 1 || echo 0) apic_keep=$APIC_KEEP flac_keys=${TW_FLAC_KEYS:-none} settle_title=${MUS_TW_SETTLE:-0}"
if [ "$TW_OK" -eq 1 ]; then
    echo "MUSIC-TAGWRITE-VERDICT: TAGWRITE-OK(16f-TW — 합성 무태그 mp3+FLAC 시드·music_tag_write relay 쓰기 3콜[cover jpg 마법수·cover=null APIC 유지·FLAC 코멘트 신설]·재독 표기 원문 = 파일 수준 round-trip[title/artist/album·APIC md5 등호 2콜]+FLAC 블록 재조립 재독+목록 행 캡처 — 육안 스탭 대기)"
else
    echo "MUSIC-TAGWRITE-VERDICT: TAGWRITE-FAIL(honest-fail — 행별 사유는 16f-TW; recon=${TW_STAGE:-0} w1=${TW1_OK:-0} w2=${TW2_OK:-0} flac=${TWF_OK:-0} caps=$TW_CAP_OK)"
fi
echo "MUSIC-CAPTURES-16F:"
for f in tagwrite_list tagwrite_list2 tagwrite_flac tagwrite_filter; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
# 표기 원문 요약 스트림(JOG-RUN-VALS 수형 — 원문 수취 부재 재발 방지 봉인 —
# 라인 원장 계약: 2런 stdout을 요약 1-2행에서 정직 대조한다).
echo "TW-RUN-VALS: run=${MUS_TW_RUN:-1} stage=${TW_STAGE:-0} writes=${TW1_OK:-0}/${TW2_OK:-0}/${TWF_OK:-0} title1='${TW_TITLE_A:-}' title2='${TW_TITLE_B:-}' artist='${TW_ARTIST:-}' album='${TW_ALBUM:-}' cover_md5=${TW_COVER_MD5:-} apic_mime=${TW_MIME:-} apic_md5_1=${TW_MD5_1:-} apic_md5_2=${TW_MD5_2:-} apic_keep=$APIC_KEEP flac=${TW_FLAC_KEYS:-} settle_title=${MUS_TW_SETTLE:-} canon_wsl=$ST_PASS(2u=$ST2U·2s=$ST2S·2m=$ST2M) captures=$TW_CAP_OK"
echo "TW-RUN-CAP: run=${MUS_TW_RUN:-1} list=[$(wc -c < "$RECEIVE/mus_wsl_tagwrite_list.png" 2>/dev/null || echo 0)B] list2=[$(wc -c < "$RECEIVE/mus_wsl_tagwrite_list2.png" 2>/dev/null || echo 0)B] flac=[$(wc -c < "$RECEIVE/mus_wsl_tagwrite_flac.png" 2>/dev/null || echo 0)B] filter=[$(wc -c < "$RECEIVE/mus_wsl_tagwrite_filter.png" 2>/dev/null || echo 0)B] remnant=$(ls "$SEED_DIR"/t_tw_* 2>/dev/null | wc -l)"
echo "=== 18i. [#115 T4] opus/m4a 태그 쓰기(도구) 세그먼트 판정 (rc는 0 유지 — 원문은 16f-OM 행별) ==="
OM_CAPT_OK=1
for f in om_list om_list2 om_opus; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] || { echo "MUSIC-FAIL(om capture missing: $P)"; OM_CAPT_OK=0; }
done
OM_ALL=0
if [ "${OM_STAGE:-0}" -eq 1 ] && [ "${OM1_OK:-0}" -eq 1 ] && [ "${OM2_OK:-0}" -eq 1 ] \
    && [ "${OM3_OK:-0}" -eq 1 ] && [ "${OM4_OK:-0}" -eq 1 ] && [ "$OM_CAPT_OK" -eq 1 ] \
    && [ -n "${OM_M4A_MD5_1:-}" ] && [ "${OM_M4A_MD5_1}" = "${OM_COVER_MD5:-}" ] \
    && [ -n "${OM_M4A_MD5_2:-}" ] && [ "${OM_M4A_MD5_2}" = "${OM_COVER_MD5:-}" ] \
    && [ -n "${OM_MBP_3:-}" ] && [ "${OM_MBP_3}" = "${OM_COVER_MD5:-}" ] \
    && [ -n "${OM_MBP_4:-}" ] && [ "${OM_MBP_4}" = "${OM_COVER_MD5:-}" ] \
    && [ "${OM_VENDOR:-}" = "om-vendor" ] && [ "${OM_HT_3:-}" = "4" ]; then
    OM_ALL=1
fi
echo "  구성: stage=${OM_STAGE:-0} w1=${OM1_OK:-0} w2=${OM2_OK:-0} w3=${OM3_OK:-0} w4=${OM4_OK:-0} captures=$OM_CAPT_OK covr_1=${OM_M4A_MD5_1:-none} covr_2=${OM_M4A_MD5_2:-none} mbp_3=${OM_MBP_3:-none} mbp_4=${OM_MBP_4:-none} vendor=${OM_VENDOR:-none} htype=${OM_HT_3:-none} cover_md5=${OM_COVER_MD5:-none} remnant=$(ls "$SEED_DIR"/t_om_* 2>/dev/null | wc -l)"
if [ "$OM_ALL" -eq 1 ]; then
    echo "MUSIC-OPUSM4A-VERDICT: OPUSM4A-OK(16f-OM — 합성 m4a+opus(2x 픽스처 승계) 시드·music_tag_write relay 쓰기 4콜[m4a covr 교체+cover=null 유지·opus MBP 교체+cover 생략 유지]·재독 표기 원문 = 파일 수준 round-trip[covr/MBP md5 등호 4콜+벤더 유지+코멘트 페이지 EOS 0x04 유지+mdat 원문 무이동]+목록 행 캡처 2종 — D5 UI 무변경 = .m4a/.opus 행이 '-' 대신 실값 표기 — 육안 스탭 대기)"
else
    echo "MUSIC-OPUSM4A-VERDICT: OPUSM4A-FAIL(honest-fail — 행별 사유는 16f-OM; stage=${OM_STAGE:-0} w1=${OM1_OK:-0} w2=${OM2_OK:-0} w3=${OM3_OK:-0} w4=${OM4_OK:-0} caps=$OM_CAPT_OK)"
fi
echo "MUSIC-CAPTURES-16F-OM:"
for f in om_list om_list2 om_opus; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
# [#115 T4 — 라인 원장 계약] 표기 원문 요약 스트림(TW-RUN-VALS 수형 — 원문
# 수취 부재 재발 방지 봉인 — 2런 stdout을 요약 1-2행에서 정직 대조).
echo "#115-RUN-VALS: run=${MUS_OM_RUN:-1} stage=${OM_STAGE:-0} writes=${OM1_OK:-0}/${OM2_OK:-0}/${OM3_OK:-0}/${OM4_OK:-0} covr_1=${OM_M4A_MD5_1:-none} covr_2=${OM_M4A_MD5_2:-none} mbp_3=${OM_MBP_3:-none} mbp_4=${OM_MBP_4:-none} vendor=${OM_VENDOR:-none} cover_md5=${OM_COVER_MD5:-} mdat=${OM_MDAT_1:-none} canon_wsl=$ST_PASS(2x=$ST2X·2u=$ST2U·2s=$ST2S·2m=$ST2M·2v=$ST2V·2w=$ST2W) captures=$OM_CAPT_OK"
echo "#115-RUN-CAP: run=${MUS_OM_RUN:-1} list=[$(wc -c < "$RECEIVE/mus_wsl_om_list.png" 2>/dev/null || echo 0)B] list2=[$(wc -c < "$RECEIVE/mus_wsl_om_list2.png" 2>/dev/null || echo 0)B] opus=[$(wc -c < "$RECEIVE/mus_wsl_om_opus.png" 2>/dev/null || echo 0)B] remnant=$(ls "$SEED_DIR"/t_om_* 2>/dev/null | wc -l)"
echo "=== 18f. [T3(#97)] 캐논 등호 — 코드 무변경 라인 원문 ==="
echo "MUS-BROWSE-CANON: T1(04e11b7)→T2(8e1d84c) 등호 원문 승계 Win 669/WSL 646/posix 296 → T1 fix r1(c6be500) 2r-b/c +2 = 런 시점 Win 671/WSL $CANON_WSL_SELFTEST/posix 296(신설 어설션 0건 — 2r 원존 무변조·본 세그먼트는 probe만의 라인 — 엔진 코드 무접촉). 본 런 WSL selftest: PASS=$ST_PASS FAIL=$ST_FAIL(캐논 $CANON_WSL_SELFTEST — step 3 계보)"
if [ "$ST_PASS" -eq "$CANON_WSL_SELFTEST" ]; then
    echo "MUS-BROWSE-CANON-EQ: OK — 런 selftest PASS == 캐논(승계 등호 — 브라우저 라인 산수 변동 0)"
else
    echo "MUSIC-FAIL(browse-canon: 런 PASS=$ST_PASS != 캐논 $CANON_WSL_SELFTEST — 계보 재확인 몫)"
fi
echo "MUSIC-END"
# [T2 fix r1] ⑤ 무전달 hard 마커종결 — MUSIC-END 원문 행 뒤의 유일 rc=1 출구.
# 복귀(16b2-10 — remove 2콜+settings 바이트 등호+트리 소각)가 선행 완료된
# 뒤이므로 원복 계약은 무손상.
if [ "${BR_ADD_FAIL_HARD:-0}" -eq 1 ]; then
    echo "MUS-BROWSE-HARD-EXIT: ⑤ 무전달 재발(hard) — [이 폴더 추가] 클릭 실패 rc=1(무전달 재발 방지 원장 — T2 fix r1 계약 4항)"
    exit 1
fi
exit 0
# honest-fail 원칙: 수치·육안 대행 미달(MUSIC-FAIL 라인·DELEGATE MISS)은 원장
# 목적이라 rc=0. hard FAIL(가드·빌드·selftest·시드·settings 주입·부팅·ping·
# launch·창·캡처·복원 실패)만 exit 1.
# EOF — 끝 개행 유지.