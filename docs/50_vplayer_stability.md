# 50. vplayer 재생 안정성 + 조그 휠 마우스 휠 스크럽 as-built

- 날짜: 2026-09-14 (SDD 6태스크 — T1 비동기 오픈 / T2 시크 / T3 오디오 / T4 휠
  스크럽 / T5 최종 게이트 / T6 본 문서, 최종 리뷰 clean + 픽스 웨이브 완결)
- 상태: 구현 완료. 코드 커밋 5fdfc4e → 6ef077d (11커밋, 88.8KB), 빌드 exit 0,
  `jkdesktop test` 0 failures, 프로브 16/18(실패 2건은 사전 귀속 타 플랜 — §4),
  e2e 통합 ALL PASS(게이트 23/23 체크, 최대 ping 53ms), 최종 리뷰 With fixes의
  봉쇄 1건(RingPush wantSeek)까지 해제.
- 선행: docs/superpowers/specs/2026-09-14-vplayer-stability-design.md (스펙,
  143f72d — §4 장부에 실행 교정 D1-cor·D2-cor 추가), docs/superpowers/plans/
  2026-09-14-vplayer-stability.md (플랜)
- 비고: 진행/룰링/리뷰 레저는 `.superpowers/sdd/2026-09-14-vplayer-stability/
  progress.md`, 태스크별 as-built 상세(측정 하네스·스크린샷·한계)는 같은 디렉터리
  task-1~5-report.md. 스크린샷 실물(vpt1~vpt5-*.png)도 동일 디렉터리.

## 1. 개요

사용자 요청 "비디오 플레이어도 재생 안정성 올리는 거랑, 조그 휠에 마우스 휠
붙이는 거도"에서 출발했다. 확정된 증상은 3종 — ① 이상한 파일에서 크래시/멈춤,
② 오디오 끊김/유실, ③ 시크/스크럽 중 멈춤·틀어짐 — 에 휠 스크럽 기능이 얹혔다.
인벤토리 스캔(스펙 §0)이 밝힌 공통 뿌리는 **스레드 모델이 아니라 배치**였다:
FFmpeg 디먹스/디코드 + sws→RGBA + SDL 오디오 링 버퍼 전부를 UI 스레드와 단일
워커 스레드가 나눠 갖는데, (a) UI 스레드가 `avformat_open_input`을 동기 실행하고,
(b) 워커가 상태 뮤텍스 `m`을 잡은 채 `avformat_seek_file`(디스크 I/O)을 돌리고,
(c) 비디오 경로의 sws/할당/백프레셔 대기가 같은 루프의 오디오 디코드를 굶긴다.

그래서 이번 패스의 본체는 4가지다. (1) **오픈을 워커 1단계로** — interrupt
deadline + 차원 상한 + 예외 방어막 + 읽기 오류 분류 (T1). (2) **시크 I/O를 락
밖으로** — 3단 분해 + 실패 시크 클록 복원 (T2). (3) **오디오 우선 리필** — 단일
워커 유지하되 200/600ms 히스테리시스 윈도우로 링을 먹이고 언더런을 카운터로
가시화 (T3). (4) **휠 스크럽** — 드래그 경로와 공유 jogTarget_/finishScrub,
릴리스는 400ms 아이들 타임아웃으로 정의 (T4). 설계 원칙은 **단일 PlayerCore/
단일 워커/락 2개(m→ringM) 불변 유지** — 새 스레드·새 락 0개.

가장 중요한 측정 교정은 **D1-cor**다: FFmpeg 7.1.101은 file 프로토콜의 블로킹
read/probe 루프에서 interrupt_callback을 전혀 consult하지 않는다(계측 cb 호출
0회 — T1 보고서 §4c). 즉 10s 데드라인·취소 플래그는 "FFmpeg가 블로킹 호출에서
반환할 때"만 유효하고, 영원히 안 돌아오는 I/O(죽은 파이프/네트워크 마운트)는
스펙 기계로 못 끊는다. **진짜 방어선은 TryClose(2s) 실패 시 Abandon** — 워커
detach + PlayerCore 의도적 수명 누수(폐기 오픈 1건당 1개 bounded)로 UI join
프리즈를 피한다. "여는 중" 상태가 취소 후에도 남는 것은 이 한계의 표시다.

## 2. 커밋 (실측 `git log --oneline acb45ea..HEAD` — 11커밋)

