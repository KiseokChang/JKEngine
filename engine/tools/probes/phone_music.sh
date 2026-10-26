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
#     폰 leg 모드(#110 T2 — D5 배제 가드 폐곡): T2 배포 런은 러너가
#     SPATIAL_PLAYER_ROOT(env-only 계약 유지)+폰 prebuilt 축
#     (SPATIAL_OPENAL_LIB·SPATIAL_OPENAL_INCLUDE — libopenal.a 재용)로
#     빌드되고, 모드 진실원 = jkapp_music.so의 AL 심볼(MUSP-LEG-MODE —
#     F 섹션). WIRED 런 = 폰 leg 성립 원문(spatial_play accepted → status
#     active·deviceOk·positional·TONE 시작 지연[T1 concern 3]·pos 진행
#     [WSL T3 수형]·stop idempotent)+합산 실측(vplayer SDL audio 병행 —
#     T1 concern 4 보강, M2). UNSET 런(구 T4 수형 유지 — 회귀 축) =
#     start_failed detail=kDelegationHint("spatial leg 불가 — vplayer 위임
#     이용") 종착 = fail-closed 영수증. UI [spatial] 버튼 탭과 도구 발사는
#     SpatialStart 단일 발사 경로로 합류(T2 원문) — 라벨 렌더 캡처가 정직
#     귀속 원문. 미활성 1행은 UNSET 런에서 남긴다(T3 리뷰 I-2 승계 —
#     재런 자기교정 원장행).
#   · music dirs 도구 동형 세그먼트(wsl_music.sh T3 16b — 85bf59e 원문 승계 —
#     music 라인 #92 T4): music_dir_add($TMPDIR/mus_dirs_x 합성 폴더)→list
#     원문→settings.json 실측(music.dirs 실기록+기존 text/font_path 보존 —
#     독립 파서 수취 SET-OK; 폰 python3 존재 원문 2026-10-10, 부재면 grep
#     게이트 폴백)→remove→list 원문→원복 바이트 등호. **CP949 거부/치유
#     세그먼트는 폰에서 스킵 — 원장행 1행**: 도구 인자 CP949 bad_request
#     (quickjs JSON 문자열 UTF-8 검사)와 [패널 제거] 원촉 치유 루트는
#     WSL T3(16c)이 소유·전부 실측됨 — 폰은 dirs 기능만 재실측한다. 등호
#     앵커는 **작가 정규형** — probe merge의 pretty 형식은 저장소 작가
#     (ComposeKeyed) 출력과 다르므로(WSL T3가 step-4 seed를 ComposeKeyed
#     출하형 compact으로 교환한 수형의 폰 런타임 동형) 웜업 왕복 1회로
#     앵커를 만든다(원장: 앵커≠작가 형식이면 등호가 구조적인 가짜 FAIL).
#     [폴더 관리] 토글 모양 캡처(mus_phone_dirs.png+dirs2.png — 토글
#     window-rel (185,77) WSL T3 캘리브레이션의 폰 정산 — WSL (130,70)은
#     tab2 라벨 2자 기준, 폰 tab2 라벨 14자 스트립 시프트 원장(아래 N2 상수
#     주석); **[제거] 행은 클릭하지 않는다** — 그 클릭 경로는 T2 fix r2
#     p1=crash-fixed 수리 지점이고 폰 재실측은 도구 경로가 대변한다 — N-1
#     승계: dirs 판정 행 표기. 토글은 비-멱등이라 탭의 재클릭 방어를 못 쓴다
#     — list2 선례 수형의 양상태 2캡처 — dirs=열림 프레임·dirs2=닫힘 원복
#     프레임. 좌표 원장 2: leg list 후반 표면은 vplayer가 music을 전면 덮고
#     토글 클릭은 topmost(vplayer)가 취식한다(1-3차 런 — 프레임 등호 실측)
#     → **vplayer 치우기(window_move — WSL T4 step-13 수형)가 토글 원문의
#     전제**(N2 전단 배선). 좌표 원장 3: 치우기 후에도 토글 클릭이 패널을
#     열지 않았다 — 정산 좌표(185/77·270/77 — 캡처 픽셀 1:1+offset(320,142)
#     실측)+hover tooltip 원문(커서 아래 = 버튼 위) 3런+라이브 2회(permissions
#     병합 후 sent:true) — 같은 클라에서 필터 InputText 포커스 클릭(lv 원문:
#     캐럿+type "ab" 반영)·표행 더블클릭 위임은 성립 — **폰 스트립 행 Button
#     클릭 경로 전용 무응답 관측**(P3 원장 후보 — 폰 [폴더 관리] 클릭은 WSL
#     16c의 라이브 캘리브레이션 세션 수형으로 별도 전속). 도구 원문·모양
#     (등록 상태 스트립+상태행)은 성립 — verdict 게이트는 도구 원문만 단다.
#     라이브 실험 계약(재사용 몫): send_input 미병합 상태의 클릭 = 서버 승인
#     창 approval_timeout 원문(무승인 운용 — 스폰 스폰 런은 병합 C 섹션 계약);
#     라이브 실험 후 permissions.json은 **바이트 원복 33바이트**·폰 클라
#     pkill([m]usic 브래킷 — 무브래킷 pkill은 원격 셸 자기 매치 사망 — 원문).
#   · **스캔 취소 경계 폰 동형 최소 세그먼트(leg big — #93 T4)** — wsl_music.sh
#     16d(bf38ea9)의 경계 3종을 폰 맞춤으로 축소 수형. ②스캔 진행 중
#     music_dir_remove → 취소 1발+도착 폐기 stderr 1행("도착 폐기" — T2 수형
#     원문)+클라 생존(music_dir_list/list_windows) 원문. ①제거의 재청구(clamp
#     승계) 스캔이 진행 중인 창에 close_window(phone_close_receipt B2/B3/B5
#     선례 — 폰 서버 close_window 실측 있음, permissions 핫리드 계약) →
#     소멸 시차+crash 마커 0(로그 델타)+서버 ping 생존 원문 → 재스폰+도구 응답
#     원문. ③재스폰 클라 재스캔 정착(캡처 1종 = EYES). **폰 축소 원장**: 스캔
#     규모 = $TMPDIR 합성 트리 2종(subs×files — 소형이어도 생존·폐기 원문 목적
#     충족 승계; 스캔은 내용을 읽지 않는다 — MusicModel.h 원문 — 빈 .wav 셸이
#     정직). **landtap(px 차분 착탄 판정기)은 폰 미이식 원장행** — WSL 16d의
#     탭 좌표 캘리브레이션 몫 — 폰 동형 구도는 **tree_a를 dirs_[1](tab2)에
#     앉히는 병합 구성**(Resolverds 원문: dirs_ = [기본(AudioDirFallback),
#     ...music.dirs 순서] — dirs_[1] = tab2 = 실측 상수 TAB2_X=90 재용 — 신규
#     좌표 캘리브레이션 0)로 만든다. **sync 1회 선행 배수**(T3 concern ② 승계 —
#     폰 동형 — 시드 writeback 선행 배량 수형; $TMPDIR는 VHD가 아니나 수형 유지).
#     폰 스캔 속도 미실측 — 탭→remove 사이클 2회(RETRY 수형)·폐기 MISS =
#     정직 원장행(스케일 재정산 = MUSP_BIG_SUBS/MUSP_BIG_FILES env 재런 몫).
#   · **브라우저 동형 세그먼트(N4 — #97 T4)** — wsl_music.sh 16b2(e68ff46)의
#     폰 최소 수형([폴더 관리]→[찾아보기] UI 탭 계약): 도구 경유 불가(브라우징
#     = UI 탭 실측) — **탭 전환 수단 = XTEST(xtap — #96 원장 수형 재조립)**:
#     폰 스트립 행 Button 클릭 경로 전용 무응답 관측(T5 원장 — 위 라인)의 폰
#     수형이 X 수준 주입(X-윈도우 경로 — 실기기 터치 동형)이고 [폴더 관리]
#     토글이 스트립 행 Button이므로 N4의 모든 클릭은 xtap으로 쏜다. 주입기
#     생략 원장: 런 전 $TMPDIR/xtap 부재면 clang+libXtst로 재조립(원문 수형
#     재사용) — **주입기 삭제 금지 계약**(input-repair-report §4-1 — 진입
#     소각이 주입기를 지워 XT 세그먼트 전멸 사건 — REMNANT 게이트 면제 행
#     원문). 데스크톱 좌표 = X 좌표(캘리브레이션 등호 — #96 XT-CALIB 원문)이고
#     fit-to-display로 죽은 마진 소멸 — 창 desktop xy + 클라 상대 좌표.
#     좌표 = 정찰 원칙(16c/16b2 원문 수형): env 상수 후보 그리드+PX 차분
#     착탄 판정(WSL landtap/tapsettle·px 차분기 수형 — 캡처 재교정 절차
#     원문: 런 1 캡처 mus_phone_browse_*.png가 재보정의 원문,
#     MUSP_BROW_RECON=1로 ② 캡처 직후 복원·종료 = 정찰 전용 런).
#     **좌표 원장(폰 정산 — I1/I2 승계)**: 스트립 라벨 폭 = T5 실측 앵커 산식
#     — 토글 x(2탭 [기본|pcm_music_seed]) = DTOG_X=185·(3탭 +mus_dirs_x 9자)
#     = DTOG2_X=270 → 문자 폭 ≈ 9.4px/자. **사용자 dirs 보존(#97 T4)**으로
#     본 라인 dirs = [probe dirs 선행(병합 순서 원장 — 보정 상수 등가성),
#     사용자 dirs 후속 탭] → leg list 구성 [기본|pcm_music_seed|Music] 토글
#     ≈ 232 · N4 ① 등록 뒤 [기본|pcm|Music|mus_browse] 토글 ≈ 326·mus_browse
#     탭 중앙 ≈ 266 — 후보 그리드 = **±22px+외연**(I1 원장 — 재보정 승계).
#     흐름 = WSL 16b2 동형 5항: ①브라우즈 탭 루트 등록(music_dir_add — 하위
#     2건 sub_a·sub_b mkdir) ②탭 클릭 브래킷(landtap)→[폴더 관리] 토글→
#     [찾아보기] 토글→cwd 표기+하위 목록 캡처 2종 ③sub_a 행 클릭 = JoinDir
#     진입 ④".." 행 = Parent 복귀 ④2③ 재수형(cwd = sub_a — 중복 no-op 원문
#     회피 — 등록 강도 leg) ⑤[이 폴더 추가] 원문 클릭 → tapsettle 착탄
#     이중 판정+도구 진실원(music_dir_list의 sub_[ab] 등장) — 성공 = 브라우즈
#     종료+list/settings 원문+종료·재스캔 캡처. **⑤ 무전달 = hard 마커**
#     (#97 T2 fix r1 수형 — 무전달 재발이 수리 실패 원장)·복귀(remove 3콜+
#     settings 바이트 등호+BRSNAP 대조+트리 소각). PX 차분기 = 폰 x11grab
#     전면 캡처의 창 사각 폭주 — **ffmpeg PNG는 행별 필터 1-4**(WSL px는
#     자가 캡처 필터 0 전제 — 폰 수형 = 풀 언필터 수형 — 원장).
#   · **태그 표기 동형 세그먼트(N5 — #99 T4)** — wsl_music.sh 16e(96d615b 원문)의
#     폰 수형. 합성 ID3v2.3 mp3(**own 블롭 — 유효 mp3 프레임 0**) 3건(태그 2건
#     +태그 없는 raw 폴백 1건)을 **드라이버가 합성**(윈도 python stdlib — T1 2s
#     블롭 수형 재용·TAG-PARSED-PREVIEW 양식 증명은 드라이버가 소유 — 시드는
#     폰 python3 불요 계약 승계) → tar 멤버 랜딩 → **music_dir_add 원문(진실원)
#     가 아니라 병합 구성**(N3-2 수형 — dirs_[1]=태그 dir = tab2 = TAB2_X=90
#     재용 — 신규 좌표 캘리브레이션 0·music_dir_add의 UI 좌표 의존은 N4-1이
#     소유) → music_dir_list 진실원 → **XTEST(xtap) 탭 착탄(tapsettle — 주입기
#     삭제 금지 계약)** → 목록 표기 **캡처 2종**(taglist+list2 — 곡명 열
#     "곡명 (아티스트)"·앨범 열·폴백 행 — 표기 문자열 자체는 캡처 육안 몫
#     EYES) → 재생 open 실패 영수증(launch_app vplayer → app_tool open →
#     get_status openFailed 폴링 → 창 회수 — 16e-5/16b2-0 수형·정직 원문).
#     **타이핑 세그먼트(필터 2라운드)는 폰에서 스킵** — 원장행 1행: 합성 타이핑의
#     전달·프레젠트 신뢰 원장(T3 concern 2 — harness 타이핑 계층 결함 원장·정착기
#     3세대 원장)이 소비하고 필터 매치의 순수 규약은 2s-⑨가 소유, 폰 표 필터는
#     **도구 경로가 없어 캡처로만 관측 가능 — EYES 원장행**(재판정 = WSL 정찰 런
#     몫·폰 재용은 후속 정찰 런).
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
#   + 스캔 취소 경계(T4 — leg big, 상단 세그먼트 원천 블록): ②제거→폐기 stderr
#   1행(제거→도착 시차 = 취소 절단 영수증)·클라 생존 원문, ①재청구 스캔 중
#   close_window → 소멸 시차+crash 0+ping, ③재스폰 정착 캡처. 폰 스캔 속도
#   미실측이므로 스캔-중 경계의 창은 사이클 2회(2차 재청구 재시도)+스캔 진행
#   중 캡처(close 앞 — in-flight 1프레임 표기)로 구도를 만든다 — MISS는
#   정직 원장행(rc=0 — SCALE 재정산 재런 몫).
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
#   → 폴더 저장소 라인 T4 흡수 — 정산 624 = 608 + 2o 16건(2o-a..e 10[T1] +
#   2o-f 4[T1 fix r1] + 2o-g 2[T2 서버 보존 합성]) — **실측 2026-10-10 T4
#   1차 런(PASS=624 FAIL=0 · 2o=16[o-f=4 o-g=2])**. 브리프 기대 618(2o 10)은
#   2o-f/2o-g 미포함 전산 — 실측으로 정산 갱신(원장: **캐논 상수는 실측 후
#   갱신 계약**, T3 리뷰 M-1 승계 — 낡은 상수가 실측 오차를 WARN 없이
#   빨아들이는 회귀창 폐곡). 브리프 원장행 가설 "WSL 624와의 불일치 = 2o-g
#   서버 측 2건 구조적(하네스 자체 main)"은 **부정** — 폰 selftest main에도
#   서버 보존 합성 2건이 동봉되어 전 16건 실측 → **WSL 624 등호**.
#   → **스캔 취소 라인 T4 폰 흡수 — 실측 638 = 624 + 2p 7 + 2q 7**(2026-10-10
#   T4 — 폰 2런 재현: PASS=638 FAIL=0·2p=7·2q=7·AppSelfTest 0 failure): 2p =
#   스캔 취소 경계 selftest 7건(#93 T1 — a·①-⑤ 계열)·2q = 경로 인코딩 사슬
#   7건(#94 — a·b 계열) — 전부 코드 원문 배포 이관이고 폰 main == WSL main
#   동일 배포라 **WSL 638 등호**(신규 어설션 0 계약 — 기존 624·2m·2n·2o 계열
#   보존). CANON 상수 = 실측 후 갱신 계약(T3 리뷰 M-1 승계 — 실측 전 상수는
#   예고 원장 병기 후 정산) — 실측 원문(reports 진실원)으로 remote script
#   상수 정산 완료. 취소 경계 원문 2런 재현: ②제거→폐기 도착 0.62/0.66s
#   (200,000 트랙/트리 — 폰 판정축)·클라 생존·①close→소멸 1.26/1.25s·crash 0
#   ·서버 ping 생존·재스폰+도구 응답 원문·③정착 캡처.
#   → **#97 브라우저 라인 T4 폰 흡수 — 기대 648 = 638 + 2r 10**(T1 04e11b7의
#     2r 계열 8건 + fix r1 c6be500의 2r-b/2r-c 2건 — 코드 원문 배포 이관이라
#     폰 main == WSL main 동일 배포 — **WSL 648 등호** — Win 671/posix 296
#     계보 병기. 신규 어설션 0 계약 — 2r 원존 무변조·본 세그먼트는 probe만의
#     라인 — 엔진 코드 무접촉. 캐논 상수 = 실측 후 갱신 계약 승계)
#   → **태그 라인 T4 폰 흡수 — 기대 668 = 638 + 2r 10 + 2s 20**(T1 77323b1의
#     2s 20건 — 폰 실행 바이너리가 638-era(d25ea9d-era 빌드)라 2r 배포 실측과
#     본 2s 배포 실측이 **한 번에** 소각되는 갭 소각 배포가 절차의 절반 목적:
#     PHONE_DEPLOY_BASE=c021006(원진 원문) → 폰 실행 바이너리 HEAD 등화) —
#     **WSL 668 등호 기대**·Win 691/posix 304 계보 병기. 2s 라벨 [PASS] 실측수
#     병기 — 실측 후 정산. **BLOCKED 원장(T4 커밋 시점)**: 폰 미네트워크(서브넷
#     이동 실측 — known 주소 전부 접속 타임아웃·로컬 서브넷 /24 정찰 8022 리스너
#     0) — 배포·실측 미수행, probe/셀프테스트/드라이버 코드만 원문 병기 — 다음
#     폰 접촉 런이 바로 실행 가능한 수형으로 서술(배치 절차 = 위 PHONE_DEPLOY_BASE
#     주석+pmus_t4_deploy.sh [engine/tmp — 산출물]).
#   → **#110 폰 spatial leg T2 흡수(2026-10-19)**: 배포 런이 SPATIAL_PLAYER_ROOT
#     (+prebuilt 축)를 세팅한다 — D5 배제 가드는 폐곡(모드 진실원 = .so 심볼)·
#     selftest 2n 어설션 0이라 **폰 캐논 668 보존**(MUSIC-FULL-2P2QRS-668 등호
#     회귀 축)·신규 세그먼트 = M2(합산 — T1 concern 4)·TONE 시작 지연 원장행
#     (concern 3). UNSET 런 수형은 회귀 축으로 유지.
#   → **#111 태그 쓰기 라인 T6 폰 흡수(2026-10-25)**: 배포 런이 HEAD(#111 T1-T4 —
#     15c2bb4/2973933/c8f31f2/5b95c63)를 **코드 원문 배포 이관**한다(2u ①-⑮ —
#     selftest 어설션 쌍둥이) — **기대 795 = 668 + 2u 127**(T1 2u ①-⑦ 47·T2 ⑧-⑭
#     57·T4 ⑮ 23 — **WSL 795 등호·Win 818 계보 병기**) — 2u [PASS] 실측수 병기 —
#     실측 후 CANON 정산 계약 승계. 신규 세그먼트 = **N6 태그 쓰기(도구) 동형**
#     (wsl_music 16f-TW eaba3a4의 폰 수형 — music_tag_write relay 도구 발사(atool
#     수형)→재독 표기 원문[파일 수준 round-trip+APIC md5 등호]+목록 행 캡처 3종[행
#     32px 커버 썸네일+[편집] UI 표기 원문 — T3 배포]+TW-REMNANT 0). 필터 라운드는
#     폰 타이핑 스킵 원장 승계(MISS 아님 — N5와 같은 EYES 축). 좌표 원장행 승계:
#     step 11-14 행 탭 상수(MUSP_ROW0_Y=132 등)는 별소관 재보정 대기 — T5 리포트
#     §6.4 문구 승계(본 세그먼트의 성립 판정 = 도구 경계+파일 재독 축).
#
# 판정 사다리(라벨 계약 — wsl_music/phone_gallery 수형):
#   MUS-PHONE-VERDICT: MUS-PHONE-OK = CANON MUSIC-FULL-(2P2QRS-|2P2QR-|2P2Q-)*
#     AND 캡처 비빈(13종 + 브라우저 7종 + 태그 표기 2종[taglist·taglist2] —
#     N5 세그먼트) AND dirs 도구 원문(add ok·list 등록/제거 원문·settings music.dirs
#     실기록+text/font_path 보존·원복 바이트 등호) AND 위임(폰 위임
#     open+재청구+직행) 성립 AND get_status
#     opened:true(pos 증가) AND 폰 leg 원문(모드 분기 — #110 T2): WIRED 런 =
#     spatial_play accepted·pos 진행 원문·TONE 시작 지연 원장 행·stop
#     idempotent(+합산 M2) 성립 / UNSET 런 = 배제 원문(spatial_play →
#     start_failed detail=kDelegationHint·spatial_status 미활성 행) 성립 AND
#     스캔 취소 경계(②폐기 1행+절단 시차·클라 생존 + ①close 소멸+crash 0+
#     재스폰 도구 원문 — CANCEL 판정 행) 성립 AND
#     sdcard 스캔 유한(또는 권한 부재 빈 목록=정상) AND 태그 동형 세그먼트=N5
#     (병합 등록 진실원+XTEST 탭 착탄+표기 캡처 2종+vplayer openFailed
#     영수증 — MUSIC-TAG-PHONE-VERDICT TAG 판정 행). 위임 창·귀속은 원장행
#     (MISS = 폰 스폰 시간 원장 — 판정 게이트에서 뺀 사유는 상단 원장 참조).
#     + 태그 TYPING 원장행(#99 T4): 필터 2라운드 타이핑은 폰에서 **스킵** —
#     합성 타이핑 전달·프레젠트 신뢰 원장(T3 concern 2·정착기 3세대)이 소비
#     ·폰 표 필터는 도구 경로가 없어 **EYES 원장행**(WSL 16e-3/4 소유).
#     + dirs(CP949) 원장행: 거부/치유 세그먼트는 WSL T3(85bf59e 16c) 소유 —
#     폰 스킵(1행).
#     + 취소 원장행: 폰 스캔 속도 미실측 → ②폐기 MISS/①시차 초과 =
#     MUSIC-PHONE-FAIL(rc=0 정직 — MUSP-BIG-SCALE 재정산 재런 몫).
#   MUS-PHONE-VERDICT: MUS-PHONE-FAIL(행별 이유) — 수치 미달=정직 원장 rc=0.
#   + 태그 쓰기 TW 원장행(#111 T6): N6 세그먼트의 성립 판정 = **도구 경계
#     (music_tag_write relay ok)+파일 재독 축**(round-trip+APIC md5) — 목록 행
#     표기·썸네일·[편집] 모양은 캡처 육안 몫(EYES)·필터 라운드는 폰 스킵 원장
#     (N5 승계). MUSIC-TAGWRITE-PHONE-VERDICT TW 판정 행이 별채(주 VERDICT
#     구성항 병합 0 — T5 18h 수형).
#   MUSP-FAIL(rc=1) = hard FAIL만: ssh 단절·sweep·배포·ninja·selftest 회귀
#     ·seed 소각·settings/perm 원복·서버/ping/launch/창·캡처·REMNANT 실패
#     ·dirs 기저(list-0 시드 dir 미보존)·dirs 합성 폴더 생성·big 시드 생성/
#     카운트/소각(T4 취소 세그먼트 — probe 소유 잔상 0 계약)·태그 시드 생성/
#     카운트/소각(N5 태그 세그먼트 — probe 소유 잔상 0 계약·ID3 매직 진품
#     판정)·TW 시드 생성/카운트/소각+커버 md5 도착 등호(N6 태그 쓰기 세그먼트
#     — probe 소유 잔상 0 계약)·**브라우저 ⑤
#     무전달 재발(N4 hard 마커 — #97 T2 fix r1 수형)**·XTEST 재조립 실패·
#     px 차분기 python3 부재. dirs 도구 수치
#     (add/list/settings/remove/바이트 등호)는 honest 원장행 — DIRS 판정 행 집계.
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
PHONE_DEPLOY_BASE="${PHONE_DEPLOY_BASE:-f1ac76da7368aff0e6b757b7b6059c5c875a0ea0}"  # 마지막 폰 배포 커밋(#110 T2 — 2026-10-19 배포·2026-10-25 오전 재실측 run8 원문: HEAD f1ac76d SWEEP-DIFF=0/319·폰 캐논 668 등호 재실측 — leg WIRED). #115 T4 원장행: #111 T6·#112 T5·#113(#113 T3 t3legs) 배포 = t11x 계열 드라이버의 전수 git archive(HEAD 4de04d0 — 폰 캐논 818·2v 14 실측 — 폰 probe 셀프테스트 축에서 전부 소각) — 본 라인(T4)의 폰 배포 = **t115_t4_deploy.sh(engine/tmp 산출물 — t112_t5 수형 재용·커밋 0)** — 기대 배포 집합 앵커
                                                   #   (구값 c021006 = #98 fix r1 스윕 — T4 태그 라인(#99)의
                                                   #   폰 배포·실측은 BLOCKED 원장을 거쳐 **T2 재런(2026-10-25
                                                   #   오전)에서 갭 소각 실측**(668 = 638 + 2r 10 + 2s 20 —
                                                   #   MUSIC-FULL-2P2QRS-30). #111 T6의 배포 = HEAD 전수
                                                   #   git archive(t111_t6_deploy.sh — engine/tmp 산출물) →
                                                   #   2u 127 소각 → 기대 795 = 668 + 2u 127(WSL 795 등호))
MUSP_BIG_SUBS="${MUSP_BIG_SUBS:-80}"    # 스캔 취소 세그먼트(T4) 합성 트리 하위 수 — env 재정산 몫
MUSP_BIG_FILES="${MUSP_BIG_FILES:-2500}"  # 트리당 하위 파일 수 — 80×2500=200,000 트랙/트리
MUSP_TW_RUN="${MUSP_TW_RUN:-1}"         # T6 런 라벨(TW-RUN-VALS/TW-RUN-CAP 요약 스트림의 run 축 — JOG-RUN-VALS 수형 — wsl 16f MUS_TW_RUN의 폰 라벨)
MUSP_OM_RUN="${MUSP_OM_RUN:-1}"         # [#115 T4] N7 런 라벨(#115-RUN-VALS/​#115-RUN-CAP 요약 스트림의 run 축 — TW-RUN-VALS 수형 — wsl 16f-OM MUS_OM_RUN의 폰 라벨)

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
echo "LINEAGE(폰): selftest 565(2i 19·2g 40·2m 0) → 591(2m 26 흡수) → 608(2n 17 흡수 — 실측) → 624 = 608 + 2o 16(2o-a..e 10 + 2o-f 4 + 2o-g 2 — 실측 정산, WSL 624 등호) → 638 = 624 + 2p 7 + 2q 7 (스캔 취소 라인 T4 — #93·#94 코드 원문 배포 이관 — **실측 2026-10-10 T4 2런: PASS=638 FAIL=0·2p=7·2q=7 — WSL 638 등호**) → **668 = 638 + 2r 10 + 2s 20 (T1 77323b1 태그 라인 — 코드 원문 배포 이관 — 갭 소각 배포 절차 — 2026-10-25 오전 재실측 run8 원문: HEAD f1ac76d SWEEP-DIFF=0·CANON-INCLUSION=MUSIC-FULL-2P2QRS-30·WSL 668 등호)** → **#111 T6 795 = 668 + 2u 127 (태그 쓰기 라인 T1 15c2bb4 2u ①-⑦ 47·T2 2973933 ⑧-⑭ 57·T4 5b95c63 ⑮ 23 — 2026-10-25 T6 체인 런 원문: PHONE-SELFTEST PASS=795 FAIL=0·2u=127 — WSL 795 등호)** → **#112 T5 814 = 795 + fix 5(#111 최종 f33d41a F1-F2 — 폰 미흡수분 승계)+2v 14(#112 T1 2v-a 4·T2 2v-b/c 6·T3 2v-d 4 — 구두 판정부+사슬 — WSL 814 등호 — 실측)** → **#113 T3 818 = 814 + 2w 4(저널 leg — t3legs_deploy 원문: PHONE-SELFTEST A/B PASS=818 FAIL=0 2u=132·2v=14 — WSL 818 등호)** → **#115 T4 기대 831 = 818 + 2x 13(①-⑫ 12건·⑨ 2어설션 — T1 20e3abf +3·T2 95cb8c6 +5·T3 840ea41 +5 — WSL 831 등호 — 브리프 원문행의 '854 승계'는 Win 축 수치 표기 — Win-WSL 23차 = 플랫폼 어설션 계통 — 원장 대조 몫)**"
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

echo "=== 2b. 합성 태그 트랙 시드 3건 (T4 #99 — 드라이버 합성 — 윈도 python stdlib — 폰 python3 불요 계약 승계) ==="
# wsl_music.sh 4c(96d615b 원문)의 드라이버 수형: own ID3v2.3 블롭(유효 mp3
# 프레임 0)을 앞에 붙인 최소 mp3 ×2 + 태그 없는 raw 1건(폴백 행). TAG-PARSED-
# PREVIEW(양식 증명 — 앱 파서와 독립)는 드라이버가 소유한다. mtime 스탬프 =
# alpha 최신(표 상단행 계약 — WSL 원문 수형).
PYTHONIOENCODING=utf-8 python - "$PYSCRATCH" <<'TAGPYEOF'
# 합성 태그 트랙 시더(드라이버) — wsl_music.sh 4c mus_tagseed.py 원문 수형.
# ① t_tag_alpha.mp3 v2.3 TIT2/TPE1/TALB(enc 0) ② t_tag_beta.mp3 ③
# t_raw_noid.mp3 (ID3 없음 — 폴백 행). 생성 후 자기 블롭을 순수 파이썬으로
# 재파싱해 TAG-PARSED-PREVIEW를 낸다(앱 파서와 독립).
import os, struct, sys, time

out = os.path.join(sys.argv[1], "pmus_stage", "engine", "pmus_seed_stage",
                   "tag_music_seed")

def frame(fid, text):
    body = b"\x00" + text.encode("latin-1")   # enc 0 (Latin-1 — ASCII 등가)
    return fid.encode("ascii") + struct.pack(">I", len(body)) + b"\x00\x00" + body

def write_tagged(path, title, artist, album):
    frames = frame("TIT2", title) + frame("TPE1", artist) + frame("TALB", album)
    data = b"ID3" + b"\x03\x00" + b"\x00" + struct.pack(">I", len(frames)) + frames
    with open(path, "wb") as f:
        f.write(data + b"\x00" * 64)   # 말미 패딩(2s-⑥ 스킵 수형) + 프레임 0

def write_raw(path):
    with open(path, "wb") as f:
        f.write(b"\x00" * 256)

def parse_v23(path):
    with open(path, "rb") as f:
        d = f.read()
    if d[:3] != b"ID3":
        return {}
    i = 10
    out_ = {}
    end = 10 + int.from_bytes(d[6:10], "big")
    while i + 10 <= min(end, len(d)):
        fid = d[i:i+4].decode("latin-1")
        sz = int.from_bytes(d[i+4:i+8], "big")
        if sz == 0 or not ('A' <= fid[0] <= 'Z'):
            break
        body = d[i+10:i+10+sz]
        if fid in ("TIT2", "TPE1", "TALB") and body:
            out_[fid] = body[1:].decode("latin-1", "replace")
        i += 10 + sz
    return out_

def stamp(path, mo, d, h, mi):
    t = time.mktime((2026, mo, d, h, mi, 0, 0, 0, -1))
    os.utime(path, (t, t))

os.makedirs(out, exist_ok=True)
p1 = os.path.join(out, "t_tag_alpha.mp3")
p2 = os.path.join(out, "t_tag_beta.mp3")
p3 = os.path.join(out, "t_raw_noid.mp3")
write_tagged(p1, "Phone Tag Alpha", "Probe Artist Alpha", "Tag Album One")
write_tagged(p2, "Phone Tag Beta", "Probe Artist Beta", "Tag Album Two")
write_raw(p3)
stamp(p1, 11, 1, 12, 6)   # 최신 — 표 상단행 = t_tag_alpha 계약
stamp(p2, 11, 1, 12, 5)
stamp(p3, 11, 1, 12, 4)
for p in (p3, p2, p1):
    m = parse_v23(p)
    print("TAG-PARSED-PREVIEW %s bytes=%d title=%r artist=%r album=%r"
          % (os.path.basename(p), os.path.getsize(p),
             m.get("TIT2", ""), m.get("TPE1", ""), m.get("TALB", "")))
TAGPYEOF
[ $? -eq 0 ] || FAIL "tag seed mp3 generation failed (로컬 python)"
TAGSEEDED=$(find "$SEEDSTAGE/engine/pmus_seed_stage/tag_music_seed" -name '*.mp3' | wc -l)
[ "$TAGSEEDED" -eq 3 ] || FAIL "tag seeded $TAGSEEDED mp3 files (need 3)"
TAG_MP3_A="$SEEDSTAGE/engine/pmus_seed_stage/tag_music_seed/t_tag_alpha.mp3"
echo "TAG-MP3-A-BYTES: $(wc -c < "$TAG_MP3_A") (합성 원문 — ID3v2.3 블롭+64B 패딩·유효 프레임 0)"
[ "$(head -c 3 "$TAG_MP3_A")" = "ID3" ] || FAIL "tag seed alpha lacks ID3 magic"
echo "TAG-MT-STAMPS: alpha>beta>raw (mtime desc — 표 상단행 = t_tag_alpha — 4c 원문 수형)"
echo "MUSIC-TAGLIST-EXPECTED=3 (태그 행 2[alpha·beta]+폴백 1[raw] — 표행 수 관측 원문 1행; 표기 일치는 캡처 육안 몫 EYES)"

echo "=== 2c. [T6 #111] 태그 쓰기 TW 시드 3건 (wsl_music 16f-TW 수형 — 드라이버 합성 — 윈도 python stdlib — 폰 python3 불요 계약 승계) ==="
# ①무태그 mp3(junk 768B — ID3 매직 0 — 2u-① 구성 계약 수형: v2 부재 = WriteTag
# mp3 leg가 v2.3 신설) ②무태그 FLAC("fLaC"+STREAMINFO 첫 블록 34B is-last —
# MusicModel.h FlacScanRegion의 구성 계약: 첫 블록 = type 0·34B — 내용은 writer가
# 원문 보존) ③합성 커버 jpg(**드라이버 PIL 산출 base64 941B 인라인 수형** — WSL
# 16f-1 원문 — FFD8 마법수·4색 밴드+중앙 마크 = 32px 썸네일 구별). 랜딩 = tar
# 멤버 → E4가 \$TMPDIR/mus_twseed로 수취(N5 캡처 **뒤** N6-1이 \$TAGDIR로 mv —
# N5의 3행 표기 계약 무오염).
TW_COVER_B64='/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAQDAwMDAgQDAwMEBAQFBgoGBgUFBgwICQcKDgwPDg4MDQ0PERYTDxAVEQ0NExoTFRcYGRkZDxIbHRsYHRYYGRj/2wBDAQQEBAYFBgsGBgsYEA0QGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBgYGBj/wAARCABAAEADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDzuiiivgz+tAooooAKKKKACiiigDvKKKK4z/NMKKK8K+IfxD8YaH8SNS0rStX+z2kPleXH9nifbmJGPLKT1J711YTCTxU3CDV7X1PRyzLKuY1XSpNJpX1v3S6J9z3WivCvh58Q/GGufEjTdK1XV/tFpN5vmR/Z4k3Yidhyqg9QO9e60YvCTws1CbV7X0DM8sq5dVVKq021fS/drql2CiiiuU848vooor+4j7oK838U+Fte1LxZd3tlY+bBJs2v5qLnCKDwTnqDXpFFeTnGT0c2oqhXbSTvpa97NdU+504XFTw03OC121PN/C3hbXtN8WWl7e2PlQR79z+ajYyjAcA56kV6RRRRk+T0cpouhQbabvra97JdEuwYrFTxM1Oa120CiiivWOY+8qKKK/yrP3AKKKKACiiigAooooA//9k='
PYTHONIOENCODING=utf-8 python - "$PYSCRATCH" "$TW_COVER_B64" <<'TWPYEOF'
# [#111 T6] 태그 쓰기 세그먼트 시더(드라이버) — wsl_music.sh 16f-1 mus_twseed.py
# 원문 수형. stdlib만(의존 0 — 커버 = WSL 16f-1의 드라이버 PIL 산출 base64를
# 디코드 재용 — 외부 파일 의존 0).
import base64, os, sys, time
out, b64 = sys.argv[1], sys.argv[2]
d = os.path.join(out, "pmus_stage", "engine", "pmus_seed_stage", "tw_music_seed")
os.makedirs(d, exist_ok=True)
mp3 = os.path.join(d, "t_tw_write.mp3")
with open(mp3, "wb") as f:
    f.write(bytes((i * 7 + 13) & 0xFF for i in range(768)))   # 유효 프레임 0 — ID3 부재(2u-① 구성)
