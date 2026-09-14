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

## 7. 사후 픽스 — 4K50 AV1 프리즈-점프/깨짐 근본 해결 (demux/decode 분리, f41dd28, 2026-09-15)

사용자 파일(`Addicted to the pain…mp4`, 3840×2160@50 AV1+opus) 기본 재생 보고
("깨지고 난리, 오디오만 멀쩔, 찔끔 찔끔 움직이다 점프")의 추적 결과. 사용자
진단("오디오 우선 적용 후 발생")이 정확했다 — 범인은 §1의 T3 구조 자체였다.

### 7.1 근원 사슬 (vpt9 진단계측 실측)

1. 오디오 링(T3 리필)이 demux 위치를 시계보다 최대 600ms 앞까지 몰아간다.
   반면 videoQ는 3프레임(50fps에서 60ms) — **demux 선행 리드를 videoQ가 절대
   못 덮는다**. 디코드된 4K RGBA 프레임(33MB)을 미래분만큼 들고 있을 수도
   없다(30프레임=1GB).
2. 결과: 디코드 위치가 항상 시계+0.2~0.34s 앞 → videoQ가 항상 "미래 프레임"으로
   만석 → 50ms 역압 대기 타임아웃 드랍(~55/5s) + 표시 게이트는 92% 굶고 시계가
   프론트 pts를 넘길 때만 3프레임 붕괴 팝 → **찔끔 움직임+점프** (popOk 12-24/5s,
   aheadAvg 0.16-0.34s 실측).
3. 리필 창의 비-KF 비디오 패킷 스킵이 mid-GOP 복귀를 만들고 dav1d는 조용히
   오염된 프레임을 출력 ("Error parsing" 스팸) → **가끔 깨짐**.
4. ffprobe로 format/video/audio start_time 모두 0 확인 — 스트림 원점 아닌
   구조적 선행임을 배제 완료. 단독 디코드 실측(84.5~98fps)으로 파일·디코더
   무죄 확인.

### 7.2 구조 픽스: demux/비디오디코드 스레드 분리

- 워커(=demux+오디오)는 비디오 패킷을 **바이트 유계 패킷 큐**(16MB≈4K AV1
  5초분, 패킷은 ~100KB)에 넣기만 한다. A/V 파일은 링 역압(5.8s)이 먼저
  demux를 세우고, 경계는 비디오 전용 파일용(파일 전부 선행읽기 방지).
- 신설 **비디오 디코드 스레드**가 큐를 순서대로 비우며 각 패킷을 **프레젠테이션
  시계 직전까지 보류**(게이트: `pts <= ClockNow() + 0.05 + 파이프라인지연`).
  패킷 순서 불변 → 레퍼런스 체인 유지, mid-GOP 스킵 원천 소멸(=깨짐 0).
- **파이프라인 지연 온라인 측정**: 스레드 디코더(dav1d)는 send 후 D프레임
  뒤에 출력된다. 고정 소폭 리드만 주면 파이프라인이 굶는다(실측 17fps).
  send-pts−received-pts의 EMA로 D·Δ를 온라인 측정해 게이트에 가산 → 파이프라인
  포화 + 출력 프레임 근시( near-due) 동시 달성. 시드는 has_b_frames/fps.
- **thread_count=1 기본값 함정**: AVCodecContext 기본이 1 — 실측 43ms/프레임
  (23fps)이 단독 프로브 threads=1=23.1fps와 정확히 일치. `hardware/2 상한 12`
  로 명시(12 threads 실측 98fps; 무한 auto는 24코어에서 4K면 수백 MB).
- **T3 리필 스킵 분기 삭제**: 인큐가 사실상 공짜라 워커가 오디오 패킷에 즉시
  도달 — audio-first가 구조적이 됨. 워터마크 멤버는 링 깊이 문서로 보존.
- **RGBA 풀**(shared refcount): 프레임당 33MB 신규 vector가 ~20ms — 풀 재활용
  후 프레임 예산 내 진입(allocSws 20→9.5ms).
- **오디오 시계 = 소비 시간**: 언더런 무음도 프레젠테이션 위치(기존은 복사
  바이트만 가산 → 무음 갭만큼 클록 영구 후퇴 누적 = 싱크 누적 이탈). 시크
  갭/EOF 게이트(audioEof) 동안은 기존대로 고정.