| 커밋 | 분류 | 내용 |
|---|---|---|
| 5fdfc4e | feat (T1) | 비동기 오픈(BeginOpen+워커 OpenStage) + interrupt 10s deadline + 차원 상한(8192/256MiB) + 워커 예외 방어막 + 읽기 오류 분류 |
| 21eae90 | docs (T1) | interrupt_callback 무효 실측 기록 — file 프로토콜 read는 unbreakable, Abandon이 방어선 (D1-cor) |
| 7f39e33 | fix (T1 후속) | duration 게시 opened 게이트(데이터 경쟁 봉합) + semantic-red kErrorRed dedup |
| da2b141 | feat (T2) | 시크 3단 분해(DoSeekStages — m 해제 후 avformat_seek_file) + 실패 시크 클록/링 복원 + seekError 일회 공지 + audio-only 시크 타임베이스 |
| 5dfaf13 | fix (T2 후속) | undo 베이스라인 seekInFlight 게이트(Ok/Failed 종결에서만 클리어) + 일시정지 중 stage (b) wall 클록 재고정 + lastError 디커플 |
| faf309b | feat (T3) | 오디오 우선 리필(200/600ms 히스테리시스, 키프레임 앵커 유지) + 언더런 카운터/audioEof 게이트 + 디바이스 실패 상태 노출 + cvRing lost-wakeup 픽스 |
| 9b6754b | fix (T3 후속) | 리필 드라이 캡→audioEof + videoQ 백프레셔를 50ms 타임드 wait로 전환(술어 자재평가) + kRefillDryCap 상수화 |
| 2bc6c7d | fix (T3 후속 2) | 타임드 wait 타임아웃 시 술어 존중 — 프레임 드롭(videoQ 유계) — 오디오 조기종료 테일의 무한 큐 성장(OOM) 봉쇄 |
| 21e23d9 | feat (T4) | 마우스 휠 스크럽(공유 jogTarget_ 40ms 디바운스, 드래그 진입/릴리스 패리티, 400ms 아이들 릴리스, OpenPath 스테일 세션 컷) |
| 9592ea2 | fix (T4 부가) | RingPush 주차 워커를 wantSeek으로 기상(술어 추가 + SeekCommon의 ringM 하 cvRing 통지) — 일시정지+풀링 시크 동결(증상③ 뿌리) 픽스 |
| 6ef077d | fix (최종 리뷰 픽스 웨이브) | RingPush 후위 검색에 wantSeek 포함 + seek 갭 무음을 audioEof 게이트로 언더런 제외 + 예외 배리어의 seekInFlight 해제 |

T5(게이트)·T6(본 문서)는 검증/문서 전용이라 제품 커밋 없음. 실측 범위 diff는
`engine/src/apps/ClientVPlayerApp.cpp` + `engine/include/apps/ClientVPlayerApp.h`
2파일(후자는 T4의 멤버 2개뿐).

## 3. 대응표 (증상 → 근원 → 픽스)

### ① 이상한 파일 크래시/멈춤 (T1)

| 항목 | 실측 |
|---|---|
| 근원 | (a) `OpenPath`가 UI 스레드에서 `avformat_open_input`/`find_stream_info` 동기 실행 — interrupt_callback 부재, 이상한 파일에서 무한 대기. (b) 프레임당 `vf.rgba.resize(videoW*videoH*4)`에 해상도 상한 검증 부재 → 워커 `bad_alloc` 미포착 = `std::terminate`. (c) vplayer 경로 try/catch 전무 |
| 픽스 | 오픈 전체를 **기존 워커의 첫 단계(OpenStage)**로 이동 — `BeginOpen`은 즉시 복귀, UI는 "여는 중..."+취소 상태. interrupt_callback+10s deadline(find_stream_info 성공 직후 같은 스레드에서 해제 — 재생 I/O에 간섭 없음). 차원 상한은 디코더 오픈 **전**(w/h 8192, W*H*4 256MiB — 경계 정확: 초과분만 거부). 워커 루프 전역 try/catch(catch(...), 오디오 콜백은 룰링 2로 제외). 읽기 오류 분류 — EOF만 조용한 ended, EAGAIN 50×10ms 유계, 기타 lastError+사유 표시, 디코드 연속 30 스트릭 |
| 수명 방어선 | TryClose(2s bounded join) 실패 → Abandon(워커 detach + core 의도적 누수) — D1-cor 실측이 만든 뒷받침. stuck open 직후 정상 파일 재생(S7)으로 end-to-end 증명 |
| 커밋 | 5fdfc4e, 21eae90, 7f39e33 |

### ② 오디오 끊김 (T3)