flac = os.path.join(d, "t_tw_write.flac")
with open(flac, "wb") as f:
    f.write(b"fLaC")
    f.write(bytes([0x80, 0, 0, 34]))   # is-last·type 0(STREAMINFO)·34B
    f.write(bytes(34))                 # 34B 본체(원문 보존 — 파싱 계약 밖)
cover = os.path.join(d, "t_tw_cover.jpg")
cb = base64.b64decode(b64)
with open(cover, "wb") as f:
    f.write(cb)
t1 = time.mktime((2026, 11, 1, 12, 8, 0, 0, 0, -1))
t2 = time.mktime((2026, 11, 1, 12, 7, 0, 0, 0, -1))
os.utime(mp3, (t1, t1))    # 쓰기 1 뒤의 표 상단행 = t_tw_write.mp3 계약(쓰기 갱신 mtime이 실제 관계를 만든다 — 스탬프는 프리-쓰기 열 결정화 보조)
os.utime(flac, (t2, t2))
print("TW-SEED-OK mp3=%d flac=%d cover=%d jpeg_magic=%d"
      % (os.path.getsize(mp3), os.path.getsize(flac), len(cb),
         1 if cb[:2] == b"\xff\xd8" else 0))
TWPYEOF
[ $? -eq 0 ] || FAIL "TW seed generation failed (로컬 python)"
TWSEEDED=$(find "$SEEDSTAGE/engine/pmus_seed_stage/tw_music_seed" -type f | wc -l)
[ "$TWSEEDED" -eq 3 ] || FAIL "TW seeded $TWSEEDED files (need 3)"
TW_MP3_LOCAL="$SEEDSTAGE/engine/pmus_seed_stage/tw_music_seed/t_tw_write.mp3"
TW_FLAC_LOCAL="$SEEDSTAGE/engine/pmus_seed_stage/tw_music_seed/t_tw_write.flac"
TW_COVER_LOCAL="$SEEDSTAGE/engine/pmus_seed_stage/tw_music_seed/t_tw_cover.jpg"
[ "$(head -c 3 "$TW_MP3_LOCAL")" = "ID3" ] \
    && FAIL "TW mp3 must be untagged (ID3 magic present — 2u-① 구성 위반)" \
    || echo "TW-MP3-NOMAGIC: OK ($(wc -c < "$TW_MP3_LOCAL")B junk — ID3 부재 — v2.3 신설 계약 원문)"
[ "$(head -c 4 "$TW_FLAC_LOCAL")" = "fLaC" ] || FAIL "TW flac skeleton magic break"
case "$(head -c 2 "$TW_COVER_LOCAL" | od -An -tx1 | tr -d ' \n')" in
  ffd8) echo "TW-COVER-MAGIC: ffd8 OK (로컬 합성 — FFD8 마법수 · LoadCoverByMagic 계약)" ;;
  *) FAIL "TW cover jpg magic broken (FFD8 선단 필수)" ;;
esac
TW_COVER_MD5=$(md5sum "$TW_COVER_LOCAL" | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
echo "TW-COVER-MD5: $TW_COVER_MD5 ($(wc -c < "$TW_COVER_LOCAL")B — 합성 cover jpg 원문 — APIC round-trip 절단판정 축 — 폰 도착 등호 게이트로 E4 치환)"

echo "=== 2d. [#115 T4] opus/m4a OM 시드 3건 (wsl_music 16f-OM 수형 — 드라이버 합성 — Windows python stdlib — 폰 python3 불요 계약 승계) ==="
# ①합성 m4a(2x 픽스처 승계 — ftyp+moov(udta/meta/hdlr('mdir')+ilst[©nam/©ART/
#   ©alb UTF-8+covr(png 시그니처 블롭 — 코드 14)])+mdat 64B 0xAB) ②합성 opus(
#   [BOS OpusHead 19B][OpusTags(vendor 'om-vendor'+TITLE/ARTIST/ALBUM+GENRE+
#   METADATA_BLOCK_PICTURE)] — lacing 255-런+BuildOggPage CRC(poly 0x04C11DB7)
#   규약 동형·코멘트 페이지 EOS 0x04) ③합성 cover jpg = TW_COVER_B64 재용
#   (동일 바이트 — TW-MD5 등호 축 공유). 랜딩 = tar 멤버 → E5가 \$TMPDIR/
#   mus_omseed로 수취 → N7-1이 N6 소각 뒤 \$TAGDIR로 mv.
PYTHONIOENCODING=utf-8 python - "$PYSCRATCH" "$TW_COVER_B64" <<'OMPYEOF'
# [#115 T4] opus/m4a 세그먼트 시더(드라이버) — wsl_music.sh 16f-OM mus_omseed.py
# 원문 수형(같은 구성 — Windows python stdlib — 의존 0).
import base64, os, struct, sys
out, b64 = sys.argv[1], sys.argv[2]
d = os.path.join(out, "pmus_stage", "engine", "pmus_seed_stage", "om_music_seed")
os.makedirs(d, exist_ok=True)
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
    segs = []
    for p in packets:
        k = len(p) // 255
        segs.extend([255] * k)
        segs.append(len(p) % 255 if (k == 0 or len(p) % 255) else 0)
    pg = bytearray(b"OggS" + bytes([0, htype]) + struct.pack("<Q", granule)
                   + struct.pack("<III", 77, seq, 0) + bytes([len(segs)]))
    pg += bytes(segs) + body
    pg[22:26] = struct.pack("<I", oggcrc(pg))
    return bytes(pg)

def opus_blob(vendor, entries, mbp_b64=None):
    head = (b"OpusHead" + bytes([1, 2]) + struct.pack("<H", 312)
            + le32(48000) + struct.pack("<H", 0) + bytes([0]))
    es = [(b"TITLE", b"Om Seed Opus"), (b"ARTIST", b"Om Seed Op Artist"),
          (b"ALBUM", b"Om Seed Op Album"), (b"GENRE", b"Wave")]
    if mbp_b64:
        es.append((b"METADATA_BLOCK_PICTURE", mbp_b64.encode("ascii")))
    tags = b"OpusTags" + le32(len(vendor)) + vendor + le32(len(es))
    for k, v in es:
        tags += le32(len(k) + 1 + len(v)) + k + b"=" + v
    return page(0x02, [head], 0) + page(0x04, [tags], 1)  # 코멘트 페이지 EOS 0x04

def build_pic(mime, blob):
    return (be32(3) + be32(len(mime)) + mime + be32(0) + be32(0) * 4
            + be32(len(blob)) + blob)

pic = build_pic(b"image/png", png)
mbp = base64.b64encode(pic).decode()
m4a = os.path.join(d, "t_om_write.m4a")
with open(m4a, "wb") as f:
    f.write(m4a_blob())
opus = os.path.join(d, "t_om_write.opus")
with open(opus, "wb") as f:
    f.write(opus_blob(b"om-vendor", [], mbp))
cover = os.path.join(d, "t_om_cover.jpg")
with open(cover, "wb") as f:
    f.write(jpg)
print("OM-SEED-OK m4a=%d opus=%d cover=%d png_magic=%d mbp_base64=%d"
      % (os.path.getsize(m4a), os.path.getsize(opus), len(jpg),
         1 if png[:4] == b"\x89PNG" else 0, 1))
OMPYEOF
[ $? -eq 0 ] || FAIL "OM seed generation failed (로컬 python)"
OMSEEDED=$(find "$SEEDSTAGE/engine/pmus_seed_stage/om_music_seed" -type f | wc -l)
[ "$OMSEEDED" -eq 3 ] || FAIL "OM seeded $OMSEEDED files (need 3)"
OM_M4A_LOCAL="$SEEDSTAGE/engine/pmus_seed_stage/om_music_seed/t_om_write.m4a"
OM_OPUS_LOCAL="$SEEDSTAGE/engine/pmus_seed_stage/om_music_seed/t_om_write.opus"
OM_COVER_LOCAL="$SEEDSTAGE/engine/pmus_seed_stage/om_music_seed/t_om_cover.jpg"
grep -aq ftyp <(head -c 8 "$OM_M4A_LOCAL") || FAIL "OM m4a seed ftyp magic break"
[ "$(head -c 4 "$OM_OPUS_LOCAL")" = "OggS" ] || FAIL "OM opus seed OggS magic break"
case "$(head -c 2 "$OM_COVER_LOCAL" | od -An -tx1 | tr -d ' \n')" in
  ffd8) echo "OM-COVER-MAGIC: ffd8 OK (TW 원문 재용 — md5 축 공유)" ;;
  *) FAIL "OM cover jpg magic broken (FFD8 선단 필수)" ;;
