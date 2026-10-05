# 슬롯 출하 도구 (slot → .jkx) — 설계 스펙 (docs/74 §5 경로 마지막 고리)

날짜: 2026-10-05
지시원: docs/67 단 2 (`:138-144`) · docs/74 §5 (결정 후 경로 — 트러스트
문 완결 후 "단 2 출하 라인 개통") · docs/76 §9 (벤치 MANI=총망라 원문,
출하 MANI는 각자 좁게 — 파일 주석 동일 결정)
원료: 탐색 에이전트 실측 인벤토리 (2026-10-05, 이 세션)

## 0. 지시원의 정리 — 실제 잔여는 둘

- "jkctl pack 뿌리 수리"는 **이미 완결**(docs/74 §4-4; b7a76a3
  `JkxManifestMerge` — 화이트리스트 폐기→필드 보존, scriptfile=/watch=
  잔존 실측). docs/74:92의 문구는 갱신 전 오래된 표기 — 이 스펙이 대신
  원장화한다. docs/67:175의 "수리 전 출하 도구 제작 금지"는 해소됨 —
  금지 조건이 사라진 뒤에 도구를 만든다.
- 실제 잔여 = **슬롯→.jkx 출하 도구**(본 스펙) + **갤러리**(별도 스펙 —
  docs/67:144 "갤러리(설치 앱 목록)는 로컬 라이브러리만" · 열린 질문 3
  `:182`. 이 스펙은 출하 도구만 다룬다 — 갤러리는 다음 문).

## 1. 문제 — 출하 모드의 갈림길 (이 스펙의 핵심 결정)

.jkx 부팅은 MANI `scriptfile=` 유무로 갈라진다
(JKAppModule_script.cpp:157-200):

- **워크숍 분기**(scriptfile= 있음): exeDir 상대 **외부 파일이 진실원**,
  부재 시 `kTemplateScript` 시딩, 라이브 편집·감시, **능력 게이트 활성**.
- **순수 SCRI 분기**(scriptfile= 없음): DLL 옆 파묻힌 `app.js` 사용
  (SideFilePath ".app.js"), 외부 진실원 없음, **게이트 비활성**.

출하 .jkx를 SCRI 모드로 싣으면 게이트·배지가 작동하지 않는다 — docs/74
결정의 전체 취지("다른 환경에서 불리는 슬롯 앱"이야말로 게이트 대상)가
빠진다. 워크숍 모드로 싣으면 수신 기기에 외부 파일이 없을 때 출하본이
아니라 `kTemplateScript`가 시딩된다 — 슬롯 내용물이 사라진다.

**결정 (Ruling — 이 스펙의 봉인):** 출하 = 워크숍 모드 + 파묻힌 SCRI 시딩.

1. 출하 MANI는 `scriptfile=`을 싣는다(워크숍 모드 → 게이트·배지·진실원
   문화가 수신 기기에서도 산다). 값 = exeDir 상대
   `state/scripts/<slot>.js` — 수신 기기의 워크숍 슬롯 레이아웃과 동일 자리.
2. `jk_app_run_client` 워크숍 분기의 시딩을 확장한다: 외부 파일 부재 시
   **DLL 옆 파묻힌 SCRI(`SideFilePath(".app.js")`)가 있으면 그것으로
   시딩**, 없으면 현행대로 `kTemplateScript`. 외부 파일이 이미
   있으면 파묻힌 SCRI는 무시(외부가 진실원 — 현행 불변).
   - 파묻힌 SCRI는 배선상 항상 추출된다(main.cpp:630-644 — SCRI 엔트리가
     있으면 무조건 DLL 옆에 `.app.js`로 떨어진다). 문은 "출하 팩이
     scriptfile= 과 SCRI를 함께 싣는다"는 것 — 둘 다 있으면 시딩 출처가
     SCRI로 전환된다. jkx-pack(scriptdemo 등)도 이 배선을 이미 통과.
3. 트레이드오크를 원장에 적는다: 수신 기기 상태에 이미 같은 슬롯 이름
   파일이 있으면 **수신 기기의 그 파일이 이긴다**(이름 충돌 = 수신자
   진실원 존중 — docs/67 사훈 "파일=진실원"). 개인 과제 범위에서 충분,
   충돌 정책(대화/덮어쓰기)은 나눔 확대 시 2단계.