- **vdecM**: vctx send/receive(비디오 스레드)와 stage-(a) 플러시(워커) 직렬화.
  순서 m→vdecM, 디코드 스레드는 vdecM을 푸시 전에 반드시 해제(역전 없음).
- 시크: stage (a)가 vdecM 아래 코덱 플러시 + vPktQ 완비(시크=워커가 실행하므로
  큐 잔여는 전부 프리시크). 게이트 홀드 중이던 패킷은 wantSeek 재검사로 폐기,
  푸시측 dropBeforePts가 이중 방어.

### 7.3 검증 (임시 계측, 제거 후 커밋)

dec 50.0fps(콘텐츠 레이트 일치), decMs/f 3.0ms, drop=0, **underruns=0**,
starve 2-12/5s(기존 280), aheadAvg 0.023-0.032s(1프레임, 기존 0.16-0.34),
dav1d "Error parsing" **0건**. 렌더 루프는 ~27Hz — 4K 33MB SDL_UpdateTexture
업로드 비용이 다음 병목(HW/NV12 경로, 아래 레저).

### 7.4 레저 갱신

- **소멸**: 듀얼 디코드 스레드 승격(§6 첫 항 — 이번이 그것), 워커 디코드-스핀,
  리필 폐기 sws 낭비, 리필 창 자체.
- **신규**: ① 텍스처 업로드 경로(d3d11va/NV12 + GPU 변환) — 4K 렌더 27Hz
  근본 해법이자 vPipeDelay/hw 스레딩의 메모리 부담도 함께 줄임. → **§9
  as-built: NV12 착수(f2350d0), 렌더율 프리미스는 측정으로 반증** —
  "4K 33MB 업로드 비용이 병목"은 이 절의 사전 가설이었다(§7.3 귀속
  정정). ② vPipeDelay
  EMA가 일시정지 중 수축하는 성질(재개 후 ~1s 재수렴) 확인용 관찰. ③ 언더런
  클록 진행 픽스와 audioEof 프록시 상호작용 재확인(audioEof=true 구간은
  여전히 고정 — 여전히 의도).

## 8. 조그 프레임 스크럽 (2026-09-15)

조그 다이얼(노브 드래그+휠)을 40ms 디바운스 키프레임 시크에서 **무음 프레임
단위 스크럽**으로 교체(스펙/플랜 2026-09-15, 커밋 7b4b292..eaf6e1f, 결함 픽스
9c7979e). 세션 시작 자동 일시정지 + `SetJog(true)`(오디오 디코드 스킵 = 완전
무음), 릴리스는 기존 `finishScrub` 정밀 시크로 재생 상태 복원.

- **jogRing** — 디코드 프레임 접미사 링(`deque<VideoFrame>`, m 보호, videoQ와
  같은 락 도메인). 상수 `kJogRingMaxSecs = 10.0` / `kJogRingMaxBytes = 1.5GB`,
  push 시 trim(backpressure 없음 — 캡 자체가 바운드). 키프레임 경계는 무관하고
  시크 stage (a)에서만 클리어된다(랜딩 후 재구축).
- **링 히트 = 즉시 표시**: UI 펌프가 다이얼 target을 `JogTo`(`jogTargetPts`,
  블로킹 0)로 전송하고 `JogFrame`이 링에서 target 이하 최신 프레임을 골라
  표시 — 역방향도 디코드 없이 재생된다(반프레임 eps 스냅).
- **다이얼 타깃 디코드 게이트**: VideoLoop 게이트가 조그 중엔
  `pts <= max(ClockNow(), jogTargetPts) + kVideoLead + vPipeDelay`로 재편 —
  다이얼이 가리키는 곳까지 디코드 전진, 이상은 hold. 조그 중 videoQ push는
  생략(일시정지라 3슬롯 캡이 전진을 막음 — 링이 조그 경로의 sink).
- **링 밖 = 키프레임 폴백**: `target < ring.front − 반프레임`이면 기존 40ms
  latest-wins `SeekScrub`. SeekCommon의 scrub 분기가 `jogTargetPts = t`를
  먹여 폴백 표시도 프레임 연속(랜딩 키프레임 → 디코드 크리프).
- **원자적 활성화**: 조그 분기 videoQ 스킵을 `jogTargetPts >= 0` 게이트로
  봉쇄(T1 스테이징, 기본 -1) → T3의 표시 전환과 동시 스위칭. 중간 커밋에
  회귀 창이 없다(리뷰 룰링: 조그 중 UI SetJog(true) 경로가 이미 활성이라
  무게이트 시 조그 화면 정지).