| 항목 | 실측 |
|---|---|
| 근원 | 단일 워커가 디먹스+비디오+오디오 전부 — 비디오 sws/할당/videoQ 백프레셔 대기가 오디오 패킷 디코드를 굶김 → 링 언더런 → 콜백 무음. 디바이스 실패는 stderr만 |
| 픽스 | **오디오 우선 리필 윈도우**: 저수위(200ms)에서 열리고 고수위(600ms)에서 닫히는 워커 전용 히스테리시스(스트림에서 유도, 매직넘버 아님). 윈도우 중 비디오 패킷은 스킵하되 **키프레임은 디코드** — 디코더 참조 앵커 유지(첫 구현의 "co located POCs unavailable" ×96 회귀의 픽스). videoQ 백프레셔 대기에 `RefillDue()` 탈출 추가. **언더런 카운터**는 콜백 무음 분기에서 relaxed atomic 1줄 — audioEof 게이트로 EOF/실패 후 무음을 굶김에서 제외. 디바이스 실패는 `오디오 장치를 열 수 없음(무음 재생)` 정보성 상태행(lastError 채널과 분리) |
| 부수 픽스 | (A) 콜백 pop 경로의 cvRing notify_all — 기존 잠재 lost-wakeup(RingPush 주차를 아무도 깨지 못함) 봉쇄. (B) 백프레셔 대기를 50ms `cv.wait_for` 타임드로 — 오디오 클록 동결 시 유일한 탈출 통로. (C) 타임아웃 반환값 존중 — 술어 거짓이면 push 대신 drop(videoQ 유계) |
| 커밋 | faf309b, 9b6754b, 2bc6c7d (+6ef077d의 audioEof 게이트) |

### ③ 시크 멈춤/틀어짐 (T2 + T4 부가 + 픽스 웨이브)

| 항목 | 실측 |
|---|---|
| 근원 | (a) 워커가 상태 뮤텍스 `m`을 잡은 채 `avformat_seek_file`(디스크 I/O) — UI `SnapNow`가 시크 시간만큼 정지. (b) `avformat_seek_file` 반환 무시 — 시크 불가 파일에서 클록만 재기준되고 화면 정지 = A/V 틀어짐. (c) **일시정지+풀 링 시크가 영구 미처리** — RingPush의 `cvRing.wait` 술어에 wantSeek 부재 + SeekCommon이 cv만 통지(T4 실측으로 발견된 기존 결함, 플랜 증상③ 뿌리) |
| 픽스 | **3단 시크(DoSeekStages)**: stage (a) m 하 큐/클록 재기준+스냅샷+seekGen bump → (b) unlock 후 avformat_seek_file — SnapNow/SetPaused/신규 시크 진행 → (c) 재잠금 후 분류(Ok/Failed/Superseded). 실패 시크는 ring/클록 쿼드러플 사전 캡처→비트정확 복원 + `시크 불가 위치` 3s 일회 적색 공지(kErrorRed 재사용). (c) 근원은 RingPush 술어에 wantSeek 추가 + SeekCommon이 ringM 잠금하 cvRing 통지(Close/TryClose 기존 형태) — 9592ea2. 최종 리뷰 픽스 웨이브(6ef077d)가 대칭 완결: RingPush **후위 검색**에도 wantSeek(시크실패+일시정지+풀링 코너에서 링 잔류 덮어씀 봉쇄) + seek 갭 무음을 audioEof로 게이트 + 예외 배리어 seekInFlight 해제 |
| 커밋 | da2b141, 5dfaf13, 9592ea2, 6ef077d |

### 기능: 조그 휠 마우스 휠 스크럽 (T4)

| 항목 | 실측 |
|---|---|
| 설계 | 노브 hover에서 휠 틱 → `jogTarget_ += wheel·sPerRev/16` (틱당 1/16회전 — 드래그 감도와 동계열), **드래그와 공유** jogTarget_/40ms 디바운스 펌프/finishScrub(프레임 스냅 precision Seek + SetJog(false) + 재생 복원) — 최신 승리, 이중 시크 없음(드래그 진입이 휠 세션 인계) |
| 릴리스 정의 | 휠엔 릴리스 이벤트 부재 — **마지막 틱 기준 400ms 아이들**에서 드래그 릴리스와 동일 마무리. `OpenPath`에서 스테일 세션 컷(휠 세션은 다음 파일에 precision Seek를 쏠 수 없다) |
| 부호/정밀도 실측 | 휠↑=전진(드래그 cw와 일치). 40틱 = +9.375s 정확(40 × sPerRev 3.75 / 16). 일시정지 시맨틱은 드래그 완전 패리티(재생 중→자동일시정지+복원, 일시정지→스크럽만) |
| 커밋 | 21e23d9 |

## 4. 검증 실측