## 2. 도구 형태 — `slot-pack`

```
./jkdesktop.exe slot-pack <slot>          # → <exeDir>/apps/<slot>.jkx
./jkdesktop.exe slot-pack <slot> <out>    # out 경로 지정 (선택)
```

- 선행: jkx-pack(Windows-only, main.cpp:3415) 옆 배선 — 서브커맨드
  인식·usage·Windows-only 게이트 동일. 이름 충돌: `jkctl pack`은 콘솔앱
  zip 배포용으로 다른 도구(jkctl main.cpp:518-583) — 건드리지 않는다.
- 출력: `JkxFile::Write` 레이아웃(팩 엔트리 3개) — pack_workshop.ps1과
  동일 구조(JKJkxFile::Write와 바이트 동일한 실측 원장, docs/60 §3):
  - **MANI** `manifest.txt` — 신작 원문(§3)
  - **MODL** `jkapp_script.dll` — 현재 빌드본(base 경로, jkx-pack 동일).
    **stale DLL 함정**(docs/60:334-339): 재빌드 없이 팩하면 오래된 DLL이
    파묻힌다 — 도구 출력 첫 행에 빌드 규율 경고를 인쇄.
  - **SCRI** `app.js` — `<exeDir>/state/scripts/<slot>.js`의 원문.
- 슬롯 원천: `<exeDir>/state/scripts/<slot>.js`(라이브 슬롯 진실원 —
  engine/build/state/scripts 실측). 파일 없으면 오류 1 종료
  (시딩-재시도 경로 없음 — 출하는 진실원이 있는 것만 팩한다).
- 도구는 슬롯 파일을 읽기만 한다(쓰기·이력 접촉 금지) — 감시·mtime
  함정(docs/67:213-219)과 무관.
- 검증 경로: 팩 완료 후 `jkdesktop.exe jkx-list` + 팩안 MANI 추출 검증은
  probe 몫(§6). 도구 자체는 조용히 0으로 끝난다(출력 경로+바이트 수 1행
  stderr 인쇄 — pack_workshop.ps1 관습).

## 3. 출하 MANI — 개별 좁은 선언 (docs/76 §9 이행)

생성 원문(모든 행 캐노니컬, 주석 없음):

```
name=<slot>
title=<slot>
width=360
height=280
module=jkapp_script.dll
script=app.js
scriptfile=state/scripts/<slot>.js
capabilities=<used tokens>
```

- width/height: 워크숍 벤치 기본 동일(360x280) — 슬롯별 커스텀은 후보
  아님(YAGNI; 창 크기는 관습 따름).
- **capabilities 결정 = 사용량 자동 분석** ("좁게 선언"의 문):
  - 슬롯 소스를 훑어 **게이트 대상 바인딩 이름**이 출현하면 해당 토큰을
    선언한다. 분석기는 JKScriptHost의 게이트 토큰 표(docs/76 §2)를
    단일 출처로 재용 — 도구가 표를 복제하지 않는다(표가 흔들리면 출하
    선언도 같이 흔들린다 = 설계 의도). 형태: JKScriptHost.cpp의 bind
    표에 토큰을 보강(정적 표가 name→token을 아는 상태) + 공용 헬퍼
    `std::vector<std::string> HostCapabilityTokensFor(const std::string& source)`
    (선언 순서 = 표 순서, 중복 없음) — slot-pack이 호출, 셀프테스트가
    직접 단언.
  - 어휘 경계 매치 — 좌우가 식별자 구성문자 `[A-Za-z0-9_$]`가 아니면
    경계. `mysetInterval`은 `setInterval` 미적중, `setInterval()`은 적중.
  - **방향성 계약**: 주석·문자열 안 유사 식별자는 오탐(오버 선언)만
    낸다 — 이는 넓은 쪽이고 게이트가 여전히 잡아준다. 반대로 미감지는
    언더 선언 → 런타임 정직 차단(fail-closed, docs/74) — 둘 다 안전
    방향. 정적 분석의 완벽을 요구하지 않는다.
  - 리저브 토큰(network — 바인딩 없음)과 미지 이름은 출현해도 무시 —
    출하 선언에 들어가지 않는다.
- `watch=` 생략(기본 0; 라이브 편집은 벤치 기능, 출하본은 설치
  아티팩트 — 수신 기기에서 워크숍 벤치로 열면 그쪽 진실원이 이미 있다).
