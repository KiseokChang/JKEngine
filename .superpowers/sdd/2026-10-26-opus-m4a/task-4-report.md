# Task 4 리포트: opus/m4a probe 세그먼트+폰 캐논+as-built (#115 T4)

**status: DONE** — 커밋 4건(push완료 `95cb8c6..c4f8201 main -> main`).

## 커밋 SHA 목록

| 커밋 | 내용 | 파일 (numstat) |
| --- | --- | --- |
| `bfe20119078b1ff3576bda400ee7d0b2c51a0eb6` | test(probes): music probe opus/m4a 세그먼트+폰 캐논 계보 갱신 (T4) | wsl_music.sh 525/1·phone_music.sh 597/17 |
| `a43e4b05aebbd94ec11ab9ab2ad21f6b0e2c4996` | fix(probes): music WSL probe — OM cover B64 리터럴 절단 수리 (T4 fix r1) | wsl_music.sh 1/1 |
| `083997ca7820d9ba0015635544b775f11aa8847f` | fix(probes): music probe — OM 재독파서 cmt_htype 오프셋 수리+폰 계보 행 정산 (T4 fix r2) | wsl 3/3·phone 3/3 |
| `c4f8201d…` (c4f8201) | docs(apps): opus/m4a as-built (T4 — §7.12+§4.1-9 부기) | docs/90 +276 |

명시 경로만 스테이지·`git add -A` 0·amend 0·**커밋 분리 = probe 커밋(bfe2011
+fix 2건)+docs 커밋(c4f8201)**·IP grep 0건(4커밋 전부 — IPv4 정규식)·
서브에이전트 0건·worktree 미사용.

## 수형 원문 요약

1. **WSL probe 세그먼트(16f-OM — 16f-TW 뒤 신설·기존 세그먼트 원존)** —
   합성 m4a+opus(2x 픽스처 승계: m4a = ftyp+moov(udta/meta/hdlr('mdir')+
   ilst[©nam/©ART/©alb UTF-8+covr png 코드 14])+mdat 0xAB 64B·opus =
   [BOS OpusHead 19B][OpusTags(vendor 'om-vendor'+3필드+GENRE+MBP base64)]
   — lacing 255-런+OggCrc(0x04C11DB7) 규약 동형)→music_tag_write relay 쓰기
   4콜(m4a covr 교체+cover=null 유지·opus MBP 교체+cover 생략 유지)→재독
   원문(파일 수준 mus_omparse.py 독립 재파싱+목록 행 캡처 3종 — EYES)
   →covr/MBP md5 등호 round-trip 4콜=90155ae6…(16f-TW cover와 동일 바이트
   원존 — md5 축 공유)+벤더 유지+코멘트 페이지 EOS 0x04 유지(2x-⑫b/D10
   병행)+mdat 원문 무이동(0xab=64 — D2 라이브 원문)→**OPUSM4A-OK 2/2**(
   engine/tmp/wsl_music_run115_r{4,5}.log — post fix r2 계약 2런·canon
   833/2x=15 2런 등호·`#115-RUN-VALS` 1행+`#115-RUN-CAP`·REMNANT 0(17e)).
   런 1-3(r1·r2·r3) = probe 결함 2건의 honest 원문(함정 원장 — fix r1/r2
   소각 — 쓰기 4콜+round-trip 원문은 전부 성립).
2. **폰 배포 런 1회(t115_t4_deploy.sh — engine/tmp 산출물·커밋 0 — t112_t5
   수형 재용)**: 전수 git archive HEAD(SWEEP-DIFF 12/472→0·post DIFF=0)·
   ninja aarch64 rc=0 SECONDS=463(leg env 유지·WHISPER_ROOT 분기
   297 entries)·마커 2r=11·2s=21·2u=136·2v=20·**2w=4·2x=15**·MUSP-LEG-SYM
   alcOpenDevice=3(spatial leg WIRED 보존)·**조달기 스킵**(tiny-ko 상존 핀
   등호 77,691,730B — 재다운로드 불요 원장행)·**selftest A/B = PASS=833
   FAIL=0 2u=132·2r=10·2s=20·2v=14·2w=4·2x=15 — 2런 등호(CANON-A/B=833)**
   — **B측 로그 인쇄 결손 정직 부기(T4 fix r3)**: B측 원문행 =
   `PHONE-SELFTEST-B rc=0 PASS=833 FAIL=0 2v=14 2w=4 2x=15` — 드라이버가
   B행에는 2u/2r/2s 수취 인쇄를 두지 않았다(2u=132·2r=10·2s=20 = A측
   원문만) — "2런 등호"의 등호 축 = PASS=833/FAIL=0/2v/2w/2x 5항.
   ·music.jkx 재팕(PACK-RC=0·ICON-ENTRIES=2)·**WHISPER_ROOT+SPATIAL
   보유 부팅 승계**(구두/spatial leg 클라 상속 — 사용자 live 세션 leg
   유지 실측 — SPATIAL 종단 영수증 = 원장행 1행(런 중 /proc 실측만 —
   종단 행은 차기 폰 런 몫))·settings 213B/permissions 33B 무변경 등호·
   jkweb 상시 유지·Music 창 복원.