- **검증(vpt9 프로브, vpt2_test.mp4 480x270@30 번인 카운터 직독)**: S1 전진
  스크럽 틱당 +7.03프레임 정확(295→302→351→406→462, 키프레임 점프 0),
  S2 링 내 역방향 **즉시**(무셋틀 샷에서 -33프레임), S3 링 경계 폴백 착지
  재페인트(505/16.833 @ 클록 00:16), S4 릴리스 정밀 시크+재생 복원,
  S5 Play/Pause + ±1F. vpt9/vpt4/vpt5 전부 ALL PASS. 오디오 무음은
  jogging 디코드 스킵 + paused 장치의 이중 구조 보장 — 외부 관측점 없음
  (사용자 실측 항목으로 유지). 첫 틱 지연(deferred minor)은 480x270에서
  미관측(~250ms 내 완전 착지) — 4K 재측정 항목.

### 8.1 결함: 폴백 웨지 (프로브가 발견, 픽스 9c7979e)

EOF 클램프까지 롱 전진 조그 → 링 경계 폴백 시크 후 그림이 프리즈(클록은
착지 시간을 가리키는데 화면은 크로싱 순간 프레임에 영구 고정, ±1F
재페인트 불가, 결정론적 재현). 근본원인 2건:

- **게이트 홀드가 시크를 놓침(주원인)**: 홀드 루프의 유일 탈출이
  `wantSeek`인데, 워커가 파킹 중(EOF)이면 시크가 1-4ms에 완주(hot page
  cache)해 10ms 게이트 폴링 창을 새고, 홀드 중인 패킷 pts는 post-seek
  고정 클록을 영영 못 넘는다(디코드 전진도 videoQ도 공백). 4K50
  demux/decode 분할(§7)부터 잠복해 온 레이스 — 이 파일에선 디코드 시간이
  길어 가려졌을 뿐. → `vSeekSeq` 세대 카운터: stage (a)가 bump(시크 실패
  롤백 없음), 게이트 루프는 dequeue 전 캡처해 불일치 시 폐기.
- **스테일 오디오 게이트(부수)**: mp4 역방향 시크가 양 트랙을 착지
  키프레임 DTS에서 재시작 → 클록 아래 오디오가 링을 만석으로 채워
  RINGPUSH 파킹 → demuxer가 비디오 게이트 아래 파킹. → `audioSkipBelow`
  드롭 게이트(시크 Ok 경로에서 target 아래 오디오 폐기, 첫 target 도달
  패킷이 해제 — 키프레임 클램프 시크의 잠재 A/V 어긋남도 제거).
- **Heisenberg 교훈**: fopen 계측이 10ms 게이트 폴을 늘려 결함을 가렸다
  (계측 빌드는 통과, 클린 빌드 재현). 클린 빌드 연속 5런이 권위.

### 8.2 레저 갱신

- **① NV12/d3d11va 텍스처 업로드 경로** — §7.4 ① 유지, 다음 병목(4K 렌더
  27Hz). 이번 작업과 무관하게 최우선. → **§9 as-built: 착수 완료, 프리미스
  반증**(업로드 바이트는 병목이 아님 — 실제 병목은 재생 표시 페이싱 캡,
  §9.4). d3d11va 제로카피(v2)는 그 캡 규명까지 파킹 유지.
- **② v2 역방향 자동 재생** — 링을 역방향 케이던스로 자동 진행, 소진 시
  이전 키프레임 시크 후 반복(스펙 §7, v1 링 기계 위에 얹음).
- **최종리뷰(opus, APPROVE — 2026-09-15) 1줄 경화 2건** (조그 영역 재접촉 시):
  stage-(a) vPktQ drain 뒤 `vSeekSeq` 제2 bump(너비 μs급 bump↔drain 갭 봉쇄),
  `audioSkipBelow`에 dropBeforePts와 동일 -0.05 슬랙. 잔여 레이스는 전부
  자가치유·1프레임 스케일로 park(ledger 트리아지).
- §7.4 ②(vPipeDelay 일시정지 수축 관찰)·③(audioEof 상호작용) 그대로 유지.

## 9. NV12 텍스처 업로드 경로 (as-built — 프리미스 반증, f2350d0, 2026-09-15)