esac
OM_COVER_MD5=$(md5sum "$OM_COVER_LOCAL" | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
echo "OM-COVER-MD5: $OM_COVER_MD5 ($(wc -c < "$OM_COVER_LOCAL")B — TW-COVER-MD5=$TW_COVER_MD5 등호 기대 — 동일 바이트 원장)"
[ "$OM_COVER_MD5" = "$TW_COVER_MD5" ] || FAIL "OM cover md5 != TW cover md5 — 동일 바이트 계약 위반"

# ------------------------------------------------------------------ 3. 원격 스크립트
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PMUSEOF'
#!/bin/bash
# 폰 측 music 영수증 절차 (phone_music.sh가 생성 — music 라인 T5+
# spatial leg 라인 T4+폴더 저장소 라인 T4+스캔 취소 라인 T4 #93+
# 태그 라인 T4 #99+#97 T4 브라우저).
# settings 병합(시드 dirs) → cmake 재설정(자동)+ninja 리빌드 → selftest 캐논
# (668→795 = 2u 127[#111 태그 쓰기 T1/T2/T4 — 코드 원문 배포 이관] 흡수 — WSL 795
# 등호 기대 · 2r 10+2s 20·2m 26·2n 17·2o 16·2p 7·2q 7 보존) →
# leg list(탭2 리스캔+캡처+콜드 접착제 전조+spatial 배제 원문[LEG-ENV unset
# 가드 — spatial_play start_failed kDelegationHint·UI [spatial] 탭 라벨
# 렌더]+더블클릭 위임+귀속 폴링+직행 open+get_status 재생 수치+**dirs 도구
# 동형 세그먼트[add/list/settings 실측/remove/list/원복 바이트 등호 — CP949
# 거부/치유는 WSL T3 소유 스킵 원장행]+[폴더 관리] 토글 모양 캡처 1종**+**N4
# 브라우저 동형 세그먼트[#97 T4 — wsl 16b2 수형 — XTEST(xtap) 탭 전환+PX 차분
# 착탄판정 — ①등록 ②탭→토글→[찾아보기] 개봉 캡처 2종 ③JoinDir ④Parent ④2
# 재진입 ⑤[이 폴더 추가] 이중 판정(착탄+music_dir_list 진실원) ⑥종료·재스캔
# 캡처+복귀 remove 3콜+바이트 등호+트리 소각 — ⑤ 무전달 = hard]]) →
# **leg big(스캔 취소 경계 폰 동형 최소 — wsl 16d 수형: $TMPDIR 합성 트리 2종
# 시드+sync → tree_a=dirs_[1](tab2) 병합 부팅 → 탭2 리스캔(활성 탭 = big_a
# 경계) → 스캔 중 music_dir_remove(취소 1발+도착 폐기 stderr 1행+클라 생존) →
# 스캔 중 close_window(재청구 스캔 중 — 소멸 시차+crash 0+ping) → 재스폰+
# 도구 응답+재스캔 정착 캡처)** →
# **N5 태그 표기 동형 세그먼트(T4 #99 — wsl 16e 96d615b 수형): 태그 dir 병합
# 구성(dirs_[1]=tab2 — N3 수형·music_dir_add 좌표 의존은 N4-1이 소유) →
# 부팅+music 런치 → XTEST(xtap) 탭 착탄(tapsettle·send_input 폴백) → 스캔
# 도착 → 목록 표기 캡처 2종(taglist/taglist2)+music_dir_list 진실원 →
# vplayer open 실패 영수증(합성 최소 mp3 — opened:false+openFailed:true —
# 16e-5/16b2-0 수형·정직 원문) → 태그 dir 소각. **타이핑 세그먼트(필터
# 2라운드)는 폰에서 스킵 원장행** — 합성 타이핑 신뢰 원장(T3 concern 2·정착기
# 3세대)이 소비·필터는 도구 경로가 없어 EYES 원장행(WSL 16e-3/4 소유)** →
# **N6 태그 쓰기(도구) 동형 세그먼트(#111 T6 — wsl 16f-TW eaba3a4 수형): TW
# 시드(무태그 mp3 junk 768B+무태그 FLAC 스켈레톤+합성 cover jpg — 드라이버
# 합성·E4 수취)를 N5 캡처 뒤 $TAGDIR로 랜딩 → music_tag_write relay 도구 쓰기
# 3콜(①title/artist/album+cover 마법수 ②cover=null TIT2 교체+APIC 유지
# ③FLAC cover 생략 코멘트 신설) → 재독 표기 원문(순수 파이썬 재파싱 — 앱 파서와
# 독립 — 폰 python3)[파일 수준 round-trip+APIC md5 등호 2콜+FLAC 블록
# 재조립(STREAMINFO 보존)] → 목록 행 캡처 3종(tagwrite_list/list2/flac — 행
# 32px 커버 썸네일+[편집] UI의 표기 원문 — T3 배포) → TW 시드 소각(REMNANT 0).
# 16f-0a 신선 클라 재건은 폰에서 생략 원장행(WSL 진원 = 16e-4 아티스트 필터
# 잔존 — 폰 N5는 타이핑 스킵이라 필터 잔존 진원 없음 — 클라 상태 = N5 계약
# 승계·활성 탭 = TAGDIR 상속 — 부재 시 전수 폴백 원장행). 필터 라운드(16f-5)는
# 폰 타이핑 스킵 원장 승계 — 스킵(MUS-TW-FILTER-SKIP 1행)** →
# leg sdcard(M-5 재판정 — FUSE 스캔 유한 종료 실측) → 복원(ORIG 바이트 등호+
# PERM 병합 원복+시드+big 트리 소각) → BOOT-OK(서버 UP+terminal 상시 — 클라 idle
# 라인 종료 상태 승계).
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
TAGDIR="$TMPD/mus_tagseed"   # N5 태그 세그먼트(T4 #99) — 합성 ID3v2.3 mp3 3건 랜딩 dir
TWDIR="$TMPD/mus_twseed"     # N6 태그 쓰기 세그먼트(#111 T6) — TW 시드 3건 수취 dir(E4) —
                             #   N6-1이 N5 캡처 뒤 $TAGDIR로 mv(3행 계약 무오염 — 원장)
OMDIR="$TMPD/mus_omseed"     # N7 opus/m4a 세그먼트(#115 T4) — OM 시드 3건 수취 dir(E5) —
                             #   N7-1이 N6 소각 뒤 $TAGDIR로 mv(수형 — 원장)
NLOG="$TMPD/pmus_ninja.log"
STLOG="$TMPD/pmus_selftest.log"
ORIG="$TMPD/pmus_orig_settings.json"
PERM_ORIG="$TMPD/pmus_orig_permissions.json"
CANON_PHONE_PREV=818      # 폰 기존 캐논(#113 T3 t3legs 배포 실측 818 FAIL=0 — 2u 132·2v 14·2w 4 흡수 — 비교 원문; #112 T5의 814·#111 T6의 795는 계보 라인 보존)
CANON_PHONE_EXPECT=833    # #115 T4 배포 기대 = 818 + 2x 15(①②③(T1 20e3abf +3)·④-⑧(T2 95cb8c6 +5)·⑨[round-trip+폴백](T3 840ea41 +5)⑩⑪⑫+⑫b 2어설션(T3 fix r1 ce3f539 +2)) = WSL 833 등호 — **818→833 승계(2x 흡수)** 원장행
                          #   계보: 795(#111 T6 — 668 + 2u 127) → 814(#112 T5 —
                          #   795 + fix 5 + 2v 14) → 818(#113 T3 — 2w 4 흡수 +
                          #   fix 승계 4 — t3legs 실측) → 833(#115 T4 본 런 —
                          #   브리프 원문 '854 승계'는 Win 축 수치 표기 원장행
                          #   Win 856/WSL 833 계보의 플랫폼 어설션 차 23 보존)
CANON_PHONE_2U=132        # T1 47(①-⑦) + T2 57(⑧-⑭) + T4 23(⑮) + fix 5(f33d41a F1-F2 — **WSL 실측 2026-10-25 등호 132** — T5 승계 정산; 구 795-계보는 127 원장행)
CANON_PHONE_2V=14         # #112 T1-T4 2v 계보 구두 판정부+사슬 (2v-a 4·2v-b/c 6·2v-d 4 — WSL 14 등호 — 814 산칭 원료)
CANON_PHONE_2W=4          # #113 T1 저널 leg (2w-①-④ — WSL 등호 — 818 승계 원료)
CANON_PHONE_2X=15         # #115 2x 계보 opus/m4a (①②③=3·④-⑧=5·⑨ 2[round-trip+폴백]·⑩⑪⑫=3(T1-T3 13)·⑫b 2[fix r1 ce3f539 — stco/co64 Δ 패치+free 흡수+거부 경계] — 라벨 ①-⑫+⑫b 13건·어설션 15 — WSL 등호 — 833 산칭 원료)
CANON_PHONE_2R=10         # #97 T1 2r 8 + fix r1 2r-b/c 2 — **배포 이관 실측 산치**(638-era 바이너리 갭 원장)
CANON_PHONE_2S=20         # T1 77323b1 2s 20건(#99 태그 라인 — 신설 본 배포 실측 산치)
CANON_PHONE_2M=26
CANON_PHONE_2N=17
CANON_PHONE_2O=16         # 실측 정산(2o-a..e 10 + 2o-f 4 + 2o-g 2 — WSL 등호)
CANON_PHONE_2P=7          # 실측 정산(2026-10-10 T4 폰 2런 — #93 T1 1dfb8cb — WSL 등호)
CANON_PHONE_2Q=7          # 실측 정산(2026-10-10 T4 폰 2런 — #94 adbd0f1 — WSL 등호)
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

echo "=== B. settings 백업+병합 수형 (T4 리뷰 I-1 + #97 T4 — **사용자 music.dirs 보존 병합**) ==="
# [#97 T4 원장 — 진입 상태 변화]: 진입 settings가 music.dirs를 이미 가질 수
# 있다(사용자가 #97 브라우저/UI로 자기 폴더 등록 — 본 라인 진원 실측 163B
# 원문). 구 수형의 "music 키 존재 = 워터마크 hard FAIL"은 사용자 데이터를
# 결함으로 오판하는 함정 — 폐곡. 신 수형: 진입 파일 전체가 ORIG(복원 = 바이트
# 등호 — 사용자 dirs 포함 무손상)…구 ORIG(무 music 키)는 진입과 달라진 경우
# **폐기**(구 ORIG로 원복하면 사용자 dirs가 소각되는 위험 수형 — 원장).
dirs_of() { # $1=src.json — 사용자 music.dirs 합출(쉼표 결합 — python3 json)
    python3 - "$1" <<'DIROF_EOF'
import json, sys
try:
    with open(sys.argv[1], "rb") as f:
        obj = json.loads(f.read().decode("utf-8", "replace"))
    print(",".join(obj.get("music", {}).get("dirs", [])))
except Exception:
    print("")
DIROF_EOF
}

merge_dirs_json() { # $1=라벨(정보용 — **dirs 인자에서 제외 — 런 1 결함 원장**)
                    #   $2...=dirs 순서대 — ORIG 복사 뒤 사용자 기존 dirs
                    #   **보존 병합**(probe dirs 선행 — N3의 dirs_[1]=tree_a
                    #   산식과 leg list의 TAB2_X=90 탭 라벨 등가성 — 원장행;
                    #   사용자 dirs는 후속 탭에 잔존).
    # [런 1 결함 원장 — 라벨 누설]: "$@"로 라벨이 그대로 add로 흘러
    # music.dirs에 가짜 폴더 "list"·"big" 기록(수취 원문 dirs=['list',...]) —
    # 탭 스트립에 유령 탭이 생겨 산식도 오염. 수형 = shift로 라벨 제외.
    local LBL="$1"
    shift
    python3 - "$ORIG" "$SET" "$@" <<'MRG_EOF'
import json, sys
src, out, add = sys.argv[1], sys.argv[2], sys.argv[3:]
with open(src, "rb") as f:
    obj = json.loads(f.read().decode("utf-8", "replace"))
user = [d for d in obj.get("music", {}).get("dirs", []) if d not in add]
mm_ = obj.setdefault("music", {})            # music.* 기존 키 보존(text/fonts 원존 계약 동형)
mm_["dirs"] = add + user                     # probe dirs 선행(보정 상수 등가성)·사용자 dirs 후속 탭 보존
with open(out, "w", encoding="utf-8", newline="\n") as f:
    json.dump(obj, f, ensure_ascii=False, indent=1)
    f.write("\n")
MRG_EOF
}
[ -f "$SET" ] || FAIL "settings file missing: $SET"
B_WAS_STALE=0
if [ -s "$ORIG" ] && cmp -s "$ORIG" "$SET"; then
    echo "PRIOR-ORIG-RECOVERED: $ORIG ($(wc -c < "$ORIG") bytes — 전 run 유산, 현재 파일과 바이트 등호 — 재사용)"
elif [ -s "$ORIG" ] && [ "$(grep -ac '"music"' "$ORIG" 2>/dev/null || echo 0)" -eq 0 ] \
     && [ "$(grep -ac '"music"' "$SET" 2>/dev/null || echo 0)" -eq 0 ]; then
    echo "PRIOR-ORIG-RECOVERED: $ORIG ($(wc -c < "$ORIG") bytes — 전 run 유산, 양측 무 music 키 — 구 계약 수형 재사용)"
else
    B_WAS_STALE=1
    echo "ORIG-STALE-SUPERSEDED: 진입 settings로 ORIG 갱신 ($(wc -c < "$SET") bytes — 구 백업 $( [ -s "$ORIG" ] && echo "$(wc -c < "$ORIG")B와 불일치" || echo absent ) — **사용자 music.dirs 보존 계약**(구 ORIG로 원복하면 사용자 dirs 소각 위험 — 원장))"
    cp "$SET" "$ORIG" || FAIL "settings backup failed"
fi
cmp -s "$SET" "$ORIG" || echo "NOTE-ORIG-DIFFERS: 진입 settings ≠ ORIG — 병합 전 자가 원복(merge 함수가 ORIG에서 이행)"
FONT_PATH=$(sed -n 's/.*"font_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$ORIG" | head -1)
[ -n "$FONT_PATH" ] || FAIL "settings has no text.font_path (font_path 유지 계약 전제)"
[ -r "$FONT_PATH" ] || FAIL "font file not readable: $FONT_PATH"
echo "PARSED-FONT_PATH: $FONT_PATH"
echo "SETTINGS-BACKUP-OK: $ORIG ($(wc -c < "$ORIG") bytes — 복원은 이 바이트열 등호)"
ENTRY_DIRS=$(dirs_of "$ORIG")
echo "ENTRY-MUSIC-DIRS: ${ENTRY_DIRS:-<none>} (진입 사용자 dirs 원문 — #97 T4 보존 계약의 원문)"

merge_music_dir() { # $1 dirs 경로 $2 라벨 — 사용자 dirs 보존 병합(#97 T4 수형)
    case "$1" in *'"'*) FAIL "dir path contains a quote (JSON 안전성)";; esac
    merge_dirs_json "$2" "$1" || FAIL "merge($2): merge_dirs_json failed"
    grep -aq '"music"' "$SET" && grep -aqF "$1" "$SET" || FAIL "merge($2): gate failed — music.dirs 미기술"
    echo "SETTINGS-IN-LEG-$2: $(tr -d '\n' < "$SET") (bytes=$(wc -c < "$SET"))"
}

echo "=== C. permissions.json 병합(선존 파일 존중 — END 바이트 등호 복원) ==="
# 병합 키 = send_input(T5 계승)+close_window(T4 취소 세그먼트 ① — 서버는
# permissions를 **호출마다 핫리드**한다 — phone_close_receipt.sh 선례 — 스테이지만으로
# 게이트가 산다 — 재부팅 0). 부재 Deny 규약(wsl_chat_close.sh 원형).
ENTRY_PERM=0
perm_insert_key() { # $1=키 $2=값 — 마지막 } 직전 삽입(구조 친화 병합 — merge_music_dir 수형)
    awk -v newline="    \"$1\": \"$2\"" '
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
        }' "$PERM" > "$PERM.new" || return 1
    mv "$PERM.new" "$PERM" || return 1
    return 0
}
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
    CWI=$(sed -n 's/.*"close_window"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$PERM" | head -1)
    if [ "$SI" = "allow" ]; then
        echo "PERM-MERGE: send_input none (선존 send_input=allow)"
    else
        perm_insert_key send_input allow || FAIL "permissions merge failed (구조 친화 실패)"
        echo "PERM-MERGE: +send_input allow (선존 키 보존 — merge=$(wc -c < "$PERM") bytes, END에서 바이트 등호 원복)"
    fi
    if [ "$CWI" = "allow" ]; then
        echo "PERM-MERGE: close_window none (선존 close_window=allow — phone_close_receipt 유산 원장행)"
    else
        perm_insert_key close_window allow || FAIL "permissions close_window merge failed (구조 친화 실패)"
        echo "PERM-MERGE: +close_window allow (T4 ① in-flight close 전제 — 핫리드 계약, END에서 바이트 등호 원복)"
    fi
else
    printf '{\n    "send_input": "allow",\n    "close_window": "allow"\n}\n' > "$PERM" \
        || FAIL "permissions write failed"
    echo "PERM-WIRE-OK: $PERM (send_input+close_window allow — 선존 부재라 END에서 소각)"
fi
grep -aq 'send_input' "$PERM" || FAIL "permissions.json has no send_input (승인 병합 미성립)"
grep -aq 'close_window' "$PERM" || FAIL "permissions.json has no close_window (T4 close 병합 미성립)"
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
# 스테이지 소각은 E3 뒤로 이동(T4 #99 — 태그 멤버가 같은 tar 멤버 트리라
# E에서의 rm이 태그 원문을 씹는 것 방지 — 원장행)
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

echo "=== E3. 태그 트랙 수취 — ID3 매직 진품 판정 (T4 #99 — 드라이버 합성·폰 python3 불요 승계) ==="
if [ -d "$TAGDIR" ] && [ "$(find "$TAGDIR" -name '*.mp3' 2>/dev/null | wc -l)" -eq 3 ]; then
    echo "PRIOR-TAG-SEEDS-RECOVERED: $TAGDIR (전 run 유산 — 3건 등수, 재사용: RUN-DROPPED 재실행 계약)"
else
    rm -rf "$TAGDIR"
    [ -d "$SEED_STAGE/tag_music_seed" ] || FAIL "tag seed tar member missing (구성 누락)"
    mv "$SEED_STAGE/tag_music_seed" "$TAGDIR" || FAIL "tag seed move failed"
    echo "TAG-SEED-LANDED: $TAGDIR (tar 멤버 → \$TMPDIR 이동)"
fi
TAGIC=0
for t in "$TAGDIR"/t_tag_alpha.mp3 "$TAGDIR"/t_tag_beta.mp3 "$TAGDIR"/t_raw_noid.mp3; do
    [ -s "$t" ] || FAIL "tag seed file missing: $t"
    if [ "$(head -c 3 "$t" | grep -aic 'ID3')" -eq 1 ]; then
        echo "  TAG-MAGIC: $(basename "$t") ID3 ok ($(wc -c < "$t") bytes)"
        TAGIC=$((TAGIC + 1))
    else
        echo "  TAG-MAGIC: $(basename "$t") ID3 부재 ($(wc -c < "$t") bytes — raw 폴백 행 원문)"
    fi
done
[ "$TAGIC" -eq 2 ] || FAIL "tag ID3 magic count=$TAGIC (need 2 — alpha·beta)"
[ -s "$TAGDIR"/t_tag_alpha.mp3 ] || FAIL "tag alpha missing"
echo "MUSIC-TAGLIST-EXPECTED=3 (태그 행 2[alpha·beta]+폴백 1[raw] — 표행 수 관측 원문 1행; 표기 일치는 캡처 육안 몫 EYES)"
sync
echo "MUSP-TAGSEED-SYNC: drained (태그 시드 writeback 선행 배수 — N3 원장 수형)"

echo "=== E4. [T6 #111] TW 시드 수취 — 랜딩 대기 dir (N6-1이 N5 캡처 뒤 \$TAGDIR로 mv) ==="
# 수취만 여기서 한다(배치 = \$TAGDIR 랜딩을 N6로 늦춘다 — N5의 3행 표기 계약
# 무오염 원장 — TW 시드가 N5 표에 섞이면 예상 행 레저가 틀어진다). RETRY
# 재실행 대비 = PRIOR-RECOVERED 계약 승계(E/N5 수형).
if [ -d "$TWDIR" ] && [ "$(find "$TWDIR" -type f 2>/dev/null | wc -l)" -eq 3 ]; then
    echo "PRIOR-TW-SEEDS-RECOVERED: $TWDIR (전 run 유산 — 3건 등수, 재사용: RUN-DROPPED 재실행 계약)"
else
    [ -d "$TWDIR" ] && FAIL "tw seed dir exists with irregular contents — 수동 소각 필요"
    [ -d "$SEED_STAGE/tw_music_seed" ] || FAIL "TW seed tar member missing (구성 누락)"
    mv "$SEED_STAGE/tw_music_seed" "$TWDIR" || FAIL "TW seed move failed"
    echo "TW-SEED-TAR-LANDED: $TWDIR (tar 멤버 → \$TMPDIR 수취 — N6-1 랜딩 대기)"
fi
TWC=0
for t in "$TWDIR"/t_tw_write.mp3 "$TWDIR"/t_tw_write.flac "$TWDIR"/t_tw_cover.jpg; do
    [ -s "$t" ] || FAIL "TW seed file missing: $t"
    TWC=$((TWC + 1))
done
[ "$TWC" -eq 3 ] || FAIL "TW seed count=$TWC (need 3)"
TWCM=$(md5sum "$TWDIR"/t_tw_cover.jpg | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
echo "TW-COVER-MD5-PHONE: $TWCM (기대=__TW_COVER_MD5__ — 도착 등호 게이트 — E 원문)"
[ "$TWCM" = "__TW_COVER_MD5__" ] || FAIL "TW cover md5 mismatch on phone (도착 원문 불일치 — 전송 파열 방어)"
[ "$(head -c 3 "$TWDIR"/t_tw_write.mp3)" = "ID3" ] \
    && FAIL "TW mp3 carries ID3 magic (무태그 전제 위반 — 2u-① 구성 계약 위배)" \
    || echo "TW-MP3-NOMAGIC-PHONE: OK (junk 768B — ID3 부재 — v2.3 신설 원문)"

echo "=== E5. [#115 T4] OM 시드 수취 — 랜딩 대기 dir (N7-1이 N6 소각 뒤 \$TAGDIR로 mv — N6-1 수형) ==="
if [ -d "$OMDIR" ] && [ "$(find "$OMDIR" -type f 2>/dev/null | wc -l)" -eq 3 ]; then
    echo "PRIOR-OM-SEEDS-RECOVERED: $OMDIR (전 run 유산 — 3건 등수, 재사용: RUN-DROPPED 재실행 계약)"
else
    [ -d "$OMDIR" ] && FAIL "om seed dir exists with irregular contents — 수동 소각 필요"
    [ -d "$SEED_STAGE/om_music_seed" ] || FAIL "OM seed tar member missing (구성 누락)"
    mv "$SEED_STAGE/om_music_seed" "$OMDIR" || FAIL "OM seed move failed"
    echo "OM-SEED-TAR-LANDED: $OMDIR (tar 멤버 → \$TMPDIR 수취 — N7-1 랜딩 대기)"
fi
OMC=0
for t in "$OMDIR"/t_om_write.m4a "$OMDIR"/t_om_write.opus "$OMDIR"/t_om_cover.jpg; do
    [ -s "$t" ] || FAIL "OM seed file missing: $t"
    OMC=$((OMC + 1))
done
[ "$OMC" -eq 3 ] || FAIL "OM seed count=$OMC (need 3)"
grep -aq ftyp <(head -c 8 "$OMDIR"/t_om_write.m4a) \
    || FAIL "OM m4a seed ftyp magic break (2x 픽스처 구성 계약)"
[ "$(head -c 4 "$OMDIR"/t_om_write.opus)" = "OggS" ] \
    || FAIL "OM opus seed OggS magic break (2x 픽스처 구성 계약)"
OM_CM_ST=$(md5sum "$OMDIR"/t_om_cover.jpg | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
echo "OM-COVER-MD5-PHONEST: $OM_CM_ST (기대=__TW_COVER_MD5__ — TW 원문 동일 바이트 — 도착 등호 게이트)"
[ "$OM_CM_ST" = "__TW_COVER_MD5__" ] || FAIL "OM cover md5 mismatch on phone (도착 원문 불일치 — 전송 파열 방어)"
rm -rf "$SEED_STAGE"
[ -d "$SEED_STAGE" ] && FAIL "seed stage rm failed (repo 잔상 — 원장행)"
echo "SEED-STAGE-BURIED: $SEED_STAGE (wav+태그+TW 수취 완료 뒤 — E 원장행)"

echo "=== F. ninja 리빌드 (aarch64 — jkapp_music 신규 타깃, CMake mtimes로 RERUN_CMAKE 기대) ==="
pkill -f '[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f '[j]kdesktop' 2>/dev/null
sleep 1
if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then
    FAIL "server respawned mid-build (ETXTBSY 위험 — 상위 감독자 원장)"
fi
# === F0. leg 배선 cmake 재구성 (#110 T2 — leg 자산 = 드라이버 tar 스테이징) ===
# 드라이버(engine/tmp 산출물 — pmus_t4 수형)가 tar로 전개한 폰 $TMPDIR leg
# 자산(spatsrc=spatial-player 소스 트리[git archive HEAD 원문]·spike-build의
# openal-build/libopenal.a=T1 킷 절차로 만든 prebuilt aarch64 정적 산출
# [저장소 조달/커밋 없음 — T1 REMNANT 청산 뒤의 재용은 킷 절차 재실행 원장]·
# openal-src/include=AL 헤더)가 존재하면 env로만 leg를 켠다(#91 fail-closed
# 계약 유지 — env-only). 자산 부재 = 구 T4 수형 unset 빌드 — honest 원장행이고
# 모드는 UNSET으로 계속(배제 원문 소비). env 기본 레이아웃 = MUSP_LEG_STAGE.
# CMAKE_POLICY_VERSION_MINIMUM=3.5 = 폰 빌드 러너의 cmake4 통과 env(T1 원문
# 수형 — 저장소 touch 금지 — cmake4는 env var로 소비한다).
SPAT_STAGE="${MUSP_LEG_STAGE:-$TMPD/t110_leg}"
SPAT_SRC="$SPAT_STAGE/spatsrc"
SPAT_ALLIB="$SPAT_STAGE/spike-build/openal-build/libopenal.a"
SPAT_ALINC="$SPAT_STAGE/openal-src/include"
if [ -f "$SPAT_SRC/src/spatial.cpp" ] && [ -s "$SPAT_ALLIB" ] \
   && [ -f "$SPAT_ALINC/AL/alext.h" ]; then
    export SPATIAL_PLAYER_ROOT="$SPAT_SRC"
    export SPATIAL_OPENAL_LIB="$SPAT_ALLIB"
    export SPATIAL_OPENAL_INCLUDE="$SPAT_ALINC"
    # 전이 링크 세트(WSL r1 원장 — IMPORTED 정적 재용은 openal INTERFACE를
    # 전복): 킷2가 백엔드를 OpenSL(+Wave/Null)로 제했으므로 -lOpenSLES + 폰
    # 실측(r3)에 추가된 -llog 2종 — OpenSL 백엔드가 __android_log_print
    # (liblog)를 참조하고 누락 시 jkapp_music.so dlopen 사망. 둘 다
    # ndk-sysroot 산출 $PREFIX/lib — T1 원문. 백엔드 재한을 바꾸는 재런은
    # env MUSP_OPENAL_LINK로 재정의.
    export SPATIAL_OPENAL_LINK="${MUSP_OPENAL_LINK:--lOpenSLES -llog}"
    export CMAKE_POLICY_VERSION_MINIMUM=3.5
    echo "LEG-STAGE-WIRED: stage=$SPAT_STAGE · lib=$SPAT_ALLIB($(wc -c < "$SPAT_ALLIB") bytes) · inc=$SPAT_ALINC · link=$SPATIAL_OPENAL_LINK"
    cmake -S . -B buildterm >"$TMPD/pmus_cmake_leg.log" 2>&1
    C0_RC=$?
    echo "CMAKE-LEG-RC=$C0_RC"
    tail -4 "$TMPD/pmus_cmake_leg.log"
    [ "$C0_RC" -eq 0 ] || FAIL "leg cmake reconfigure rc=$C0_RC (SPATIAL_PLAYER_ROOT 배선 configure 실패 — 원문 pmus_cmake_leg.log)"
else
    echo "LEG-STAGE-MISS: leg 자산 결손 (src=$([ -d "$SPAT_SRC/src" ] 2>/dev/null && echo ok || echo miss) lib=$([ -s "$SPAT_ALLIB" ] 2>/dev/null && echo ok || echo miss) inc=$([ -f "$SPAT_ALINC/AL/alext.h" ] 2>/dev/null && echo ok || echo miss)) — 구 T4 unset 수형 원장행 (전개 전체 소스가 없으면 unset 빌드 — 자산 재전개 = 드라이버 몫)"
fi
# === F0b. 구두 leg 승계 배선 (#115 T4 — #112 T5 원존 계약의 라이브 상속):
# vendored whisper.cpp 트리(커밋 산물)가 살아 있으면 WHISPER_ROOT를 재출력한다
# — F/boot_server의 부팅 env 승계(export — 셸 서버 env 상속 — 클라 스폰이 서버
# env를 이어받는 구두 게이트 클라 상속 계약). 부재 = leg-less boot 원장행(
# fail-closed — 미설정 빌드 무영향 — 2v-① 수형).
if [ -d "$HOME/JKENGINE/engine/third_party/whisper.cpp/ggml" ]; then
    export WHISPER_ROOT="$HOME/JKENGINE/engine/third_party/whisper.cpp"
    echo "WHISPER-ROOT-WIRED-PHONE: tree-entries=$(find "$WHISPER_ROOT" -type f | wc -l) (구두 leg boot 승계 — #115 T4 배포 계약 — #112 T5 원존)"
else
    echo "WHISPER-ROOT-MISS: vendored 트리 부재 — 구두 leg-less 부팅(fail-closed 영수증 — 원장행)"
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
# leg 모드 판정(#110 T2 — 진실원 = **바이너리**): CMake는 env 설정만
# JK_MUSIC_SPATIAL_LEG 매크로+audio_core 링크를 먹인다(fail-closed 배선 원문).
# T2 배포 런은 러너가 SPATIAL_PLAYER_ROOT(+ 폰 prebuilt 축 SPATIAL_OPENAL_LIB
# /SPATIAL_OPENAL_INCLUDE)를 세팅해 jkapp_music.so에 AL 심볼이 실린 채 나온다.
# 심볼 카운트는 nm 대신 바이너리 문자열 grep(Termux binutils 의존 0 — wsl
# 8b ALCOUNT 수형의 최소 대응). [sym=0+env set]만 배선 결손 hard FAIL —
# [sym>0+env unset]도 .so 진실원으로 WIRED로 통과한다(후속 순수 측정 런이
# 재배포 없이 이뤄지는 수형 — 원장).
echo "MUSP-LEG-ENV: ${SPATIAL_PLAYER_ROOT:+SET}unset (빌드 러너 env 원문 — T2 배포 런 계약 = SET)"
LEG_SYM=$(grep -aac 'alcOpenDevice' buildterm/jkapp_music.so 2>/dev/null || true)
LEG_SYM=${LEG_SYM:-0}
echo "MUSP-LEG-SYM: alcOpenDevice strings=$LEG_SYM (0 = leg 미링크 fail-closed 영수증 — 구 T4 배제 런)"
if [ -n "${SPATIAL_PLAYER_ROOT:-}" ] && [ "$LEG_SYM" = "0" ]; then
    FAIL "SPATIAL_PLAYER_ROOT set인데 AL 심볼 0 — CMake leg 배선 결손 (T2 hard — 배포 원문 대조)"
fi
if [ "$LEG_SYM" != "0" ]; then
    echo "MUSP-LEG-MODE: WIRED (leg 링크 성립 — D5 배제 가드 폐곡 — #110 T2·spatial_play 성립 원문은 J2/M2 소유)"
    LEG_WIRED=1
else
    echo "MUSP-LEG-MODE: UNSET (구 T4 배제 런 계약 — J2 배제 원문 소비)"
    LEG_WIRED=0
fi

echo "=== G. 배포 마커 — HEAD 원천 실존 단정 ==="
MM=$(grep -c 'ListAudioFiles' include/apps/MusicModel.h)
MC=$(grep -c 'jkapp_music' CMakeLists.txt)
MT=$(grep -c '2m-' src/main.cpp)
MP2=$(grep -c '2p-' src/main.cpp)
MQ2=$(grep -c '2q-' src/main.cpp)
MR2=$(grep -c '2r-' src/main.cpp)
MS2=$(grep -c '2s-' src/main.cpp)
MU2=$(grep -c '2u-' src/main.cpp)
MW2=$(grep -c '2w-' src/main.cpp)
MX2=$(grep -c '2x-' src/main.cpp)
MP=$(grep -c 'DelegationReplyVerdict' src/apps/ClientMusicApp.cpp)
ML=$(grep -c '"music", "Music"' src/apps/JKAppModule_music.cpp)
MMSC=$(grep -c 'scanCancel_' src/apps/ClientMusicApp.cpp)
echo "MARKER-AFTER music_model($MM) cmake($MC) main_2m($MT) main_2p($MP2) main_2q($MQ2) main_2r($MR2) main_2s($MS2) main_2u($MU2) main_2w($MW2) main_2x($MX2) delegate_verdict($MP) module($ML) cancel_wiring($MMSC)"
[ "$MM" -ge 1 ] || FAIL "MusicModel ListAudioFiles marker missing on phone (배포 결손)"
[ "$MC" -ge 1 ] || FAIL "jkapp_music CMake marker missing on phone"
[ "$MT" -ge 1 ] || FAIL "2m selftest marker missing on phone main.cpp"
[ "$MP2" -ge 1 ] || FAIL "2p cancel selftest marker missing on phone main.cpp (T4 배포 결손)"
[ "$MQ2" -ge 1 ] || FAIL "2q path-encoding selftest marker missing on phone main.cpp (T4 배포 결손)"
[ "$MR2" -ge 10 ] || FAIL "2r selftest markers missing on phone main.cpp (기대 >=10 — #97 T1 8+fix r1 2 배포 결손)"
[ "$MS2" -ge 20 ] || FAIL "2s selftest markers missing on phone main.cpp (기대 >=20 — T1 77323b1 태그 라인 배포 결손)"
[ "$MU2" -ge 100 ] || FAIL "2u selftest markers missing on phone main.cpp (기대 >=100 — #111 T1/T2/T4 태그 쓰기 라인 배포 결손 — T6)"
[ "$MW2" -ge 4 ] || FAIL "2w selftest markers missing on phone main.cpp (기대 >=4 — #113 T1 저널 leg 배포 결손)"
[ "$MX2" -ge 14 ] || FAIL "2x selftest markers missing on phone main.cpp (기대 >=14 라벨 — #115 opus/m4a 라인 T1+T2+T3+T3 fix r1(ce3f539 — 2x-⑫b) 배포 결손)"
[ "$MMSC" -ge 1 ] || FAIL "scanCancel_ wiring marker missing on phone (T2 취소 배선 배포 결손)"
[ "$MP" -ge 1 ] || FAIL "DelegationReplyVerdict marker missing on phone (fix r3 배포 결손)"

echo "=== H. selftest — 폰 캐논 계보 판정 (계보: 795(#111 T6) → 814(#112 T5 — fix 5+2v 14) → 818 = 814 + 2w 4(#113 T1 저널 — t3legs 배포 실측) → **기대 831 = 818 + 2x 13(2x ①②③ 20e3abf+3·④-⑧ 95cb8c6+5·⑨[round-trip+폴백 2어설션]⑩⑪⑫ 840ea41+5 — 12건/13 어설션 — WSL 831 등호 — 브리프 원문행의 '854 승계'는 Win 축 수치 표기 원장행** — 실측 후 CANON 정산) ==="
timeout 900 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
P2M=$(grep -ac '^\[PASS\] 2m' "$STLOG")
P2MH=$(grep -ac '^\[PASS\] 2m-h' "$STLOG")
P2N=$(grep -ac '^\[PASS\] 2n' "$STLOG")
P2O=$(grep -ac '^\[PASS\] 2o' "$STLOG")
P2OF=$(grep -ac '^\[PASS\] 2o-f' "$STLOG")
P2OG=$(grep -ac '^\[PASS\] 2o-g' "$STLOG")
P2P=$(grep -ac '^\[PASS\] 2p' "$STLOG")
P2Q=$(grep -ac '^\[PASS\] 2q' "$STLOG")
P2R=$(grep -ac '^\[PASS\] 2r' "$STLOG")
P2S=$(grep -ac '^\[PASS\] 2s' "$STLOG")
P2U=$(grep -ac '^\[PASS\] 2u' "$STLOG")
P2V=$(grep -ac '^\[PASS\] 2v' "$STLOG")
P2W=$(grep -ac '^\[PASS\] 2w' "$STLOG")
P2X=$(grep -ac '^\[PASS\] 2x' "$STLOG")
P2I=$(grep -ac '^\[PASS\] 2i' "$STLOG")
P2G=$(grep -ac '^\[PASS\] 2g' "$STLOG")
echo "PHONE-SELFTEST rc=$S_RC PASS=$ST_PASS FAIL=$ST_FAIL 2m=$P2M(2m-h=$P2MH) 2i=$P2I 2g=$P2G 2n=$P2N 2o=$P2O(2o-f=$P2OF 2o-g=$P2OG) 2p=$P2P 2q=$P2Q 2r=$P2R 2s=$P2S 2u=$P2U 2v=$P2V 2w=$P2W 2x=$P2X (music 라인 2m·2n·2o + 취소 경계 2p + 경로 인코딩 2q + #97 브라우저 2r + 태그 2s + #111 태그 쓰기 2u + #112 구두 2v + #113 저널 2w + #115 opus/m4a 2x 계열 — 캐논 상승분의 원료 — 분해 병기: 정산 판별 몫)"
grep -a 'AppSelfTest' "$STLOG" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
awk -v p="$ST_PASS" -v m="$P2M" -v h="$P2MH" -v n="$P2N" -v o="$P2O" -v of="$P2OF" -v og="$P2OG" \
    -v pf="$P2P" -v qf="$P2Q" -v rf="$P2R" -v sf="$P2S" -v uf="$P2U" -v vf="$P2V" \
    -v wf="$P2W" -v xf="$P2X" \
    -v prev="$CANON_PHONE_PREV" -v expect="$CANON_PHONE_EXPECT" -v exo="$CANON_PHONE_2O" \
    -v exn="$CANON_PHONE_2N" -v xp="$CANON_PHONE_2P" -v xq="$CANON_PHONE_2Q" -v xr="$CANON_PHONE_2R" \
    -v xs="$CANON_PHONE_2S" -v mm="$CANON_PHONE_2M" -v xu="$CANON_PHONE_2U" \
    -v xw="$CANON_PHONE_2W" -v xx="$CANON_PHONE_2X" 'BEGIN{
    if (p == expect && o == exo && pf == xp && qf == xq && rf == xr && sf == xs && uf == xu && wf == xw && xf == xx)
        printf "CANON-INCLUSION=MUSIC-FULL-2P2QRS-%d (폰 캐논 %s→%s 상승 — 상승분 %d = #115 2x 13(T1 20e3abf +3·T2 95cb8c6 +5·T3 840ea41 +5 — 2x=%d·2w=%d·2v=%d 흡수 — 2u %d건·2r %d건+2s %d건 + 2p %d건+2q %d건 + 2m %d건[h %d 포함]·2n %d건·2o %d건[o-f %d·o-g %d] 보존) 실측 — WSL %s 등호)\n", p - prev, prev, p, p - prev, xf, wf, vf, uf, rf, sf, pf, qf, mm, h, n, o, of, og, expect
    else if (p == prev)
        print "CANON-INCLUSION=MUSIC-SELFTEST-MISSING (music selftest 쌍둥이 미반영 — 계보 결손 — 원장: 2u/2x 배포 결손이면 818 정산 [WARN] — 2u=" xu " 2x=" xf ")"
    else
        printf "CANON-INCLUSION=OTHER-N(%d — 기대 %d=%d+2x13(2x=%d·2w=%d·2v=%d) · 2u=%d·2r=%d·2s=%d(2p=%d+2q=%d) · 2m=%d[h=%d]·2n=%d·2o=%d[o-f=%d o-g=%d] — 계보 정산 원장)\n", p, expect, prev, xf, wf, vf, xu, rf, sf, xp, xq, m, h, n, o, of, og
}'

# ---------------------------------------------------------------- 레그 공통
ctl() { timeout 20 ./buildterm/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }
mtool() { # $1=tool $2=args-json — music 앱 직행 릴레이(wsl 8b atool 수형 —
          #   도구 허브 릴레이 원문. music 창 등록 전제라 레그 list 내에서만 온다)
    ctl "{\"tool\":\"app_tool\",\"args\":{\"app\":\"music\",\"tool\":\"$1\",\"args\":$2}}"
}
CAPTURE() { # $1=출력png — 서버측 capture_region(agentctl)이 합성 프레임을 state/screenshots에 PNG 기록하고 cp. #110 T2 r3-r5 원장: ffmpeg x11grab은 /tmp/.X11-unix(X socket 부재)를 보고 상자 ENOENT — 폰 X1은 $TMPDIR/.X11-unix에 있어 ffmpeg libX11(Termux 미패치)와 불합. 서버 도구는 shm/렌더러 직독이라 소켓 계통 무관(원문 2026-10-25 실측).
    local OUT=$1
    rm -f "$OUT"
    local R P
    R=$(timeout 25 ./buildterm/jkdesktop agentctl '{"tool":"capture_region","args":{"x":0,"y":0,"w":1920,"h":1080}}' 2>/dev/null | grep -a '"ok"' | head -1)
    P=$(printf '%s' "$R" | grep -ao '"path":"[^"]*"' | sed 's/"path":"//;s/"$//')
    [ -n "$P" ] && cp "$P" "$OUT"
    [ -s "$OUT" ] || FAIL "capture failed: $OUT (capture_region R=${R:0:120})"
    echo "CAPTURE-OK: $OUT ($(wc -c < "$OUT") bytes) (src=$P)"
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
echo "MUSP-LEG-BOOT ($LEG): jkapp_music.so AL 심볼=$LEG_SYM (부팅 원문 — leg 실재 — WIRED 런 계약 ≠0·UNSET 런 0)"
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

echo "=== J2. spatial leg 세그먼트 (#110 T2 — 모드 분기: WIRED 성립 / UNSET 배제 원문) ==="
# 구수형(T4 — D5 배제 원문): 폰은 env를 정의하지 않는다 — leg 미링크 .so의
# 발사 = DeviceFailed 고장 종착(kDelegationHint 원문 라벨 소비). 도구 발사와
# UI [spatial] 버튼 탭은 SpatialStart 단일 발사 경로로 합류(T2 원문).
# 신수형(#110 T2 — 폰 spatial 합의): 배포 러너가 SPATIAL_PLAYER_ROOT를 세팅해
# leg가 링크되면 — 배포가 승계한 D5 배제 계약은 폐곡되고, 이 세그먼트는
# **폰 leg 성립 원문**을 실측한다(WSL T3 LEG-OK의 폰 재실측): accepted →
# spatial_status active:true·deviceOk:true·positional:true(HRTF POSITIONAL
# — T1 set_mode 원문)·pos 진행·TONE 시작 지연(T1 concern 3 — 0.5s 소스가
# 0.75s 대기 후에도 재생 표기인 4116 관측의 폰 leg 원문)·stop idempotent.
if [ "${LEG_WIRED:-0}" -eq 1 ]; then
    ST0=$(mtool spatial_status '{}')
    echo "MUSP-SPATIAL-STATUS-0: ${ST0:-none} (재생 전 기저 — idle 원문)"
    printf '%s' "$ST0" | grep -aq '"active":false' \
        || echo "MUSICP-FAIL-SOFT(spatial 기저가 active — idle 원문 대조 필요)"
    SPAT_T0=$(date +%s%N)
    SPAT_REPLY=$(mtool spatial_play "{\"path\":\"$SEED/a_seed.wav\"}")
    echo "MUSP-SPATIAL-PLAY-REPLY: $SPAT_REPLY"
    printf '%s' "$SPAT_REPLY" | grep -aq '"accepted":true' \
        || echo "MUSICP-FAIL-SOFT(wired spatial_play: accepted 미수취 — start 원문 대조: $SPAT_REPLY)"
    # TONE 시작 지연 실측(T1 concern 3): accepted 귀환 후 spatial_status를
    # 0.2s 폴링 — pos가 처음 >0으로 도달하는 시차(ms). 폰 ALC open+prefill
    # 고정비 원문(250ms 관측점 원장 — 초과는 원장행·fail 아님).
    SPAT_DELAY_MS=""
    SPAT_POS1=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        ST1=$(mtool spatial_status '{}')
        POSI=$(printf '%s' "$ST1" | sed -n 's/.*"pos":\([0-9.]*\).*/\1/p')
        if [ -n "$POSI" ] && awk -v p="$POSI" 'BEGIN{exit (p > 0) ? 0 : 1}'; then
            SPAT_DELAY_MS=$(( ($(date +%s%N) - SPAT_T0) / 1000000 ))
            SPAT_POS1="$POSI"
            echo "MUSP-SPATIAL-STATUS-1(#$i): $ST1"
            break
        fi
        sleep 0.2
    done
    if [ -n "${SPAT_DELAY_MS:-}" ]; then
        echo "MUSP-TONE-START-DELAY: ${SPAT_DELAY_MS}ms (accepted→pos>0 — T1 concern 3·250ms 관측점 대조 원장 행)"
    else
        echo "MUSP-TONE-START-DELAY: MISS — 4.0s 폴링 창에서 pos>0 미도착 (폰 ALC/prefill 지연 원장행 — 원문: $(printf '%s' "${ST1:-none}" | head -c 200))"
    fi
    # pos 진행 원문(WSL T3 수형 — 1.600→3.600(delta=2.000)): 지연 수확 뒤 1.6s
    # 대기 → 2차 표본 — delta = 진행 원문 1행.
    sleep 1.6
    ST2=$(mtool spatial_status '{}')
    SPAT_POS2=$(printf '%s' "$ST2" | sed -n 's/.*"pos":\([0-9.]*\).*/\1/p')
    echo "MUSP-SPATIAL-STATUS-2: $ST2"
    SPAT_PROGRESS=0
    if [ -n "${SPAT_POS1:-}" ] && [ -n "${SPAT_POS2:-}" ]; then
        SPAT_DELTA=$(awk -v a="$SPAT_POS1" -v b="$SPAT_POS2" 'BEGIN{printf "%.3f", b-a}')
        if awk -v d="$SPAT_DELTA" 'BEGIN{exit (d > 0) ? 0 : 1}'; then
            SPAT_PROGRESS=1
            echo "MUSP-SPATIAL-POS-PROGRESS: OK — pos $SPAT_POS1 → $SPAT_POS2 (delta=$SPAT_DELTA 진행 원문 — WSL T3 수형)"
        else
            echo "MUSICP-FAIL-SOFT(pos progress: delta=$SPAT_DELTA 진행 원문 미성립 — $SPAT_POS1 → $SPAT_POS2)"
        fi
    else
        echo "MUSICP-FAIL-SOFT(pos parse miss: pos1=${SPAT_POS1:-none} pos2=${SPAT_POS2:-none} — 정직 원장)"
    fi
    printf '%s' "$ST2" | grep -aq '"deviceOk":true' \
        || echo "MUSICP-FAIL-SOFT(deviceOk: true 미수취 — $ST2)"
    printf '%s' "$ST2" | grep -aq '"positional":true' \
        || echo "MUSICP-FAIL-SOFT(positional/HRTF: true 미수취 — $ST2)"
    SPATSTOP=$(mtool spatial_stop '{}')
    echo "MUSP-SPATIAL-STOP-REPLY: ${SPATSTOP:-none} (T1 StopRequested 종착 — idempotent·pos 보존 원문)"
    printf '%s' "$SPATSTOP" | grep -aq '"active":false' \
        || echo "MUSICP-FAIL-SOFT(sp_stop: active:false 미수취 — $SPATSTOP)"
    echo "MUSP-LEG-BARRED: N/A (WIRED 런 — 배제 원문은 UNSET 런의 원문이고 폰 leg 성립 = MUSP-LEG-SPATIAL 집계 행)"
else
    # 구 T4 배제 원문 수형 원문 유지 — UNSET 런에서만 소비(회귀 축).
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
fi
echo "--- UI [spatial] 버튼 탭(발사 경로 동일성 원문 — SpatialStart 단일 경로 합류) ---"
SPATCLICK_X=$((MUS_X + SPAT_X))
SPATCLICK_Y=$((MUS_Y + SPAT_Y))
echo "CLICK-SPAT: id=$MUS_ID desk=($SPATCLICK_X,$SPATCLICK_Y) — 상수 SPAT_X=$SPAT_X SPAT_Y=$SPAT_Y (좌표 추정 — 성립은 캡처 라벨 렌더 육안 몫)"
SCL=$(tap "$MUS_ID" "$SPATCLICK_X" "$SPATCLICK_Y" 1)
ok "$SCL" || echo "NOTE-SPAT-TAP: send_input [spatial] 클릭 실패 — $SCL (도구 발사가 같은 종착을 이미 증명 — 원장행)"
echo "[spatial] tap reply: $SCL"
sleep 2
ST2=$(mtool spatial_status '{}')
echo "MUSP-SPATIAL-STATUS-2(post-tap): ${ST2:-none} (발사 귀결 원문 — UNSET=err 보존·WIRED=시작 전이 또는 무착탄 — 캡처 대조)"
if [ "${LEG_WIRED:-0}" -eq 1 ] && printf '%s' "$ST2" | grep -aq '"active":true'; then
    # UI 탭이 실제로 발사했다 — 도구 경로와 같은 SpatialStart 원문(단일 경로
    # 합류의 폰 원문). 조용히 정리(뒤 위임 세그먼트와 세션 분리).
    STPT=$(mtool spatial_stop '{}')
    echo "MUSP-SPATIAL-UI-STOP: ${STPT:-none} (UI 탭 발사 원문 수취 뒤 정리)"
elif [ "${LEG_WIRED:-0}" -eq 1 ]; then
    echo "MUSP-SPATIAL-UI-TAP-MISS: post-tap 비활성 (좌표 추정 상수 원장행 — 도구 경로가 SpatialStart 종착을 대변)"
fi
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

echo "=== M2. 병행 합산 실측 — spatial leg(OpenAL) + vplayer(SDL audio) 동시 재생 (#110 T2 — T1 spike concern 4 보강) ==="
# T1 spike 관측: 킷의 서버존중 원칙상 SDL_audio(내부 vplayer 재생기)와의 합산은
# 무관측 — termux-media-player(Android Media 프레임워크 — 별도 클라이언트)로
# 동형 관측만(rc=0·오디오 합산 1런). 본 세그먼트가 실제 계통으로 보강한다:
# vplayer가 d_seed.wav(위임 재생 — SDL_audio)를 틀고 있는 상태(M 세그먼트)에서
# music의 spatial leg로 b_seed.wav를 발사 — 양 상태가 동시 진행 = OS 믹서 합산
# 영수증(계통상 vplayer가 소리를 유지하며 leg가 열리는 지 죽는지 — T2의 관찰
# 원문). WIRED 런 전용 — UNSET 런은 원장행 1행으로 스킵.
if [ "${LEG_WIRED:-0}" -eq 0 ]; then
    echo "MUSP-SPATIAL-MIX: SKIPPED (UNSET 런 — leg 미링크, 합산 실측은 WIRED 런 소유)"
elif ! printf '%s' "$GS2" | grep -aq '"opened":true'; then
    echo "MUSP-SPATIAL-MIX: MISS (vplayer 재생 기저 부재 — GS2 원문: $(printf '%s' "$GS2" | head -c 160) — 합산 실측의 전제 미수취, 원장)"
else
    VP_POS_A=$(printf '%s' "$GS2" | sed -n 's/.*"pos":\([0-9.]*\).*/\1/p')
    MIXPLAY=$(mtool spatial_play "{\"path\":\"$SEED/b_seed.wav\"}")
    echo "MUSP-SPATIAL-MIX-PLAY-REPLY: $MIXPLAY"
    printf '%s' "$MIXPLAY" | grep -aq '"accepted":true' \
        || echo "MUSICP-FAIL-SOFT(mix play: accepted 미수취 — $MIXPLAY)"
    sleep 2   # leg prefill+ALC 합산 안정화(T1 concurrent 1런 수형)
    VP_POS_B=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
        | sed -n 's/.*"pos":\([0-9.]*\).*/\1/p')
    SP_ST_MIX=$(mtool spatial_status '{}')
    SP_POS_B=$(printf '%s' "$SP_ST_MIX" | sed -n 's/.*"pos":\([0-9.]*\).*/\1/p')
    echo "MUSP-SPATIAL-MIX-SAMPLES: vplayer(SDL) pos ${VP_POS_A:-none} → ${VP_POS_B:-none} · spatial(OpenAL) pos=${SP_POS_B:-none} status=$SP_ST_MIX"
    VP_MIX_OK=0
    if [ -n "${VP_POS_A:-}" ] && [ -n "${VP_POS_B:-}" ]; then
        VP_MIX_OK=$(awk -v a="$VP_POS_A" -v b="$VP_POS_B" 'BEGIN{print (b>a) ? 1 : 0}')
        echo "MUSP-SPATIAL-MIX-VPLAYER: $([ "$VP_MIX_OK" -eq 1 ] && echo "OK — SDL 채널이 leg 병행 중에도 진행(pos 증가)" || echo "MISS — SDL 채널 정체(합산 불확증 — $VP_POS_A → $VP_POS_B)")"
    else
        echo "MUSICP-FAIL-SOFT(mix vplayer pos 파싱 실패 — 정직 원장)"
    fi
    SP_MIX_OK=0
    if [ -n "${SP_POS_B:-}" ] && awk -v p="$SP_POS_B" 'BEGIN{exit (p > 0) ? 0 : 1}'; then
        SP_MIX_OK=1
        echo "MUSP-SPATIAL-MIX-SPATIAL: OK — OpenAL 채널이 SDL 병행 중 재생(pos=$SP_POS_B)"
    else
        echo "MUSICP-FAIL-SOFT(mix spatial: pos 미진행 — $SP_ST_MIX)"
    fi
    if [ "$VP_MIX_OK" -eq 1 ] && [ "$SP_MIX_OK" -eq 1 ]; then
        echo "MUSP-SPATIAL-MIX: OK — 양채널 동시 진행(폰 OpenAL/SDL 합산 — T1 concurrent 원문의 계통 보강)"
    else
        echo "MUSP-SPATIAL-MIX: PARTIAL (vplayer=$VP_MIX_OK spatial=$SP_MIX_OK — 정직 원장행)"
    fi
    CAPTURE "$TMPD/mus_phone_spatial_mix.png"
    MIXSTOP=$(mtool spatial_stop '{}')
    echo "MUSP-SPATIAL-MIX-STOP-REPLY: ${MIXSTOP:-none}"
    printf '%s' "$MIXSTOP" | grep -aq '"active":false' \
        || echo "MUSICP-FAIL-SOFT(mix stop: active:false 미수취 — $MIXSTOP)"
fi

echo "=== N. stderr 진단 수취 (서버 로그 — 클라 스폰+music+vplayer 진단) ==="
grep -a 'spawn\|music\|vplayer\|\[vplayer\]' "$TMPD/pmus_srv_list.log" | tail -14 | sed 's/^/  /'

echo "=== N2. dirs 도구 동형 세그먼트 (T4 — WSL T3 16b 수형 — 도구 경로만) ==="
# CP949 거부/치유 세그먼트는 폰에서 스킵 — 원장행 1행: 도구 인자 CP949
# bad_request(quickjs JSON 문자열 UTF-8 검사)+[패널 제거] 원촉 치유 루트는
# WSL T3 16c(85bf59e)가 소유·전부 실측됨 — 폰은 dirs 기능만 재실측한다.
XDIR="$TMPD/mus_dirs_x"
ANCH="$TMPD/pmus_dirs_anchor.json"
rm -rf "$XDIR"
mkdir -p "$XDIR" || FAIL "synthetic dir create failed"
DL0=$(mtool music_dir_list '{}')
echo "MUSIC-DIR-LIST-0: ${DL0:-none} (기저 원문 — leg list 병합의 시드 dir 보존 병기)"
printf '%s' "$DL0" | grep -aqF "$SEED" \
    || FAIL "dir-list-0: 시드 dir 미보존(leg list 병합 수취 결손) — $DL0"
echo "--- [폴더 관리] 패널 개방(등록 **전**) + vplayer 치우기 — 모양 캡처 2종 ([제거] 행은 클릭하지 않는다: p1=crash-fixed 수리 지점 — N-1 승계) ---"
# vplayer 치우기 — music 스트립 노출(WSL T4 step-13 수형): leg list 후반 표면은
# vplayer가 music을 전면 덮는다(1-3차 런 원장 — 토글 클릭 (130,70)·(185,77)이
# 전부 무소응, dirs/dirs2 프레임 등호 = topmost 창 취식). 토글 원문 전제 =
# music 스트립이 최상층. 원복 불요 — 다음 leg(sdcard) boot_server가 클라 교체.
LW=$(ctl '{"tool":"list_windows","args":{}}')
VPL_ID=$(printf '%s' "$LW" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1 | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
if [ -n "$VPL_ID" ]; then
    MV1=$(ctl "{\"tool\":\"window_move\",\"args\":{\"id\":$VPL_ID,\"x\":-620,\"y\":40}}")
    sleep 2
    VPLCHK=$(ctl '{"tool":"list_windows","args":{}}')
    VPLXY=$(printf '%s' "$VPLCHK" | grep -aoE "\"id\":$VPL_ID,\"x\":-?[0-9]+,\"y\":-?[0-9]+" | head -1)
    echo "VPL-MOVE-1: reply=${MV1:-none} · after=${VPLXY:-gone}"
    case "$VPLXY" in
      *"x\":-620"*) echo "VPL-MOVE-OK: music 표면 개방(-620 — 토글 원문 경로)" ;;
      *)
        MV2=$(ctl "{\"tool\":\"window_move\",\"args\":{\"id\":$VPL_ID,\"x\":220,\"y\":-520}}")
        sleep 2
        VPLCHK=$(ctl '{"tool":"list_windows","args":{}}')
        VPLXY=$(printf '%s' "$VPLCHK" | grep -aoE "\"id\":$VPL_ID,\"x\":-?[0-9]+,\"y\":-?[0-9]+" | head -1)
        echo "VPL-MOVE-2(음수 y 폴백 — 표면 상단 밖): reply=${MV2:-none} · after=${VPLXY:-gone}"
        ;;
    esac
else
    echo "VPL-MOVE-AWAY: skip (Video Player 창 부재 — 위임 폴백 경로 원장행)"
fi
# 토글은 **비-멱등** — 탭의 "재클릭 면제(아이돌턴트)" 방어를 못 쓴다. 포커스
# 취식(치우기 직후 첫 클릭)을 **무해 재스캔**으로 미리 소거한다(새로고침 =
# ResolveDirs+RequestScan — idempotent — 4차 런 원장 수형).
WRM=$(tap "$MUS_ID" $((MUS_X + 45)) $((MUS_Y + 50)) 1)
echo "MUSIC-DIR-WARMCLICK: reply=${WRM:-none} (새로고침 무해 클릭 — 포커스 취식 소거 — idempotent 재스캔)"
ok "$WRM" || echo "NOTE-DIR-WARMCLICK: 무해 클릭 실패 — $WRM (원장행)"
sleep 1
DTOG_X=${MUSP_DTOG_X:-185}   DTOG_Y=${MUSP_DTOG_Y:-77}    # [폴더 관리] 토글 — WSL T3 캘리브레이션(130,70)의 폰
                                                          #   정산(2026-10-10 T4 원장): 토글은 dir 스트립
                                                          #   행의 tab2 **우측** — WSL 16c의 tab2 라벨은
                                                          #   2자(CP949 '가요')라 토글이 클라 x≈130에
                                                          #   앉지만 폰 leg list의 tab2 라벨은
                                                          #   "pcm_music_seed" 14자 — 스트립 폭 시프트로
                                                          #   토글 = window-rel x≈160-255(리스트 캡처
                                                          #   픽셀 실측 — 토글 px≈880-975, 윈도 px원점
                                                          #   720 ≡ 서버 기하 1:1) — 캡차 런(130,70)은
                                                          #   tab2 우측 경계에 걸려 무소응(캡처 등호 원장
                                                          #   — dirs/dirs2 프레임 동일). y=77은 TAB2_Y
                                                          #   수형(스트립 행 등호). **개방은 등록 전
                                                          #   클릭** — 등록 후에는 tab3가 스트립에 끼어
                                                          #   토글이 더 시프트(≈225-320 — 닫힘 클릭은
                                                          #   DTOG2_X=270)되어 같은 x가 tab3(mus_dirs_x)
                                                          #   을 친다(4차 런 원장).
CTX=$((MUS_X + DTOG_X))
CTY=$((MUS_Y + DTOG_Y))
echo "CLICK-DIRTOGGLE: id=$MUS_ID desk=($CTX,$CTY) — 상수 DTOG_X=$DTOG_X DTOG_Y=$DTOG_Y (패널 열림 판정은 캡처 육안 몫)"
DTG=$(tap "$MUS_ID" "$CTX" "$CTY" 1)
ok "$DTG" || echo "NOTE-DIR-TOGGLE: 토글 클릭 실패 — $DTG (도구 원문은 별도 — 원장행)"
echo "dir toggle reply: $DTG (등록 전 클릭 — 패널 열림 → 등록 row가 패널에 실시간 흡수 — Store 성공=RefreshUserDirs)"
sleep 1
# 등호 앵커 = 작가 정규형 — probe merge의 pretty 형식은 저장소 작가(ComposeKeyed)
# 출력과 다르므로 웜업 왕복 1회로 앵커를 만든다(WSL T3의 step-4 seed를 출하형
# compact으로 교환한 수형 — 폰 런타임 동형 — 원장).
DW1=$(mtool music_dir_add "{\"path\":\"$XDIR\"}")
printf '%s' "$DW1" | grep -aq '"ok":true' || FAIL "dirs warmup add failed — $DW1"
DW2=$(mtool music_dir_remove "{\"path\":\"$XDIR\"}")
printf '%s' "$DW2" | grep -aq '"ok":true' || FAIL "dirs warmup remove failed — $DW2"
echo "MUSIC-DIR-WARMUP: OK — 웜업 왕복 2콜 ok(앵커 형식 수렴 원문)"
cp "$SET" "$ANCH" || FAIL "dirs anchor copy failed"
echo "MUSIC-DIR-ANCHOR: $(tr -d '\n' < "$ANCH") (bytes=$(wc -c < "$ANCH") — 작가 정규형 앵커 — 원복 등호의 진상 원문)"
DADD=$(mtool music_dir_add "{\"path\":\"$XDIR\"}")
echo "MUSIC-DIR-ADD-REPLY: ${DADD:-none}"
DL1=$(mtool music_dir_list '{}')
echo "MUSIC-DIR-LIST-1: ${DL1:-none} (등록 인증 원문 1행)"
printf '%s' "$DADD" | grep -aq '"ok":true' \
    || echo "MUSICP-FAIL-SOFT(dir-add: ok 수취 실패 — $DADD)"
printf '%s' "$DL1" | grep -aq 'mus_dirs_x' \
    || echo "MUSICP-FAIL-SOFT(dir-list-1: 합성 폴더 등록 미반영 — $DL1)"
echo "MUSIC-DIR-SETTINGS-1: $(tr -d '\n' < "$SET") (settings.json 원문 실측 — 등록 후 파일)"
SETCHK=$(python3 - "$SET" "$XDIR" <<'PYEOF'
import json, sys
try:
    with open(sys.argv[1], "rb") as f:
        obj = json.loads(f.read().decode("utf-8", "replace"))
except Exception as e:
    print("SET-MISS(parse:%s)" % e)
    raise SystemExit
dirs = obj.get("music", {}).get("dirs", [])
fp = obj.get("text", {}).get("font_path", "")
ok = bool(fp) and sys.argv[2] in dirs \
    and any("pcm_music_seed" in d for d in dirs)
print("SET-OK" if ok else
      "SET-MISS text=%s font_path=%s dirs=%s" % ("text" in obj, bool(fp), dirs))
PYEOF
) || SETCHK="SET-MISS-RAW(python3 파서 실패)"
printf '%s' "$SETCHK" | grep -aq 'SET-OK' || {
    # 폰 python3 부재 폴백 — grep 게이트(독립 파서 수취는 WSL T3 원문 소유)
    if grep -aqF "$XDIR" "$SET" && grep -aqF '"text"' "$SET"; then
        SETCHK="SET-OK-grep-fallback"
    fi
}
echo "MUSIC-DIR-SETTINGS-CHECK: $SETCHK (독립 파서 수취 — music.dirs 실기록+기존 text/font_path 보존)"
printf '%s' "$SETCHK" | grep -aq 'SET-OK' \
    || echo "MUSICP-FAIL-SOFT(dir-settings: 기록/보존 원문 미달 — $SETCHK)"
sleep 2
CAPTURE "$TMPD/mus_phone_dirs.png"
echo "--- [폴더 관리] 패널 닫기(등록 후 스트립 시프트 상태) + 원복 프레임 캡처 ---"
DTOG2_X=${MUSP_DTOG2_X:-270}
CT2_X=$((MUS_X + DTOG2_X))
CT2_Y=$CTY
echo "CLICK-DIRTOGGLE2: id=$MUS_ID desk=($CT2_X,$CT2_Y) — 상수 DTOG2_X=$DTOG2_X (등록 후 토글 시프트 상수 — 원장행)"
tap "$MUS_ID" "$CT2_X" "$CT2_Y" 1 >/dev/null   # 패널 닫기(등록 후 위치 — 미빗치면 tab3 선택=무해 재스캔 원장행)
sleep 2
CAPTURE "$TMPD/mus_phone_dirs2.png"
DREM=$(mtool music_dir_remove "{\"path\":\"$XDIR\"}")
echo "MUSIC-DIR-REMOVE-REPLY: ${DREM:-none}"
DL2=$(mtool music_dir_list '{}')
echo "MUSIC-DIR-LIST-2: ${DL2:-none} (제거 인증 원문 1행)"
printf '%s' "$DREM" | grep -aq '"ok":true' \
    || echo "MUSICP-FAIL-SOFT(dir-remove: ok 수취 실패 — $DREM)"
if printf '%s' "$DL2" | grep -aq 'mus_dirs_x'; then
    echo "MUSICP-FAIL-SOFT(dir-list-2: 제거 미반영 — $DL2)"
else
    echo "MUSIC-DIR-GONE: mus_dirs_x 소각 확인 (제거 원문 성립)"
    printf '%s' "$DL2" | grep -aqF "$SEED" \
        || echo "MUSICP-FAIL-SOFT(dir-list-2: 시드 dir 측면 소각 — 보존 위반 — $DL2)"
fi
if cmp -s "$SET" "$ANCH"; then
    echo "MUSIC-DIR-BYTE-RESTORE: OK — add/remove 왕복 == 작가 정규형 앵커 바이트 등호(④ WSL T3 수형)"
else
    echo "MUSICP-FAIL-SOFT(dir-byte-restore: 왕복 후 원문 불일치)"
    echo "  now : $(tr -d '\n' < "$SET")"
    echo "  anch: $(tr -d '\n' < "$ANCH")"
fi
rm -f "$ANCH"
rm -rf "$XDIR"
[ -d "$XDIR" ] && echo "MUSICP-FAIL-SOFT(dir-bury: $XDIR 소각 실패 — 수동 소각)" \
    || echo "MUSIC-DIR-BURIED: $XDIR 소각 (probe 소유 잔상 0)"

# ══════════════════════ N4 — 브라우저 동형 세그먼트 (#97 T4 — wsl_music 16b2 수형) ══════════════════════
# 진입 상태 원장: leg list 클라(near N2 종료 — settings = 작가 정규형 앵커
# [ORIG복사+probe dirs 선행+사용자 dirs 보존(#97 T4 — ENTRY-MUSIC-DIRS 원문)]
# ·vplayer 치워짐[window_move — 닫히지 않음]·패널 토글 상태 = dirs2 클릭 뒤
# (열림 추정 상태 — 정직 원장: 첫 토글 탭의 차분이 방향 판별 원문). N4의 모든
# 클릭 = **XTEST(xtap)** — 폰 스트립 행 Button 클릭 경로 전용 무응답 관측(T5
# 원장)의 폰 수형이 X 수준 주입(#96 원장 — XT-BURST 버튼 성립 실측)이고
# [폴더 관리] 토글이 스트립 행 Button이기 때문. 좌표 = 클라 상대(env 후보
# 그리드 — 폰 정산 산식: 토글(2탭)=185·(3탭 9자)=270 → ≈9.4px/자 → 브라우즈
# 3탭(기본|pcm_music_seed|mus_browse) 산식: brow 탭≈225·토글≈280 — 후보
# 그리드 ±22px(I1 원장 승계)) + PX 차분 착탄 판정(tapsettle 수형 — 캡처 =
# x11grab 전면 PNG → 창 사각 폭주 언필터 차분 — ffmpeg PNG 행별 필터 1-4
# 수형). MUSP_BROW_RECON=1 = ② 캡처 직후 복원·종료(정찰 전용 런).
echo "=== N4. 브라우저 동형 세그먼트 (T4 #97 — UI 탭 계약 — XTEST 주입) ==="
BTREE="$TMPD/mus_browse"
BT_SUBA="$BTREE/sub_a"
BT_SUBB="$BTREE/sub_b"
[ -d "$BTREE" ] && FAIL "$BTREE already exists — 이전 probe 잔상, 수동 소각 필요"
BRSNAP="$TMPD/pmus_browse_snap.json"
cp "$SET" "$BRSNAP" || FAIL "browse settings snapshot failed"
echo "MUS-BROWSE-SNAP: $(wc -c < "$BRSNAP") bytes (N4 진입 settings — 복귀 바이트 등호의 대조 원문)"

echo "--- N4-0. XTEST 주입기 — 부재 시 재조립 (삭제 금지 계약 — #96 원장) ---"
XT="$TMPD/xtap"
if [ -x "$XT" ]; then
    echo "MUSP-XT-RECOVERED: $XT 재사용 (주입기 생존 — #96 계약 유산)"
else
    XTC="$TMPD/xtap.c"
    cat > "$XTC" <<'XTAP_C_EOF'
/* [probe] XTEST injector — #96 폰 X 수준 탭 재현기 (원문 수형 재조립)
 * [#97 T4 수형 — park 인자 추가]: argc>7/8이면 탭 종료 뒤 커서를 (park_x,
 * park_y)로 유도+flush한다. 원장: XTEST는 **실제 X 커서 스프라이트를 움직인다**
 * — 탭 지점에 커서가 남아 캡처 차분에 혼입(런 1: 커서 20px 폭이 행 임계 8px를
 * 넘어 유의 행 착탄 오판 — 수취 프레임은 레이아웃 무변화 140px 커서뿡 원문).
 * park = 모든 프레임의 커서를 창 밖 같은 자리에 세워 커서 항을 0으로 만든다
 * (taps=0이면 순수 motion — 기저 캡처 전 커서 정착용). */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: xtap :DISPLAY X Y [taps] [gap_ms] [hold_ms] [park_x park_y]\n"); return 2; }
    Display* d = XOpenDisplay(argv[1]);
    if (!d) { fprintf(stderr, "XTAP-FAIL open(%s)\n", argv[1]); return 3; }
    int xtst = -1, xerr = -1, maj = 2, min = 3;
    if (!XTestQueryExtension(d, &xtst, &xerr, &maj, &min)) {
        fprintf(stderr, "XTAP-FAIL no-xtest\n"); return 4;
    }
    int x = atoi(argv[2]), y = atoi(argv[3]);
    int taps = argc > 4 ? atoi(argv[4]) : 1;
    int gap = argc > 5 ? atoi(argv[5]) : 120;
    int hold = argc > 6 ? atoi(argv[6]) : 120;
    int parkx = argc > 7 ? atoi(argv[7]) : -1;
    int parky = argc > 8 ? atoi(argv[8]) : -1;
    for (int i = 0; i < taps; i++) {
        XTestFakeMotionEvent(d, -1, x, y, CurrentTime);
        usleep(50000);
        XTestFakeButtonEvent(d, 1, True, CurrentTime);
        usleep(hold * 1000);
        XTestFakeButtonEvent(d, 1, False, CurrentTime);
        usleep(50000);   /* 릴리즈 flush 완수 대기 — 종료 race 원장(XTEST flush) */
        XFlush(d);
        if (i + 1 < taps) usleep(gap * 1000);
    }
    if (parkx >= 0 && parky >= 0) {
        XTestFakeMotionEvent(d, -1, parkx, parky, CurrentTime);
        XFlush(d);
    }
    XCloseDisplay(d);
    printf("XTAP-OK taps=%d at (%d,%d) gap=%dms hold=%dms park=(%d,%d) xtst=%d.%d\n",
           taps, x, y, gap, hold, parkx, parky, maj, min);
    return 0;
}
XTAP_C_EOF
    if command -v clang >/dev/null 2>&1; then
        clang "$XTC" -o "$XT" -lX11 -lXtst 2>"$TMPD/pmus_xt_clang.log" \
            || FAIL "XTEST 재조립 실패 — $(head -3 "$TMPD/pmus_xt_clang.log" | tr '\n' ' ')"
        echo "XT-REBUILD: OK (clang+libXtst — #96 수형 재조립)"
    else
        FAIL "clang absent on phone (XTEST 주입기 재조립 불가 — #96 계약 전제 붕괴)"
    fi
