# 66. JKEdit/터미널 한자 변환 — Phase A 실측 + 수술 as-built (NO-GO → **GO → 완결**)

> 날짜: 2026-09-26. 계기: docs/65 O1 행(한자 키 LANG2 미처리) 수술 착수 전
> GO/NO-GO 게이트. 판정: IMM/TSF 채널 NO-GO → **HanjaDic COM 채널 GO — 수술 재개.**
> 승인 플랜: `~/.claude/plans/curried-jingling-sunbeam.md`(2026-09-26 승인).
> §6-§9: 수술(B1-B4)·콘솔 게이트·라이브 e2e as-built — 2026-09-26 전부 완결.

## 1. 요청과 사용자 확정 (2026-09-26)

- 변환 단위 = 직전 완성 음절 1자 (MS IME 단일 글자 변환과 동일)
- 사전 = **시스템 IME 사전** — 내장 폴백(REFHAN.COD 등) 없음, API 사망 시
  **중단+보고** (폴백으로 조용히 넘어가지 않는 것을 명시 합의)
- 범위 = JKEdit + 터미널 둘 다

## 2. 측정 1 — IMM 채널: ImmGetConversionList 완전 사망

`probe_hanja_dict.cpp` + `probe_hanja_dict2.cpp`(engine/tools/probes, 콘솔 유닛
프로브 — "창 없는 콘솔에서 API가 사는가"를 동시 측정):

- 한국어 HKL 존재: `GetKeyboardLayoutList` low WORD 0x0412 = `04120412` 1개 확인.
- 전 조합 0 반환: 소스 인코딩(EUC-KR C7D1 / KSSM D065) × 플래그(GCL_CONVERSION /
  GCL_REVERSECONVERSION) × hIMC(NULL / ImmCreateContext) — 10조합 전부 `got=0`,
  2단 호출 크기 측정(`dwBufLen=0`)도 0. 대조군 "가"/"하나" 포함.
- GetLastError 변화 없음(채질 0xdeadbeef 유지) → 오류가 아니라 API 내부
  short-circuit. 신형 한국어 IME가 이 사전 조회 채널을 지원하지 않는다.
- 보충(probe_hanja_dict2): (a) `ImmGetConversionListW` GetProcAddress — 수출되어
  있으나 역시 0 (b) 구형 KLID `00000412` LoadKeyboardLayout → HKL이 동일
  `04120412`(구형 IME 엔진 미설치 — 로드 불능) (c) 실창+ImmAssociateContext+
  ImmSetOpenStatus(TRUE)+NATIVE|FULLSHAPE → 역시 0 (d) 플래그 4(GCL_PRECONVERSION)
  → 0. ×2 실행 동일.
- **판정: IMM 경로 사망 확정.** 레슨 재확인(docs/61 §18): IMM 관측은 시대에 따라
  부패한다 — 신형 IME 시대에 구조 조회는 사라졌다.

## 3. 측정 2 — TSF 채널: ITfFnSearchCandidateProvider 미노출

`probe_hanja_dict3.cpp` — 현대판 시스템 사전 채널(ITfFunctionProvider →
ITfFnSearchCandidateProvider::GetSearchCandidates; Windows Terminal류가 쓰는
경로). MinGW msctf.h 결측 인터페이스는 SDK ctffunc.h GUID/vtable 순서로 수동 선언:

- `TF_CreateThreadMgr` + `Activate` 성공, 한국어 프로파일 2개
  (구형 MS IME `A028AE76…` fActive=-1, 신형 IME `A1E2B86B…`).
- `GetFunctionProvider(clsid)` → **양 프로파일 모두 TF_E_NOINTERFACE
  (0x80040503)** — 포커스 없는 스레드에서도, 실창+DocumentMgr+Context(NULL
  텍스트스토어)+SetFocus 정식 셋업 후에도 동일.
- `EnumFunctionProviders` → **function providers=0** — 포커스 문맥에 등록된
  프로바이더 자체가 존재하지 않는다. 어디에도 SearchCandidateProvider가 없다.
- ×2 실행 동일(결정론).
- 비고: (1) MinGW의 `ITfThreadMgr`는 `Initialize`가 `Activate`로 개명된 WIDL
  생성판 — vtable 슬롯 동일, `Activate` 호출로 측정 (2) `TF_CreateThreadMgr`는
  `GetModuleHandleA("msctf.dll")`에 걸리지 않는다 — `LoadLibraryA` 필요.