3. **폰 probe 동형 2런(N7 — phone_music.sh ·
   engine/tmp/phone_music_run115_r{1,2}.log)**:
   **`PHONE-SELFTEST rc=0 PASS=833 FAIL=0 2m=26 2i=19 2g=40 2n=17 2o=16
   2p=7 2q=7 2r=10 2s=20 2u=132 2v=14 2w=4 2x=15 — CANON-INCLUSION=
   MUSIC-FULL-2P2QRS-15 (폰 캐논 818→833 상승 — WSL 833 등호) — 2런 등호`**
   ·N7 = r1 `OPUSM4A-FAIL`(honest — probe 파서 cmt_htype off-by-one —
   fix r2(083997c)) → r2 **`OPUSM4A-PHONE-OK`**(합성 지시 쓰기 4콜+재독
   표기 round-trip md5 등호·htype=4·mdat 0xab=64+목록 행 캡처 3종·OM
   소각/REMNANT gone)·폰 probe 2런 중 편집 0.
4. **docs**: `docs/90_music_spatial_leg_asbuilt.md` §7.12 신설(§7.12.0
   체인·§7.12.1 배선 D1-D6(재생 비-목표 D6 원장·moov-후행 2차 read
   원장행·확장자/매agic 분담·C1 stco 패치)·§7.12.2 캐논 계보+probe 실측
   영수증·§7.12.3 함정 원장(T1/T2/T3+fix r1+T4 전량 수납)·§7.12.4
   비-목표+EYES 4건·§7.12.5 커밋/IP 게이트)+§4.1-9 park 행 7건(추가만).

## 캐논 원장(4축)

캐논 최종: **Win 856/WSL 833/posix 304(승계)/폰 833 — 2x 15 전부 흡수**.
- probe 상수 갱신 원존: CANON_WSL_SELFTEST 795→833(bfe2011)·폰 계보 행
  818→833(bfe2011+083997c 정산).
- 브리프 원문행 2건의 수치 정정 원장: ①"WSL 831·2x 12건" → fix r1(ce3f539)
  흡수 후 **833·2x 15 어설션(라벨 ①-⑫+⑫b 13건)** ②폰 "854 승계" → Win
  축 수치 표기로 판정(폰 = WSL 등호 계보 — **833**) — Win-WSL 23차 =
  플랫폼 어설션 계통 원장 대조 몫(docs/90 §7.12.2 원장행).
- 측정 중 공유 워킹 트리 경합 발각(T3 fix r1 진행 중 — 미커밋 +141/+320행)
  → hold(런 hold — moving target 측정 무영장) → fix r1 커밋(ce3f539) 후
  재개(조정자 지시 — 캐논 833 확정 후 1회 갱신+재런) — 협조 원장행.

## concerns (T4 잔여/유계)

1. **EYES 대기(사용자 선언만 결제)** ①Win/폰 실기기 .m4a/.opus 행 실값
   표기(스캔 열거 .opus/.m4b/.mp4 행 등장 승인) ②커버아트 체감(행 썸네일+
   [편집] round-trip) ③재생 밴드("-" 유지 — D6 원존) ④moov-후행 대형
   (비-faststart >1MB 머리 밖) .m4a 관찰 시 2차 read 재판정 소재 — 원장
   계보(moov-후행 = 쓰기 성립·재독 머리 한정 비대칭 원장행).
2. **폰 N4 ⑤ 무전달 hard 마커 재발(2런)** — 갱신 65 잔여 유계(XTEST 탭
   좌표 재보정 별소관) — 말단 rc=1 종결(INH_HARD 수형 원존)·본 라인
   정산(N6/N7/캔논/REMNANT)은 그 위에서 원문 성립 — 라인 잔여 원장행.
3. **t115_t4_deploy.sh verdict "bad substitution"**(CANON A/B 토큰 결합
   표기 — 드라이버 몫·remote 영수증 완전·판정 무영향) — pmus/finres 선례
   동류 원장행(라인 배포는 재런 불요).
4. **wsl.exe env 전파 함정 소각(원장행)** — MUS_*_RUN 라벨 = WSLENV
   "VAR/u" 접미 필요(r5부터 run=2 표기 성립 — r4는 run=1 표기 — 런
   대조는 로그 파일명 병용 — 무영향).
5. 폰 서브넷 이동 재발(구 후보 4건 MISS→현 Wi-Fi 서브넷 재정찰·주소
   미인쇄 계약 준수) — 갱신 65 유형 원장행.
6. **T3 재심사 유계 3건(fix r3 반영 — docs/90 §7.12.1/§7.12.3)**:
   (u1) 2x-⑫b(b) "파일 총량 등호" 명시 어셜션 부재(`bufB.size()==
   blobB.size()` — mdat 말미 box 지형 총량 오류 무음 통과 여지)(u2)
   covr-내 "stco" 우연 매agic = 보행 설계 서술로만 입증 — 전용 fixture
   미구축(u3) co64 leg 미실측(stco만 합성 — "co64 Δ 패치" = 대칭 2행
   구현·실측 부재 표기) — 차기 selftest 재판정 몫.