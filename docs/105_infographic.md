# 105a — 사용 환경·시나리오 인포그래픽 (2026-11-08)

본체 원장 = docs/105_usage_environment_scenario.md. 시각 요약 — 본체 원장의
시각 부록(mermaid — GitHub/뷰어 렌더 전제).

## 1. 물리 토폴로지

```mermaid
flowchart LR
    subgraph ROOM["🛋 거실 룸 (현장 — 폰 단독 폐곡)"]
        TV["🖥 TV (RTK 1920x1080)"]
        PH["📱 폰 Galaxy Fold<br/>(컴퓨터 본체)"]
        TP["🖱 트랙패드"]
        TV -- "DeX 도킹<br/>(표면 = logical 2 가상 Desktop)" --> PH
    end
    subgraph DEV["🔧 개발실 (현장 밖)"]
        PC["💻 PC<br/>빌드·ollama·adb"]
    end
    PH <-..-> |"무선 adb — 연결 시에만<br/>배포·회수·재페어(재발급)"| PC
```

## 2. 시나리오 A — 거실 구두 제어 (사이클)

```mermaid
flowchart TB
    R0["죽었을 때: TV에 DeX 환이 남는다<br/>(표면 2 상재 — 위계 원존)"] --> R1
    R1["⏺ 복구 = DeX 홈 JK 아이콘 한 탭<br/>부팅은 TV 한 탭"] --> RUN
    RUN["🖥 jkdesktop 풀스크린 데스크탑"] --> MIC
    MIC["🎤 폰 구두 (콘트롤 앱 MIC)"] --> STT["STT (삼성 인식기)<br/>— 런타임 마이크 권한"]
    STT --> TALK["POST :8091/talk<br/>(같은 폰 루프백 — 앱 내 jkweb)"]
    TALK --> R
    subgraph R["라우터 3단"]
        R1X["① exact 즉발 어휘<br/>개봉/닫기/정리 — LLM 0"] --> R2X
        R2X["② 패턴 열<br/>앱어·별명·조사 흡수"] --> R3X
        R3X["③ LLM 턴"]
    end
    R3X --> EXE["행동 실행<br/>앱 개봉/닫기·창·재생·하루 정리"]
```

## 3. 시나리오 B/C — 폰 단독 + 개발 사이클 (사용 기록 폐곡 루프)

```mermaid
flowchart LR
    subgraph FIELD["현장 A/B 무인 사용"]
        USE["세션 턴 기록 · use_journal<br/>bridge_report_*.txt · srv/crash 원문"]
    end
    subgraph LINK["폰↔PC 연결 (업데이트 시)"]
        HAR["회수 (reports 흐름 —<br/>결함 = 라이브 진단+패치)"]
        BUL["PC 빌드 → install -r"]
    end
    FIELD -- "연결 순간" --> HAR -- "패치·기능 소재" --> BUL -- 재배포 --> FIELD
```

## 4. 자연어 갭 — 턴 축별 현장 동작

```mermaid
flowchart LR
    IN["자유 발화"] --> R
    R["① exact ✔ · ② 패턴 ✔"] -- "무PC 동작" --> OK["✅ 현장 OK"]
    R --> LLM{"③ LLM 턴"}
    LLM -- "PC 릴레이 (ollama :11434)" --> PCGATE["❌ 현장 불가 (PC 부재)"]
    LLM -- "cloud 턴 (API 키 — 대기)" --> OKC["✅ 유일 현장 경로 —<br/>API 키 = 결제 1순위"]
```
