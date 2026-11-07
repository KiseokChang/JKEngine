# 104 — jkdesktop-apk 통일 라인 as-built (2026-11-01 → 11-07)

## 0. 이 문서

플랜 `docs/superpowers/plans/2026-11-01-jkdesktop-apk-unified.md`의 as-built.
T1-T10의 착상 순서대 커밋 체인·함정 원존·측정표를 정산한다. 세부의 딥 원문은
각 라인의 전용 리포트에 위임한다.

## 1. 커밋 체인 (역순 — 최신)

| 커밋 | 라인 | 요지 |
|---|---|---|
| f63c55a | 브라우저 T1 | BrowserActivity.kt+manifest+JS intent-URI leg |
| db9a086 | 폰 글자 깨짐 | 타이틀 비트맵 경로/대소·경로 양형 개방 |
| 175df36 | 터미널 T9 | 셸 -i 대화형+폰 폰트 사다리 |
| 33f972f | 벡터 폰트 | 시스템 폰트 사다리(OneUI-SECCJK) |
| 5526e4d | T9-라이트 | APK state seam 3-gap CWD 수형 |
| e6c7771 | 구두 별명 | 4종 승계+2y-㉒ 갱신 |
| 407c095 | T7 LLM | HTTP 어댑터 통일+클라 사신 신설 |
| 84c7021 | 터미널 tofu | OneUI VF ASCII 미커버 확정+DroidSansMono |
| 063d029/8b81569 | 창 리사이즈 | 숨은 창 SetWindowSize+재결합 |
| 6cdbbb3 | 챠드 게이트 | 더블클릭 챠드+1n-f8 수리 |
| f63c55a 전 | 입력 라인 2차 | ReinstateImages(캐시 UAF)+드라이버 1자 |

## 2. 결함 원장 (본 세션 신관)

### 2.1 DeX 캡션/창 리사이즈 — 063d029·8b81569 계열
- 1차 원장 "핸드셰이크 미발사" = 정정 필요 — re-bound 원문 존재.
- 진원 = 숨은 SDL 창의 크기 미동기(r30 창표면 직결 우회와 리사이즈의 충돌).

### 2.2 챠드 — 6cdbbb3
- DeX 트랙패드 = 좌우 동시 버튼 불가(합성 규약) → 더블클릭 챠드 신설.
- 원문: [mouse] button=1 clicks=1..4 연속 — button=3 부재.

### 2.3 창 타이틀 글자 깨짐(Terwinal) — db9a086
- 진원 = 비트맵 폰트 경로의 폰트 부재(대소문자 갭)+내장 폴백 표 변형.
- 수형 = OpenFontFile의 JKENGINE_FONT_DIR leg+대소 양형.

### 2.4 터미널 본문 tofu — 84c7021
- 4판 계측 원문 정산(정보 수집 우선 루프 — 사용자 지시) — [cmap-why] 원문이
  결정: OneUI Sans KR VF의 cmap = ASCII 미커버(gi(':')=0).
- 수형 = DroidSansMono(ASCII·등폭)+OneUI(VF — 한글) 폴백 조합.
- 렛슨: 프로브의 "PASS"가 .notdef도 흡수할 수 있다(모양 무검증 한계) —
  래스터 프라이버시 원존.

### 2.5 시스템 폰트 사다리 — 33f972f
- stb_truetype = CFF 거부(NotoSansCJK·SECCJK.ttc) — glyf 단면 축만 통과.

### 2.6 T7 — 407c095
- jk::net 클라 사신 신설(ConnectTcp·Recv) — RecvAll exact-n과 HTTP 부정합 원존.
- cloud TLS = 폰 플랫폼 JNI(JkHttp.java) — 조달 0 원존.

## 3. 캐논

- Win selftest = 985 PASS 0 failure 최종(신설 챠드 4+별명 1+1n-g 19·2y-d2 갱신 포함).
- 2m-e junction 2건 = 일시 환경 실패(I: exFAT TMP 우회) → C: 11GB 정리 복귀 재측정 PASS 원문(stfinal2) — TMP = C: NTFS 필수(가득 사건 원존).
  C: 가득(0MB) 사건의 TMP 우회로부터. C: 정리 = 사용자 몫.
- WSL/posix/폰 축 = 미접촉(본 세션) — 다음 접촉 재측정 원존.

## 4. EYES (사용자 결제 대기)

1. 터미널 체감 — 프롬프트·입력(PTY stdin 경계 원존).
2. 브라우저 렌더 — DeX 시스템 대화(무선디버깅 재승인) 후 주소행 개봉.
3. cloud LLM 턴 — API 키 준비 대기.
4. C: 디스크 정리 — TMP exFAT 우회 해소(2m-e 정상화).

## 5. 다음 라인

- 브라우저 Task 2 (측정 하니스) → Task 4 (폰 probe) → Task 5 (GeckoView).
- T8 오디오 — openal(OpenSL) 조달 — install_android_sdk.sh §openal 신설.
- lf/helix 조달 — install_lf_helix_posix.sh의 static 바이너리 lib*.so 포장.
- 하루 정리 친구 — 사용자 백로그 신설(11-04 말미) — 세션 기록+use_journal+
  docs 원장을 먹는 일일 회고 턴 — 별도 스펙 착수.