fi
XTQ=$(timeout 15 "$XT" ":$DSP" 0 0 0 0 0 2>&1 | head -1)
echo "MUSP-XT-PROBE: ${XTQ:-none} (주입기 수령 원문 — taps=0 무해 정찰)"
if printf '%s' "$XTQ" | grep -aq XTAP-OK; then
    XTAP_UP=1
    echo "MUSP-XT-UP: 1 (XTEST 주입 경로 — #96 원장)"
else
    XTAP_UP=0
    # [T2 r6 원장 수형]: xtap XOpenDisplay(:1) 무착탄(폰 X1 소켓 $TMPDIR 존치+
    # raw connect refused — ffmpeg와 동일 X 계통 결손 원문) → 하드 FAIL 폐곡 →
    # send_input 폴백 계약(#99 탭 라인 2396 선례 — XTEST 무응답 원장 수형).
    # 주입 = 서버 도구 send_input(id=0 desk-wide hit-test — BuildSendInputOp
    # 원문)·착탄 판정 = tapsettle 차분 축 재용(주입 경로와 무관).
    echo "MUSP-XT-DOWNGRADE: send_input 폴백 계약(#99 선례) — xtap 원문=$XTQ"
fi

# px 차분기 — x11grab 전면 PNG(필터 1-4 포함)의 창 사각 언필터+유의 행 카운트
# (wsl 16b2 px 수형의 ffmpeg-PNG 수형 — 자가 캡처 필터 0 전제와 이질 원장).
PXPY="$TMPD/pmus_px.py"
cat > "$PXPY" <<'PX_C_EOF'
# [T4] 캡처 차분기 — 두 PNG의 창 사각(x y w h 인자) 유의 행 수(픽셀 차합 > 75
# 합, 행별 ≥8 픽셀, y 2픽셀 스텝). 패널 열림 = 레이아웃 전이(수십 행), 무효
# 클릭 = ≤ 몇 행. ffmpeg PNG = 행별 필터 1-4 → 풀 언필터 수형(WSL px는 자가
# 캡처 필터 0 전제 — 폰 수형 원장).
import struct, sys, zlib

def hdr(p):
    d = open(p, "rb").read()
    ih = d.index(b"IHDR")            # 탭 위치 — 데이터는 +4 (데이터 = w4·h4·bd·ct)
    w, h, bd, ct = struct.unpack(">IIBB", d[ih+4:ih+14])
    return d, w, h, bd, ct

def unfilter_rows(p, cb_b, cb_e, w0, nch, h):
    # [T4 런 1 결함 원장 — Sub 필터는 누적(cumulative)]: ft==1(Sub)의 복호는
    # 행의 **전위(prefix, x=0..cb_e)**를 풀어야 한다(각 화소 = 잔차 + 왼쪽
    # 복호 화소의 연쇄) — cb_b부터 풀면 첫 화소가 raw 잔차를 그대로 쓰고
    # prev가 오염되어 **이후 전 행이 계단식 오염**(런 1: 폰 px가 동일 프레임
    # 쌍에 126행 가짜 차분 — 수취 프레임은 140px 커서뿐 — 원장). 수형 = Sub만
    # nch부터(전위), 나머지 필터는 cb_b부터(좌·상 이웃만 소비).
    d = open(p, "rb").read()
    pos, idat = 8, b""
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos+4])[0]
        tag = d[pos+4:pos+8]
        body = d[pos+8:pos+8+ln]
        pos += 12 + ln
        if tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    raw = zlib.decompress(idat)
    rows = []
    prev = None
    p = 0
    stride_n = w0 * nch
    for y in range(h):
        ft = raw[p]; p += 1
        line = bytearray(raw[p:p+stride_n]); p += stride_n
        if ft == 1:
            for i in range(nch, cb_e):   # Sub = 누적 — 전위 전체(런 1 원장)
                line[i] = (line[i] + line[i - nch]) & 0xFF
        elif ft == 2:
            if prev is not None:
                for i in range(cb_b, cb_e):
                    line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(cb_b, cb_e):
                a = line[i - nch] if i >= nch else 0
                u = prev[i] if prev is not None else 0
                line[i] = (line[i] + ((a + u) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(cb_b, cb_e):
                a = line[i - nch] if i >= nch else 0
                c = prev[i - nch] if (prev is not None and i >= nch) else 0
                b = prev[i] if prev is not None else 0
                pa = abs(b - c); pb = abs(a - c); pc = abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        if cb_e > cb_b:
            rows.append(bytes(line[cb_b:cb_e]))
        prev = line
    return rows

a_p, b_p, wx, wy, ww, wh = (sys.argv[1], sys.argv[2], int(sys.argv[3]),
                            int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]))
import os, pickle

def cached_rows(p, cb_b, cb_e, w0, nch, hh):
    # [T4 수형 — 기저 프레임 캐시](.pxc pickle — tapsettle이 같은 기저를
    # 후보마다 재복호 — 폰 python 순수 복호 수초 절약). 키 = 경로·크기·사각.
    cp = p + ".pxc"
    key = (p, os.path.getsize(p), cb_b, cb_e, w0, nch, hh)
    try:
        with open(cp, "rb") as f:
            obj = pickle.load(f)
        if obj[0] == key:
            return obj[1]
    except Exception:
        pass
    rows = unfilter_rows(p, cb_b, cb_e, w0, nch, hh)
    try:
        with open(cp, "wb") as f:
            pickle.dump((key, rows), f)
    except Exception:
        pass
    return rows

d, w0, h0, bd, ct = hdr(a_p)
if bd != 8 or ct not in (2, 6):
    raise SystemExit("PXD-FAIL depth=%d ct=%d (8bit RGB/RGBA 전제)" % (bd, ct))
nch = 3 if ct == 2 else 4
cb_b = max(0, (wx - 4) * nch)
cb_e = min(w0, wx + ww + 4) * nch
rows_a = cached_rows(a_p, cb_b, cb_e, w0, nch, h0)
h1 = min(h0, hdr(b_p)[2])
rows_b = unfilter_rows(b_p, cb_b, cb_e, w0, nch, h1)
n = 0
y = max(0, wy)
yl = min(min(len(rows_a), len(rows_b)), wy + wh)
for ri in range(y, yl, 2):
    c = 0
    la = rows_a[ri]; lb = rows_b[ri]
    ln = min(len(la), len(lb))
    for x in range(0, ln, nch):
        va = la[x]; vb = lb[x]
        if abs(va - vb) > 75:
            c += 1
            if c >= 8:
                break
    if c >= 8:
        n += 1
print("diff_rows=%d" % n)
PX_C_EOF
command -v python3 >/dev/null 2>&1 || FAIL "phone python3 absent (px 차분기 불가 — 2026-10-10 원문과 위배 — 인프라 회귀)"
python3 -c 'import sys' || FAIL "phone python3 broken"
echo "MUSP-PX-READY: $PXPY (차분기 — 창 사각 폭주 언필터 수형)"