## 3.5. 측정 3 — HanjaDic COM 채널: **GO** (사용자 "다른 사전 소스 검토" 지시 후)

`System32\IME\IMEKR\DICTS\imkrhjd.dll` — 한국어 IME가 자체 운용하는 한자 사전을
COM 서버로 수출한다(InprocServer32, HKCR `imkrhjd.hanjadic` ProgID 등록).

- `probe_hanja_dict4.cpp` — TypeLib 덤프(imkrapi.dll + imkrhjd.dll). imkrapi.dll의
  `ImeCommonAPI_KOR_Desktop_V1`({20cd9315-…})는 **문서화되지 않은 인터페이스**
  (SDK 헤더 부재, 타입라이브러리에 본체 인터페이스 미등록) → 부적합 판정.
  imkrhjd.dll의 **`IHanjaDic {AD75F3AC-18CD-48C6-A27D-F1E9A7DCE432}`**는 타입라이브러리
  완비(27 메서드) — 채택. coclass CLSID `{4c870c20-4ed3-4235-9a2a-7185f8a87d06}`,
  `IHanjaDic2 {DA341716-…}` 병존.
- 서명(타입라이브러리 VT 덤프 확정):
  `GetHanjaChars(USHORT wHangulChar, BSTR* pbstrHanjaChars, VARIANT_BOOL* pvRet)`
  — 한글 **1자**(Unicode 코드) → 한자 문자열(UTF-16, 1자씩 연결).
  `GetHanjaWords(BSTR, SAFEARRAY**, VARIANT_BOOL*)` — 단어 경로(v1 미사용).
  존재하지 않는 코드(조합자모 0x1100) → vb=0 + BSTR NULL — 실패 모드 클린.
- `probe_hanja_dict5.cpp` ×2 ALL PASS(5체크):
  - `GetHanjaChars('한' 0xD55C)` → **14자: 韓(U+97D3) 漢(U+6F22) 限 寒 翰 恨 閑
    旱 汗 浣 閒 悍 寒? 瀚** — 漢/韓 포함, MS IME 관례 순(빈도순 추정) 유지.
  - 재호출 결정론 ×2 PASS. 대조군(조합자모) 클린 실패.
  - 참고: `GetHanjaWords("한국")` → 韓國 汗國 限局 寒國 寒菊 5단어 — 단어 변환
    v2 여지 기록.
- **판정: GO.** 시스템 사전 채널 확정 = `IHanjaDic` COM. JKHanjaDict의
  Provider가 이 채널을 감싼다(플랜 B1의 Provider 추상화에 정확히 대응 —
  승인 플랜 원문의 ImmGetConversionList 파이프라인만 COM 호출로 교체).

## 4. 중간 판정: NO-GO(1차) → 사용자 지시 "다른 사전 소스 검토" → **GO(최종)**

