# music 스캔 안전화 — 취소 가능 스캔+폴더 제거/종료 경계 (#93) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** 스캔 중 폴더 제거(dirs/외부 삭제)·앱 종료가 안전 — 스캔 취소 기구+도착 필터+파괴 빠른 join.

**Architecture:** `ListAudioFiles(root, cancel)` 취소 콜백 오버로드(디렉터리 경계마다 체크 — 바운드)·소멸자 cancel→join(블록 상한 해소 — T2 park C1 정식 소각)·[제거] → 취소+재스캔+도착 멤버십 필터(제거된 루트 결과 폐기).

**Spec:** docs/superpowers/specs/2026-10-10-music-spatial-leg-design.md 계약 승계+본 플랜 결정 원장.

## Global Constraints

- **신설 결정(컨트롤러 재량·기각시 수리):** ①취소 체크 단위=디렉터리 경계(엔트리 256개마다 보조 체크 — 바운드) ②소멸자=cancel 요청→join(무한 대기 소각 — 대체로 ms급) ③[제거]성공=취소+재스캔 1발 ④도착 결과가 제거된 루트 소속이면 무음 폐기+stderr 진단 1행(정직 — UI는 무변화) ⑤외부 삭제(스캔 중 디렉터리 소멸)=ec 중립 유지(이미 계약)+selftest 1건.
- idle 계약(#89)·CP949/T2 원존(MusicDirStore·도구 6종) 무변조·P1 수형(행 사본) 무손상.
- 캐논(현 Win 647/WSL 624/posix 296/폰 624) — 신설 `2p` 계보 표기.
- 커밋 계약 전부(명시 경로·IP 0건·push·amend 금지). 빌드 ninja 표준·WSL 내부 리다이렉트. 서브에이전트 스폰 금지.

---

### Task 1: ListAudioFiles 취소 오버로드(순수)+2p

**Files:**
- Modify: `engine/include/apps/MusicModel.h` — `using ScanCancelFn = std::function<bool()>;`(참=취소)·`ScanAudioTree(..., const ScanCancelFn& cancel)` 폭탄 전달(재귀 경계마다 `if (cancel()) return;` — 엔트리 256개마다 보조 체크)·`std::vector<Track> ListAudioFiles(const std::string& root, const ScanCancelFn& cancel);` — 기존 무인자 오버로드는 항상-false 콜백 위임(원존 무변조).
- Test: `engine/src/main.cpp` **2p** — ①cancel 사전 설정=즉시 빈 반환(트리 구성·호출 1회 원문 단정) ②카운터 콜백(디렉터리 경계 K회 후 참)=빈 반환+호출 횟수 상한 단정 ③루트가 스캔 시작 전 사라짐 → 빈 반환(ec 중립·crash 0) ④기존 무인자 오버로드 계보 불변(기존 트리 원문 수치) — 2p ~6건.
- [ ] **Step 1:** 2p 원문/2. 구현/3. 빌드+selftest 계보(Win 647→+n·WSL 624→+n·posix 296 사유)/4. 커밋 `feat(apps): music 스캔 취소 기구 — 디렉터리 경계 콜백 (T1)`

### Task 2: 앱 배선 — 소멸자 취소 join+[제거] 취소+도착 필터

**Files:**
- Modify: `engine/src/apps/ClientMusicApp.cpp`(+h) — ①소멸자: join 전 cancel 요청(atomic) — 진행 중 스캔이 경계서 끊김(빠른 join) ②[제거] 성공 경로(ManageRemove): 진행 중 스캔 취소 1발+재스캔(기존) — P1 수형(행 사본) 무손상 ③**도착 필터:** 도착 결과의 루트가 현재 userDirs(또는 dirs 셋)에 없으면 폐기+stderr 진단 1행 — 우편함 스냅샷 root 동봉 ④외부 삭제 시나리오 = ec 유지(원장 주석 1행).
- [ ] **Step 1:** 배선/2. 빌드 3축 등호(신설 어설션 없음 — T1 소유)/3. 커밋 `feat(apps): music 스캔 취소 배선 — 종료·제거 경계 안전화 (T2)`

### Task 3: WSL probe — 종료/제거 경계 실측 세그먼트

**Files:**
- Modify: `engine/tools/probes/wsl_music.sh` — 세그먼트 신설: ①대형 합성 트리($TMPDIR·하위 40개×파일 30개) 등록 → **스캔 진행 중 창 close**(서버 close 창 도구) → 서버 로그 원문(MUSIC-FAIL 0·클라 사망 원문 0·재스폰 후 도구 응답 원문) ②스캔 진행 중 `music_dir_remove`(해당 폴더) → 클라 생존+도착 폐기 원문(stderr 1행) ③캐논 등호 원문.
- [ ] **Step 1:** probe 확장+런 2회(재현 인증)/2. 커밋 `test(probes): music WSL probe — 스캔 취소 경계 실측 (T3)`

### Task 4: 폰 probe — 동형 최소 세그먼트+캐논 흡수

**Files:**
- Modify: `engine/tools/probes/phone_music.sh` — 폰 동형 최소(스캔 중 remove+생존 원문)+폰 selftest 캐논(624→+2p n)·probe CANON 상수 원장 승계.
- [ ] **Step 1:** probe 확장+폰 실측/2. 커밋 `test(probes): music 폰 probe — 스캔 취소 동형 세그먼트 (T4)`

### Task 5: docs/90 §7.7 갱신

- [ ] **Step 1:** §7.7 신설(스캔 안전화 원장 — 체인/함정/캐논/park/EYES)+§4.1 불변/2. 커밋 `docs(apps): music 스캔 안전화 as-built 갱신 (T5)`

## 사용자 게이트 요약

1. 실기기 체감(스캔 중 종료/제거 스무스함) = 사용자 선언(EYES).
2. 결정 원장 5건 = 재량.