- `name`+`module` 필수 파스 계약(JKJkxFile.h:62-64) 만족.
- 파묻힌 SCRI와 scriptfile=이 함께 실리는 것은 현행 파스·추출 배선의
  통과 케이스 — 새 파스 분기 없음(§4 시딩 확장뿐).

## 4. 엔진 소수 변경 — 시딩 출처 전환 (§1-2 구현 자리)

- `JKAppModule_script.cpp` 워크숍 분기(:157-186): `ReadTextFile(path)`
  실패 시 — 현행은 곧바로 `kTemplateScript` 시딩 — 그 앞에
  `SideFilePath(".app.js")` 원문 읽기 시도를 넣어 있으면 그 원문으로
  시딩, 없으면 `kTemplateScript`(현행 회귀 유지).
- `SetAgentAppName(meta->name)` 그대로 — 수신 기기의 에이전트 도구
  등록 이름 = <slot>(수신자 app_tool 게이트 식별 유지).
- 변경은 이 한 곳뿐 — 파스·게이트·배지·추출 배선 불변.

## 5. 셀프테스트 (RunAppSelfTest 패턴)

1. **사용량 분석기**: (a) timer+canvas+fs+agent+widget 조합 원문 →
   선언 셋 정확 일치·표 순서 보존. (b) 식별자 경계 —
   `mysetInterval` 미적중, `setInterval()` 적중. (c) 문자열·주석
   폴스포짓은 오버 방향만 — 유사 표기가 input 선언을 유발함을 문서화
   단언. (d) network·미지 이름 무시.
2. **출하 MANI 원문**: 조립기 문자열 전체 비교(§3 캐노니컬 행) —
   Parse 통과 + capabilities 원문 재검.
3. **시딩 출처 전환**: (a) 파묻힌 SCRI 존재+외부 부재 → 외부 = SCRI
   원문. (b) SCRI 부재 → kTemplateScript(현행 회귀). (c) 외부 존재 →
   외부 무변(SCRI 무시). — headless 파일 시나리오.
4. 회귀: 기존 script demo·workshop 셀프테스트 전부 녹색 유지.

## 6. probe (라이브 end-to-end — engine/tools/probes/probe_slot_ship.ps1)

- scratch 슬롯(`shipscratch`) 원천 생성 → `slot-pack shipscratch` →
  `jkx-list` 조사(entries=3·이름) + 팩안 MANI 원문 추출(grep:
  `capabilities=` 좁은 셋 · `scriptfile=state/scripts/shipscratch.js`)
  → scratch 슬롯·팩 소각. **사용자 슬롯 접촉 금지**(probe-ws 격리 패턴).
- `jkctl install` 경로는 **아웃 오브 스코프** — 수신 기기 설치는 폰
  실기기 사용자 배포 몫(docs/76 §9 마이그레이션).

## 7. 아웃 오브 스코프

- **갤러리** — 별도 스펙(로컬 라이브러리만 대전제 유지, apps/*.jkx
  MODL 스캔 재용 선례 = engine/tools/jkctl/main.cpp:686). 출하 도구가
  이 스펙으로 완료된 뒤 시작.
- 서명·검수(docs/74 §2 B/C) — 나눔 확대 시 2단계.
- MANI 충돌 정책(동명 슬롯 수신) — §1-3 트레이드오크 원장만.
- 2KB 스키마 한도 — 불변(사용자 몫 정책).
- `jkctl install`·폰 게이트웨이 — 불변·건드리지 않음.

## 8. 마이그레이션

- 워크숍 템플릿 `kTemplateScript`(JKAppModule_script.cpp:100-112)에
  capabilities 선언 예시 심기 — **최종 리뷰 M-2(플랜 갭)의 추적 봉합**:
  이번 플랜의 태스크로 명시한다(갱신 31에서 "출하 도구 작업 때 같이"
  예약된 그것).
- 기존 슬롯(bang-gu 등)의 출하: 사용량 자동 분석이 `input`도 선언해
  준다 — bang-gu 출하 팩은 `input`이 선언돼 주입이 살아난다. 능력 게이트
  I-1 논쟁(벤치 MANI)과는 별도 선로 — 벤치는 벤치, 출하 팩은 각자.