# 사용→리뷰→OS 업데이트 사이클 지도 (윤곽 확정 전 공개 초안 — 2026-10-25)

> 3축 지도 + 사이클 흐름. mermaid — GitHub/호환 뷰어에서 렌더. 원장: 대화 2026-10-25
> (사용자 지시 "사용 내용을 그대로 가져와서 리뷰를 거쳐 OS를 업데이트하는 사이클").

## 1. 사이클 한 바퀴 (목표형 — 세션당/당일 단위)

```mermaid
flowchart LR
    subgraph USE["① 사용 (폰·Win)"]
        U1["터치·명령·재생"]
        U2["체감 선언<br/>(EYES 게이트)"]
        U3["오류 자동 제출<br/>bridge_report_*.txt"]
    end
    subgraph HARVEST["② 수확 (자동)"]
        H1["/reports 수확<br/>(이미 있음)"]
        H2["events_list·receipts<br/>(도구 존재)"]
        H3["세션 개시 자동 수확<br/>(★신설 후보)"]
    end
    subgraph REVIEW["③ 리뷰·분류"]
        R1{"에이전트 분류<br/>결함 / 개선 / 체감"}
        R2["원장 반영<br/>(docs·메모리)"]
    end
    subgraph UPDATE["④ OS 업데이트"]
        P1["결함 = 라이브 진단<br/>+패치 루프"]
        P2["개선 = 백로그<br/>+park 배치(일괄 소각)"]
    end
    subgraph DEPLOY["⑤ 배포+재실측"]
        D1["드라이버 1줄 증분<br/>(폰 자동화 완료)"]
        D2["캐논 등호<br/>+probe 원문"]
    end
    U1 & U2 & U3 --> H1 & H2
    H1 & H2 & H3 --> R1
    R1 --> R2 --> P1 & P2
    P1 & P2 --> D1 --> D2 --> USE
```

## 2. 빠진 조각 3개 (진짜 공사 부분 — ①②③ 실장 완료 #113 T1-T3, 2026-10-26)

```mermaid
flowchart LR
    G1["#113-1 ✔ 실장 T1<br/>사용 행위 구조화 로그<br/>(서버 도구 1콜 1행 저널<br/>state/use_journal.jsonl)"]
    G2["#113-2 ✔ 실장 T2<br/>수확 probe jkharvest.sh<br/>(수확 원문+분류 요약 1회 —<br/>실행 주체 = 세션/D6)"]
    G3["#113-3 ✔ 실장 T3<br/>EYES 결제 행 규약<br/>EYES-항목: PASS|FAIL|PENDING<br/>(docs/90 §5.1)"]
    G1 & G2 & G3 --> C["빠른 사이클<br/>(세션당 한 바퀴)"]
```

실장 부기(2026-10-26 — 1-3행 원칙):
- **#113-1 = T1(1e52392)** — `JKWindowServer::HandleAgentQuery` 도구 사슬 말미
  훅 1곳(DRY)·append-only JSONL·쓰기 실패 = 조용히 스킵(도구 무영향 — D1/D5).
- **#113-2 = T2(2fa0224·fix r2 5fce208)** — 수확 probe `jkharvest.sh`: 저널
  파일 직독+bridge_report 목록+분류 요약+`HARVEST-VERDICT` 1행 — Win/WSL/폰
  공통(폰 = OBSERVE 읽기 접점만)·**세션 개시의 "자동" 실행도 D6에 따라 세션(
  Claude)이 수행** — v1 = 수확+분류만 자동, 조치(배치)는 세션 수행 유지.
- **#113-3 = T3(본 문서+docs/90 §5.1)** — 결제 행 형식 규약·사용자 선언만
  결제 원칙 불변(형식만 규약 — 도구/선언 구분은 §5.1 표 3치 PASS|FAIL|PENDING).

## 3. 라인 순서 (방향 확정 — 2026-10-25)

```mermaid
flowchart LR
    A["#111 태그 쓰기<br/>T6-T7+최종 리뷰<br/>(진행 중 — 자동)"] --> B["#112 STT 구두<br/>whisper.cpp 로컬<br/>조작+대화 — 설계 조사 중"]
    B --> C["#113 사용→리뷰→<br/>OS 업데이트 루프<br/>빈 조각 3개 집중"]
    C --> D["폰 체감 게이트<br/>spatial 4건+편집 패널<br/>(사용자 선언 몫)"]
    D -.-> A
```

- **A축(앱 폴리싱)=최소 증분** — deferred minor는 batch park 겸행 소각.
- **C축(STT 구두+사이클)=주축** — 사용자 그림(DeX+구두 OS) 수렴.
- 좌표 재보정·폰 X 서계 등 원장행 = 사이클 ④단계(업데이트)의 결함 후보급으로 순환 소각.