§7.4 ①(4K 렌더 ~27Hz 병목 해제)의 착수 결과. 결론부터: **NV12는 착실히
착수해 유지하지만, 목표였던 렌더율 향상은 측정으로 기각됐다** — §7.3의
"4K 33MB 업로드 비용이 다음 병목" 귀속은 사전 가설이었고, 실측은 그 반대를
가리킨다(§9.3). 본 절은 스펙/플랜 2026-09-15 vplayer-nv12-upload의
as-built이며, 성공 서술이 아니라 **"착수 완료 + 프리미스 반증"** 기록이다
(문서 정직 관례).

### 9.1 착수 내용 (커밋 f2350d0, 단일 커밋 — 중간 커밋 회귀창 없음)

- `VideoFrame`: `rgba` → `pix` + `bool nv12`, `bytesFor/FrameBytes` 헬퍼
  (바이트 소비처 전부 갱신 — 디코드 풀·차원 게이트·jogRing 2곳).
- 차원 게이트: 짝수 차원 → `AV_PIX_FMT_NV12` 디코드 전환 + `SDL_UpdateNVTexture`
  (Y @0, UV @w·h, pitch w); 홀수 차원은 RGBA 폴백(오픈 시 결정).
- 메모리: 4K 프레임 33MB → 16.5MB — 디코드 풀/파이프라인/jogRing이 절반.
  4K 33MB 텍스처는 조그 링 바이트 캡(1.5GB)에서 ~0.9s였지만 NV12는
  **~1.8s**(산술값 — 실측 아님, 1.5GB÷16.5MB).
- 상태행에 렌더 게이지 `렌더 NNHz`(1초 창 업로드 횟수) 상시 표기 — 판정
  지표. 일시정지 중엔 마지막 값을 유지(알려진 성질, 판정은 재생 중 샷으로).

### 9.2 측정 (vpt10_nv12.ps1, tmp/vpt10_4k.mp4 3840x2160@50 H.264, 3전체 런)

재생 중(버튼 라벨로 상태 확인) 상태행 게이지 직독:

| 런 | 샘플 (클록 실시간 확인) |
|---|---|
| 1 | 26, 16, 22, 21 Hz |
| 2 | 27, 31, 26 Hz |
| 3 | 30, 26, 28 Hz |

**범위 16-31Hz, 대표값 26-31Hz — §7.3 베이스라인 ~27Hz와 동일, 개선 0.**
플랜 판정 바(>= 40Hz) FAIL. 나머지 시나리오 전부 통과: 컬러바 크로마
클린(testsrc2 참조 프레임 대조), 홀수 해상도 RGBA 폴백 정상(481x271),
조그 링 히트/릴리스·재생/일시정지 토글 정상, vpt9/vpt4/vpt5 회귀 ALL PASS.

### 9.3 프리미스 반증의 증거 (측정이 말하는 것)

- **480p 대조군**: 480x270@30 재생 중 게이지 15-16Hz(번인 카운터와 클록
  실시간 일치 확인). 업로드가 자명하게 싼 크기에서도 같은 캡 → 캡은
  프레임 크기/포맷과 무관.
- **같은 바이너리의 조그 세션은 47-59Hz** — 디코드-게이트+링 표시 경로는
  업로드를 50fps 근처까지 올린다 → 업로드 경로 자체는 50fps급 여유.
- 따라서 재생 중 16-31Hz 캡은 **업로드 바이트가 아니라 프레임 전달/표시
  페이싱**이다. §7.3의 귀속("4K 33MB SDL_UpdateTexture 비용이 다음 병목")
  은 렌더 루프 전체 기준의 임시계측에서 병목 위치를 단정한 것 — 위치는
  그대로(재생 표시 루프)지만 원인 귀속이 틀렸다.

### 9.4 레저 갱신 — ① 해소 (재생 페이싱 캡 픽스, task-4, 2026-09-15)

(구) 최상위 ① "재생 표시 페이싱 캡"은 후속 진단(task-3-report,
systematic-debugging, 계측 후 전량 원복)으로 규명됐고, 후속 픽스(task-4,
제안 A 단독)로 **해소**됐다. 후보 (a)(b)(c)는 전부 반증 — UI 체인(타이머→렌더
→팝 시도)은 61Hz 만능(실측 ro/tim/sync/att=61/61/61/61). 근본원인은 디코드
공급의 B-frame 클럼프(3-4프레임 버스트, 게이트가 DTS 순 통과하며 저-pts
B-패킷 연쇄 통과) + `PopVideoFrame`의 latest-wins 드롭 루프(구
`ClientVPlayerApp.cpp:1528-1533`)가 버스트를 "1장 표시 + 나머지 폐기"로
소진한 것이었다(항등식 ok+drop=vpush=콘텐츠 fps, ok≈vpush/2).