- **빌드/자가테스트**: 전 태스크 빌드 exit 0 + `./jkdesktop test` 0 failures.
  T5 게이트에서 **mtime 게이트가 스테일 바이너리를 잡음** — 첫 증분 빌드가
  no-op인 채 `jkapp_vplayer.dll`이 최종 커밋보다 2분34초 늙음(내용 동일하나
  게이트 조항 위반) → 강제 재빌드 후 통과(artifact 11:20:34 > 커밋 11:15:44).
  교훈: 게이트는 테스트 전 재빌드 필수.
- **프로브**: 18종 중 16 rc=0. 실패 2건(probe_filedlg_fix, probe_terminal_reflow)
  은 사전 귀속된 타 플랜 기존 결함 — 동일 서명 재현으로 귀속 확정, 게이트 불가산.
- **① 파손/여는 중 (vpt1 + T5)**: 잘린 mp4(moov 없음)/0바이트/헤더 변조 3종 →
  분류된 적색 openError, 크래시 0 (`vpt5-s1a-openerror-trunc.png` 등). 무응답
  파이프 → "여는 중..."+취소 렌더링(`vpt5-s1d-opening.png`), 취소 클릭 무사,
  stuck open 직후 정상 파일 재생(TryClose/Abandon 실증). 13s 행 오픈 무크래시 +
  90s 정상 재생 회귀 STABLE(첫 구현의 swresample 누락 회귀를 잡아낸 게이트).
- **② 오디오 (vpt3/vpt5)**: hi-res 1080p 45s 연속 재생 `underrun: 1` 고정(오픈
  프라이밍 트랜지언트 — 양 시대 공통). 오디오 조기종료 파일: 픽스 전 **129→303**
  (8s 펌프) → 픽스 후 **1 고정**(t26/t34), 테일 8s 메모리 92→94MB 플랫(픽스 전
  ~+160MB@480x270 — 1080p에선 OOM 규모; 최종 픽스웨이브 재검증 33→33MB 플랫).
  hi-res 시크 후 카운터 **1→5** → seek 갭 audioEof 게이트로 **1→2**(잔여 +1은
  Ok 후 프라이밍 실출력 무음 — 설계상 카운트 유지). 시크 후 오디오 회복 38:32→
  38:37 (클록 전진 = 회복).
- **③ 시크 (vpt2/vpt5)**: 정상 mp4 슬라이더 시크 12회 + 스크럽 버스트 e2e
  12/12 ALL PASS — 게이트 실측 최대 ping **53ms**(T2 리포트 표기는 S1 49/S2 45/
  S3 34ms, 진행 레저 표기는 69ms — 표기 불일치는 리포트 표가 실측 원본). 실패
  시크는 라이브 트리거 2종 실측 발견으로 검증(Ruling 1의 "코드 검증으로 대체"
  상회): mpeg-ts 네임드파이프 + raw h264 ES — 적색 일회 공지 렌더 + 클록 수치
  복원(`clock restored to 5.514s` stderr) + UI 응답 유지.
- **휠 (vpt4/vpt5)**: 재생 중 40틱 5.667→15.0(+9.375s 정확) + 400ms 창 내
  자동일시정지 + 릴리스 후 복원; 일시정지 28.033 → 40틱 후 18.658→프레임 스냅
  18.633. RingPush 시크-동결 픽스 전/후 픽셀 실측: 전 4시나리오 diff 전부 **0**
  +시계 동결 → 후 **9837~10741** 샘플 변화, s3b 시계 00:21 = 비디오 tcode
  21.067/frame 632 (632/30) 정합.
- **회귀 (T5)**: 재생/일시정지 토글/볼륨 0.80→0.40/jog cw-ccw ALL PASS.
  avDelay는 하네스 합성 입력 4회 시도 실패(미작동) — T1~T4 diff가 avDelay에
  무관(주석만)으로 보완. 스크린샷 29장(vpt5-*) + REQUIRED 2장(여는 중 s1d,
  오류 s1a) 캡처.

## 5. 실행 직감

1. **interrupt_callback은 블로킹 파일 I/O를 깨지 못한다 (D1-cor)** — 최소 재현
   프로그램으로 계측: silent pipe 25s, 8KB/s dribble 40s, cb가 15s에 abort하도록
   설치된 상태에서도 **cb 호출 0회**. "취소 가능" 문언은 프로토콜별로 측정해야
   한다 — 콜백은 네트워크 프로토콜용이고 file 프로토콜엔 폴링 지점이 없다.
   취소 설계는 콜백 등록이 아니라 **끊지 못할 I/O에 대한 수명 포기(Abandon)**
   설계까지 포함한다. 코드는 남긴다 — I/O가 반환하는 소스에선 유효하니까.