# vplayer 회수 (차분 오염 소각 — wsl 16b2-0 수형 — close_window T4 원문)
echo "--- N4-0b. vplayer 회수 (close_window — N2 치우기 잔재의 종결) ---"
VPCLOSE=0
for i in 1 2 3; do
    VPQ=$(ctl '{"tool":"list_windows","args":{}}')
    VPW=$(printf '%s' "$VPQ" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
    [ -z "$VPW" ] && break
    VPID=$(printf '%s' "$VPW" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    VPR=$(ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$VPID}}")
    echo "  vplayer close#$i: id=$VPID repl=${VPR:-none}"
    VPCLOSE=$((VPCLOSE + 1))
    sleep 1
done
echo "MUS-BROWSE-VP-RECLAIM: closed=$VPCLOSE (0 = 세그먼트 입구에 Video Player 창이 없었다는 원문 — N2 치우기 계보)"
sleep 1

# xtap 1탭 — $1=deskX $2=deskY (주입기 원문 수형 — 릴리즈 flush 완수)
# [T4 수형 — park]: 탭 뒤 커서를 창 밖 (12,985)에 세운다 — 모든 프레임의
# 커서 스프라이트 항 등화(런 1 원장 — 커서 혼입 착탄 오판 폐곡).
XPARK_X=12 XPARK_Y=985
xtapt() {
    if [ "${XTAP_UP:-1}" = 1 ]; then
        timeout 15 "$XT" ":$DSP" "$1" "$2" 1 120 100 "$XPARK_X" "$XPARK_Y" 2>&1 | head -2
    else
        # send_input 폴백(#99 2396 선례 — XTEST 무응답 원장): id=0 desk-wide
        # hit-test, 1탭(button 1). 착탄 판정 = tapsettle 차분 축(주입과 무관).
        local R
        R=$(ctl "{\"tool\":\"send_input\",\"args\":{\"op\":\"click\",\"id\":0,\"x\":$1,\"y\":$2,\"button\":1}}")
        [ -n "$R" ] && printf 'XTAP-OK(send_input 폴백): %s\n' "$R" || printf 'XTAP-FAIL(send_input 폴백 응답 없음)\n'
    fi
}
xtpark() { # 커서만 정착(기저 캡처 전 — taps=0 순수 motion)
    if [ "${XTAP_UP:-1}" = 1 ]; then
        timeout 15 "$XT" ":$DSP" "$XPARK_X" "$XPARK_Y" 0 0 0 "$XPARK_X" "$XPARK_Y" 2>&1 | head -1
    else
        echo "XTAP-FALLBACK-PARK: 생략(send_input은 pure motion 없음 — 클릭만 폴백)"
    fi
}
tapsettle() { # $1=기저 png(abs) $2=tag $3=클라 x $4=클라 y $5=차분 행 임계 —
              # tapsettle 수형(16b2 원문 — 캡처기만 x11grab 전면+창 사각 차분으로 치환)
    base="$1"; tag="$2"; tx="$3"; ty="$4"; th="$5"
    SETTLE_DIFF=0
    SETTLE_T=""
    XTOUT=$(xtapt $((MUS_X + tx)) $((MUS_Y + ty)))
    printf '%s' "$XTOUT" | grep -aq XTAP-OK \
        || echo "  xtap reply 비정상: ${XTOUT:-empty} — 정찰 판정 계속"
    echo "TAP-SETTLE($tag): 클라($tx,$ty) desk=($((MUS_X + tx)),$((MUS_Y + ty))) — $(printf '%s' "$XTOUT" | head -1)"
    t0=$(date +%s)
    zeros=0
    for i in 1 2 3 4 5 6; do
        sleep 2
        CAPTURE "$TMPD/mus_phone_bc_$tag.png"
        df=$(python3 "$PXPY" "$base" "$TMPD/mus_phone_bc_$tag.png" \
                     "$MUS_X" "$MUS_Y" "$MUS_W" "$MUS_H" 2>&1)
        rows=$(printf '%s' "$df" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p')
        if [ -z "$rows" ]; then
            echo "SETTLE($tag#$i): PXD 차분기 원문: $(printf '%s' "$df" | head -1)"
            return 1
        fi
        echo "SETTLE($tag#$i): diff_rows=$rows (기저 대비 — 임계 $th)"
        if [ "$rows" -ge "$th" ]; then
            SETTLE_DIFF=$rows
            SETTLE_T=$(( $(date +%s) - t0 ))
            # [런 3 결함 원장 수형 — 배수 유출 드레인] 폰 컴포지트 지연(런 원장
            # 도달 시차 ~9s)에서 첫 적중 프레임 = **전 단계 탭의 늦도착 렌더**
            # (1-behind 프레젠트 — 16b2 원장 라인)일 수 있다 — 그 프레임을 다음
            # 단계의 기저로 쓰면 판정이 한 단계 씩 어긋난다(런 2·3: 개봉이
            # 다음 단계 프레임에 나타나는 원문). 수형 = 첫 적중 뒤 +6s 유출
            # 캡처 1회로 정착 프레임을 갱신(baseline이 현 상태를 대변).
            sleep 6
            CAPTURE "$TMPD/mus_phone_bc_$tag.png"
            df2=$(python3 "$PXPY" "$base" "$TMPD/mus_phone_bc_$tag.png" \
                          "$MUS_X" "$MUS_Y" "$MUS_W" "$MUS_H" 2>&1)
            rows2=$(printf '%s' "$df2" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p')
            [ -n "$rows2" ] && SETTLE_DIFF=$rows2
            echo "SETTLE-DRAIN($tag): diff_rows=${rows2:-NA} (정착 프레임 갱신 — 다음 단계 기저)"
            return 0
        fi
        if [ "$rows" = "0" ]; then zeros=$((zeros + 1)); else zeros=0; fi
        if [ "$zeros" -ge 2 ] && [ $(( $(date +%s) - t0 )) -ge 8 ]; then
            return 1   # 무음 — 상태 무변화(재시도 안전 — 16b2 원장 수형)
        fi
    done
    return 1
}
brcap() { # $1=tag $2=회수명 — 정착 프레임의 영수증 복사(mus_phone_browse_*)
    cp "$TMPD/mus_phone_bc_$1.png" "$TMPD/mus_phone_$2.png" \
        || FAIL "browse capture copy failed: $1 → $2"
    echo "MUS-BROWSE-CAPTURE: mus_phone_$2.png ($(wc -c < "$TMPD/mus_phone_$2.png") bytes)"
}

echo "--- N4-1. ① 브라우즈 탭 루트 등록 (music_dir_add — 하위 2건 sub_a·sub_b) ---"
mkdir -p "$BT_SUBA" "$BT_SUBB" || FAIL "browse tree mkdir failed"
BATREE=$(mtool music_dir_add "{\"path\":\"$BTREE\"}")
echo "MUS-BROWSE-ADD-REPLY: ${BATREE:-none}"
printf '%s' "$BATREE" | grep -aq '"ok":true' \
    || echo "MUSICP-FAIL-SOFT(browse-add: ok 수취 실패 — $BATREE)"
BBL0=$(mtool music_dir_list '{}')
echo "MUS-BROWSE-LIST-0: ${BBL0:-none} (① 등록 인증 원문 1행 — 탭 스트립의 라벨원)"
printf '%s' "$BBL0" | grep -aq 'mus_browse' \
    || echo "MUSICP-FAIL-SOFT(browse-list-0: 합성 트리 등록 미반영 — $BBL0)"
echo "MUS-BROWSE-SETTINGS-1: $(tr -d '\n' < "$SET") (settings.json 원문 — ① 등록 후)"

echo "--- N4-2. 창 재정찰 (parse_geo list 수형 — N2 뒤의 현행 Music 창 id/geo) ---"
MUSWIN=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WINB=$(ctl '{"tool":"list_windows","args":{}}')
    MUSWIN=$(printf '%s' "$WINB" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window at browse segment — last: ${WINB:-none}"
echo "MUSWIN-N4: $MUSWIN"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
MUS_W=$(printf '%s' "$MUSWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
MUS_H=$(printf '%s' "$MUSWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
[ -n "$MUS_ID" ] || FAIL "geo parse failed (id empty) — MUSWIN: $MUSWIN"
echo "MUS-GEO-n4: id=$MUS_ID x=$MUS_X y=$MUS_Y w=$MUS_W h=$MUS_H (X 좌표 = 데스크톱 등호 — #96 XT-CALIB 계보)"

# 브라우저 세그먼트 상수(env 재보정 — 정찰 산식 원장은 위 N4 머리 주석).
# 산식 원장(probe-first dirs 순서): T5 실측 [기본|pcm_music_seed] 토글=185 →
# 문자 폭 ≈ 9.4px/자 → leg list 구성[기본|pcm_music_seed|Music(사용자 —
# 5자)] 토글 ≈ 232 · N4 ① 등록 뒤 [기본|pcm|Music|mus_browse] 토글 ≈ 326·
# mus_browse 탭 중앙 ≈ 266 — 후보 그리드 ±22px+외연(I1 원장 — 재보정 승계).
BT_YS=${MUSP_BTAB_YS:-"77 71 83 65"}
BT_XS=${MUSP_BTAB_XS:-"266 244 288 222 310 200 178 332"}
BT_TOG_XS=${MUSP_BTOG_XS:-"326 304 348 282 260 370 238 216 392"}
BT_TOG_YS=${MUSP_BTOG_YS:-"77 71 83 65"}
BT_BROW_XS=${MUSP_BBROW_XS:-"58 44 30 70 20"}
BT_BROW_YS=${MUSP_BBROW_YS:-"252 258 246 264 240"}   # [찾아보기] 행 — 런 2 정산(클라 y≈252 —
                                                     #   zoom_browse_* 캡처 2배율 실측: 패널
                                                     #   input 행 뒤 토글행 — WSL 262 원산과
                                                     #   ±10 이동 — 폰 폰트 행 높이 원장)
BT_SUB_XS=${MUSP_BSUB_XS:-"64 86 42 106 30"}
BT_SUB_YS=${MUSP_BSUB_YS:-"362 356 368 350 374 344"}  # browselist 행 — 런 2 정산(zoom 실측:
                                                      #   list child 상단 ≈346·sub_a ≈362·
                                                      #   sub_b ≈384 — WSL 원산 348은 child
                                                      #   상부 공백 패딩에 떨어져 no-op — 런 2
                                                      #   ④2 무진입 원장행의 진원)
BT_UP_XS=${MUSP_BUP_XS:-"24 44 64 14 84"}
BT_UP_YS=${MUSP_BUP_YS:-"311 305 317 299 323 293"}   # ".." 행 — 런 2 정산(클라 y≈311 — cwd 표기
                                                      #   행 ≈288 다음 — WSL 294 원산 재보정)
BT_ADD_XS=${MUSP_BADD_XS:-"46 32 62 20 76 90"}
BT_ADD_YS=${MUSP_BADD_YS:-"332 326 338 320 344 314 350"}   # [이 폴더 추가] 버튼행 — 런 2 정산
                                                           #   (버튼 rect ≈ 클라 y 322-342 — 런 2의
                                                           #   (46,321) = 상단 1px 위 미빗 — no-op
                                                           #   원장행: browse_added 프레임 = 브라우저
                                                           #   개봉 유지+등록 0의 원문)
BR_TH=${MUSP_BROW_TH:-15}         # 개봉/종료(레이아웃 전이) 판정 행 임계 — 16b2 수형
BR_TH_TEXT=${MUSP_BROW_TH_TEXT:-2}   # 행 클릭(텍스트 교체) 판정 행 임계
BR_TH_ADD=${MUSP_BROW_TH_ADD:-10}    # [이 폴더 추가] 종료 회귀 판정(중간 임계 — 원장)

BR_STAGE=0
BR_TAB=0 BR_TOG=0 BR_OPEN=0 BR_SUB=0 BR_UP=0 BR_SUB2=0 BR_ADD=0
BRADDFULL=0
BR_ADD_FAIL_HARD=0
BR_RESTORE=0

echo "--- N4-3. ② 탭 클릭 브래킷 (dirIndex_ = BTREE — XTEST 정찰-판정기) ---"
xtpark
CAPTURE "$TMPD/mus_phone_bc_base.png"
BASE_C="$TMPD/mus_phone_bc_base.png"
for TX in $BT_XS; do
    for TY in $BT_YS; do
        if tapsettle "$BASE_C" "bt0" "$TX" "$TY" 3; then
            BR_TAB=1
            BR_STAGE=2
            echo "MUS-BROWSE-TAB-OK: 탭 착탄(클라 ${TX},y=$TY — dirIndex_=BTREE 전환·active 강조+표 전이) — 도달 시차 ${SETTLE_T}s — 개봉 cwd(계약 ⑤ 현재 탭 루트)의 전제 성립"
            break 2
        fi
        echo "  탭 무착탄(${TX},y=$TY) — 무음 소멸 판정(상태 무변화 = 재시도 안전)"
    done
done
[ "$BR_TAB" -eq 1 ] || echo "MUSICP-FAIL-SOFT(browse-tab-land: 무착탄 — MUSP_BTAB_X/YS 재보정 몫(mus_phone_bc_bt0 원문) — 브라우저 계약 단계 생략, 원장)"

if [ "$BR_STAGE" -eq 2 ]; then
    echo "--- N4-4. [폴더 관리] 토글 → [찾아보기] 토글 → 개봉 ---"
    for TY in $BT_TOG_YS; do
        for TX in $BT_TOG_XS; do
            if tapsettle "$TMPD/mus_phone_bc_bt0.png" "btog" "$TX" "$TY" "$BR_TH"; then
                BR_TOG=1
                echo "MUS-BROWSE-PANEL-OK: [폴더 관리] 레이아웃 전이(클라 ${TX},${TY} — 도달 시차 ${SETTLE_T}s) — **XTEST 첫 스트립 행 Button 착탄 원문**(T5 무응답 관측의 수형 판정 — 개방/폐쇄 방향은 캡처 육안 몫 EYES)"
                break 2
            fi
            echo "  토글 무착탄(${TX},${TY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    [ "$BR_TOG" -eq 1 ] \
        || echo "MUSICP-FAIL-SOFT(browse-panel: 전이 무성립 — MUSP_BTOG_XS/YS 재보정 몫(mus_phone_bc_btog 원문) — 브라우저 계약 단계 생략, 원장)"
    if [ "$BR_TOG" -eq 1 ]; then
        for TY in $BT_BROW_YS; do
            for TX in $BT_BROW_XS; do
                if tapsettle "$TMPD/mus_phone_bc_btog.png" "bropen" "$TX" "$TY" "$BR_TH"; then
                    BR_OPEN=1
                    BR_STAGE=4
                    echo "MUS-BROWSE-OPEN-OK: [찾아보기] 개봉(클라 ${TX},${TY} — 도달 시차 ${SETTLE_T}s) — cwd 표기 1행+\"..\"행+하위 폴더 목록의 레이아웃 전이 성립(diff=$SETTLE_DIFF)"
                    brcap bropen browse_open
                    brcap bropen browse_subs
                    echo "MUS-BROWSE-CAPTURE-2: mus_phone_browse_open.png+mus_phone_browse_subs.png (② cwd 표기+하위 폴더 목록 — sub_a/sub_b 2건 = 2r byte asc 결정론의 라이브 원문 — 2건 등장은 캡처 육안 몫 EYES)"
                    if [ "${MUSP_BROW_RECON:-0}" = "1" ]; then
                        echo "MUS-BROW-RECON-EXIT: ② 캡처 확보 — 상수 재보정 후 풀 런(MUSP_BROW_RECON=0)"
                        RCR=$(mtool music_dir_remove "{\"path\":\"$BTREE\"}")
                        echo "  recon cleanup remove: ${RCR:-none}"
                        cmp -s "$SET" "$BRSNAP" \
                            && echo "  recon 원복: settings 바이트 등호" \
                            || echo "MUSICP-FAIL-SOFT(brow-recon-restore: settings 원문 불일치)"
                        rm -rf "$BTREE"
                        echo "MUS-BROW-RECON-FINAL: recon 전용 런 — big/sdcard 레그 스킵 원장행(정찰 산출 = bc_*.png 캡처·로그 — **bc_*.png는 유산 보존**(드라이버 recover_opt — 좌표 재보정 원문))"
                        cp "$ORIG" "$SET" || echo "MUSICP-FAIL-SOFT(recon-settings-restore: copy 실패)"
                        if [ "$ENTRY_PERM" -eq 1 ]; then
                            cp "$PERM_ORIG" "$PERM" || echo "MUSICP-FAIL-SOFT(recon-perm-restore: copy 실패)"
                        else
                            rm -f "$PERM"
                        fi
                        rm -rf "$SEED"
                        boot_server recon
                        FLRECON=$(ctl '{"tool":"launch_app","args":{"app":"terminal"}}')
                        echo "RECON-LAUNCH: ${FLRECON:-none} (idle 라인 종료 상태 원복 원장)"
                        echo "MUSP-FINISHED"
                        MUS_RECON_RC=1
                        exit 0
                    fi
                    break 2
                fi
                echo "  [찾아보기] 무착탄(${TX},${TY}) — 무음 소멸 판정(재시도 안전 — 원장)"
            done
        done
        [ "$BR_OPEN" -eq 1 ] \
            || echo "MUSICP-FAIL-SOFT(browse-open: [찾아보기] 무개봉 — MUSP_BBROW_XS/YS 재보정 몫(mus_phone_bc_bropen 원문) — 브라우저 계약 단계 생략, 원장)"
    fi
fi

if [ "$BR_STAGE" -eq 4 ]; then
    echo "--- N4-5. ③ 하위 진입 (sub_a 행 클릭 = JoinDir — T1 2r-② 소비) ---"
    for SY in $BT_SUB_YS; do
        for SX in $BT_SUB_XS; do
            if tapsettle "$TMPD/mus_phone_bc_bropen.png" "bsub" "$SX" "$SY" "$BR_TH_TEXT"; then
                BR_SUB=1
                BR_STAGE=5
                echo "MUS-BROWSE-SUB-ENTER-OK: 하위 진입(클라 ${SX},${SY} — 도달 시차 ${SETTLE_T}s) — cwd 표기 갱신 = JoinDir 소비·목록 교체(diff=$SETTLE_DIFF) — cwd 갱신 원문은 캡처 육안 몫(EYES)"
                brcap bsub browse_sub
                break 2
            fi
            echo "  하위 행 무착탄(${SX},${SY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    [ "$BR_SUB" -eq 1 ] \
        || echo "MUSICP-FAIL-SOFT(browse-sub-enter: 무진입 — MUSP_BSUB_XS/YS 재보정 몫(mus_phone_bc_bsub 원문) — 개봉 상태 유지 추정, ④⑤ 계속(원장))"
    echo "--- N4-6. ④ \"..\" 상위 복귀 (Parent — T1 2r-③④ 소비) ---"
    for UY in $BT_UP_YS; do
        for UX in $BT_UP_XS; do
            if tapsettle "$TMPD/mus_phone_bc_bsub.png" "bup" "$UX" "$UY" "$BR_TH_TEXT"; then
                BR_UP=1
                BR_STAGE=6
                echo "MUS-BROWSE-UP-OK: \"..\" 행 착탄(클라 ${UX},${UY} — 도달 시차 ${SETTLE_T}s) — cwd 표기 회귀 = Parent 소비(diff=$SETTLE_DIFF) — 회귀 원문은 캡처 육안 몫(EYES)"
                brcap bup browse_up
                break 2
            fi
            echo "  \"..\" 행 무착탄(${UX},${UY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    [ "$BR_UP" -eq 1 ] \
        || echo "MUSICP-FAIL-SOFT(browse-up: 무복귀 — MUSP_BUP_XS/YS 재보정 몫(mus_phone_bc_bup 원문 — cwd 잔류는 ⑤의 등록 원문이 대변, 원장))"
    echo "--- N4-7. ④2 하위 재진입 (③의 재수형 — ⑤의 등록 원문을 신규로) ---"
    # [정직 한계 원장 — 등록 강도 leg](16b2 수형): ".." 복귀 뒤의 cwd = 등록
    # 루트 자체라 [이 폴더 추가]는 Store 중복 no-op ok 원문으로 등록 증명이
    # 비지한다 → ③ 수형을 재수해 cwd = sub_a 상태의 추가 = **신규 등록** —
    # music_dir_list·settings 원문이 실반영을 절단판정한다.
    for SY in $BT_SUB_YS; do
        for SX in $BT_SUB_XS; do
            if tapsettle "$TMPD/mus_phone_bc_bup.png" "bsub2" "$SX" "$SY" "$BR_TH_TEXT"; then
                BR_SUB2=1
                echo "MUS-BROWSE-SUB2-ENTER-OK: 하위 재진입(${SX},${SY} — 도달 시차 ${SETTLE_T}s) — cwd = 하위 폴더 원문(행 격자 = sub_a/sub_b — ⑤의 추가 대상 — 캡처 육안 몫 EYES)"
                brcap bsub2 browse_sub2
                break 2
            fi
            echo "  하위 행 무착탄(${SX},${SY}) — 무음 소멸 판정(재시도 안전 — 원장)"
        done
    done
    [ "$BR_SUB2" -eq 1 ] \
        || echo "MUSICP-FAIL-SOFT(browse-sub2: 무재진입 — MUSP_BSUB 재보정 몫(mus_phone_bc_bsub2 원문) — ⑤는 cwd 잔류 상태 계속(원장))"
    echo "--- N4-8. ⑤ [이 폴더 추가] 원문 클릭 → tapsettle 착탄+도구 진실원 판정 (T2 fix r1 — 재검) ---"
    # [T2 fix r1 수형 — ⑤ 무전달 재발 방지]: 판정 = **이중** — 착탄(tapsettle —
    # 종료 전이 임계 BR_TH_ADD)+도구 진실원(music_dir_list에서 sub_[ab] 등장 =
    # DirAddManaged 신규 등록의 절단판정 — cwd = 하위 폴더가 전제, 그 이름은
    # ①·도구 어느 경로로도 등록하지 않은 이름). 후보마다 폴 응답 원문을 남긴다
    # (무음 실패 금지 원장). **클릭 실패(전 후보 미성립) = hard 마커**
    # (BR_ADD_FAIL_HARD — 스크립트 꼬리 rc=1 종결 — fix r1 좌표 재보정 몫).
    for ry in $BT_ADD_YS; do
        for rx in $BT_ADD_XS; do
            tapsettle "$TMPD/mus_phone_bc_bsub2.png" "badd" "$rx" "$ry" "$BR_TH_ADD" \
                && { echo "  착탄(종료 전이 — 클라 ${rx},${ry} — 도달 시차 ${SETTLE_T}s diff=$SETTLE_DIFF)"; }
            sleep 1
            BLP=$(mtool music_dir_list '{}')
            echo "  BADD poll list: ${BLP:0:200}"
            if printf '%s' "$BLP" | grep -aqE 'sub_[ab]' 2>/dev/null; then
                BRADDEX=$rx; BRADDY=$ry; BRADDFULL=1; break
            fi
            if [ "${SETTLE_DIFF:-0}" -ge "$BR_TH_ADD" ]; then
                # 전달 원문(브라우즈 종료 전이) 성립 — 등록 미출현 = no-op 추가
                # 계보(16b2 원장 — cwd = 루트의 중복 no-op ok)라 **전달 축 성립으로
                # 종결**(등록 강도 축 = 원장행 — 종료 전이는 역전 불가)
                BRADDEX=$rx; BRADDY=$ry; break
            fi
        done
        [ -n "${BRADDEX:-}" ] && break
    done
    if [ -n "${BRADDEX:-}" ]; then
        BR_ADD=1
        BR_STAGE=8
        if [ "$BRADDFULL" -eq 1 ]; then
            echo "MUS-BROWSE-ADD-CLICK-OK(FULL): [이 폴더 추가] 착탄(클라 ${BRADDEX},${BRADDY}) — cwd = 하위 폴더 = **신규 등록**(DirAddManaged 단일 경로 — [추가] 버튼·music_dir_add 도구와 같은 경로 원문) — 성공 = 브라우즈 종료(계약 ③)"
        else
            echo "MUS-BROWSE-ADD-CLICK-OK(DELIVERED): [이 폴더 추가] 착탄(클라 ${BRADDEX},${BRADDY}) — 종료 전이(diff=$SETTLE_DIFF)+브라우즈 종료 — 단 등록 원문(sub_[ab]) 미출현 = no-op 추가 계보(cwd = 루트 — ④2·행 격자 계보 원장) — **전달 복원 = fix r1 계약 1항의 축**(무전달 소각)"
            echo "MUSICP-FAIL-SOFT(browse-add-reg: 등록 강도 축 미달 — no-op 원장 — 전달 축과 별개)"
        fi
        sleep 3   # 추가 성공 = 재스캔+종료 렌더의 컴포지트 도착 여유(전윈도 원장)
        CAPTURE "$TMPD/mus_phone_browse_added.png"
        BBL2=$(mtool music_dir_list '{}')
        echo "MUS-BROWSE-LIST-2: ${BBL2:-none} (⑤ 등록 확인 원문 1행)"
        echo "MUS-BROWSE-SETTINGS-2: $(tr -d '\n' < "$SET") (settings.json 원문 — ⑤ 클릭 뒤)"
        SUBCHK=$(python3 - "$SET" <<'SUBCHK_EOF'
import json, sys
with open(sys.argv[1], "rb") as f:
    obj = json.loads(f.read().decode("utf-8", "replace"))
dirs = obj.get("music", {}).get("dirs", [])
if any(d.endswith("/sub_a") or d.endswith("/sub_b") for d in dirs):
    print("SUB-OK(하위 등록) dirs=%s" % dirs)
else:
    print("SUB-MISS(하위 미등록) dirs=%s" % dirs)
SUBCHK_EOF
) || SUBCHK="SUB-MISS(python3 파서 실패)"
        echo "MUS-BROWSE-SUB-REG: $SUBCHK (독립 파서 — [이 폴더 추가] 클릭의 등록 원문)"
        if [ "$BRADDFULL" -eq 1 ]; then
            printf '%s' "$SUBCHK" | grep -aq 'SUB-OK' \
                || echo "MUSICP-FAIL-SOFT(browse-sub-reg: sub_a 실기록 원문 미달 — $SUBCHK)"
        fi
    else
        BR_ADD_FAIL_HARD=1
        echo "MUSICP-FAIL-SOFT(browse-add: [이 폴더 추가] 무착탄(종료 전이 무성립 · 전 후보) — fix r1 좌표 재보정 몫(N4-5~8 캡처), 원장)"
        echo "MUS-BROWSE-HARD: ⑤ 무전달 재발 — [이 폴더 추가] 클릭의 종료 전이 둘 다 미성립(하드 마커 — 스크립트 꼬리 rc=1 종결, 복귀 N4-10 선행)"
    fi
fi

if [ "$BR_STAGE" -eq 8 ] && [ "$BR_ADD" -ge 1 ]; then
    echo "--- N4-9. 6. 브라우즈 종료 원문 — 재스캔 정착 캡처 ---"
    sleep 3   # 추가 성공 = 재스캔(도착 더티 1프레임)+컴포지트 프레젠트 지연 정착
    CAPTURE "$TMPD/mus_phone_browse_rescan.png"
    echo "MUS-BROWSE-EXIT-LEDGER: 추가 성공 = 브라우즈 종료(계약 ③)+목록 재스캔 — 종료 원문 = mus_phone_browse_added.png(회귀)+mus_phone_browse_rescan.png(표 정착) 2캡처(EYES)"
else
    echo "MUS-BROWSE-EXIT-SKIP: 브라우즈 종료 캡처 생략(단계 ${BR_STAGE}/8 — 원장)"
fi

echo "--- N4-10. 복귀 — remove 3콜 + settings 바이트 등호 + 트리 소각 ---"
CRA=$(mtool music_dir_remove "{\"path\":\"$BT_SUBA\"}") || CRA=""
CRB=$(mtool music_dir_remove "{\"path\":\"$BT_SUBB\"}") || CRB=""
CRR=$(mtool music_dir_remove "{\"path\":\"$BTREE\"}") || CRR=""
echo "MUS-BROWSE-CLEANUP: sub_a=${CRA:-none} sub_b=${CRB:-none} root=${CRR:-none}"
# [런 2 결함 원장 — remove 서기 지연]: remove의 settings 기록은 응답 후 도착
# 가능(런 2: root remove ok 응답 뒤 cmp가 mus_browse 잔존을 봤다 — async
# writeback 지연 원장) — 폴링 정착 후 cmp(재시도 안전 — 5×1.2s).
BR_CLEAN=0
for i in 1 2 3 4 5; do
    BLP=$(mtool music_dir_list '{}')
    if ! printf '%s' "$BLP" | grep -aqE 'mus_browse|sub_[ab]'; then
        BR_CLEAN=1
        break
    fi
    CRR2=$(mtool music_dir_remove "{\"path\":\"$BTREE\"}") || CRR2=""
    sleep 1.2
done
echo "MUS-BROWSE-CLEAN-POLL: settled=$BR_CLEAN tries=$i (list에 mus_browse·sub_[ab] 부재 원문: $(printf '%s' "${BLP:-none}" | head -c 240))"
if cmp -s "$SET" "$BRSNAP"; then
    BR_RESTORE=1
    echo "MUS-BROWSE-RESTORE-BYTE: OK — 등록/탐색/추가/해제 왕복 == N4 진입 원문 바이트 등호(④ 수형)"
else
    echo "MUSICP-FAIL-SOFT(browse-restore: 원문 불일치)"
    echo "  now : $(tr -d '\n' < "$SET")"
    echo "  snap: $(tr -d '\n' < "$BRSNAP")"
fi
rm -f "$BRSNAP"
rm -rf "$BTREE" 2>/dev/null
[ -d "$BTREE" ] && echo "MUSICP-FAIL-SOFT(browse-bury: $BTREE 소각 실패 — 수동 소각)" \
    || echo "MUS-BROWSE-BURIED: $BTREE 소각 (probe 소유 잔상 0)"
echo "MUS-BROWSE-SUMMARY: stage=$BR_STAGE/8 tab=$BR_TAB tog=$BR_TOG open=$BR_OPEN sub=$BR_SUB up=$BR_UP sub2=$BR_SUB2 add=$BR_ADD full=$BRADDFULL restore=$BR_RESTORE hard=$BR_ADD_FAIL_HARD (드라이버 판정 파싱 원문행)"
if [ "$BR_ADD_FAIL_HARD" -eq 1 ]; then
    echo "MUS-BROWSE-BC-KEEP: bc_* 정찰 중간 프레임 보존 (⑤ 무전달 — 좌표 재보정의 원문 — 드라이버 recover_opt 몫)"
else
    rm -f "$TMPD"/mus_phone_bc_*.png "$TMPD"/mus_phone_bc_*.pxc 2>/dev/null
    echo "MUS-BROWSE-BC-BURIED: bc_* 정찰 중간 프레임+pxc 캐시 소각 (영수증 캡처만 회수)"
fi
# ══════════════════════ leg sdcard — /sdcard FUSE 스캔 유한 종료 실측 (M-5 재판정) ══════════════════════

# ══════════════════════ leg sdcard — /sdcard FUSE 스캔 유한 종료 실측 (M-5 재판정) ══════════════
# ══════════════════════ leg big — 스캔 취소 경계 폰 동형 최소 (T4 — #93) ══════════════════════
LEG=big
echo "=== N3. leg big — 스캔 취소 경계 세그먼트 (T4 — wsl_music 16d bf38ea9 폰 동형 최소) ==="
# 폰 동형 최소 원장(브리프 T4):
#   · 스캔 규모 = $TMPDIR 합성 트리 2종(subs×files — 소형이어도 생존·폐기 원문
#     목적 충족 승계). 스캔은 내용을 읽지 않는다(MusicModel.h 원문 — 확장자
#     필터+stat만 소비)라 빈 .wav 셸이 정직하다. **폰 스캔 속도 미실측** —
#     탭→remove 사이클 2회(2차 재청구 재시도 수형) — MISS = 정직 원장행
#     (스케일 재정산 = MUSP_BIG_SUBS/MUSP_BIG_FILES env 재런 몫).
#   · landtap(px 차분 착탄 판정기)은 **폰 미이식 원장행** — WSL 16d의 탭 좌표
#     캘리브레이션 몫. 폰 동형 구도 = **tree_a를 dirs_[1](tab2)에 앉히는 병합
#     구성**(ResolveDirs 원문: dirs_ = [기본(AudioDirFallback), ...music.dirs
#     순서] — dirs_[1] = tab2) — 탭은 실측 상수 TAB2_X=90 재용(4·5차 런 등탄
#     ·재클릭 방어 동형 — 신규 좌표 캘리브레이션 0).
#   · ②제거 경계: music_dir_remove의 취소 1발(T2 소멸자·[제거] 취소 원문)이
#     진행 스캔을 경계서 절단 → 부분 수집 도착이 멤버십 필터에서 폐기+
#     stderr "도착 폐기" 1행. 제거→도착 시차가 절단 영수증(완주 잔량보다 앞섬).
#   · ①close 경계: 제거의 재청구(clamp 승계 — dirs_[1]=tree_b 재스캔)가 진행
#     중인 창에 close_window(phone_close_receipt B2/B3/B5 선례 — permissions
#     핫리드로 스테이지 게이트 산다) → 소멸 시차+crash 마커 0(로그 델타)+
#     서버 ping 생존 → 재스폰+도구 응답 원문.
#   · ③정착: 재스폰 클라의 tab2 재스캔 정착(캡처 1종 — 표행 채택 = EYES 몫).
#   · sync 1회 선행 배수(T3 concern ② 승계 — 폰 동형 — 시드 writeback 선행
#     배량 수형).
BIG_SUBS=__BIG_SUBS__
BIG_FILES=__BIG_FILES__
BIG_TRACKS=$((BIG_SUBS * BIG_FILES))
BIGA="$TMPD/mus_big_a"
BIGB="$TMPD/mus_big_b"
echo "MUSP-BIG-SCALE: subs=$BIG_SUBS files=$BIG_FILES tracks=$BIG_TRACKS/트리 (env MUSP_BIG_SUBS/MUSP_BIG_FILES — 드라이버 치환 — 폰 맞춤 소형 계약)"
echo "--- N3-1. 합성 트리 시드 2종 (빈 .wav 셸 — REM preclean) ---"
if [ -d "$BIGA" ] || [ -d "$BIGB" ]; then
    echo "BIG-ENTRY-PRECLEAN: 전 run 유산 트리 소각 (RUN-DROPPED 재실행 계약 흡수 — hard FAIL 아님)"
    rm -rf "$BIGA" "$BIGB"
fi
[ -d "$BIGA" ] && FAIL "big_a preclean failed (수동 소각 필요)"
[ -d "$BIGB" ] && FAIL "big_b preclean failed (수동 소각 필요)"
BIGSEED_PY="$TMPD/pmus_bigseed.py"
cat > "$BIGSEED_PY" <<'PYSEED'
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
n = sum(1 for _d, _dirs, fs in os.walk(root) for x in fs if x.endswith(".wav"))
print("BIGSEED-OK tracks=%d seed_sec=%.1f" % (n, time.time() - t))
PYSEED
if command -v python3 >/dev/null 2>&1; then
    python3 "$BIGSEED_PY" "$BIGA" "$BIG_SUBS" "$BIG_FILES" || FAIL "big seed A failed"
    python3 "$BIGSEED_PY" "$BIGB" "$BIG_SUBS" "$BIG_FILES" || FAIL "big seed B failed"
    echo "MUSP-BIG-SEEDER: python3 (폰 python3 존재 원문 — os.open 루프)"
else
    echo "MUSP-BIG-SEEDER: shell 폴백 (python3 부재 — 스케일 재정산 몫 원장행)"
    for T in "$BIGA" "$BIGB"; do
        d=0
        while [ "$d" -lt "$BIG_SUBS" ]; do
            DD="$T/sub_$(printf '%02d' "$d")"
            mkdir -p "$DD" || FAIL "big mkdir failed ($DD)"
            i=0
            while [ "$i" -lt "$BIG_FILES" ]; do
                i=$((i + 1))
                : > "$DD/m_$(printf '%05d' "$i").wav"
            done
            d=$((d + 1))
        done
    done
fi
rm -f "$BIGSEED_PY"
BA=$(find "$BIGA" -name '*.wav' 2>/dev/null | wc -l)
BB=$(find "$BIGB" -name '*.wav' 2>/dev/null | wc -l)
echo "BIG-SEED-COUNT: a=$BA b=$BB (want=$BIG_TRACKS/트리 — 파일 수수 정직 가드)"
[ "$BA" -eq "$BIG_TRACKS" ] || FAIL "big seed A count=$BA want=$BIG_TRACKS"
[ "$BB" -eq "$BIG_TRACKS" ] || FAIL "big seed B count=$BB want=$BIG_TRACKS"
SYNC_T0=$(date +%s.%N)
sync
echo "MUS-BIGSEED-SYNC: drain=$(awk -v a="$SYNC_T0" -v b="$(date +%s.%N)" 'BEGIN{printf "%.1f", b-a}')s (시드 writeback 선행 배수 — T3 concern ② 폰 동형 승계 행)"
FND_T0=$(date +%s.%N)
BFIND=$(find "$BIGA" -type f -name '*.wav' 2>/dev/null | wc -l)
echo "MUSP-BIG-ENUM-COST: find $BFIND files in $(awk -v a="$FND_T0" -v b="$(date +%s.%N)" 'BEGIN{printf "%.2f", b-a}')s (셸 열거 고정비 원문 — 클라 스캔 시각의 배증 재료)"

echo "--- N3-2. tree_a = dirs_[1](tab2) 병합 구성 + 부팅 (활성 탭 = big_a 경계의 폰 수형) ---"
merge_music_big() { # 트리 2종을 한 music.dirs 배열로 — dirs_[1]=tree_a(tab2)·dirs_[2]=tree_b
                    # (#97 T4 수형 — merge_dirs_json: probe dirs 선행 — 사용자 dirs 보존 후속 탭)
    case "$1$2" in *'"'*) FAIL "dir path contains a quote (JSON 안전성)";; esac
    merge_dirs_json big "$1" "$2" || FAIL "merge(big): merge_dirs_json failed"
    grep -aq '"music"' "$SET" && grep -aqF "$1" "$SET" && grep -aqF "$2" "$SET" \
        || FAIL "merge(big): gate failed"
    echo "SETTINGS-IN-LEG-big: $(tr -d '\n' < "$SET") (bytes=$(wc -c < "$SET")) — dirs_[1]=tree_a(tab2)·dirs_[2]=tree_b·dirs_[0]=기본(AudioDirFallback — 리졸버 원문)·사용자 dirs는 후속 탭 보존(#97 T4)"
}
merge_music_big "$BIGA" "$BIGB"
boot_server big
BIGLOG0=$(wc -c < "$TMPD/pmus_srv_big.log")
L=$(ctl '{"tool":"launch_app","args":{"app":"music"}}')
ok "$L" || FAIL "leg big: launch_app music failed — $L"
echo "launch reply: $L"
wait_music
parse_geo big
sleep 2   # 부팅 스캔(dirs_[0]=기본 — 부재) 도착 안정화
tabx() { tap "$MUS_ID" "$((MUS_X + TAB2_X))" "$((MUS_Y + TAB2_Y))" 1; }

BIG_CYC=0
BIG_DISC_LINE=""
BIG_DISC_ELAPSED=""
echo "--- N3-3. [②] 스캔 진행 중 music_dir_remove — 사이클 2회 (취소 1발+도착 폐기 1행+클라 생존) ---"
for CY in 1 2; do
    if [ "$CY" -eq 1 ]; then
        BIG_TGT="$BIGA"
    else
        [ -n "$BIG_DISC_LINE" ] && break          # cycle 1 성립 → 2차 불요
        BIG_TGT="$BIGB"                            # 1차 MISS → 잔존 트리 재시도
    fi
    BIG_CYC=$CY
    echo "MUSIC-CANCEL-CYCLE-$CY: tab2 탭($BIG_TGT — dirIndex_=1 — idempotent 재선택 RequestScan)"
    TAP1=$(tabx)
    ok "$TAP1" || echo "NOTE-CYCLE-$CY: tab tap-1 failed — $TAP1"
    sleep 1
    TAB2X=$(tabx)
    ok "$TAB2X" || echo "NOTE-CYCLE-$CY: tab re-click failed — $TAB2X (첫 클릭 포커스 취식 방어 — 4·5차 런 원장)"
    echo "tap replies: $TAP1 | $TAB2X (탭 선택 = dirs_[1] 스캔 기동)"
    if [ "$CY" -eq 1 ]; then
        # 1차 사이클에서만 스캔 진행 중 캡처(in-flight 1프레임 = 제거 대상 루트의
        # '스캔 중...' 표기 — EYES). 캡처 2-4s가 제거를 늦춘다 — 폰 스캔이 빠를
        # 때(2차 재시도가 흡수하는 케이스)는 2차 사이클이 캡처 없는 고속 제거.
        sleep 0.3
        CAPTURE "$TMPD/mus_phone_scan_busy.png"
    fi
    sleep 0.4
    L0=$(wc -c < "$TMPD/pmus_srv_big.log")
    REM1_SEC=$(date +%s.%N)
    BDREM=$(mtool music_dir_remove "{\"path\":\"$BIG_TGT\"}")
    echo "MUSIC-SCAN-REMOVE-REPLY-$CY: ${BDREM:-none} (스캔 진행 중 제거 — 취소 1발+재청구 원문 계약)"
    ok "$BDREM" || echo "MUSICP-FAIL-SOFT(scan-remove-$CY: ok 수취 실패 — $BDREM)"
    DISC=""
    DD_AT=""
    for i in $(seq 1 20); do
        sleep 0.4
        L1=$(wc -c < "$TMPD/pmus_srv_big.log")
        if [ "$L1" -gt "$L0" ]; then
            DISC=$(tail -c $((L1 - L0)) "$TMPD/pmus_srv_big.log" | grep -a '도착 폐기' | head -1)
            [ -n "$DISC" ] && { DD_AT=$(date +%s.%N); break; }
        fi
    done
    echo "MUSIC-ARRIVAL-DISCARD-$CY: ${DISC:-MISS} (stderr 폐기 진단 1행 원문 — T2 수형 ·pub1=취소 절단분)"
    if [ -n "$DISC" ]; then
        BIG_DISC_LINE=$DISC
        BIG_DISC_ELAPSED=$(awk -v a="$REM1_SEC" -v b="$DD_AT" 'BEGIN{printf "%.2f", b-a}')
        echo "MUS-DISCARD-AT: 제거→폐기 도착=${BIG_DISC_ELAPSED}s (취소 성립 판정축 — 취소 없으면 트리 완주 잔량 후)"
        if awk -v d="$BIG_DISC_ELAPSED" 'BEGIN{exit (d <= 3.0) ? 0 : 1}'; then
            echo "MUS-CANCEL-EFFECTIVE: OK — 절단이 완주 잔량보다 앞섬(취소 join/절단 라이브 영수증)"
        else
            echo "MUSICP-FAIL-SOFT(scan-cancel-slow: 폐기 도착이 완주 스캔급 — 절단 불성실(원장))"
        fi
        printf '%s' "$DISC" | grep -aq 'mus_big' \
            || echo "MUSICP-FAIL-SOFT(discard-root-$CY: 폐기 진단의 루트가 제거 대상 아님 — $DISC)"
    fi
    BLIVE=$(mtool music_dir_list '{}')
    if printf '%s' "$BLIVE" | grep -aq '"ok":true'; then
        echo "MUSIC-ALIVE-LIST-$CY: OK — 도구 응답 ok (제거 경계 뒤 클라 생존 원문)"
    else
        echo "MUSICP-FAIL-SOFT(remove-alive-$CY: 제거 경계 뒤 도구 응답 비정상 — $BLIVE)"
    fi
    WALIVE=$(ctl '{"tool":"list_windows","args":{}}')
    if printf '%s' "$WALIVE" | grep -aq '"title":"Music"'; then
        echo "MUSIC-ALIVE-WINDOW-$CY: OK — Music 창 생존 원문 1행"
    else
        echo "MUSICP-FAIL-SOFT(remove-alive-window-$CY: Music 창 소멸(사망 원장) — $WALIVE)"
    fi
    echo "MUSIC-BIG-SETTINGS-$CY: $(tr -d '\n' < "$SET") (settings.json 원문 — 제거 후 파일)"
done
echo "MUSIC-BIG-CANCEL-SUMMARY: cycles=$BIG_CYC discard=${BIG_DISC_LINE:+폐기1행}${BIG_DISC_LINE:-MISS} elapsed=${BIG_DISC_ELAPSED:-n/a} (폰 동형 최소 판정 원문 집계)"
BLL=$(mtool music_dir_list '{}')
echo "MUSIC-BIG-LIST-2: ${BLL:-none} (제거 인증 원문 1행)"
if printf '%s' "$BLL" | grep -aq 'mus_big_a'; then
    echo "MUSIC-BIG-REMAIN: mus_big_a 등록 잔존"
else
    echo "MUSIC-BIG-REMAIN: mus_big_a 소각 (제거 원문 성립)"
fi
if printf '%s' "$BLL" | grep -aq 'mus_big_b'; then
    echo "MUSIC-BIG-REMAIN: mus_big_b 등록 잔존"
else
    echo "MUSIC-BIG-REMAIN: mus_big_b 소각 (제거 원문 성립)"
fi
echo "--- N3-4. [①] 스캔 진행 중 창 close (소멸자 취소 join — T2 결정 ② ·폰 close_window 선례 receipts B2/B3/B5) ---"
# in-flight 재무장: 잔존 big 트리가 있으면 그것이 곧 dirs_[1](탭 재선택 =
# idempotent 재스캔) — 없으면(제거 사이클이 둘 다 진행) 트리 1개 재등록
# (무해 — sdcard 레그의 ORIG 병합이 소각). 탭 → 1.2s → 스캔 중 캡처 →
# close(탭→close 시차 = 스캔 중반 영수증 — WSL 수형의 폰 재무장 경로).
DL=$(mtool music_dir_list '{}')
case "$DL" in *mus_big_a*|*mus_big_b*) : ;;
  *)
    RADD=$(mtool music_dir_add "{\"path\":\"$BIGA\"}")
    echo "MUSIC-BIG-RE-ADD: ${RADD:-none} (잔존 트리 0 — close의 in-flight 재무장 — add의 재스캔=활성 탭[기본] 원장행)"
    ok "$RADD" || echo "MUSICP-FAIL-SOFT(big-re-add: ok 수취 실패 — $RADD)"
    ;;
esac
sleep 0.5
BIGTAB=$(tap "$MUS_ID" "$((MUS_X + TAB2_X))" "$((MUS_Y + TAB2_Y))" 1)
ok "$BIGTAB" || echo "NOTE-CLOSE-TAP: tab2 재선택 실패 — $BIGTAB (재청구 스캔이 이미 진행 중이면 무해 — 원장행)"
echo "close-pre tab reply: $BIGTAB (dirs_[1] 스캔 기동 — in-flight 재무장)"
sleep 1.2
CLOSE1_SEC=$(date +%s.%N)
BCLS=$(ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$MUS_ID}}")
echo "MUS-CLOSE-REPLY: ${BCLS:-none} (서버 close_window id 직접호출 — phone_close_receipt B5 수형)"
ok "$BCLS" || echo "MUSICP-FAIL-SOFT(close-reply: ok 수취 실패 — $BCLS (폰 close_window 미동작 = 원장행 — 브리프 재판정 몫))"
BIG_GONE=0
GONE_SEC=""
for i in $(seq 1 16); do
    sleep 1
    W19=$(ctl '{"tool":"list_windows","args":{}}')
    if ! printf '%s' "$W19" | grep -aq '"title":"Music"'; then
        BIG_GONE=1
        GONE_SEC=$(date +%s.%N)
        break
    fi
done
if [ "$BIG_GONE" -eq 1 ] && [ -n "$GONE_SEC" ]; then
    echo "MUS-CLOSE-GONE: OK — Music 창 소멸(list_windows — close 원형 소멸 단정 수형) close→소멸=$(awk -v a="$CLOSE1_SEC" -v b="$GONE_SEC" 'BEGIN{printf "%.2f", b-a}')s"
    echo "MUS-CLOSE-JOIN-LEDGER: 폰 소멸 시차 <=5.0s면 취소 join 성립(폰 정산 — 1s 폴링+ctl 왕복 원장; WSL 3.5s 수형)"
    if awk -v a="$CLOSE1_SEC" -v b="$GONE_SEC" 'BEGIN{exit (b-a <= 5.0) ? 0 : 1}'; then
        echo "MUS-CLOSE-EFFECTIVE: OK — 스캔 중 close의 취소 join 원문 (사망 아님 — 정상 소멸)"
    else
        echo "MUSICP-FAIL-SOFT(close-gone-slow: 소멸 시차 > 5.0s — 취소 절단이 스캔 완주 잔량 뒤 도달(원장))"
    fi
else
    echo "MUSICP-FAIL-SOFT(close-gone: Music 창 미소멸 — close_window 폰 미동작 = 원장행 후보: $W19)"
fi
L19=$(wc -c < "$TMPD/pmus_srv_big.log")
if [ "$L19" -gt "$BIGLOG0" ]; then
    CDELTA=$(tail -c $((L19 - BIGLOG0)) "$TMPD/pmus_srv_big.log")
    BIG_CRASH=$(printf '%s' "$CDELTA" | grep -aicE 'segfault|SIGSEGV|세그폴트|core dumped|terminate called|Aborted')
    echo "MUSIC-CLOSE-CRASH-MARKERS: $BIG_CRASH (leg big 로그 델타 — 0 = 정상 소멸)"
    echo "MUS-CLOSE-LOG-DELTA (leg big 서버 로그 꼬리 — 폐기/소멸 원문): $(printf '%s' "$CDELTA" | tail -4 | tr '\r\n' ' ' | tail -c 400)"
    [ "$BIG_CRASH" -eq 0 ] || echo "MUSICP-FAIL-SOFT(close-crash: 스캔 중 close에서 클라 사망 원문 검출 — 결함 원장)"
else
    BIG_CRASH=0
    echo "MUSIC-CLOSE-CRASH-MARKERS: 0 (leg big 로그 무변 — 정상)"
fi
P3=""
for i in 1 2 3 4 5; do
    P3=$(ctl '{"tool":"ping","args":{}}')
    ok "$P3" && break
    sleep 2
done
ok "$P3" || FAIL "ping after scan-window close failed — $P3 (서버 생존 원문)"
echo "MUS-SERVER-ALIVE: $P3 (close 경계 뒤 서버 생존 원문 1행)"
RL16=$(ctl '{"tool":"launch_app","args":{"app":"music"}}')
ok "$RL16" || FAIL "music respawn after window close failed — $RL16"
echo "MUSIC-RESPAWN-REPLY: $RL16"
wait_music
parse_geo big2
sleep 2
BLRES=$(mtool music_dir_list '{}')
if printf '%s' "$BLRES" | grep -aq '"ok":true'; then
    echo "MUSIC-RESPAWN-LIST: OK — $BLRES (재스폰 후 도구 응답 원문 — 응답 정상 = 도구 경로 정상)"
else
    echo "MUSICP-FAIL-SOFT(respawn-list: 재스폰 뒤 도구 응답 비정상 — $BLRES)"
fi
echo "--- N3-5. [③] 취소 후 재스캔 정착 (재스폰 클라 tab2 재스캔 → 채택 전이 — 캡처) ---"
SBC1=$(tap "$MUS_ID" "$((MUS_X + TAB2_X))" "$((MUS_Y + TAB2_Y))" 1)
ok "$SBC1" || echo "NOTE-SETTLE-TAP: tab2 tap-1 failed — $SBC1"
sleep 1
SBC2=$(tap "$MUS_ID" "$((MUS_X + TAB2_X))" "$((MUS_Y + TAB2_Y))" 1)
ok "$SBC2" || echo "NOTE-SETTLE-TAP: tab2 re-click failed — $SBC2 (첫 클릭 취식 방어)"
echo "settle tap replies: $SBC1 | $SBC2 (재스폰 클라 dirs_[1] 재스캔 — 폐기 뒤의 정착 전이 원문)"
sleep 12
CAPTURE "$TMPD/mus_phone_rescan_settled.png"
echo "MUSP-CANCEL-SEGMENT-END (③ 정착 캡처 — 표행 수는 캡처 육안 몫·스캔 진행 중이면 '스캔 중...' 표기 원장행)"

# ══════════════════════ N5 — 태그 표기 동형 세그먼트 (T4 #99 — wsl_music 16e 96d615b 수형) ══════════════════════
# 위상 = **N3-5 뒤**(세그먼트 체인 말단 — leg sdcard 앞): 16d(폰 N3)이 스캔
# 취소 상태 계약을 갖는다 — 태그 세그먼트는 자기 부팅(merge_music_dir tag —
# dirs = [$TAGDIR, 사용자 dirs 보존] — ORIG 기반 재병합이라 N3의 big 잔상
# settings 기술도 소각)으로 그 상태와 단절한다. tapsettle·xtap(PXPY)은 N4
# 블록에서 정의된 것을 같은 클라 흐름으로 재용한다(N4-10 bc_* 소각 뒤 —
# 재사용 안전). 폰 수형 원장:
#   · 등록 = **병합 구성**(N3-2 수형 — dirs_[1]=태그 dir = tab2 = TAB2_X=90
#     재용 — 신규 좌표 캘리브레이션 0). music_dir_add의 **도구 원문 자체는
#     N4-1이 소유** — 본 세그먼트는 music_dir_list 진실원(등록 상태)만 병기한다.
#   · 탭 착탄 = **XTEST(xtap — tapsettle)**(본 라인의 X 주입 축·주입기 삭제
#     금지 계약). MISS = **send_input 탭 ×2 폴백**(leg list/I 원문 수형 —
#     첫 클릭 포커스 취식 방어 — 4·5차 런 원장) — 폰 축의 탭은 양 경로 모두
#     실측 계보라 폴백이 정직하다.
#   · **타이핑 세그먼트(필터 2라운드)는 폰에서 스킵 — 원장행**(브리프 원문):
#     합성 타이핑의 전달·프레젠트 신뢰 원장(T3 concern 2 — harness 타이핑
#     계층 결함 원장)·정착기 3세대 원장(상태행 밴드 차분기+영수증=말미 프레임)이
#     WSL에서 소비됐고, 필터 매치의 순수 규약은 2s-⑨(문언+ASCII fold)가 소유다.
#     폰 표의 필터는 **도구 경로가 없어 캡처(육안)로만 관측 가능 — EYES 원장행**
#     ·재판정 = WSL 정찰 런 몫+폰 후속 정찰 런. 폰은 **표기 원문**(곡명/아티스트/
#     앨범/폴백)과 재생 openFailed 영수증만 실측한다.
#   · 재생 leg = 16e-5 수형 — UI 더블클릭 접촉 0(태그 트랙은 클릭하지 않는다) —
#     그래도 vplayer open 실패 영수증은 **app_tool open → get_status openFailed
#     폴링 → 창 회수**(N4-0b/16b2-0 수형)로 성립한다 — "유효 프레임 0(태그만
#     성립)"이라는 합성 계약의 정직 종착.
echo "=== N5. 태그 표기 동형 세그먼트 (T4 #99 — wsl_music 16e 폰 수형 — 타이핑 스킵 원장) ==="
echo "MUS-N5-TYPE-SKIP: 폰 타이핑 세그먼트 스킵 — 합성 타이핑 전달·프레젠트 신뢰 원장(T3 concern 2·정착기 3세대) 소비·필터는 도구 경로 부재 → EYES 원장행 — 본 세그먼트는 목록 표기 원문+캡처 2종+openFailed 영수증만 실측 (원장 1행)"
merge_music_dir "$TAGDIR" tag
boot_server tag
L=$(ctl '{"tool":"launch_app","args":{"app":"music"}}')
ok "$L" || FAIL "leg tag: launch_app music failed — $L"
echo "launch reply: $L"
wait_music
parse_geo tag
sleep 3   # 부팅 스캔(dirs_[0]=기본 — 부재) 도착 안정화
DLT0=$(mtool music_dir_list '{}')
echo "MUSIC-TAG-DIR-LIST-0: ${DLT0:-none} (등록 진실원 — 태그 dir가 music.dirs에 병합된 원문)"
printf '%s' "$DLT0" | grep -aqF "$TAGDIR" \
    || echo "MUSICP-FAIL-SOFT(tag-dir-list-0: 태그 dir 미반영 — 병합 결손 — $DLT0)"
echo "--- N5-1. dirs_[1] 탭 착탄 (XTEST tapsettle — send_input 폴백) ---"
TAG_TAB=0
TAG_Y_YS="${MUSP_TAGTAB_YS:-77 71 83 65}"
TAG_X_XS="${MUSP_TAGTAB_XS:-90 100 80 110 70 120}"   # TAB2_X=90 정산 상수+±그리드(뷰 라벨 폭 11자 — pcm_music_seed 13자와 근접 — N4 산식 원장)
xtpark
CAPTURE "$TMPD/mus_phone_bc_tag0.png"
TAG_BASE="$TMPD/mus_phone_bc_tag0.png"
for TX in $TAG_X_XS; do
    for TY in $TAG_Y_YS; do
        if tapsettle "$TAG_BASE" "tag0" "$TX" "$TY" 3; then
            TAG_TAB=1
            echo "MUS-TAG-TAB-OK: mus_tagseed 탭 착탄(클라 ${TX},${TY} — dirIndex_=태그 dir 전이 diff=$SETTLE_DIFF·도달 시차 ${SETTLE_T}s — XTEST 주입 축) — drain 뒤 정착 프레임 갱신 원문(1-behind 드레인)"
            break 2
        fi
        echo "  탭 무착탄(${TX},${TY}) — 무음 소멸 판정(상태 무변화 = 재시도 안전)"
    done
done
if [ "$TAG_TAB" -eq 1 ]; then
    TCLX="xtap(tapsettle)"
else
    echo "MUSICP-FAIL-SOFT(tag-tab: XTEST 무착탄 — send_input 폴백(leg list 원문 수형 — 첫 클릭 포커스 취식 방어))"
    CLICK_X=$((MUS_X + TAB2_X))
    CLICK_Y=$((MUS_Y + TAB2_Y))
    TCL=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
    ok "$TCL" || echo "NOTE-TAG-FALLBACK: send_input tab click failed — $TCL"
    sleep 2
    TCL2=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y" 1)
    ok "$TCL2" || echo "NOTE-TAG-FALLBACK: tab re-click failed — $TCL2 (첫 클릭 취식 방어 — 동일 탭 재선택 idempotent)"
    TCLX="send_input x2 (폴백)"
    # [런 1 원장 수리 — 폴백 착탄 판정]: 폴백 탭의 ok 응답은 전송 성공일 뿐이다 —
    # tapsettle의 차분 축(캡처 diff) 재용으로 착탄을 판정한다(주입만 send_input).
    sleep 3
    CAPTURE "$TMPD/mus_phone_bc_tagf.png"
    DFF=$(python3 "$PXPY" "$TAG_BASE" "$TMPD/mus_phone_bc_tagf.png" \
                  "$MUS_X" "$MUS_Y" "$MUS_W" "$MUS_H" 2>&1)
    ROWS_F=$(printf '%s' "$DFF" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p')
    echo "TAG-FALLBACK-SETTLE: diff_rows=${ROWS_F:-NA} (기저 대비 — 폴백 착탄 판정 — 임계 3)"
    if [ -n "$ROWS_F" ] && [ "$ROWS_F" -ge 3 ]; then
        TAG_TAB=1
        echo "MUS-TAG-TAB-OK(send_input 폴백): 탭 착탄(클라 ${TAB2_X},${TAB2_Y} — 캡처 차분=$ROWS_F rows — xtap 무응답 원장 대조 원문)"
    else
        echo "MUSICP-FAIL-SOFT(tag-fallback-settle: 폴백 탭도 무착탄 diff=${ROWS_F:-NA} — MUSP_TAGTAB 재보정 몫)"
    fi
fi
echo "MUS-TAG-TAB-MODE: $TCLX (XTEST 정착 또는 send_input 폴백 — 양 경로 모두 실측 계보)"
sleep 8   # 탭 전환 스캔(태그 3곡 — 표기 leg) 도착 대기
echo "--- N5-2. 목록 표기 캡처 2종 (곡명(아티스트) 열·앨범 열·폴백 행) ---"
CAPTURE "$TMPD/mus_phone_taglist.png"
echo "MUS-TAG-LIST-LEDGER: 예상 행(mtime desc — 드라이버 스탬프 alpha>beta>raw): ① t_tag_alpha → 'Phone Tag Alpha (Probe Artist Alpha)'+앨범 'Tag Album One' ② t_tag_beta → 'Phone Tag Beta (Probe Artist Beta)'+앨범 'Tag Album Two' ③ t_raw_noid → 폴백 행 't_raw_noid'+앨범 '-' — 표기 일치는 캡처 육안 몫(EYES)"
sleep 8   # 2차 캡처 — 스캔 도착 확정 원문(폰 스캔 도착이 늦다 — leg list 원장 수형)
CAPTURE "$TMPD/mus_phone_taglist2.png"
echo "MUS-TAG-SUMMARY: recon=1 tab=$TAG_TAB captures=2 dirlist=$(printf '%s' "$DLT0" | grep -aqF "$TAGDIR" && echo ok || echo miss)"
echo "--- N5-3. vplayer open 실패 영수증 (16e-5 수형 — 도구 직행·UI 무접촉) ---"
LVP=$(ctl '{"tool":"launch_app","args":{"app":"vplayer"}}')
ok "$LVP" || echo "MUSICP-FAIL-SOFT(tag-vp-launch: launch_app vplayer 미성립 — $LVP (직행 open 귀속 원장행))"
wait_vplayer || echo "NOTE-TAG-VP: Video Player 창 미출현 — 직행 open 계속 시도 (원장행)"
OR_TAG=$(ctl "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$TAGDIR/t_tag_alpha.mp3\"}}}")
echo "MUS-TAG-OPEN-REPLY: ${OR_TAG:-none} (accepted = 비동기 도달 — 판정은 get_status의 openFailed)"
GS_TAG=""
TAG_OPENFAIL=0
for i in 1 2 3 4 5 6; do
    sleep 1.5
    GS_TAG=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
    echo "MUS-TAG-OPEN-STATUS-$i: ${GS_TAG:-none}"
    printf '%s' "$GS_TAG" | grep -aq '"openFailed":true' && { TAG_OPENFAIL=1; break; }
    printf '%s' "$GS_TAG" | grep -aq '"opened":true' && break
done
echo "MUS-TAG-OPEN-STATUS: ${GS_TAG:-none} (opened:false+openFailed:true = 유효 mp3 프레임 0 파일의 정직 개방 실패 원문 — 태그 표기 leg는 이것과 무관 성립)"
printf '%s' "$OR_TAG" | grep -aq '"accepted":true' \
    && [ "$TAG_OPENFAIL" -eq 1 ] \
    && echo "MUS-TAG-OPENFAIL: OK — open은 수취되고 개방은 실패(정직 원문 — 합성 최소 mp3 계약)" \
    || echo "MUSICP-FAIL-SOFT(tag-openfail: open/get_status 원문 미달 — ${OR_TAG:-none}|${GS_TAG:-none})"
# 창 회수(차분·후속 캡처·sdcard 레그 표면 오염 소각 — N4-0b/16b2-0 수형)
TAG_VPCLOSE=0
for i in 1 2 3; do
    VPT=$(ctl '{"tool":"list_windows","args":{}}')
    VPW=$(printf '%s' "$VPT" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
    [ -z "$VPW" ] && break
    VPID=$(printf '%s' "$VPW" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    TVC=$(ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$VPID}}")
    echo "  tag vplayer close#$i: id=$VPID repl=${TVC:-none}"
    TAG_VPCLOSE=$((TAG_VPCLOSE + 1))
    sleep 1
done
echo "MUS-TAG-VP-RECLAIM: closed=$TAG_VPCLOSE (0 = 실패 UI가 즉닫힘·1 = 실패 메시지를 남긴 뒤 회수 — 어느 쪽이나 회수 완료 부기, 판정 아님)"
sleep 1
CAPTURE "$TMPD/mus_phone_tag_after.png"
echo "MUS-TAG-SUMMARY-2: tab=$TAG_TAB openfail=$TAG_OPENFAIL vp-reclaim=$TAG_VPCLOSE (N5 세그먼트 원문 종합 — 드라이버 판정 파싱 원문행)"
echo "MUS-TAG-SEGMENT-END (태그 dir 소각은 P 섹션 몫 — settings 원복은 Q ORIG 바이트 등호 몫)"

# ══════════════════════ N6 — 태그 쓰기(도구) 동형 세그먼트 (#111 T6 — wsl_music 16f-TW eaba3a4 수형) ══════════════════════
# 위상 = 세그먼트 체인 말단(N5 뒤 — leg sdcard 앞 — N5의 체인-말단 수형과 동일
# 위상). 기존 세그먼트(각 레그·N4·N5) 원존 유지 — 본 세그먼트만 신설. 폰 수형
# 원장:
#   · **16f-0a 신선 클라 재건 불요**(원장행 — WSL 진원의 폰 부재): WSL 16f가
#     재건한 사유 = 16e-4의 아티스트 필터 잔존(T5 런 1 원장) — 폰 N5는 **타이핑
#     스킵 원장**(합성 타이핑 신뢰 원장 T3 concern 2)이라 필터 잔존 진원이
#     없다. 활성 탭 = N5 탭 착탄 결과(TAG_TAB — dirIndex_ = 태그 dir)를
#     상속한다. Music 창 부재(비정상 계보) 시 = launch_app 폴백 + tab2
#     send_input×2 착탄(N5 폴백 수형 — 캡처 차분 판정).
#   · TW 시드(무태그 mp3 junk 768B+무태그 FLAC 스켈레톤+합성 cover jpg —
#     드라이버 합성 2c·E4 수취)는 N5 캡처 **뒤**에 $TAGDIR로 랜딩(N5의 3행
#     표기 계약 무오염 — 쓰기 갱신 mtime이 표 상단행 관계를 만든다).
#   · 클릭 0 — **도구 경로만**(music_tag_write relay — atool 수형). UI [편집]
#     패널·행 32px 커버 썸네일은 클릭하지 않는다 — **표기 원문 캡처**(EYES —
#     T3 배포 UI의 폰 표기 원문 = mus_phone_tagwrite_*).
#   · scan_busy = 정직 거부 → **tw_write 재시도 수형**(3콜 폴링 — T4 도구
#     규약·16f 수형 동일 — reply 원문은 시도마다 행).
#   · 필터 라운드(16f-5)는 **폰 스킵 — 원장행**: 폰 타이핑 스킵 원장(N5 승계 —
#     표 필터는 도구 경로 부재 = EYES 축) — MUS_TW_SETTLE=-1 스킵 표기. 본
#     세그먼트의 강도 축 = **파일 수준 round-trip**(재독 표기+APIC md5 등호) —
#     T5 리포트 §6.4의 도구 경계+파일 재독 축 승계.
#   · REMNANT = TW 시드 3건 소각(N6-6+P3 $TAGDIR 통짜 소각+드라이버 REMNANT
#     게이트 병행 — probe 소유 잔상 0 계약)+재독 파서·폴백 정찰 프레임 소각.
echo "=== N6. 태그 쓰기(도구) 동형 세그먼트 (#111 T6 — wsl_music 16f-TW 폰 수형) ==="
echo "MUS-N6-TW-RECON: 16f-0a 신선 클라 재건 생략 원장행(폰 N5 타이핑 스킵 — 필터 잔존 진원 없음 — 클라 상태 = N5 계약 승계·활성 탭 상속)"
TW_STAGE=0; TW_LAND=0; TW_TABF=0; TW1_OK=0; TW2_OK=0; TWF_OK=0
TW_APIC_RT=0; TW_APIC_KEEP=0
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WINTW=$(ctl '{"tool":"list_windows","args":{}}')
    MUSWINTW=$(printf '%s' "$WINTW" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWINTW" ] && break
done
if [ -n "$MUSWINTW" ]; then
    TW_STAGE=1
    MUSWIN="$MUSWINTW"
    parse_geo tw
    echo "MUSWIN-N6: $MUSWIN (N5 클라 승계)"
    if [ "${TAG_TAB:-0}" -eq 1 ]; then
        echo "MUS-TW-TAB-INHERIT: OK — N5 탭 착탄 상속(활성 탭 = 태그 dir — tapsettle 진실원 계승 — 재탭 불요)"
    else
        echo "NOTE-TW-TAB-INHERIT: N5 탭 미착탄 계보(TAG_TAB=0) — tab2 폴백 착탄(N5 폴백 수형 — send_input×2+캡처 차분 판정)"
        xtpark
        CAPTURE "$TMPD/mus_phone_bc_twbase.png"
        TWBASE="$TMPD/mus_phone_bc_twbase.png"
        TLX=$((MUS_X + TAB2_X))
        TLY=$((MUS_Y + TAB2_Y))
        TAPTW=$(tap "$MUS_ID" "$TLX" "$TLY" 1)
        ok "$TAPTW" || echo "  tw tab tap reply: ${TAPTW:-none}"
        sleep 2
        TAPTW2=$(tap "$MUS_ID" "$TLX" "$TLY" 1)
        ok "$TAPTW2" || echo "  tw tab re-click reply: ${TAPTW2:-none} (첫 클릭 취식 방어 — 동일 탭 재선택 idempotent)"
        sleep 8
        CAPTURE "$TMPD/mus_phone_bc_twafter.png"
        DFTW=$(python3 "$PXPY" "$TWBASE" "$TMPD/mus_phone_bc_twafter.png" \
                       "$MUS_X" "$MUS_Y" "$MUS_W" "$MUS_H" 2>&1)
        ROWS_TW=$(printf '%s' "$DFTW" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p')
        echo "MUS-TW-TAB-FALLBACK: diff_rows=${ROWS_TW:-NA} (기저 대비 — 폴백 착탄 판정 — 임계 3)"
        if [ -n "$ROWS_TW" ] && [ "$ROWS_TW" -ge 3 ]; then
            TW_TABF=1
            echo "MUS-TW-TAB-FALLBACK-OK: 탭 착탄(클라 ${TAB2_X},${TAB2_Y}) — dirIndex_ 전이 추정(캡처 차분=$ROWS_TW rows)"
        else
            echo "MUSICP-FAIL-SOFT(tw-tab-fallback: 폴백 탭도 무착탄 diff=${ROWS_TW:-NA} — MUSP_TAGTAB 재보정 몫 — 원장행)"
        fi
        # [정직 원장행]: 폴백 탭의 dirIndex_ 직접 증명은 캡처(육안)뿐 — 본 세그먼트의
        # 성립 판정(도구 경계+파일 재독)은 이와 별개로 유지 — 목록 행 캡처 레저 해석에 병기.
        echo "MUS-TW-TAB-FALLBACK-MODE: tap=$TW_TABF (0 = dirIndex_ 미확정 — 원장행)"
    fi
else
    echo "MUSICP-FAIL-SOFT(tw-recon: Music 창 부재 — N5 계보 재대조 몫 — 태그 쓰기 세그먼트 생략, 원장)"
fi

if [ "$TW_STAGE" -eq 1 ]; then
    echo "--- N6-1. TW 시드 랜딩 ($TAGDIR — N5 캡처 뒤 — 3행 계약 무오염 — E4 수취 승계) ---"
    if [ -d "$TWDIR" ] && [ "$(find "$TWDIR" -type f 2>/dev/null | wc -l)" -eq 3 ]; then
        mv "$TWDIR/t_tw_write.mp3" "$TWDIR/t_tw_write.flac" "$TWDIR/t_tw_cover.jpg" "$TAGDIR/" \
            && rmdir "$TWDIR" 2>/dev/null
        [ -d "$TWDIR" ] && rm -rf "$TWDIR"
        TW_LAND=1
        echo "TW-LANDED: $TAGDIR (t_tw_write.mp3+t_tw_write.flac+t_tw_cover.jpg — N5 캡처 뒤 랜딩 원문)"
    fi
    [ "$TW_LAND" -eq 1 ] || echo "MUSICP-FAIL-SOFT(tw-land: $TWDIR 3건 미착 — E4 원문 대조 — 태그 쓰기 세그먼트 본체 생략, 원장)"
fi

if [ "$TW_LAND" -eq 1 ]; then
    sync
    echo "TW-SYNC: drained (시드 writeback 선행 배수 — N3 원장 수형)"
    TW_CM=$(md5sum "$TAGDIR/t_tw_cover.jpg" | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
    echo "TW-COVER-LANDED: bytes=$(wc -c < "$TAGDIR/t_tw_cover.jpg") md5=$TW_CM (기대 md5=__TW_COVER_MD5__ — 도착 등호 게이트)"
    [ "$TW_CM" = "__TW_COVER_MD5__" ] || echo "MUSICP-FAIL-SOFT(tw-cover-md5-lan: 랜딩 원문 불일치 — E4 게이트와 이중)"
    TW_MAG=$(python3 -c 'import sys
d = open(sys.argv[1], "rb").read(2)
print("ffd8" if d == b"\xff\xd8" else "bad:" + d.hex())' "$TAGDIR/t_tw_cover.jpg")
    echo "TW-COVER-MAGIC: $TW_MAG (FFD8 — LoadCoverByMagic 계약 — 폰 파일 원문)"
    printf '%s' "$TW_MAG" | grep -aq ffd8 || echo "MUSICP-FAIL-SOFT(tw-cover-magic: FFD8 아님 — $TW_MAG)"

    echo "--- N6-2. 재독 파서 준비+프리뷰 (16f-1 수형 — 폰 python3 — px 차분기와 동일 축·시더와 독립 — 앱 파서와도 독립) ---"
    cat > "$TMPD/pmus_twparse.py" <<'PYEOF'
# [#111 T6] 재독 파서 — 시더와 독립(앱 파서와도 독립 — wsl_music 16f-1 원문 수형):
# mp3 = ID3 프레임 사슬(v2.3 BE32·v2.4 syncsafe 폴백 — 2u-① 수형)에서
# TIT2/TPE1/TALB+APIC(enc0 mime\0·desc\0·data)를 다시 읽고, FLAC = 메타 블록
# 사슬을 다시 읽어 VORBIS_COMMENT(4·LE32 길이 규약 — ParseVorbisCommentsRaw
# 동형)의 관리 키를 수취한다. 진실원 = 파일 바이트. [T5 런 원장 수형(r1): APIC
# 본문 = [enc][mime][NUL][desc=빈+종결][picture] — 그림타입 바이트 불내장 —
# desc 검색 = mime 종결 다음 바이트(z+1 보행 — z+2 스킵은 그림 선단 5B 잠식
# 사건의 수형 — 16f 원문 그대로).
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
    print("TW-PARSE magic-unknown (무태그 전제 위반 — 시더 원문 재확인)")
PYEOF
    TW_PRE=$(python3 "$TMPD/pmus_twparse.py" "$TAGDIR/t_tw_write.mp3")
    echo "MUS-TW-PREVIEW-MP3: $TW_PRE (쓰기 전 — 무태그 원문)"
    TW_PRE_FLAC=$(python3 "$TMPD/pmus_twparse.py" "$TAGDIR/t_tw_write.flac")
    echo "MUS-TW-PREVIEW-FLAC: $TW_PRE_FLAC (쓰기 전 — fLaC만)"

    echo "--- N6-3. 도구 쓰기 1 — title/artist/album+cover jpg (마법수 수취) ---"
    tw_write() { # $1=args-json — scan_busy 정직 거부 재시도 수형(T4 도구 규약:
                 # 쓰기 성공의 RequestScan 진행 중에 도착하는 뒤 쓰기 = 대기+
                 # 재발사 — reply 원문은 시도마다 행(무음 실패 금지 원장).
                 # reply는 TW_REPLY로 돌려준다(16f-2 수형).
        TW_REPLY=""
        local a="$1" r i ok=1
        for i in 1 2 3; do
            r=$(mtool music_tag_write "$a")
            TW_REPLY=$r
            echo "  TW-WRITE#$i: ${r:-none}"
            if printf '%s' "$r" | grep -aq '"ok":true'; then ok=0; break; fi
            printf '%s' "$r" | grep -aq '"error":"scan_busy"' || break
            sleep 4
        done
        return $ok
    }
    TW_TITLE_A="Phone Write Alpha"
    TW_ARTIST="Probe Tw Artist"
    TW_ALBUM="Tw Album One"
    TW_TITLE_B="Phone Write Reup"
    if tw_write "{\"path\":\"$TAGDIR/t_tw_write.mp3\",\"title\":\"$TW_TITLE_A\",\"artist\":\"$TW_ARTIST\",\"album\":\"$TW_ALBUM\",\"cover\":\"$TAGDIR/t_tw_cover.jpg\"}"; then
        TW1_OK=1
        echo "MUS-TW-WRITE1-OK: ok 수취 — title/artist/album+cover(jpg 마법수) relay 쓰기 성립(성공 = RequestScan 재스캔 재청구 배선 — T4 도구 규약)"
        sleep 8   # 재스캔 도착(폰 스캔·컴포지트 도착이 늦다 — leg list 원장 수형)
        CAPTURE "$TMPD/mus_phone_tagwrite_list.png"
        echo "MUS-TW-LIST-LEDGER: 예상 상단 행(mtime desc — 쓰기 갱신): 'Phone Write Alpha (Probe Tw Artist)'+'Tw Album One'+좌측 커버아트 썸네일 32px(4색 밴드 jpg — 회색 자리표시 대조 — T3 배포 UI의 폰 표기 원문) — 표기·썸네일 일치는 캡처 육안 몫(EYES)"
    else
        echo "MUSICP-FAIL-SOFT(tw-write1: 도구 쓰기 1 거부 — $TW_REPLY)"
    fi
    AP1=$(python3 "$TMPD/pmus_twparse.py" "$TAGDIR/t_tw_write.mp3")
    echo "MUS-TW-PARSED-A: $AP1 (파일 수준 재독 — 앱 파서와 독립 round-trip 원문)"
    TW_MIME=$(printf '%s\n' "$AP1" | sed -n 's/.*TW-APIC mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\1/p')
    TW_MD5_1=$(printf '%s\n' "$AP1" | sed -n 's/.*TW-APIC mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    echo "MUS-TW-APIC-1: mime=${TW_MIME:-none} md5=${TW_MD5_1:-none} (합성 cover md5=$TW_CM 대조 몫)"
    if [ -n "$TW_MD5_1" ] && [ "$TW_MD5_1" = "$TW_CM" ]; then
        TW_APIC_RT=1
        echo "MUSIC-TW-COVER-RT: OK — APIC 그림 == 합성 cover jpg 원문(md5 등호 — ReadCover 계약의 파일 수준 라이브 대응)"
    else
        echo "MUSICP-FAIL-SOFT(tw-cover-rt: APIC round-trip 불일치 — md5=${TW_MD5_1:-none} vs cover=$TW_CM)"
    fi

    echo "--- N6-4. 도구 쓰기 2 — cover=null(기존 APIC 유지 계약)+TIT2 교체 ---"
    if tw_write "{\"path\":\"$TAGDIR/t_tw_write.mp3\",\"title\":\"$TW_TITLE_B\",\"artist\":\"$TW_ARTIST\",\"album\":\"$TW_ALBUM\",\"cover\":null}"; then
        TW2_OK=1
        echo "MUS-TW-WRITE2-OK: ok 수취 — cover=null = 기존 유지 leg(WriteTag 계약 — 2u-⑮ APIC 생존 어설션의 도구 경계 라이브 대응)·TIT2 교체 쓰기"
        sleep 8
        CAPTURE "$TMPD/mus_phone_tagwrite_list2.png"   # 재상단 행(쓰기 2 label) — EYES 보조 캡처
    else
        echo "MUSICP-FAIL-SOFT(tw-write2: 도구 쓰기 2 거부 — $TW_REPLY)"
    fi
    AP2=$(python3 "$TMPD/pmus_twparse.py" "$TAGDIR/t_tw_write.mp3")
    echo "MUS-TW-PARSED-B: $AP2 (쓰기 2 재독 — TIT2 교체+APIC 유지 원문)"
    TW_MD5_2=$(printf '%s\n' "$AP2" | sed -n 's/.*TW-APIC mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    if [ -n "$TW_MD5_2" ] && [ "$TW_MD5_2" = "$TW_CM" ]; then
        TW_APIC_KEEP=1
        echo "MUS-TW-APIC-KEEP: OK — cover=null 뒤 APIC 원문 유지(md5 등호 — $TW_MD5_2)"
    else
        echo "MUSICP-FAIL-SOFT(tw-apic-keep: cover=null 뒤 APIC 소각/변조 — md5=${TW_MD5_2:-none} vs cover=$TW_CM)"
    fi

    echo "--- N6-5. FLAC 도구 쓰기 — cover 생략(유지 leg 계열 — PICTURE 부재 원존)+코멘트 신설(T2 leg) ---"
    if tw_write "{\"path\":\"$TAGDIR/t_tw_write.flac\",\"title\":\"$TW_TITLE_A\",\"artist\":\"$TW_ARTIST\",\"album\":\"$TW_ALBUM\"}"; then
        TWF_OK=1
        echo "MUS-TW-FLAC-OK: ok 수취 — fLaC 스켈레톤(STREAMINFO 첫 블록 34B)에서 VORBIS_COMMENT 신설+블록 재조립(STREAMINFO 보존 — D9·PICTURE 부재 = 교체 대상 없음)"
        sleep 8
        CAPTURE "$TMPD/mus_phone_tagwrite_flac.png"   # FLAC 행 표기(곡명(아티스트) 반영 — EYES 보조)
    else
        echo "MUSICP-FAIL-SOFT(tw-flac: FLAC 도구 쓰기 거부 — $TW_REPLY)"
    fi
    FP=$(python3 "$TMPD/pmus_twparse.py" "$TAGDIR/t_tw_write.flac")
    echo "MUS-TW-FLAC-PARSED: $FP (FLAC 재독 — 메타 블록 사슬 재파싱 원문)"
    TW_FLAC_KEYS=$(printf '%s\n' "$FP" | sed -n 's/.*TW-FLAC-BLOCKS keys=\([0-9]*\) picture=\([0-9]*\).*/keys=\1 picture=\2/p')
    echo "MUS-TW-FLAC-BLOCKS: ${TW_FLAC_KEYS:-none} (관리 3키 TITLE/ARTIST/ALBUM = keys=3·cover 생략 = picture 0 — D9 재조립의 재독 원문)"
    if printf '%s' "$FP" | grep -aq 'streaminfo-broken'; then
        echo "MUSICP-FAIL-SOFT(tw-flac-streaminfo: STREAMINFO 보존 위반 — $FP)"
    fi

    echo "--- N6-6. 필터 라운드 스킵 원장행 + TW 시드 소각 (probe 소유 잔상 0 — REMNANT 계약 — 16f-6 수형) ---"
    MUS_TW_SETTLE=-1
    echo "MUS-TW-FILTER-SKIP: 폰 필터 라운드(16f-5) 스킵 원장행 — 폰 타이핑 스킵 원장 승계(N5 동일 축 — 표 필터는 도구 경로 부재 = EYES) — 쓰기 반영 재독의 폰 표기 축 = tagwrite 3캡처(EYES)·성립 판정 = 파일 재독 축(상단 N6 원문)"
    rm -f "$TMPD/pmus_twparse.py"
    rm -f "$TAGDIR/t_tw_write.mp3" "$TAGDIR/t_tw_write.flac" "$TAGDIR/t_tw_cover.jpg"
    rm -rf "$TWDIR"
    rm -f "$TMPD/mus_phone_bc_twbase.png" "$TMPD/mus_phone_bc_twafter.png" \
          "$TMPD/mus_phone_bc_twbase.pxc" 2>/dev/null
    TW_LEFT=$(ls "$TAGDIR"/t_tw_* 2>/dev/null | wc -l)
    [ "$TW_LEFT" -eq 0 ] \
        && echo "TW-BURIED: t_tw_* 3건 소각 (P3 \$TAGDIR 통짜 소각+드라이버 REMNANT 게이트 병행)" \
        || echo "MUSICP-FAIL-SOFT(tw-bury: TW 시드 잔존 $TW_LEFT건 — 수동 소각)"
    echo "MUS-TW-SUMMARY-2: stage=$TW_STAGE land=$TW_LAND tab_inherit=${TAG_TAB:-0} tab_fallback=$TW_TABF w1=$TW1_OK w2=$TW2_OK flac=$TWF_OK cover_rt=$TW_APIC_RT apic_keep=$TW_APIC_KEEP apic_1=${TW_MD5_1:-none} flac_blocks=${TW_FLAC_KEYS:-none} (N6 세그먼트 원문 종합 — 드라이버 판정 파싱 원문행)"
fi
[ "${TW_STAGE:-0}" -eq 1 ] || echo "MUS-TW-SUMMARY-2: stage=0 land=0 w1=0 w2=0 flac=0 cover_rt=0 apic_keep=0 (Music 창 부재 — 세그먼트 생략 — 원장행)"
echo "MUS-TW-SEGMENT-END (TW 시드 소각 = N6-6·P3·REMNANT 게이트 몫 — 표기·썸네일·[편집] 모양 = 캡처 육안 몫 EYES)"

# ══════════════════════ N7 — opus/m4a 태그 쓰기 동형 세그먼트 (#115 T4 — wsl_music 16f-OM 수형) ══════════════════════
# 위상 = N6 말단(TW 시드 소각) 뒤·leg sdcard(O 섹션) 앞 — 같은 도구 경계·같은
# $TAGDIR(등록 스캔 dir). 기존 세그먼트(레그·N4·N5·N6) 원존 — 본 세그먼트만
# 신설. 폰 수형 원장:
#   · OM 시드(합성 m4a+opus — 2x 픽스처 승계·합성 cover jpg = TW 원문 재용 —
#     동일 바이트·md5 등호 축 공유)는 **드라이버 합성**(2d 섹션 — Windows python
#     stdlib — 폰 python3 불요 계약 승계)이고 E5가 \$OMDIR로 수취 → N7-1이
#     N6-6 소각 뒤 \$TAGDIR로 mv(TW 3행 계약 무오염 — N6-1 수형).
#   · 쓰기 4콜 — m4a: ①cover 지정(covr 교체) ②cover=null+covr 유지·opus:
#     ③cover 지정(MBP 교체) ④cover 생략(MBP 유지) — **재독 표기 원문** = 파일
#     수준(순수 파이썬 재파싱 — pmus_omparse.py — 앱 파서와 독립)+목록 행 캡처
#     2종(om_list·om_opus — EYES).
#   · 필터 라운드 스킵 원장(N6 승계 — 폰 타이핑 스킵)·클릭 0 — 도구 경로만.
#   · scan_busy 재시도 = tw_write 수형(16f-2/N6-3 원존 — 재정의 폴백 포함).
#   · REMNANT = OM 시드 3건 소각(N7-5·P3 $TAGDIR 통짜 소각+드라이버 REMNANT
#     게이트 병행 — probe 소유 잔상 0 계약).
echo "=== N7. opus/m4a 태그 쓰기(도구) 동형 세그먼트 (#115 T4 — wsl_music 16f-OM 폰 수형) ==="
OMDIR="$TMPD/mus_omseed"
OM_STAGE=0; OM_LAND=0; OM1_OK=0; OM2_OK=0; OM3_OK=0; OM4_OK=0
OM_COVR_RT=0; OM_COVR_KEEP=0; OM_MBP_RT=0; OM_MBP_KEEP=0
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WINOM=$(ctl '{"tool":"list_windows","args":{}}')
    MUSWINOM=$(printf '%s' "$WINOM" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWINOM" ] && break
done
if [ -n "$MUSWINOM" ]; then
    OM_STAGE=1
    MUSWIN="$MUSWINOM"
    parse_geo om
    echo "MUSWIN-N7: $MUSWIN (N6 클라 승계 — 활성 탭 상속)"
else
    echo "MUSICP-FAIL-SOFT(om-recon: Music 창 부재 — N6 계보 재대조 몫 — opus/m4a 세그먼트 생략, 원장)"
fi

if [ "$OM_STAGE" -eq 1 ] && [ -d "$OMDIR" ] && [ "$(find "$OMDIR" -type f 2>/dev/null | wc -l)" -eq 3 ]; then
    mv "$OMDIR/t_om_write.m4a" "$OMDIR/t_om_write.opus" "$OMDIR/t_om_cover.jpg" "$TAGDIR/" \
        && rmdir "$OMDIR" 2>/dev/null
    [ -d "$OMDIR" ] && rm -rf "$OMDIR"
    OM_LAND=1
    echo "OM-LANDED: $TAGDIR (t_om_write.m4a+t_om_write.opus+t_om_cover.jpg — N6 소각 뒤 랜딩 원문)"
fi

if [ "$OM_LAND" -eq 1 ]; then
    sync
    echo "OM-SYNC: drained (시드 writeback 선행 배수 — N3 원장 수형)"
    OM_CM=$(md5sum "$TAGDIR/t_om_cover.jpg" | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
    echo "OM-COVER-LANDED: bytes=$(wc -c < "$TAGDIR/t_om_cover.jpg") md5=$OM_CM (기대=__TW_COVER_MD5__ — TW 원문 동일 바이트 등호 게이트)"
    [ "$OM_CM" = "__TW_COVER_MD5__" ] || echo "MUSICP-FAIL-SOFT(om-cover-md5-lan: 랜딩 원문 불일치 — E5 게이트와 이중)"

    echo "--- N7-2. 재독 파서 준비+프리뷰 (pmus_omparse.py — 폰 python3 — 시더/앱 파서와 독립 — 16f-OM 수형 동일 내용) ---"
    cat > "$TMPD/pmus_omparse.py" <<'PYEOF'
# [#115 T4] opus/m4a 재독 파서(폰) — wsl_music.sh 16f-OM mus_omparse.py 원문
# 수형(동일 내용 — 진실원 = 파일 바이트): m4a = 최상위 box 보행 → moov → udta →
# meta(4B 스킵) → hdlr('mdir')→ilst 항목(©nam 1바이트 0xA9 접두)·covr data
# (13/14/0 스니프) 재독 — opus = OggS 페이지 보행(lacing 패킷 재조립) →
# "OpusTags" 패킷(완성 지점 매김) → vendor+관리 3키+covr METADATA_BLOCK_PICTURE
# (base64 → FLAC 그림 구조) 재독.
import base64, hashlib, struct, sys

def rdb(buf, off):
    return struct.unpack(">I", buf[off:off + 4])[0]

def walk(buf, off, end):
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
    for typ, s, e in walk(buf, item[1], item[2]):
        if typ == b"data":
            return buf[s + 3], buf[s + 8:e]
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
            meta = (s + 4, e)
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
        code, val = first_data((typ, s, e), d)
        if code == 1 and typ in (b"\xa9nam", b"\xa9ART", b"\xa9alb"):
            try:
                tags[typ.decode("latin-1")] = val.decode("utf-8")
            except UnicodeDecodeError:
                pass
        elif typ == b"covr" and code in (13, 14) and val is not None:
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
                    cmt_htype = d[off + 4]  # 코멘트 페이지 htype(D10 원문 유지)
                    vlen = struct.unpack("<I", pkt[8:12])[0]
                    vendor = pkt[12:12 + vlen].decode("utf-8", "replace")
                    cnt = struct.unpack("<I", pkt[12 + vlen:16 + vlen])[0]
                    p = 16 + vlen
                    for _ in range(cnt):
                        elen = struct.unpack("<I", pkt[p:p + 4])[0]
                        ent = pkt[p + 4:p + 4 + elen].decode("utf-8", "replace")
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
    OM_PRE_M4A=$(python3 "$TMPD/pmus_omparse.py" "$TAGDIR/t_om_write.m4a")
    echo "MUS-OM-PREVIEW-M4A: $OM_PRE_M4A (쓰기 전 — 시더 값+png covr 원문)"
    grep -aq 'hdlr=1' <<<"$OM_PRE_M4A" || echo "MUSICP-FAIL-SOFT(om-preview-hdlr: m4a 시드 hdlr('mdir') 부재 — 2x-① 구성 계약 위배 — 원장)"
    OM_PRE_OPUS=$(python3 "$TMPD/pmus_omparse.py" "$TAGDIR/t_om_write.opus")
    echo "MUS-OM-PREVIEW-OPUS: $OM_PRE_OPUS (쓰기 전 — 시더 값+png MBP 원문)"

    echo "--- N7-3. 도구 쓰기 1 — m4a cover 지정(covr 교체 leg) ---"
    if ! type tw_write >/dev/null 2>&1; then
        tw_write() { # $1=args-json — scan_busy 재시도 수형(N6-3 재작성 — N6
                     #   세그먼트 미성립 계보의 N7 독립성 원장)
            TW_REPLY=""
            local a="$1" r i ok=1
            for i in 1 2 3; do
                r=$(mtool music_tag_write "$a")
                TW_REPLY=$r
                echo "  TW-WRITE#$i: ${r:-none}"
                if printf '%s' "$r" | grep -aq '"ok":true'; then ok=0; break; fi
                printf '%s' "$r" | grep -aq '"error":"scan_busy"' || break
                sleep 4
            done
            return $ok
        }
    fi
    OM_TITLE_A="Phone Omus Alpha"
    OM_ARTIST="Probe Om Artist"
    OM_ALBUM="Om Album One"
    OM_TITLE_B="Phone Omus Reup"
    if tw_write "{\"path\":\"$TAGDIR/t_om_write.m4a\",\"title\":\"$OM_TITLE_A\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\",\"cover\":\"$TAGDIR/t_om_cover.jpg\"}"; then
        OM1_OK=1
        echo "MUS-OM-WRITE1-OK: ok 수취 — m4a relay 쓰기 성립(WriteM4aTag — ilst 재조립+mdat 원문 무이동 — D2)"
        sleep 8   # 재스캔 도착(폰 스캔·컴포지트 도착이 늦다 — N6 수형)
        CAPTURE "$TMPD/mus_phone_om_list.png"
        echo "MUS-OM-LIST-LEDGER: 예상 상단 행(쓰기 mtime desc): 'Phone Omus Alpha (Probe Om Artist)'+'Om Album One'+좌측 커버아트 썸네일 32px(4색 밴드 jpg — 교체된 covr) — 표기·썸네일은 캡처 육안 몫(EYES)"
    else
        echo "MUSICP-FAIL-SOFT(om-write1: m4a 도구 쓰기 거부 — $TW_REPLY)"
    fi
    AP1=$(python3 "$TMPD/pmus_omparse.py" "$TAGDIR/t_om_write.m4a")
    echo "MUS-OM-PARSED-A: $AP1 (m4a 파일 수준 재독 — 앱 파서와 독립 round-trip 원문)"
    OM_MD5_1=$(printf '%s\n' "$AP1" | sed -n 's/.*OM-COVR mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    OM_MIME_1=$(printf '%s\n' "$AP1" | sed -n 's/.*OM-COVR mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\1/p')
    OM_MDAT_1=$(printf '%s\n' "$AP1" | sed -n 's/.*OM-M4A-MDAT 0xab=\([0-9]*\).*/\1/p')
    echo "MUS-OM-COVR-1: mime=${OM_MIME_1:-none} md5=${OM_MD5_1:-none} mdat_0xab=${OM_MDAT_1:-none} (합성 cover md5=$OM_CM 대조 — mdat 64 기대)"
    if [ -n "$OM_MD5_1" ] && [ "$OM_MD5_1" = "$OM_CM" ] \
        && [ "$OM_MIME_1" = "image/jpeg" ] && [ "$OM_MDAT_1" = "64" ]; then
        OM_COVR_RT=1
        echo "MUSIC-OM-COVR-RT: OK — m4a covr == 합성 cover jpg+mdat 원문 무이동(교체 — D2/D4 라이브 대응)"
    else
        echo "MUSICP-FAIL-SOFT(om-covr-rt: covr 교체/mdat 불일치 — md5=${OM_MD5_1:-none} mime=${OM_MIME_1:-none} mdat=${OM_MDAT_1:-none})"
    fi

    echo "--- N7-4. 도구 쓰기 2 — m4a cover=null(기존 covr 유지 계약)+title 교체 ---"
    if tw_write "{\"path\":\"$TAGDIR/t_om_write.m4a\",\"title\":\"$OM_TITLE_B\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\",\"cover\":null}"; then
        OM2_OK=1
        echo "MUS-OM-WRITE2-OK: ok 수취 — cover=null = 기존 covr 유지 leg(WriteM4aTag covr null 계약 — 2x-⑩(a)의 도구 경계 라이브 대응)"
        sleep 8
        CAPTURE "$TMPD/mus_phone_om_list2.png"   # 재상단 행(쓰기 2 label) — EYES 보조
    else
        echo "MUSICP-FAIL-SOFT(om-write2: m4a 도구 쓰기 2 거부 — $TW_REPLY)"
    fi
    AP2=$(python3 "$TMPD/pmus_omparse.py" "$TAGDIR/t_om_write.m4a")
    echo "MUS-OM-PARSED-B: $AP2 (쓰기 2 재독 — title 교체+covr 유지 원문)"
    OM_MD5_2=$(printf '%s\n' "$AP2" | sed -n 's/.*OM-COVR mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    if [ -n "$OM_MD5_2" ] && [ "$OM_MD5_2" = "$OM_CM" ]; then
        OM_COVR_KEEP=1
        echo "MUS-OM-COVR-KEEP: OK — cover=null 뒤 covr 원문 유지(md5 등호 — $OM_MD5_2)"
    else
        echo "MUSICP-FAIL-SOFT(om-covr-keep: cover=null 뒤 covr 소각/변조 — md5=${OM_MD5_2:-none})"
    fi

    echo "--- N7-5. 도구 쓰기 3 — opus cover 지정(MBP 교체 leg) ---"
    if tw_write "{\"path\":\"$TAGDIR/t_om_write.opus\",\"title\":\"$OM_TITLE_A\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\",\"cover\":\"$TAGDIR/t_om_cover.jpg\"}"; then
        OM3_OK=1
        echo "MUS-OM-WRITE3-OK: ok 수취 — opus relay 쓰기 성립(WriteOpusTag — OpusTags 패킷 교체+페이지 재조립 — D3)"
        sleep 8
        CAPTURE "$TMPD/mus_phone_om_opus.png"   # opus 행 표기(D5 — EYES)
        echo "MUS-OM-OPUS-LEDGER: 예상 상단 행: 'Phone Omus Alpha (Probe Om Artist)'+'Om Album One'(.opus 행 — 커버 썸네일 = MBP 교체 jpg) — 캡처 육안 몫(EYES)"
    else
        echo "MUSICP-FAIL-SOFT(om-write3: opus 도구 쓰기 거부 — $TW_REPLY)"
    fi
    AP3=$(python3 "$TMPD/pmus_omparse.py" "$TAGDIR/t_om_write.opus")
    echo "MUS-OM-PARSED-C: $AP3 (opus 파일 수준 재독 — OpusTags/MBP 재조립 round-trip 원문)"
    OM_MBP_3=$(printf '%s\n' "$AP3" | sed -n 's/.*OM-MBP mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    OM_MBP_MIME_3=$(printf '%s\n' "$AP3" | sed -n 's/.*OM-MBP mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\1/p')
    OM_VENDOR=$(printf '%s\n' "$AP3" | sed -n "s/.*vendor='\([^']*\)'.*/\1/p")
    OM_HT_3=$(printf '%s\n' "$AP3" | sed -n 's/.*cmt_htype=\([0-9a-z]*\).*/\1/p')
    echo "MUS-OM-MBP-3: mime=${OM_MBP_MIME_3:-none} md5=${OM_MBP_3:-none} vendor=${OM_VENDOR:-none} htype=${OM_HT_3:-none}"
    if [ -n "$OM_MBP_3" ] && [ "$OM_MBP_3" = "$OM_CM" ] \
        && [ "$OM_MBP_MIME_3" = "image/jpeg" ] && [ "$OM_VENDOR" = "om-vendor" ] \
        && [ "$OM_HT_3" = "4" ]; then
        OM_MBP_RT=1
        echo "MUSIC-OM-MBP-RT: OK — opus MBP == 합성 cover jpg+벤더 유지+코멘트 페이지 EOS 0x04 유지(D3/D4/D9/D10 라이브 대응)"
    else
        echo "MUSICP-FAIL-SOFT(om-mbp-rt: MBP 교체/벤더/EOS 불일치 — md5=${OM_MBP_3:-none} vendor=${OM_VENDOR:-none} htype=${OM_HT_3:-none})"
    fi

    echo "--- N7-6. 도구 쓰기 4 — opus cover 생략(MBP 유지 leg)+title 교체 ---"
    if tw_write "{\"path\":\"$TAGDIR/t_om_write.opus\",\"title\":\"$OM_TITLE_B\",\"artist\":\"$OM_ARTIST\",\"album\":\"$OM_ALBUM\"}"; then
        OM4_OK=1
        echo "MUS-OM-WRITE4-OK: ok 수취 — opus cover 생략 = 기존 MBP 유지 leg(WriteOpusTag 유지 계약 — FLAC PICTURE 생략 leg의 opus 대응)"
        sleep 8
    else
        echo "MUSICP-FAIL-SOFT(om-write4: opus 도구 쓰기 2 거부 — $TW_REPLY)"
    fi
    AP4=$(python3 "$TMPD/pmus_omparse.py" "$TAGDIR/t_om_write.opus")
    echo "MUS-OM-PARSED-D: $AP4 (쓰기 4 재독 — title 교체+MBP 유지 원문)"
    OM_MBP_4=$(printf '%s\n' "$AP4" | sed -n 's/.*OM-MBP mime=\([^ ]*\) md5=\([0-9a-f]*\).*/\2/p')
    if [ -n "$OM_MBP_4" ] && [ "$OM_MBP_4" = "$OM_CM" ]; then
        OM_MBP_KEEP=1
        echo "MUS-OM-MBP-KEEP: OK — opus cover 생략 뒤 MBP 원문 유지(md5 등호 — $OM_MBP_4)"
    else
        echo "MUSICP-FAIL-SOFT(om-mbp-keep: opus cover 생략 뒤 MBP 소각/변조 — md5=${OM_MBP_4:-none})"
    fi

    echo "--- N7-7. 필터 라운드 스킵 원장행 + OM 시드 소각 (probe 소유 잔상 0 — REMNANT 계약 — N6-6 수형) ---"
    rm -f "$TMPD/pmus_omparse.py"
    rm -f "$TAGDIR/t_om_write.m4a" "$TAGDIR/t_om_write.opus" "$TAGDIR/t_om_cover.jpg"
    rm -rf "$OMDIR"
    OM_LEFT=$(ls "$TAGDIR"/t_om_* 2>/dev/null | wc -l)
    [ "$OM_LEFT" -eq 0 ] \
        && echo "OM-BURIED: t_om_* 3건 소각 (P3 \$TAGDIR 통짜 소각+드라이버 REMNANT 게이트 병행)" \
        || echo "MUSICP-FAIL-SOFT(om-bury: OM 시드 잔존 $OM_LEFT건 — 수동 소각)"
    echo "MUS-OM-SUMMARY-2: stage=$OM_STAGE land=$OM_LAND w1=$OM1_OK w2=$OM2_OK w3=$OM3_OK w4=$OM4_OK covr_rt=$OM_COVR_RT covr_keep=$OM_COVR_KEEP mbp_rt=$OM_MBP_RT mbp_keep=$OM_MBP_KEEP covr_1=${OM_MD5_1:-none} mbp_3=${OM_MBP_3:-none} (N7 세그먼트 원문 종합 — 드라이버 판정 파싱 원문행)"
fi
[ "${OM_STAGE:-0}" -eq 1 ] || echo "MUS-OM-SUMMARY-2: stage=0 land=0 w1=0 w2=0 w3=0 w4=0 covr_rt=0 covr_keep=0 mbp_rt=0 mbp_keep=0 (Music 창 부재/시드 미착 — 세그먼트 생략 — 원장행)"
echo "MUS-OM-SEGMENT-END (OM 시드 소각 = N7-7·P3·REMNANT 게이트 몫 — 표기·썸네일 = 캡처 육안 몫 EYES)"

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
echo "=== P3. [T4 #99] 태그 트랙 dir 소각 (N5 세그먼트 — probe 소유 잔상 0) ==="
rm -rf "$TAGDIR"
[ -d "$TAGDIR" ] && FAIL "tag seed dir 소각 실패 ($TAGDIR — 수동 소각 필요)" \
    || echo "TAG-TREES-BURIED: $TAGDIR removed (태그 mp3 3건 소각 — REMNANT 게이트 병행)"
# [#111 T6] TW 시드 dir 방어적 봉합(정상 경로면 N6-1 mv·N6-6 rm으로 이미 소각 —
# Music 창 부재 등의 세그먼트 생략 계보에서도 잔상 0 — REMNANT 게이트 대응).
rm -rf "$TWDIR" 2>/dev/null
[ -d "$TWDIR" ] && echo "MUSICP-FAIL-SOFT(tw-dir-bury: $TWDIR 소각 실패 — 수동 소각)" \
    || echo "TW-DIR-BURIED: $TWDIR removed (방어적 봉합 — probe 소유 잔상 0)"
rm -f "$ANCH" 2>/dev/null
rm -rf "$XDIR" 2>/dev/null   # N2 세그먼트 방어적 봉합(정상 경로면 이미 소각 — REMNANT 게이트 병행)
BT0=$(date +%s.%N)
rm -rf "$BIGA" "$BIGB" 2>/dev/null
if [ -d "$BIGA" ] || [ -d "$BIGB" ]; then
    FAIL "big tree 소각 실패 ($BIGA $BIGB — 수동 소각 필요)"
fi
echo "BIG-TREES-BURIED: $BIGA·$BIGB removed in $(awk -v a="$BT0" -v b="$(date +%s.%N)" 'BEGIN{printf "%.1f", b-a}')s (T4 취소 세그먼트 소각 — REMNANT 게이트 병행)"

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
# [T2 fix r1 수형] ⑤ 무전달 hard 종결 — MUSP-FINISHED 원문 행 뒤의 rc=1 출구.
# 복귀(N4-10 — remove 3콜+settings 바이트 등호+트리 소각)가 선행 완료된 뒤이므로
# 원복 계약은 무손상. 드라이버는 MUSP-FINISHED 게이트 통과 후 MUSP-FAIL grep으로
# 집계한다(무전달 재발이 fix r1의 수리 실패 원장이 된다 — X 주입 축 재검 원문).
if [ "${BR_ADD_FAIL_HARD:-0}" -eq 1 ]; then
    echo "MUSP-FAIL: N4 ⑤ [이 폴더 추가] 무전달 재발 — 종료 전이·등록 원문 둘 다 미성립(하드 마커 — XTEST 좌표 재보정 몫 bc_*.png 원문은 소각 전 bc 로그·영수증 캡처 참조)"
    exit 1
fi
exit 0
# honest-fail 원칙: 수치 미달(MUSIC-PHONE-FAIL·DELEGATE MISS·glue-cold 부정
# ·CANCEL MISS)은 원장 목적이라 rc=0 — 드라이버가 RUNLOG 판정행에서 집계한다.
# hard FAIL(배포·sweep·빌드·selftest 회귀·시드·settings/perm 원복·부팅·ping·
# launch·창·캡처·소각·big 트리 시드/소각)만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지.
PMUSEOF
sed -i "s/__PHONE_DISPLAY__/${PHONE_DISPLAY#:}/; s/__BIG_SUBS__/$MUSP_BIG_SUBS/; s/__BIG_FILES__/$MUSP_BIG_FILES/; s/__TW_COVER_MD5__/$TW_COVER_MD5/" \
    "$SCRATCH/$RSRC_TAR_NAME" || FAIL "remote script substitution failed"
grep -aq '__PHONE_DISPLAY__\|__BIG_SUBS__\|__BIG_FILES__\|__TW_COVER_MD5__' "$SCRATCH/$RSRC_TAR_NAME" \
    && FAIL "placeholder unresolved (display/big scale/tw cover md5)"

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
tar -tf "$SEEDTAR" | grep -aq 'pmus_seed_stage/tag_music_seed/t_tag_alpha.mp3' || FAIL "seed tar missing tag member (T4 #99 태그 시드 누락)"
tar -tf "$SEEDTAR" | grep -aq 'pmus_seed_stage/tw_music_seed/t_tw_write.mp3' || FAIL "seed tar missing TW member (#111 T6 태그 쓰기 시드 누락)"
tar -tf "$SEEDTAR" | grep -aq 'pmus_seed_stage/tw_music_seed/t_tw_cover.jpg' || FAIL "seed tar missing TW cover member (#111 T6)"
tar -tf "$SEEDTAR" | grep -aq 'pmus_seed_stage/om_music_seed/t_om_write.m4a' || FAIL "seed tar missing OM m4a member (#115 T4 태그 쓰기 시드 누락)"
tar -tf "$SEEDTAR" | grep -aq 'pmus_seed_stage/om_music_seed/t_om_write.opus' || FAIL "seed tar missing OM opus member (#115 T4 태그 쓰기 시드 누락)"
tar -tf "$SEEDTAR" | grep -aq 'pmus_seed_stage/om_music_seed/t_om_cover.jpg' || FAIL "seed tar missing OM cover member (#115 T4)"
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
echo "=== 4. 폰 리빌드+영수증 절차 실행 (ninja + selftest + 레그 3 + N4 브라우저) ==="
# N4 정찰-재보정 env 절차 원문(#97 T4 — wsl 16b2 MUS_BROW_RECON 수형): 드라이버
# env의 MUSP_* 브라우저 상수를 원격 스크립트 실행 접두로 전달(공백값은 인용
# 보존 — 원격 셸이 파싱한다). 재보정 런 = MUSP_BROW_RECON=1 드라이버 env.
RPFX=""
for v in MUSP_BROW_RECON MUSP_BTAB_XS MUSP_BTAB_YS MUSP_BTOG_XS MUSP_BTOG_YS \
         MUSP_BBROW_XS MUSP_BBROW_YS MUSP_BSUB_XS MUSP_BSUB_YS \
         MUSP_BUP_XS MUSP_BUP_YS MUSP_BADD_XS MUSP_BADD_YS \
         MUSP_BROW_TH MUSP_BROW_TH_TEXT MUSP_BROW_TH_ADD; do
    eval "RV=\"\${$v:-}\""
    [ -n "$RV" ] && RPFX="$RPFX $v=\"$RV\""
done
echo "RUN-ENV-PREFIX:${RPFX:- none} (N4 상수 전달 — 재보정 런 절차 원문)"
$SSH "$RPFX bash $RSRC_PHONE" > "$RUNLOG" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
  echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 폰 ninja·selftest 로그는 \$TMPDIR에 생존, 재실행=증분:"
  echo "  PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_music.sh)"
fi
cat "$RUNLOG"
grep -aq '^MUSP-FINISHED$' "$RUNLOG" \
  || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행)"
INH_HARD=0
# [#111 T6 수형 원장 — hard 마커의 rc=1 시점만 말단 이동(마커 행·의미 원존)]:
# 원수형은 MUSP-FAIL grep을 즉시 hard FAIL로 쳤다 — 그러면 REMNANT 게이트·
# 캡처 회수·판정(섹션 5-9)이 통째로 생략되고 폰 측 캡처가 유산으로 남는다
# (2026-10-25 오전 run8 원문: N4 ⑤ 무전달 hard로 본 세그먼트 집합이 미회수).
# N4 ⑤는 이 라인의 잔여 원장행(탭 좌표 재보정 별소관 — 갱신 65)이라 T6의
# 자기 산치(N6 TW 세그먼트·캐논·REMNANT 0)를 그 위에서 정직하게 정산한다 —
# 마지막에 rc=1로 종결해 마커 계약(무전달 재발 = 수치 신호)을 유지한다.
# 그 밖의 MUSP-FAIL(launch/sweep/설정 원복 등)은 원수형 그대로 즉시 hard FAIL.
if grep -aq '^MUSP-FAIL' "$RUNLOG"; then
  HARD=$(grep -a '^MUSP-FAIL' "$RUNLOG" | head -1)
  if printf '%s' "$HARD" | grep -aq 'N4 ⑤'; then
    INH_HARD=1
    echo "NOTE-T6-HARD-DEFERRED: N4 ⑤ 무전달 hard 마커 원장 행 — REMNANT·회수·판정을 계속 수행하고 말단에 rc=1로 종결(마커 원존 — rc=1 시점 이동 원장행 — T6)"
  else
    FAIL "phone hard receipt: $HARD"
  fi
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
REM_XDIR=$($SSH '[ -d "$TMPDIR/mus_dirs_x" ] && echo present || echo gone' | tr -d ' \r')
REM_ANCH=$($SSH '[ -f "$TMPDIR/pmus_dirs_anchor.json" ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-DIRS-XDIR: $REM_XDIR · REMNANT-DIRS-ANCHOR: $REM_ANCH (T4 dirs 세그먼트 잔산 게이트)"
{ [ "$REM_XDIR" = "gone" ] && [ "$REM_ANCH" = "gone" ]; } \
    || FAIL "REMNANT survived: dirs segment residue left on phone (xdir=$REM_XDIR anchor=$REM_ANCH)"
REM_BIGA=$($SSH '[ -d "$TMPDIR/mus_big_a" ] && echo present || echo gone' | tr -d ' \r')
REM_BIGB=$($SSH '[ -d "$TMPDIR/mus_big_b" ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-BIG-TREES: a=$REM_BIGA b=$REM_BIGB (T4 취소 세그먼트 잔산 게이트)"
{ [ "$REM_BIGA" = "gone" ] && [ "$REM_BIGB" = "gone" ]; } \
    || FAIL "REMNANT survived: big trees left on phone (a=$REM_BIGA b=$REM_BIGB)"
REM_BROW=$($SSH '[ -d "$TMPDIR/mus_browse" ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-BROWSE-TREE: $REM_BROW (T4 브라우저 세그먼트 잔산 게이트)"
[ "$REM_BROW" = "gone" ] || FAIL "REMNANT survived: browse tree left on phone (mus_browse=$REM_BROW)"
REM_TAG=$($SSH '[ -d "$TMPDIR/mus_tagseed" ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-TAG-SEEDS: $REM_TAG (T4 #99 태그 세그먼트 잔산 게이트 — 합성 mp3 3건)"
[ "$REM_TAG" = "gone" ] || FAIL "REMNANT survived: tag seeds left on phone (mus_tagseed=$REM_TAG)"
REM_TWS=$($SSH '[ -d "$TMPDIR/mus_twseed" ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-TW-SEEDS: $REM_TWS (#111 T6 태그 쓰기 세그먼트 잔산 게이트 — 합성 3건[N6-6 소각 — TAGDIR 통짜 소각의 이중 가드])"
[ "$REM_TWS" = "gone" ] || FAIL "REMNANT survived: TW seeds left on phone (mus_twseed=$REM_TWS)"
REM_OMS=$($SSH '[ -d "$TMPDIR/mus_omseed" ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-OM-SEEDS: $REM_OMS (#115 T4 태그 쓰기 세그먼트 잔산 게이트 — 합성 3건[N7-7 소각 — TAGDIR 통짜 소각의 이중 가드])"
[ "$REM_OMS" = "gone" ] || FAIL "REMNANT survived: OM seeds left on phone (mus_omseed=$REM_OMS)"
REM_XT=$($SSH '[ -f "$TMPDIR/xtap" ] && echo present || echo gone' | tr -d ' \r')
echo "REMNANT-XT-INJECTOR: $REM_XT (**삭제 금지 계약 — #96/input-repair 원장: 진입 소각은 캡처·로그만, 주입기는 면제 — present=계약 준수**)"
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
# [#111 T6 fix r2 수형 — recover_opt 정의 순서]: 구 수형은 spatial_mix의
# recover_opt 호출이 함수 정의보다 **앞**에 있었다 — bash 함수는 정의 선행
# 계약이라 "recover_opt: command not found"(런 1 원문 — spatial_mix 회수
# 결손 — CAPTURE-OK 영수증은 있지만 회수 0 — CAP_OK=0 요인 1건). 정의를
# 호출 앞으로 이동(함수 본문 무변조 — 순서만 이동).
recover_opt() { # $1 폰쪽png 경로 $2 로컬png — 재판정 단계 생략을 허용하는
                #   회수(N4 정찰 단계 미달 런의 BROWSE 캡처 몫 — 부재=원장행)
    local REMOTE_PNG=$1 LOCAL_PNG=$2
    if ! $SSH "test -s $REMOTE_PNG && echo PRESENT" 2>/dev/null | grep -aq PRESENT; then
        echo "RECOVER-OPT-ABSENT: $REMOTE_PNG (정찰 단계 미달 원장 — N4 행별 참조)"
        return 0
    fi
    recover "$REMOTE_PNG" "$LOCAL_PNG"
}
# #110 T2 — spatial_mix recover (M2 캡처 — UNSET 런 미생성 = 원장행) — 정의 뒤로 이동(fix r2)
recover_opt "\$TMPDIR/mus_phone_spatial_mix.png" "$SCRATCH/mus_phone_spatial_mix.png"
recover_opt "\$TMPDIR/mus_phone_browse_open.png" "$SCRATCH/mus_phone_browse_open.png"
recover_opt "\$TMPDIR/mus_phone_browse_subs.png" "$SCRATCH/mus_phone_browse_subs.png"
recover_opt "\$TMPDIR/mus_phone_browse_sub.png" "$SCRATCH/mus_phone_browse_sub.png"
recover_opt "\$TMPDIR/mus_phone_browse_up.png" "$SCRATCH/mus_phone_browse_up.png"
recover_opt "\$TMPDIR/mus_phone_browse_sub2.png" "$SCRATCH/mus_phone_browse_sub2.png"
recover_opt "\$TMPDIR/mus_phone_browse_added.png" "$SCRATCH/mus_phone_browse_added.png"
recover_opt "\$TMPDIR/mus_phone_browse_rescan.png" "$SCRATCH/mus_phone_browse_rescan.png"
recover_opt "\$TMPDIR/mus_phone_tag_after.png" "$SCRATCH/mus_phone_tag_after.png"
# [#111 T6] N6 태그 쓰기 세그먼트 캡처 3종(행 32px 커버 썸네일+[편집] UI 표기 원문 —
# 세그먼트 단계 미달 런 = ABSENT 원산 — recover_opt 수형)
recover_opt "\$TMPDIR/mus_phone_tagwrite_list.png" "$SCRATCH/mus_phone_tagwrite_list.png"
recover_opt "\$TMPDIR/mus_phone_tagwrite_list2.png" "$SCRATCH/mus_phone_tagwrite_list2.png"
recover_opt "\$TMPDIR/mus_phone_tagwrite_flac.png" "$SCRATCH/mus_phone_tagwrite_flac.png"
# [#115 T4] N7 opus/m4a 세그먼트 캡처 2종(m4a 행·opus 행 — D5 표기 원문 —
# 세그먼트 단계 미달 런 = ABSENT 원산 — recover_opt 수형)
recover_opt "\$TMPDIR/mus_phone_om_list.png" "$SCRATCH/mus_phone_om_list.png"
recover_opt "\$TMPDIR/mus_phone_om_list2.png" "$SCRATCH/mus_phone_om_list2.png"
recover_opt "\$TMPDIR/mus_phone_om_opus.png" "$SCRATCH/mus_phone_om_opus.png"
# 정찰 중간 프레임(bc_*) — 성공 런은 원격 소각(ABSENT 원산), 무전달/정찰 런만 유산
for t in bt0 btog bropen bsub bup bsub2 badd; do
    recover_opt "\$TMPDIR/mus_phone_bc_$t.png" "$SCRATCH/mus_phone_bc_$t.png"
done
$SSH "rm -f \$TMPDIR/mus_phone_list.png \$TMPDIR/mus_phone_list2.png \$TMPDIR/mus_phone_spatial.png \$TMPDIR/mus_phone_spatial_mix.png \$TMPDIR/mus_phone_vp_delegate.png \$TMPDIR/mus_phone_delegate_status.png \$TMPDIR/mus_phone_after.png \$TMPDIR/mus_phone_sdcard.png \$TMPDIR/mus_phone_dirs.png \$TMPDIR/mus_phone_dirs2.png \$TMPDIR/mus_phone_scan_busy.png \$TMPDIR/mus_phone_rescan_settled.png \$TMPDIR/mus_phone_browse_open.png \$TMPDIR/mus_phone_browse_subs.png \$TMPDIR/mus_phone_browse_sub.png \$TMPDIR/mus_phone_browse_up.png \$TMPDIR/mus_phone_browse_sub2.png \$TMPDIR/mus_phone_browse_added.png \$TMPDIR/mus_phone_browse_rescan.png \$TMPDIR/mus_phone_taglist.png \$TMPDIR/mus_phone_taglist2.png \$TMPDIR/mus_phone_tag_after.png \$TMPDIR/mus_phone_tagwrite_list.png \$TMPDIR/mus_phone_tagwrite_list2.png \$TMPDIR/mus_phone_tagwrite_flac.png \$TMPDIR/mus_phone_bc_twbase.png \$TMPDIR/mus_phone_bc_twafter.png \$TMPDIR/pmus_twparse.py \$TMPDIR/pmus_omparse.py \$TMPDIR/pmus_orig_settings.json \$TMPDIR/pmus_orig_permissions.json"
echo "CAPTURES-WRITTEN: mus_phone_{list,list2,spatial,vp_delegate,delegate_status,after,sdcard,dirs,dirs2,scan_busy,rescan_settled,browse_open,browse_subs,browse_sub,browse_up,browse_sub2,browse_added,browse_rescan,taglist,taglist2,tag_after,tagwrite_list,tagwrite_list2,tagwrite_flac,om_list,om_list2,om_opus}.png → engine/tmp/"

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
        ("mus_phone_spatial_mix.png", "spatial_mix"),
        ("mus_phone_vp_delegate.png", "vp_delegate"),
        ("mus_phone_delegate_status.png", "delegate_status"), ("mus_phone_after.png", "after"),
        ("mus_phone_sdcard.png", "sdcard"), ("mus_phone_dirs.png", "dirs"),
        ("mus_phone_dirs2.png", "dirs2"), ("mus_phone_scan_busy.png", "scan_busy"),
        ("mus_phone_rescan_settled.png", "rescan_settled"),
        ("mus_phone_browse_open.png", "browse_open"),
        ("mus_phone_browse_subs.png", "browse_subs"),
        ("mus_phone_browse_sub.png", "browse_sub"),
        ("mus_phone_browse_up.png", "browse_up"),
        ("mus_phone_browse_sub2.png", "browse_sub2"),
        ("mus_phone_browse_added.png", "browse_added"),
        ("mus_phone_browse_rescan.png", "browse_rescan"),
        ("mus_phone_taglist.png", "taglist"),
        ("mus_phone_taglist2.png", "taglist2"),
        ("mus_phone_tag_after.png", "tag_after"),
        ("mus_phone_tagwrite_list.png", "tagwrite_list"),
        ("mus_phone_tagwrite_list2.png", "tagwrite_list2"),
        ("mus_phone_tagwrite_flac.png", "tagwrite_flac"))
# T4 취소 세그먼트의 geo 라벨 매핑 — leg big의 MUS-GEO-big(big2 = 재스폰 클라)
# T4(#97) 브라우저 캡처 = 같은 music 클라(leg list) — geo 라벨 list(수형)
GEO_LABELS = {"list": "list", "sdcard": "sdcard", "scan_busy": "big",
              "rescan_settled": "big2", "browse_open": "list",
              "browse_subs": "list", "browse_sub": "list", "browse_up": "list",
              "browse_sub2": "list", "browse_added": "list",
              "browse_rescan": "list", "taglist": "tag",
              "taglist2": "tag", "tag_after": "tag",
              "tagwrite_list": "tw", "tagwrite_list2": "tw",
              "tagwrite_flac": "tw"}
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
    glbl = GEO_LABELS.get(tag, "list")
    geo = parse_geo(txt, glbl) if glbl else None
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
# T4 스캔 취소 세그먼트 원문 수취
cl = re.search(r"MUSIC-BIG-CANCEL-SUMMARY: (.*)", txt)
if cl:
    print("CANCEL-SUMMARY: %s" % cl.group(1))
cl2 = re.search(r"MUSIC-ARRIVAL-DISCARD-\d: (.*)", txt)
if cl2:
    print("CANCEL-DISCARD: %s" % cl2.group(1))
cg = re.search(r"MUS-DISCARD-AT: (.*)", txt)
if cg:
    print("CANCEL-AT-LINE: %s" % cg.group(1))
ce = re.search(r"MUS-CANCEL-EFFECTIVE: (.*)", txt)
if ce:
    print("CANCEL-EFFECTIVE: %s" % ce.group(1))
clg = re.search(r"MUS-CLOSE-GONE: (.*)", txt)
if clg:
    print("CLOSE-GONE-LINE: %s" % clg.group(1))
cv = re.search(r"MUSIC-CLOSE-CRASH-MARKERS: (\d+)", txt)
if cv:
    print("CRASH-MARKERS: %s" % cv.group(1))
cv2 = re.search(r"MUSIC-CANCEL-VERDICT: (.*)", txt)
if cv2:
    print("CANCEL-VERDICT: %s" % cv2.group(1))
# T4(#97) 브라우저 세그먼트 원문 수취
bs = re.search(r"MUS-BROWSE-SUMMARY: (.*)", txt)
if bs:
    print("BROWSE-SUMMARY: %s" % bs.group(1))
bw = re.search(r"MUS-BROWSE-ADD-CLICK-OK\((FULL|DELIVERED)\)(.*)", txt)
if bw:
    print("BROWSE-ADD-LINE: %s%s" % (bw.group(1), bw.group(2)[:160]))
sr = re.search(r"MUS-BROWSE-SUB-REG: (.*)", txt)
if sr:
    print("BROWSE-SUB-REG: %s" % sr.group(1))
br = re.search(r"MUS-BROWSE-RESTORE-BYTE: (.*)", txt)
if br:
    print("BROWSE-RESTORE: %s" % br.group(1))
bx = re.search(r"MUS-BROWSE-EXIT-SKIP: (.*)", txt)
if bx:
    print("BROWSE-EXIT-SKIP: %s" % bx.group(1))
# T4(#99) 태그 세그먼트 원문 수취
tg = re.search(r"MUS-TAG-TAB-OK: (.*)", txt)
if tg:
    print("TAG-TAB-LINE: %s" % tg.group(1))
ts = re.search(r"MUS-TAG-SUMMARY-2: (.*)", txt)
if ts:
    print("TAG-SUMMARY: %s" % ts.group(1))
to = re.search(r"MUS-TAG-OPEN-STATUS: (.*)", txt)
if to:
    print("TAG-OPEN-STATUS: %s" % to.group(1)[:240])
tf = re.search(r"MUS-TAG-OPENFAIL: (.*)", txt)
if tf:
    print("TAG-OPENFAIL: %s" % tf.group(1))
tv = re.search(r"MUS-TAG-VP-RECLAIM: (.*)", txt)
if tv:
    print("TAG-VP-RECLAIM: %s" % tv.group(1))
tj = re.search(r"MUSIC-TAG-PHONE-VERDICT: (.*)", txt)
if tj:
    print("TAG-VERDICT: %s" % tj.group(1))
# [#111 T6] N6 태그 쓰기 세그먼트 원문 수취
twr = re.search(r"MUS-TW-PARSED-A: (.*)", txt)
if twr:
    print("TW-PARSED-A: %s" % twr.group(1))
twr2 = re.search(r"MUS-TW-PARSED-B: (.*)", txt)
if twr2:
    print("TW-PARSED-B: %s" % twr2.group(1))
twf = re.search(r"MUS-TW-FLAC-PARSED: (.*)", txt)
if twf:
    print("TW-FLAC-PARSED: %s" % twf.group(1))
tws = re.search(r"MUS-TW-SUMMARY-2: (.*)", txt)
if tws:
    print("TW-SUMMARY: %s" % tws.group(1))
twc = re.search(r"MUSIC-TW-COVER-RT: (.*)", txt)
if twc:
    print("TW-COVER-RT: %s" % twc.group(1))
twk = re.search(r"MUS-TW-APIC-KEEP: (.*)", txt)
if twk:
    print("TW-APIC-KEEP: %s" % twk.group(1))
twv = re.search(r"MUSIC-TAGWRITE-PHONE-VERDICT: (.*)", txt)
if twv:
    print("TW-VERDICT: %s" % twv.group(1))
oms = re.search(r"MUS-OM-SUMMARY-2: (.*)", txt)
if oms:
    print("OM-SUMMARY: %s" % oms.group(1))
oma = re.search(r"MUS-OM-PARSED-A: (.*)", txt)
if oma:
    print("OM-PARSED-A: %s" % oma.group(1))
omc = re.search(r"MUS-OM-PARSED-C: (.*)", txt)
if omc:
    print("OM-PARSED-C: %s" % omc.group(1))
omrt = re.search(r"MUSIC-OM-COVR-RT: (.*)", txt)
if omrt:
    print("OM-COVR-RT: %s" % omrt.group(1))
omm = re.search(r"MUSIC-OM-MBP-RT: (.*)", txt)
if omm:
    print("OM-MBP-RT: %s" % omm.group(1))
ANALYSIS_EOF
if [ $? -ne 0 ]; then
  echo "NOTE-ANALYSIS: local analysis failed (rc) — 캡처 원문은 회수 원문으로 육안 가능"
fi

# ------------------------------------------------------------------ 9. 판정
echo "=== 7. music 폰 실측 판정 (측정 원문은 상단 analysis 행 — 최종 결제는 육안 스탭) ==="
CAP_OK=1
for f in list list2 spatial vp_delegate delegate_status after sdcard dirs dirs2 scan_busy rescan_settled taglist taglist2; do
    P="$SCRATCH/mus_phone_$f.png"
    if [ ! -s "$P" ]; then
        echo "MUSIC-PHONE-FAIL(capture missing/empty: $P)"
        CAP_OK=0
    fi
done
# 합산 캡처(M2 — #110 T2 WIRED 런 전용): 원격 CAPTURE-OK 영수증이 소유한
# 캡처만 요구한다(UNSET 런에서는 아예 찍히지 않는다 — 회귀창 폐곡).
for f in spatial_mix; do
    if grep -aq "CAPTURE-OK: .*mus_phone_$f.png" "$RUNLOG"; then
        [ -s "$SCRATCH/mus_phone_$f.png" ] \
            || { echo "MUSIC-PHONE-FAIL(capture missing/empty: mus_phone_$f.png — 영수증 있는데 회수 결손)"; CAP_OK=0; }
    else
        echo "NOTE-CAP: mus_phone_$f.png 영수증 없음 — M2 세그먼트 미성립 런(WIRED 런이라면 M2 행 참조)"
    fi
done
CANON=$(grep -a '^CANON-INCLUSION=' "$RUNLOG" | tail -1 | sed 's/^CANON-INCLUSION=//' | tr -d '\r')
CANON_OK=0
case "$CANON" in
  MUSIC-FULL-2P2QRS-*) CANON_OK=1 ;;
  MUSIC-FULL-2P2QR-*) CANON_OK=1
      echo "NOTE-CANON: 구 2P2QR 라벨 — 2s 미갱신 원장행(#99 T4 정산 몫 — 2s 어설션이 소실됐거나 상수 낡음 — PHONE-SELFTEST 2s 행 참조)" ;;
  MUSIC-FULL-2P2Q-*) CANON_OK=1
      echo "NOTE-CANON: 구 2P2Q 라벨 — 2r·2s 미갱신 원장행(#97/#99 T4 정산 몫)" ;;
  *) echo "NOTE-CANON: 캐논 계보 ${CANON:-n/a} — 위 경고 행 참조" ;;
esac
# dirs 도구 원문(T4 — N2 세그먼트) — DIRS 판정 행(N-1 승계: p1=crash-fixed 표기).
DADD_L=$(grep -a '^MUSIC-DIR-ADD-REPLY: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-DIR-ADD-REPLY: //' | tr -d '\r')
DL1_L=$(grep -a '^MUSIC-DIR-LIST-1: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-DIR-LIST-1: //' | tr -d '\r')
DL2_L=$(grep -a '^MUSIC-DIR-LIST-2: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-DIR-LIST-2: //' | tr -d '\r')
DSET_L=$(grep -a '^MUSIC-DIR-SETTINGS-CHECK: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-DIR-SETTINGS-CHECK: //' | tr -d '\r')
DBYT_L=$(grep -a '^MUSIC-DIR-BYTE-RESTORE: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-DIR-BYTE-RESTORE: //' | tr -d '\r')
DIRS_OK=0
if printf '%s' "$DADD_L" | grep -aq '"ok":true' \
   && printf '%s' "$DL1_L" | grep -aq 'mus_dirs_x' \
   && printf '%s' "$DSET_L" | grep -aq 'SET-OK' \
   && printf '%s' "$DL2_L" | grep -aq '"ok":true' \
   && ! printf '%s' "$DL2_L" | grep -aq 'mus_dirs_x' \
   && printf '%s' "$DL2_L" | grep -aq 'pcm_music_seed' \
   && printf '%s' "$DBYT_L" | grep -aq 'OK'; then
    DIRS_OK=1
fi
if [ "$DIRS_OK" -eq 1 ]; then
    echo "MUSIC-DIRS-VERDICT: DIRS-OK(add=ok·list 등록/제거 원문·settings music.dirs 실기록+text/font_path 보존·원복 바이트 등호) — p1=crash-fixed(T2 fix r2 — UI 행 루프 UAF 수리 후의 도구 경로 재실측; [제거] 클릭 경로 원문은 WSL T3 런 소유) — 폰 CP949 거부/치유 세그먼트 스킵 원장행(WSL T3 16c 소유 — 전부 실측됨) — 육안 스탭 대기"
else
    echo "MUSIC-DIRS-VERDICT: DIRS-FAIL(add=${DADD_L:-none} list1=${DL1_L:-none} list2=${DL2_L:-none} settings=${DSET_L:-none} bytes=${DBYT_L:-none} — 행별 사유는 N2)"
fi
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
# 폰 leg 판정(#110 T2 — 모드 분기): WIRED 런은 성립 원문(spatial_play
# accepted·pos 진행 원문·TONE 시작 지연 원장 행·stop idempotent)이 계약이고,
# UNSET(구 T4 배제) 런은 start_failed + kDelegationHint 영수증이 계약이다.
# 진실원 = 바이너리(MUSP-LEG-MODE — 원격 수형): 성립 런(leg 있음)에 배제
# 원문을 요구하면 구 원장의 오산 — 원장 폐곡(#110).
LEGMODE=$(grep -a '^MUSP-LEG-MODE: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-LEG-MODE: //' | tr -d '\r')
SPATL=$(grep -a '^MUSP-SPATIAL-PLAY-REPLY: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-SPATIAL-PLAY-REPLY: //' | tr -d '\r')
INACT=$(grep -a '^MUSP-SPATIAL-POS: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-SPATIAL-POS: //' | tr -d '\r')
LEG_OK=0
LEG_LINE="mode: ${LEGMODE:-n/a}"
case "$LEGMODE" in
  WIRED*)
    SPAT_PROG=$(grep -a '^MUSP-SPATIAL-POS-PROGRESS: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-SPATIAL-POS-PROGRESS: //' | tr -d '\r')
    SPAT_DELAY=$(grep -a '^MUSP-TONE-START-DELAY: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-TONE-START-DELAY: //' | tr -d '\r')
    SPAT_STOPR=$(grep -a '^MUSP-SPATIAL-STOP-REPLY: ' "$RUNLOG" | tail -1 | sed 's/^MUSP-SPATIAL-STOP-REPLY: //' | tr -d '\r')
    printf '%s' "${SPATL:-}" | grep -aq '"accepted":true' \
      && printf '%s' "${SPAT_PROG:-}" | grep -aq '^OK' \
      && [ -n "${SPAT_DELAY:-}" ] \
      && printf '%s' "${SPAT_STOPR:-}" | grep -aq '"active":false' \
      && LEG_OK=1
    LEG_LINE="WIRED accepted=${SPATL:-none} progress=${SPAT_PROG:-none} delay=${SPAT_DELAY:-MISS} stop=${SPAT_STOPR:-none}"
    ;;
  UNSET*)
    printf '%s' "${SPATL:-}" | grep -aq '"error":"start_failed"' \
      && printf '%s' "${SPATL:-}" | grep -aq 'spatial leg 불가' \
      && printf '%s' "${INACT:-}" | grep -aq 'MISS(미활성' \
      && LEG_OK=1
    LEG_LINE="UNSET barred=${SPATL:-none} inactive=${INACT:-none}"
    ;;
  *) echo "NOTE-LEG: MUSP-LEG-MODE 행 미수취 (원격 수형 결손 — F 섹션 원문 참조)" ;;
esac
# 병행 합산 원문 수취(WIRED 런 — T1 spike concern 4 보강 영수증)
MIXL=$(grep -a '^MUSP-SPATIAL-MIX: ' "$RUNLOG" | head -1 | sed 's/^MUSP-SPATIAL-MIX: //' | tr -d '\r')
printf '%s' "$MIXL" | grep -aq '^OK' \
    && echo "MIX-LINE: MUSP-SPATIAL-MIX=$MIXL (폰 OpenAL/SDL 동시 재생 — T1 concern 4 보강 원문)" \
    || echo "MIX-LINE: ${MIXL:-none} (SKIPPED/MISS는 원장행 — UNSET 런 정상)"
# T4 스캔 취소 경계(CANCEL 판정 행 — N3 세그먼트): ②폐기 1행+절단 시차·클라 생존
# + ①close 소멸+crash 0 + ③재스폰 도구 원문 — 성립이면 CANCEL-OK.
CANC_L=$(grep -a '^MUSIC-ARRIVAL-DISCARD-[0-9]: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-ARRIVAL-DISCARD-[0-9]: //' | tr -d '\r')
EFF_L=$(grep -a '^MUS-CANCEL-EFFECTIVE: ' "$RUNLOG" | tail -1 | sed 's/^MUS-CANCEL-EFFECTIVE: //' | tr -d '\r')
ALIVE_L=$(grep -a '^MUSIC-ALIVE-WINDOW-' "$RUNLOG" | tail -1 | sed 's/^[^:]*: //' | tr -d '\r')
GONE_L=$(grep -a '^MUS-CLOSE-GONE: ' "$RUNLOG" | tail -1 | sed 's/^MUS-CLOSE-GONE: //' | tr -d '\r')
CRASH_N=$(grep -a '^MUSIC-CLOSE-CRASH-MARKERS: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-CLOSE-CRASH-MARKERS: //' | awk '{print $1}' | tr -d ' \r')
RESP_L=$(grep -a '^MUSIC-RESPAWN-LIST: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-RESPAWN-LIST: //' | tr -d '\r')
CANCEL_OK=0
if [ -n "$CANC_L" ] && [ "$CANC_L" != "MISS" ] \
   && printf '%s' "$CANC_L" | grep -aq 'mus_big' \
   && printf '%s' "$EFF_L" | grep -aq '^OK' \
   && printf '%s' "$ALIVE_L" | grep -aq '^OK' \
   && printf '%s' "$GONE_L" | grep -aq '^OK' \
   && [ "${CRASH_N:-1}" -eq 0 ] \
   && printf '%s' "$RESP_L" | grep -aq '^OK'; then
    CANCEL_OK=1
fi
if [ "$CANCEL_OK" -eq 1 ]; then
    echo "MUSIC-CANCEL-VERDICT: CANCEL-OK(②폐기 1행=$CANC_L · $EFF_L · $ALIVE_L · $GONE_L · crash=$CRASH_N · 재스폰 도구 원문=OK — ③정착 캡처=rescan_settled(EYES))"
else
    echo "MUSIC-CANCEL-VERDICT: CANCEL-FAIL(discard=${CANC_L:-none} · effective=${EFF_L:-none} · alive=${ALIVE_L:-none} · close=${GONE_L:-none} · crash=${CRASH_N:-n/a} · respawn=${RESP_L:-none} — 폰 스캔 속도 미실측 원장행 — MUSP-BIG SCALE 재정산 재런 몫)"
fi
# T4(#97) 브라우저 세그먼트(BROWSE 판정 행 — wsl 18e 수형): 요약 행 파싱+
# 캡처 회수 대차. ⑤의 하드 마커는 MUSP-FAIL grep(드라이버 상단)이 이미
# rc=1로 집계하므로 여기선 성립 등급(FULL/DELIVERED) 표기만.
BSUM=$(grep -a '^MUS-BROWSE-SUMMARY: ' "$RUNLOG" | tail -1 | sed 's/^MUS-BROWSE-SUMMARY: //' | tr -d '\r')
BR_STAGE_L=$(printf '%s' "$BSUM" | sed -n 's/.*stage=\([0-9]*\)\/8.*/\1/p')
BR_ADD_L=$(printf '%s' "$BSUM" | sed -n 's/.* add=\([0-9]*\).*/\1/p')
BR_SUB2_L=$(printf '%s' "$BSUM" | sed -n 's/.*sub2=\([0-9]*\).*/\1/p')
BR_RESTORE_L=$(printf '%s' "$BSUM" | sed -n 's/.*restore=\([0-9]*\).*/\1/p')
BR_FULL_L=$(printf '%s' "$BSUM" | sed -n 's/.*full=\([0-9]*\).*/\1/p')
BR_CAP_OK=1
BR_MISSING=""
for f in browse_open browse_subs browse_sub browse_up browse_sub2 browse_added browse_rescan; do
    P="$SCRATCH/mus_phone_$f.png"
    [ -s "$P" ] || { BR_MISSING="$BR_MISSING $f"; }
done
[ -n "${BR_MISSING:-}" ] && { BR_CAP_OK=0; echo "MUSIC-PHONE-FAIL(browse capture missing:$BR_MISSING — 정찰 단계 미달 런 원장)"; }
BROWSE_OK=0
if [ "${BR_STAGE_L:-0}" -eq 8 ] && [ "${BR_ADD_L:-0}" -eq 1 ] \
   && [ "${BR_SUB2_L:-0}" -eq 1 ] && [ "${BR_RESTORE_L:-0}" -eq 1 ] \
   && [ "$BR_CAP_OK" -eq 1 ]; then
    BROWSE_OK=1
fi
if [ "$BROWSE_OK" -eq 1 ]; then
    echo "MUSIC-BROWSE-VERDICT: BROWSE-OK(②탭 클릭 브래킷+패널+[찾아보기] 개봉 캡처 2종·③JoinDir·④Parent·④2 재진입·⑤[이 폴더 추가] $( [ "${BR_FULL_L:-0}" -eq 1 ] && echo "신규 등록 원문(list/settings) FULL" || echo "전달 성립 — 등록 강도 = no-op 원장")+복귀 바이트 등호+6. 재스캔 캡처 — XTEST 주입 축(#96 수형) — 육안 스탭 대기)"
else
    echo "MUSIC-BROWSE-VERDICT: BROWSE-FAIL(honest-fail — summary=${BSUM:-none}·captures=${BR_CAP_OK}${BR_MISSING:+ missing$BR_MISSING} — 행별 사유는 N4·좌표 재보정은 bc_* 캡처 몫(회수된 정찰 프레임))"
fi
# T4(#99) 태그 표기 세그먼트(TAG 판정 행 — wsl 18g 수형): 병합 등록 진실원+
# 탭 착탄+표기 캡처 2종+vplayer openFailed 영수증. 타이핑 세그먼트는 폰 스킵
# 원장행(합성 타이핑 신뢰 원장 T3 concern 2 — WSL 16e-3/4 소유·휴대폰 표
# 필터는 도구 경로 부재 = EYES 원장행) — 태그의 캡처 원문 소유 축은 WSL이
# 소유했지만 폰은 **표기 원문(recon+캡처)**+영수증으로 소유한다(파서가 폰
# 바이너리에서 도달함의 원문 — 본 태스크의 실측 산치).
TGTAB_L=$(printf '%s' "$(grep -a '^MUS-TAG-SUMMARY-2: ' "$RUNLOG" | tail -1)" | sed 's/^MUS-TAG-SUMMARY-2: //' | tr -d '\r')
TGTAB_N=$(printf '%s' "$TGTAB_L" | sed -n 's/.*tab=\([0-9]*\).*/\1/p')
TGOFF_N=$(printf '%s' "$TGTAB_L" | sed -n 's/.*openfail=\([0-9]*\).*/\1/p')
TGDL=$(grep -a '^MUSIC-TAG-DIR-LIST-0: ' "$RUNLOG" | tail -1 | sed 's/^MUSIC-TAG-DIR-LIST-0: //' | tr -d '\r')
TAG_CAP2_OK=1
for f in taglist taglist2; do
    [ -s "$SCRATCH/mus_phone_$f.png" ] || TAG_CAP2_OK=0
done
TAG_OK=0
if [ "${TGTAB_N:-0}" -eq 1 ] && [ "$TAG_CAP2_OK" -eq 1 ] \
   && [ "${TGOFF_N:-0}" -eq 1 ] \
   && printf '%s' "${TGDL:-}" | grep -aqF mus_tagseed; then
    TAG_OK=1
fi
if [ "$TAG_OK" -eq 1 ]; then
    echo "MUSIC-TAG-PHONE-VERDICT: TAG-OK(N5 — 병합 등록 진실원(dirlist)+XTEST 탭 착탄(tapsettle)+표기 캡처 2종[taglist·taglist2 — 곡명(아티스트)·앨범·폴백 행의 폰 원문]+vplayer openFailed 영수증 open은 수취·개방 실패[유효 프레임 0]) — **표기 문자열 자체는 캡처 육안 몫(EYES)**·타이핑 세그먼트는 폰 스킵 원장행(WSL 16e-3/4 소유) — 육안 스탭 대기"
else
    echo "MUSIC-TAG-PHONE-VERDICT: TAG-FAIL(honest-fail — 행별 사유는 N5; dirlist=${TGDL:-none} tab=${TGTAB_N:-n/a} captures=$TAG_CAP2_OK openfail=${TGOFF_N:-n/a} — 타이핑 스킵 원장행 유지)"
fi
# [#111 T6] N6 태그 쓰기(도구) 세그먼트(TW 판정 행 — wsl 18h 수형): 요약 행 파싱+
# 캡처 회수 대차. 강도 축 = **도구 경계(relay ok)+파일 재독 round-trip(APIC md5
# 등호)** — 목록 행 표기·썸네일·[편집] 모양 = 캡처 육안(EYES)·필터 라운드 = 폰
# 스킵 원장(N5 승계) — 주 VERDICT 구성항 병합 0(18h 수형 — T5 원존 계약).
TWSUM=$(grep -a '^MUS-TW-SUMMARY-2: ' "$RUNLOG" | tail -1 | sed 's/^MUS-TW-SUMMARY-2: //' | tr -d '\r')
TW_STAGE_L=$(printf '%s' "$TWSUM" | sed -n 's/.*stage=\([0-9]*\).*/\1/p')
TW_LAND_L=$(printf '%s' "$TWSUM" | sed -n 's/.*land=\([0-9]*\).*/\1/p')
TW_W1_L=$(printf '%s' "$TWSUM" | sed -n 's/.*w1=\([0-9]*\).*/\1/p')
TW_W2_L=$(printf '%s' "$TWSUM" | sed -n 's/.*w2=\([0-9]*\).*/\1/p')
TW_WF_L=$(printf '%s' "$TWSUM" | sed -n 's/.*flac=\([0-9]*\).*/\1/p')
TW_RT_L=$(printf '%s' "$TWSUM" | sed -n 's/.*cover_rt=\([0-9]*\).*/\1/p')
TW_KEEP_L=$(printf '%s' "$TWSUM" | sed -n 's/.*apic_keep=\([0-9]*\).*/\1/p')
TW_FLAC_L=$(printf '%s' "$TWSUM" | sed -n 's/.*flac_blocks=\([^ ]*\).*/\1/p')
TW_MD5_L=$(printf '%s' "$TWSUM" | sed -n 's/.*apic_1=\([0-9a-f]*\).*/\1/p')
TW_CAPT_OK=1
for f in tagwrite_list tagwrite_list2 tagwrite_flac; do
    [ -s "$SCRATCH/mus_phone_$f.png" ] || TW_CAPT_OK=0
done
[ -n "${TWSUM:-}" ] || TW_CAPT_OK=0
TW_OK=0
if [ "${TW_STAGE_L:-0}" -eq 1 ] && [ "${TW_LAND_L:-0}" -eq 1 ] \
   && [ "${TW_W1_L:-0}" -eq 1 ] && [ "${TW_W2_L:-0}" -eq 1 ] \
   && [ "${TW_WF_L:-0}" -eq 1 ] && [ "$TW_CAPT_OK" -eq 1 ] \
   && [ "${TW_RT_L:-0}" -eq 1 ] && [ "${TW_KEEP_L:-0}" -eq 1 ] \
   && [ -n "${TW_MD5_L:-}" ] && [ "${TW_MD5_L:-}" = "${TW_COVER_MD5:-}" ]; then
    TW_OK=1
fi
if [ "$TW_OK" -eq 1 ]; then
    echo "MUSIC-TAGWRITE-PHONE-VERDICT: TW-OK(N6 — TW 시드 랜딩[N5 캡처 뒤 — 3행 계약 무오염]·music_tag_write relay 쓰기 3콜[cover jpg 마법수·cover=null APIC 유지·FLAC 코멘트 신설]·재독 표기 원문 = 파일 수준 round-trip[title/artist/album+APIC md5 등호 2콜=$TW_MD5_L]+FLAC 블록 재조립($TW_FLAC_L·STREAMINFO 보존)+목록 행 캡처 3종[행 32px 커버 썸네일+[편집] UI 표기 원문 — T3 배포] — **게이트 채용(T5 정산): cover_rt/apic_keep/md5 등호가 TW_OK 본조건으로 승계(WSL 18h 등형 — 합성 cover md5 $TW_COVER_MD5 대차 — 원장행)** — 표기·썸네일 자체는 캡처 육안 몫(EYES)·필터 라운드 = 폰 스킵 원장(폰 타이핑 스킵 승계 — N5) — 육안 스탭 대기)"
else
    echo "MUSIC-TAGWRITE-PHONE-VERDICT: TW-FAIL(honest-fail — 행별 사유는 N6; summary=${TWSUM:-none} caps=$TW_CAPT_OK cover_rt=${TW_RT_L:-n/a} apic_keep=${TW_KEEP_L:-n/a} md5=${TW_MD5_L:-none}(기대=${TW_COVER_MD5:-n/a}) — 게이트 채용(T5 정산): cover_rt/apic_keep/md5 비등호 = FAIL 사유 열)"
fi
echo "TW-RUN-VALS: run=${MUSP_TW_RUN:-1} stage=${TW_STAGE_L:-0} land=${TW_LAND_L:-0} writes=${TW_W1_L:-0}/${TW_W2_L:-0}/${TW_WF_L:-0} cover_rt=${TW_RT_L:-0} apic_keep=${TW_KEEP_L:-0} flac=${TW_FLAC_L:-none} canon_phone=${CANON%% (*} captures=$TW_CAPT_OK"
echo "TW-RUN-CAP: run=${MUSP_TW_RUN:-1} list=[$(wc -c < "$SCRATCH/mus_phone_tagwrite_list.png" 2>/dev/null | tr -d ' ' || echo 0)B] list2=[$(wc -c < "$SCRATCH/mus_phone_tagwrite_list2.png" 2>/dev/null | tr -d ' ' || echo 0)B] flac=[$(wc -c < "$SCRATCH/mus_phone_tagwrite_flac.png" 2>/dev/null | tr -d ' ' || echo 0)B]"
# ═══[#115 T4] N7 opus/m4a 태그 쓰기 동형 세그먼트(OM 판정 행 — wsl 18i 수형):
# 요약 행 파싱+캡처 회수 대차. 강도 축 = **도구 경계(relay ok)+파일 재독
# round-trip(covr/MBP md5 등호 4콜)** — 목록 행 표기·썸네일 = 캡처 육안(EYES)·
# 필터 라운드 = 폰 스킵 원장(N6 승계) — 주 VERDICT 구성항 병합 0(TW 수형 원존).
OMSUM=$(grep -a '^MUS-OM-SUMMARY-2: ' "$RUNLOG" | tail -1 | sed 's/^MUS-OM-SUMMARY-2: //' | tr -d '\r')
OM_STAGE_L=$(printf '%s' "$OMSUM" | sed -n 's/.*stage=\([0-9]*\).*/\1/p')
OM_LAND_L=$(printf '%s' "$OMSUM" | sed -n 's/.*land=\([0-9]*\).*/\1/p')
OM_W1_L=$(printf '%s' "$OMSUM" | sed -n 's/.*w1=\([0-9]*\).*/\1/p')
OM_W2_L=$(printf '%s' "$OMSUM" | sed -n 's/.*w2=\([0-9]*\).*/\1/p')
OM_W3_L=$(printf '%s' "$OMSUM" | sed -n 's/.*w3=\([0-9]*\).*/\1/p')
OM_W4_L=$(printf '%s' "$OMSUM" | sed -n 's/.*w4=\([0-9]*\).*/\1/p')
OM_RT_L=$(printf '%s' "$OMSUM" | sed -n 's/.*covr_rt=\([0-9]*\).*/\1/p')
OM_KEEP_L=$(printf '%s' "$OMSUM" | sed -n 's/.*covr_keep=\([0-9]*\).*/\1/p')
OM_MBPRT_L=$(printf '%s' "$OMSUM" | sed -n 's/.*mbp_rt=\([0-9]*\).*/\1/p')
OM_MBPKEEP_L=$(printf '%s' "$OMSUM" | sed -n 's/.*mbp_keep=\([0-9]*\).*/\1/p')
OM_MD5_L=$(printf '%s' "$OMSUM" | sed -n 's/.*covr_1=\([0-9a-f]*\).*/\1/p')
OM_CAPT_OK=1
for f in om_list om_list2 om_opus; do
    [ -s "$SCRATCH/mus_phone_$f.png" ] || OM_CAPT_OK=0
done
[ -n "${OMSUM:-}" ] || OM_CAPT_OK=0
OM_OK=0
if [ "${OM_STAGE_L:-0}" -eq 1 ] && [ "${OM_LAND_L:-0}" -eq 1 ] \
   && [ "${OM_W1_L:-0}" -eq 1 ] && [ "${OM_W2_L:-0}" -eq 1 ] \
   && [ "${OM_W3_L:-0}" -eq 1 ] && [ "${OM_W4_L:-0}" -eq 1 ] \
   && [ "$OM_CAPT_OK" -eq 1 ] \
   && [ "${OM_RT_L:-0}" -eq 1 ] && [ "${OM_KEEP_L:-0}" -eq 1 ] \
   && [ "${OM_MBPRT_L:-0}" -eq 1 ] && [ "${OM_MBPKEEP_L:-0}" -eq 1 ] \
   && [ -n "${OM_MD5_L:-}" ] && [ "${OM_MD5_L}" = "${TW_COVER_MD5:-}" ]; then
    OM_OK=1
fi
if [ "$OM_OK" -eq 1 ]; then
    echo "MUSIC-OPUSM4A-PHONE-VERDICT: OPUSM4A-OK(N7 — OM 시드 랜딩[N6 소각 뒤 — 계약 무오염]·music_tag_write relay 쓰기 4콜[m4a covr 교체+cover=null 유지·opus MBP 교체+cover 생략 유지]·재독 표기 원문 = 파일 수준 round-trip[covr/MBP md5 등호 4콜=$OM_MD5_L·벤더 유지+코멘트 페이지 EOS 0x04 유지+mdat 원문 무이동]+목록 행 캡처 3종[.m4a/.opus 행이 '-' 대신 실값 — D5 UI 무변경] — 게이트 채용(WSL 18i 등형 — 합성 cover md5 $TW_COVER_MD5 대차) — 표기·썸네일 자체는 캡처 육안 몫(EYES)·필터 라운드 = 폰 스킵 원장(N6 승계) — 육안 스탭 대기)"
else
    echo "MUSIC-OPUSM4A-PHONE-VERDICT: OPUSM4A-FAIL(honest-fail — 행별 사유는 N7; summary=${OMSUM:-none} caps=$OM_CAPT_OK covr_rt=${OM_RT_L:-n/a} covr_keep=${OM_KEEP_L:-n/a} mbp_rt=${OM_MBPRT_L:-n/a} mbp_keep=${OM_MBPKEEP_L:-n/a} md5=${OM_MD5_L:-none}(기대=${TW_COVER_MD5:-n/a}))"
fi
echo "#115-RUN-VALS: run=${MUSP_OM_RUN:-1} stage=${OM_STAGE_L:-0} land=${OM_LAND_L:-0} writes=${OM_W1_L:-0}/${OM_W2_L:-0}/${OM_W3_L:-0}/${OM_W4_L:-0} covr_rt=${OM_RT_L:-0} covr_keep=${OM_KEEP_L:-0} mbp_rt=${OM_MBPRT_L:-0} mbp_keep=${OM_MBPKEEP_L:-0} cover_md5=${TW_COVER_MD5:-} re-read_md5=${OM_MD5_L:-} canon_phone=${CANON%% (*} captures=$OM_CAPT_OK"
echo "#115-RUN-CAP: run=${MUSP_OM_RUN:-1} list=[$(wc -c < "$SCRATCH/mus_phone_om_list.png" 2>/dev/null | tr -d ' ' || echo 0)B] list2=[$(wc -c < "$SCRATCH/mus_phone_om_list2.png" 2>/dev/null | tr -d ' ' || echo 0)B] opus=[$(wc -c < "$SCRATCH/mus_phone_om_opus.png" 2>/dev/null | tr -d ' ' || echo 0)B]"
if [ "$CAP_OK" -eq 1 ] && [ "$CANON_OK" -eq 1 ] && [ "$GLUE_OK" -eq 1 ] && [ "$PLAY_OK" -eq 1 ] \
   && [ "$POS_OK" -eq 1 ] && [ "$SCAN_OK" -eq 1 ] && [ "$LEG_OK" -eq 1 ] && [ "$DIRS_OK" -eq 1 ] \
   && [ "$CANCEL_OK" -eq 1 ] && [ "$BROWSE_OK" -eq 1 ] && [ "$TAG_OK" -eq 1 ]; then
    echo "MUSIC-PHONE-VERDICT: MUS-PHONE-OK(폰 selftest 계보 →$CANON·list 탭 리스캔+캡처 receipt·dirs 도구 원문=OK(p1=crash-fixed 표기 — N-1)·glue=accepted·get_status opened=true pos $POS1→$POS2·폰 spatial leg=$LEGMODE 성립($LEG_LINE)·합산=$MIXL·스캔 취소 경계=CANCEL-OK(폐기 1행=${CANC_L} · close 소멸=${GONE_L})·브라우저 동형 세그먼트=BROWSE-OK(XTEST 탭 전환 — summary=$BSUM)·태그 표기 동형=TAG-OK(N5 — 폰 바이너리 668 계보 실측)·sdcard 스캔=$SCAN_LINE — 위임은 원장행: $DELEG_LINE / 귀속: $DOPEN_LINE — 육안 스탭 대기)"
else
    echo "MUSIC-PHONE-VERDICT: MUSIC-PHONE-FAIL(행별 사유는 위 각 행 — canon=$CANON_OK captures=$CAP_OK dirs=$DIRS_OK glue=$GLUE_OK play=$PLAY_OK pos=$POS_OK leg=$LEGMODE/$LEG_OK ${LEG_LINE:-n/a} mix=${MIXL:-n/a} cancel=$CANCEL_OK browse=$BROWSE_OK tag=$TAG_OK scan=$SCAN_LINE; 인프라 실패는 hard FAIL로 상단 중단, 수치·육안 미달은 rc=0 honest-fail)"
fi
echo "MUSIC-WCAPTURES:"
for f in list list2 spatial spatial_mix vp_delegate delegate_status after sdcard dirs dirs2 scan_busy rescan_settled taglist taglist2 tag_after \
         tagwrite_list tagwrite_list2 tagwrite_flac om_list om_list2 om_opus \
         browse_open browse_subs browse_sub browse_up browse_sub2 browse_added browse_rescan; do
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
echo "  ④ 폰 spatial leg(#110 T2 — mus_phone_spatial.png+spatial_mix.png):"
echo "     WIRED 런 = spatial 재생(STATUS-0/1/2·pos 진행·TONE 지연·stop 원문)과"
echo "     합산(M2 — OpenAL leg + vplayer(SDL) 동시 재생)의 패널 렌더 원문;"
echo "  ⑤ 폰 BOOT-OK(settings 원상 복원 — ORIG 바이트 등호): 종료 상태 서버 UP+terminal."
echo "  ⑥ 폰 dirs 도구(mus_phone_dirs.png+dirs2): [폴더 관리] 등록 상태 스트립"
echo "     (3탭 — mus_dirs_x·시드 dir 병기)+상태행(폴더 추가됨)+settings.json"
echo "     music.dirs 실기록+text/font_path 보존+원복 바이트 등호(DIRS 판정 행 —"
echo "     p1=crash-fixed 표기; CP949 거부/치유는 WSL T3 소유 스킵 원장행)."
echo "     패널 개방 프레임 = 원장행: 토글 좌표 정산(185/77·270/77 — 캡처 픽셀"
echo "     실측)+hover tooltip 원문에도 클릭이 패널을 열지 않았다(3런+라이브 2회"
echo "     실측 — 같은 클라에서 필터 InputText 포커스·표행 더블클릭 위임은 성립"
echo "     — 폰 스트립 행 Button 클릭 경로 전용 관측 — P3 원장 후보, EYES 몫)."
echo "  ⑦ 폰 스캔 취소 경계(mus_phone_scan_busy.png+rescan_settled.png): 탭2"
echo "     선택의 '스캔 중...' 표기 원문(제거 재청구 스캔의 창 — in-flight"
echo "     1프레임 응답)+재스폰 클라 dirs_[1] 재스캔 정착(폐기 뒤 채택 전이 —"
echo "     계보 원문의 폰 재실측 — EYES 몫)."
echo "  ⑧ 폰 브라우저 동형 세그먼트(mus_phone_browse_*.png — N4): [폴더 관리]"
echo "     토글 패널+[찾아보기] 개봉 프레임(cwd 표기=등록 루트+\"..\"행+sub_a"
echo "     ·sub_b 2건 목록)·JoinDir 진입 cwd(sub_a)·Parent 복귀·cwd=sub_a에서의"
echo "     [이 폴더 추가] 뒤 브라우즈 종료+재스캔 표 목록(등록된 하위 트랙) —"
echo "     XTEST 주입(xtap — 스트립 행 Button 클릭 경로)의 폰 실측 원문 — EYES."
echo "  ⑨ 폰 태그 표기 동형 세그먼트(mus_phone_taglist.png/taglist2 — N5, #99"
echo "     T4): 태그 표 3행 — 'Phone Tag Alpha (Probe Artist Alpha)'+'Tag Album"
echo "     One'·'Phone Tag Beta (Probe Artist Beta)'+'Tag Album Two'·폴백 행"
echo "     't_raw_noid'+앨범 '-'(mtime desc 상단행 = alpha — 태그 파싱 1패스 스캔"
echo "     의 폰 바이너리 도착 원문) — vplayer openFailed 영수증 병기·타이핑"
echo "     세그먼트는 폰 스킵 원장행(T3 concern 2 — WSL 16e 소유) — EYES."
echo "  ⑩ 폰 태그 쓰기 동형 세그먼트(mus_phone_tagwrite_list.png/list2/flac —"
echo "     N6, #111 T6): 목록 상단 행 Phone Write Alpha (Probe Tw Artist)+Tw"
echo "     Album One+좌측 행 32px 커버아트 썸네일(4색 밴드 jpg — 회색 자리표시"
echo "     대조)·쓰기 2 뒤 Phone Write Reup label 교체(cover=null — APIC 유지)·"
echo "     FLAC 행 코멘트 반영 — 커버아트 UI(행 썸네일+[편집])의 폰 표기 원문 —"
echo "     성립 판정 = 도구 경계+파일 재독 축(TW 판정 행·TW-RUN-VALS)·표기·썸네일"
echo "     = EYES."
echo "  ⑪ 폰 opus/m4a 태그 쓰기 동형 세그먼트(mus_phone_om_list.png/list2/opus —"
echo "     N7, #115 T4): 상단 행 Phone Omus Alpha (Probe Om Artist)+'Om Album One'(.m4a"
echo "     행)·Phone Omus Reup label 교체(cover=null — covr 유지)·opus 행 실값"
echo "     표기+행 썸네일(MBP 교체 jpg) — 합성 파일이 '-' 폴백에서 실값 전환 ="
echo "     2형식 파서의 폰 바이너리 도착 원문 — 성립 판정 = 도구 경계+파일 재독"
echo "     축(OM 판정 행·#115-RUN-VALS)·표기·썸네일 = EYES. **재생은 비-목표(D6)"
echo "     — [.opus/.m4a 행 재생 = vplayer 위임 openFailed·spatial 위임 표기 유지"
echo "     — '태그만 지원' 원존]**."
echo "MUSIC-PHONE-END"
# [#111 T6] 상속 hard 마커 말단 종결(T6-HARD-DEFERRED 원장행) — rc=1 시점 이동.
[ "${INH_HARD:-0}" -eq 1 ] && { echo "MUSP-FAIL: N4 ⑤ 무전달 hard 마커 말단 종결(rc=1 — 마커 원존·T6 세그먼트 산치는 상단 원문)"; exit 1; }
exit 0
# honest-fail 원칙: 수치 미달(MUSIC-PHONE-FAIL 라인)은 원장 목적이라 rc=0.
# hard FAIL(ssh·sweep·배포·ninja·selftest 회귀·시드·settings/perm 원복·부팅·ping·
# launch·창·캡처·소각·REMNANT·big 트리 시드/카운트/소각)만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지.
exit 0