**픽스 (제안 A — 표시층 드롭 규칙 교정, 단일 커밋)**:

- 드롭 루프를 "진짜 만기분만" 폐기로 교정 —
  `videoQ.front().pts < clock - frameInterval` (frameInterval = 1/fps,
  PlayerCore의 avg_frame_rate 값, 없으면 1/30 폴백). 만기 전 프레임은
  videoQ에 보존돼 다음 렌더 틱에 자기 차례에 팝된다(첫 팝 게이트
  `front.pts > clock + 0.02 → return false` 불변).
- videoQ 캡 3 → 6 (`kVideoQMaxFrames`) 병행 — 버스트 보관 여유(~200ms@30 /
  ~120ms@50).
- 조그 기계(jogRing/JogTo/JogFrame/vSeekSeq/audioSkipBelow)·디코드 게이트
  (:1437)·SeekCommon 무접촉. 새 스레드/뮤텍스 없음.

**신규 측정치** (임시 계측 1초 창 카운터 → 전량 원복 후 클린 바이너리로
vpt10 공식 런, 게이지는 샷 직독):

| 항목 | before (§9.2) | after |
|---|---|---|
| 4K50 게이지 (S1 3샷) | 26-31Hz (범위 16-31) | 37/32/39Hz (계측 ok 29-43/s, mean ~36) |
| 480p30 게이지 (S6) | 15-16Hz | 22/23Hz (계측 ok 17-26/s, mean ~22) |
| drop | ≈ok (버스트 통째 소진, ok≈vpush/2) | 4K 9-21/s, 480p 4-11/s |
| vpush | 콘텐츠 fps | 동일 (불변) |
| 조그 링 히트 (S3) | 47-59Hz | 44/53Hz — 무영향 확인(44Hz도 동일 게이지의 새 바이너리 샷) |
| 홀수 RGBA 폴백 (S5) | 정상 | 정상 (481x271) |

**브리프 기대치 미달과 잔여 판정 (정직 기록)** — 브리프 기대(480p 28-31,
4K 45-50, drop≈0)는 **미달**이다. 임시 계측의 lateness 카운터가 잔여를
말한다: 드롭된 프레임은 "보존됐어야 할 미래 프레임"이 아니라 **도착 시점에
이미 만기분**이다 — 푸시 시점 lateness(clock−pts) arrAvg 4K +5~+16ms /
480p +22~+30ms, 드롭 프레임 lateness 평균 4K 29-35ms / 480p 47-54ms(프레임
간격 20/33ms 직후), 미래 도착은 푸시의 ~30%뿐(q 0-3). 디코드 클록 게이트가
just-in-time으로 프레임을 내보내는 한(§7 구조) 드롭 규칙 교정만으로 회수되지
않는 공급층 지연이 남는다 — task-3 §5 제안 B(공급 평탄화 / 게이트 리드
상향)가 후속 레버. **결론: ~0.5× 캡은 해소(4K 0.65-0.78×, 480p 0.73-0.87×),
조그 상한(47-59Hz)까지는 미달 — 잔여 캡은 공급측이다.**

- **v2 d3d11va 제로카피(스펙 §7) 재판단 보류 유지** — 선결 조건은 제안 B
  (공급층 잔여 캡 해소).
- §7.4 ②(vPipeDelay 일시정지 수축)·③(audioEof 상호작용) 유지.

### 9.5 판정 방법 한계 (기록)

- 브리프의 S3 "번인 프레임 번호 비교"는 4K에서 불가 — 3840→윈도우
  다운스케일에서 번인 글리프가 ~10px로 붕괴(확대 크롭으로 판독 불능,
  shots/zoom-*.png 증거 보존). 대체 판정: UI 클록 초 단위 왕복
  (00:08→00:09→00:08) + 무셋틀 샷의 그림 내용 변화(컨트롤러 승인).
  프레임 단위 정밀 대조가 필요하면 큰 번인 글리프를 태운 4K 미디어
  재생성 필요.
- NV12 변환 CPU 시간: 외부 관측점이 없어 미측정(조그 47-59Hz가 전환 포함
  디코드 경로의 여유를 간접 시사).