2. **`cv.wait_for`는 타임아웃에 술어 거짓으로 돌아온다** — 기존 `cv.wait`는
   술어 참 보장, `wait_for(pred)`는 반환값이 술어의 값. 반환값을 무시하면
   타임아웃 경로가 조건 미충족 상태로 fall-through한다 — T3가 두 픽스 라운드에
   걸친 실측 라운드트립(라운드 1: 타임드 wait로 전환, 라운드 2: 반환값 존중→
   드롭). 타임드 wait는 "탈출 통로"이지 "푸시 허가"가 아니다.
3. **"done" 술어 항(wantSeek)은 주차 지점마다, 그리고 대기 후 검사마다** —
   RingPush 대기 술어에 wantSeek을 넣어도 후위 검사가 `stop || seekGen != gen`
   이면 시크 실패+일시정지+풀링 코너에서 잔류 샘플을 덮어쓴다(최종 리뷰 봉쇄
   1건, 6ef077d). 술어 항 추가는 **그 항을 소비하는 모든 검사의 대칭 완결**이
   전제다. 같은 맥락: 대기를 깨우는 통지는 대기가 실제 기다리는 cv로, 같은
   락 홀드에서 해야 check-vs-park 창이 닫힌다(SeekCommon의 ringM 하 cvRing
   notify).
4. **진단 지표의 정직성** — seek 갭 동안 링이 비어 콜백이 무음을 세는 것은
   굶김이 아니라 시크의 부산물이다. audioEof 게이트 없이는 언더런 지표가
   1→5로 펌프돼 "고장"처럼 보였고, 게이트 후 1→2에서 잔여 +1은 진짜 프라이밍
   무음으로 남는다. 지표가 정직해야 튜닝도 정직해진다.
5. (보태기) **"SDL_InitSubSystem이 워커에서 호출돼서"라는 첫 가설은 틀렸다** —
   T1의 swresample 크래시 회귀는 OpenStage 재작성에서 `swr_alloc_set_opts2`/
   audioRate 블록이 통째로 **탈락**한 누락이었다. 스레드 이동 회귀를 만나면
   이동 전후 init 델타 전무를 먼저 비교한다. 그리고 파이프 홀더는 리스너를
   선(先)생성해야 한다 — 연결 후 생성은 gate fopen과 open 사이 빈 창에서
   ERROR_PIPE_BUSY → 블로킹 대신 즉시 EINVAL.

## 6. 후속 (레저 — progress.md 최종 리뷰 트리아지)

- **듀얼 디코드 스레드 승격** (스펙 §5, D2의 실측 후 승격 판단 항) — 오디오
  우선 정책이 3단 실측으로 완성됐으므로, 승격 시 T3 이연인 워커 디코드-스핀
  (오디오 조기종료 테일에서 ~20회/초 풀 해상도 디코드+드롭 — 메모리는 유계,
  CPU 미유계)과 리필 폐기 프레임 sws_scale 낭비가 함께 소멸한다.
- **jogUi 120px 게이트 이하 축소 시 휠 세션 자가해제 불가** (T4 Minor-2, 드래그
  동일 기존 형태) + **OpenPath의 jogActive_ 미컷** (T4 Minor-3, 드래그 기존) —
  공유 finishScrub 정리와 함께 묶어 후속.
- **Failed 시크의 audioEof 복원 = undoEnded 프록시** (픽스웨이브 이월 Minor) —
  드라이캡+시크실패 코너에서 단기 미카운트→재상승 자가교정, 비봉쇄.
- **seekInFlight 캐스케이드 불변식 디버그 단정** — Superseded 경로에서
  클리어하면 승계 캐스케이드가 팬텀 클록을 캡처한다는 T2 발견(코드 주석 기록)을
  assertion으로 기계화.
- **T5 관찰: 일시정지 휠/드래그 해제 후 클록/장면 ~2s 지연 간헐** — 최종 리뷰
  가설 교정: 정밀 시크 키프레임 착지 → GOP decode-forward 크립 = 기존 드래그와
  동일 형태(회귀 아님, ACCEPT-AS-IS). 계측 런으로 videoQ 정체 검증 후 종료.
- (레저 잔여) stage-(a) 코덱 플러시의 Failed 복원(Ok 경로 이연이 근본 해법),
  UI 스레드 fopen 존재 검사의 데드 UNC 블록(T2에서 OpenStage 이동 완료 —
  잔여 경로 점검), SDL 장치 제거/재열결 감지, 오류 히스토리(현재 최근 1건).