IMM/TSF 두 채널 사망 시점 판정은 NO-GO였고 승인 합의대로 수술을 중단·보고했다.
사용자 결정(2026-09-26): **"다른 사전 소스 검토"**. 재조사에서 시스템 한국어 IME의
**자체 한자 사전 COM 서버**가 생존함을 실측 — 사용자의 원래 선택("시스템 IME
사전")을 그대로 충족하므로 승인 플랜 수술을 재개한다.

## 5. 재사용 가능한 부산물

- 프로브 5개(`probe_hanja_dict{,2,3,4,5}.cpp`)는 "한국어 IME 사전 채널 생존
  확인" 재사용 부품 — IMM/TSF/HanjaDic COM 세 채널을 선형 확인 가능.
- TSF 수동 선언 블록(IID/vtable), TypeLib VT 덤프기(dict4)는 MinGW 결측분을
  채우는 참고 구현.
- 한자 키 채널 자체(LANG2 스캔코드 145, VK_HANJA 0x19, LL 훅 우회 필요성,
  docs/61:308의 "146" 기록 편차)는 사전 채널과 독립 — 플랜 B1-B4 그대로 유효.
## 6. 수술 as-built (Phase B1-B4) — 2026-09-26 전부 완결

### B1. JKHanjaDict (include/JKHanjaDict.h + src/JKHanjaDict.cpp, jkcore 멤버)
- 계약(`jk::hanja`): `Candidates(kssmSyllable, out)` / `Available()` /
  `SetProviderForTest(nullptr=시스템 사전 복귀, 주입 중 Available()=true)`.
  KSSM 쌍 인코딩 = `(first << 8) | second`.
- 인코딩 파이프라인: 조회 `KssmCodepointToUnicode(pair)` → COM(Unicode 1자) →
  UTF-8 → `Utf8ToKssm` → 쌍 복원 불가(`?` 치환) 후보 탈락(렌더 불가 한계와 일치).
- EnsureInit: CoInitializeEx(STA, CHANGED_MODE 허용) → CoCreateInstance(INPROC) →
  OpenMainDic → **채질 1회** `'한'(조합형 0xD065)` 14 후보(§3.5 실측과 동일 기준).
  캐시: 음절→후보 unordered_map, UI 스레드 전용.
- **실사 프로브 신설(`hanja_dict_live_probe.cpp`)**: 콘솔 게이트 전부가 가짜
  프로바이더 주입이라 EnsureInit의 실제 COM 경로가 무검증이었다. 실측
  Available()=1, Candidates(0xD065)=14, 재호출 결정론. 클라이언트 프로세스
  문맥 실패 가설을 배제하는 근거로 쓰였다(§8).

### B2. ImeHanja 이벤트 배관 (ImeToggle과 1:1)
1. 열거: `JKEvent.h` ImeHanja(클라 열거값 **12** — 라이브 로그 관측용),
   `JKWireProtocol.h` `ImeHanja = 11`(끝에 추가 — 스테일 .o 호환, docs/65 레슨).
2. 훅: `JKImeHook_win32.cpp` `g_hanjaEvent` 2회째 `SDL_RegisterEvents`, down 엣지
   발화 + keyup 해제(VK_HANGUL과 동시 봉합). **원시 키 차단은 §8 결함 1 픽스.**
3. 서버: JKWindowServer.cpp 훅 이벤트 분기 — 포커스 클라 푸시 +
   `[ime] hanja -> client` 로그(+`fflush` — §8 레슨).
4. 클라 번역: JKClientSurface.cpp InputEvent 스위치에 ImeHanja case.
5. 포커스 디스패치: JKWindow.cpp:441 ImeHanja 추가(docs/61 결함 #16 동형 누락 방지).
6. 소비자: JKEdit/TerminalView 핸들러 + 싱글 프로세스 폴백 LANG2 스캔코드
   분기(**145** — docs/61:308의 "146"은 기록 편차, SDL_scancode.h:258-259 실측).

### B3. JKEdit 후보 모드
- 상태: `hanjaActive_/hanjaList_/hanjaPage_/hanjaSel_/hanjaPos_/hanjaOrig_/
  hanjaSwallow_`. 진입 가드: readOnly/비포커스/비내부한글/imeComposing_/선택/
  cursorPos_<2/cp∉0xAC00..0xD7A3/Candidates 빔 → no-op. 대상이 이미 한자 = no-op.
- **단일 커밋점**: KeyDown 숫자는 흡수만, **Char 숫자 '1'-'9'가 커밋**. Esc 취소,
  Enter 커밋, Left/Right 전역 ±1, PageUp/Down 페이지, Backspace/HOME/END/DELETE
  흡수(커밋 대상 보호), 그 밖 키 = 취소+통과(IME 관행). MouseDown/OnKillFocus 취소.
- 커밋: `hanjaOrig_` 가드 + 2바이트 제자리 치환(레거시 Proc_Hanja 동형),
  cursorPos_=pos+2.
- **게이트가 잡은 실결함(T24e2)**: Enter 커밋 직후 Char '\r'이 개행으로 삽입 —
  swallow 확인을 Char 분기 **선두**로 옮겨 픽스(커밋이 hanjaActive_를 내려도
  플래그는 살아 있어야 하므로). 터미널도 동일 패턴.

### B4. 터미널
- TerminalHangulInput: `ComposingSyllable()`(조합 중 음절의 KSSM 쌍, 슬롯
  0x8441→0) + `CommitHanja(kssmHanja)`(조합 해제 + `KssmCodeToUtf8` 1자 pty 송출).
- TerminalView: ImeHanja는 **조합 중일 때만** 오픈(비조합 no-op), 키 게이트는
  HandleKeyDown 선두(ClearPreEdit보다 앞), 커밋 단일점=Char 숫자, Enter 커밋 후
  Char '\r' 1회 흡수(셸 엔터 유출 차단), 팝업=커서 셀 앵커 세로 목록(≤9행, 하단
  근접 시 상향 플립), 1페이지 초과 시 "n/m" 페이지 지시자.

## 7. 콘솔 게이트 ×2 — 전부 ALL PASS (2026-09-26)

| 프로브 | 결과 |
|---|---|
| jkedit_probe | 87 PASS ×2 (기존 57 + 한자 30) |
| terminal_hangul_probe | 44/44 ×2 (ComposingSyllable/CommitHanja T15-T16) |
| terminal_hangul_view_probe | 27/27 ×2 (팝업 배선 T10a-f) |
| jktext_probe / jktext_view_probe | 73 PASS ×2 / PASS ×2 (회귀) |

사전 주입은 전부 `SetProviderForTest`(후보[0]=韓 고정) — 결정론. 실제 사전
채널은 §6 실사 프로브와 §8 라이브 e2e가 커버.

## 8. 라이브 e2e (docs/65 게이트 C) — ×2 ALL PASS

`tools/probes/o1_hanja_e2e.ps1`(합성 입력, ×2): 라이브 스택에서
g k s(=한) 타이핑 → SendInput VK_HANJA → 서버 `[ime] hanja -> client 2 (sent)`
→ 클라 `ev type=12` 도착 → 팝업 페인트(screenshot diff 1512) → 숫자 '1' 커밋 →
pty `send=3 bytes [E9 9F 93]` = **韓**. 스크린샷 눈확인: 커서 아래 세로 목록
**1韓(선택 반전) 2漢 3限 4寒 5翰 …** + 앵커 행 "1/2" 페이지 지시자 — 시스템
사전 후보가 그대로 렌더된다.

**결함 1 (라이브 실측): 합성 VK_HANJA 원시 키다운이 팝업을 닫는다.** 실물 키는
OS IME가 삼켜 SDL에 도달하지 않는다(docs/61:313). 합성 입력(SendInput)은 삼킴을
우회해 새어 들어오고, 이 리그에선 스캔 0x1D로 번역돼(SDL scancode 0xE0=LCTRL,
`ev key=0x400000E0 mod=0x40` 관측) 도착 — 팝업의 "그 밖 키=취소+통과" 규칙이
방금 연 팝업을 즉시 닫고, 이어지는 숫자가 일반 경로(조합 확정)로 흘렀다.
**픽스: LL 훅이 VK_HANJA down/up을 통째로 삼는다**(전경 가드 내 — 타 앱 무영향).
실물/합성 동작이 일치된다.

**레슨 1: `jkapp_terminal.dll`은 별도 빌드 타깃.** 클라 모드 터미널 뷰 코드는
jkdesktop.exe가 아니라 apps/jkapp_terminal.dll로 로드된다 — `ninja jkwinserver
jkdesktop`만 돌리면 TerminalView 수정이 라이브에 반영되지 않는다(스테일 DLL로
한동안 헤맴). 라이브 스택 검증 전에는 **풀빌드**가 원칙.

**레슨 2: 서버 `[ime]` 로그는 블록버퍼.** jkwinserver 로그는 stdout 파이프로
리다이렉트 시 버퍼링돼 강제 종료 시 유실 — `[ime] hanja` 로그 직후
`fflush(nullptr)` 추가. 프로브 증거가 이제 결정론.

**관측 설비 보강**: `[preedit] result` 로그에 send 바이트 hex를 붙였다 —
`send=3`만으로는 조합 확정(한=ED 95 9C)과 한자 커밋(韓=E9 9F 93)을 가려낼 수
없어 ×2 판정이 한 번 오탐했다(처음 프로브가 EC 97 93으로 착각한 것도 동일
실수). `[preedit] ev` 필터에 ImeHanja도 추가(미추가로 "미도착" 오판).

## 9. 잔여·열린 항목

- **사용자 눈확인**: 실물 한자키로 JKEdit/터미널 각각 (숫자 커밋·Enter 커밋·Esc·
  화살표·Backspace 의감(취소+흡수) — 위화감 있으면 재판, 플랜 위험 8).
- VK_HANGUL 합성 누출은 미차단(기존 측정 동작 유지) — 실물 키는 OS IME가
  삼키므로 라이브 영향 없음. 필요 시 VK_HANJA와 동일 패턴으로 차단.
- 한 줄 JKEdit 스트립이 비좁으면 콤보 선례 폴백(v1 밖, 플랜 위험 3).
- 대상이 이미 한자 = v1 no-op(역방향 사전 문제, 문서화).
