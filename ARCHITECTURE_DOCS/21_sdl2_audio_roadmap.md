# SDL2 JKWindow 오디오 시스템 로드맵

> `prototype/sdl2_jkwindow` 위에 오디오 서브시스템을 단계적으로 도입하여, 단순 UI 피드백 재생부터 멀티프로세스 오디오 컴포지터, 나아가 DAW 개발 인프라까지 확장하는 장기 계획입니다.
>
> 현재 JKENGINE/SDL2 프로토타입은 Windows 기반 단일 프로세스/단일 스레드 메인 루프입니다. 이 로드맵은 메인 루프 구조를 해치지 않으면서도, 향후 멀티프로세스 및 DAW 확장에 대비한 믹서 뼈대를 단계적으로 쌓는 것을 목표로 합니다.

---

## 1. 목표와 범위

### 목표

- JKWindow 프레임워크에 게임/UI용 사운드 재생 기능 추가.
- SDL2 기반 자체 오디오 믹서를 구축하여 UI 스레드와 오디오 콜백 스레드의 완전한 격리.
- 향후 런처 ↔ 게임 프로세스 분리를 위한 Windows IPC 오디오 컴포지터 뼈대 확보.
- 최종적으로 JKWindow 위에 DAW/가상악기/플러그인 호스트 개발이 가능한 인프라 구축.

### 범위

- **In scope**: `prototype/sdl2_jkwindow/` 내부의 오디오 서브시스템 설계 및 구현.
- **Out of scope**: 원본 DOS JKENGINE 오디오 코드 복원, 웹 포팅용 Web Audio 구현, 상용 DAW의 완전한 기능 재현.

---

## 2. 설계 철학

| 원칙 | 설명 |
|------|------|
| **단계적 확장** | Phase 1에서 상용 라이브러리(SDL_mixer)로 빠르게 가치를 입증하고, 이후 단계에서 점진적으로 자체 엔진으로 교체합니다. |
| **스레드 격리** | 메인 UI 루프와 오디오 콜백 스레드 사이에는 뮤텍스를 사용하지 않습니다. SPSC 락프리 큐와 `std::atomic` 상태만으로 통신합니다. |
| **프로세스 격리 대비** | Phase 2에서 설계한 큐/명령 구조는 Phase 3에서 Windows 공유 메모리 기반 IPC로 그대로 마이그레이션 가능합니다. |
| **버스 기반 라우팅** | SFX, BGM, UI, 시스템 알림 등을 독립적인 오디오 버스로 분리하여 볼륨/우선순위/라우팅 정책을 개별 적용합니다. |
| **오디오 포커스** | 타이젠/안드로이드의 오디오 포커스 개념을 프레임워크 내부에 경량화하여, 시스템 알림이 게임 SFX를 선점할 수 있도록 합니다. |

---

## 3. 현재 상태 요약

| Phase | 이름 | 상태 | 핵심 산출물 |
|-------|------|------|-------------|
| 1 | SDL_mixer 기반 단순 재생 | ⏳ 미시작 | `JKSoundManager` + WAV/OGG 재생 |
| 2 | 자체 커스텀 믹서 | ⏳ 미시작 | `JKAudioMixer`, 락프리 큐, 더킹, 보이스 스틸링 |
| 3 | Windows IPC 멀티프로세스 컴포지터 | ⏳ 미시작 | `JKAudioServer`, 공유 메모리 큐, 프로세스당 오디오 채널 |
| 4 | DAW 개발 인프라 | ⏳ 미시작 | WASAPI/ASIO 백엔드, 오디오 그래프(DAG), CLAP 호스트 |

---

## 4. Phase 상세

---

### Phase 1: SDL_mixer 기반 단순 재생

#### 목표

런처 버튼 클릭음, 지뢰찾기/테트리스 효과음(SFX)과 배경음악(BGM)을 최소한의 공수로 재생합니다. 복잡한 동기화 없이 OS 백그라운드 오디오 스레드에 위임합니다.

#### 추가 파일

```
prototype/sdl2_jkwindow/
├── include/
│   └── JKSoundManager.h
├── src/
│   └── JKSoundManager.cpp
└── assets/sounds/
    ├── mine_click.wav
    ├── mine_flag.wav
    ├── mine_explosion.wav
    ├── tetris_drop.wav
    ├── tetris_clear.wav
    ├── tetris_gameover.wav
    └── tetris_theme.ogg
```

#### 클래스 설계

```cpp
namespace jk {

class JKSoundManager {
public:
    static JKSoundManager& GetInstance();

    bool Init();
    void Quit();

    bool LoadSFX(const std::string& id, const std::string& filepath);
    bool LoadBGM(const std::string& id, const std::string& filepath);

    void PlaySFX(const std::string& id, const std::string& busId = "UI", int loops = 0);
    void PlayBGM(const std::string& id, int loops = -1);

    void SetMasterVolume(float volume);              // 0.0 ~ 1.0
    void SetBusVolume(const std::string& busId, float volume);
    void SetBGMVolume(float volume);

private:
    JKSoundManager() = default;

    std::unordered_map<std::string, Mix_Chunk*> sfxCache_;
    std::unordered_map<std::string, Mix_Music*> bgmCache_;
    std::unordered_map<std::string, float> busVolumes_;

    float masterVolume_ = 1.0f;
    float bgmVolume_ = 1.0f;
};

}
```

#### 핵심 정책

| 항목 | 값 | 근거 |
|------|-----|------|
| 샘플레이트 | 44100Hz | 표준 CD 음질, 리샘플링 부하 최소화 |
| 포맷 | `MIX_DEFAULT_FORMAT` | SDL_mixer 기본 설정 |
| 채널 수 | 32 | `Mix_AllocateChannels(32)` |
| 버퍼 크기 | 512 또는 1024 | 512는 반응성 우수, 1024는 안정성 우수 |
| SFX 포맷 | 16-bit PCM WAV | 앞 짤림 방지, 메모리 캐싱 |
| BGM 포맷 | OGG | 스트리밍, 용량 절약 |

#### 볼륨 계산

```cpp
int finalVolume = static_cast<int>(masterVolume_ * busVolume * MIX_MAX_VOLUME);
```

#### JKWindow 통합

```cpp
// main.cpp 또는 JKApplication::Init()
auto& audio = jk::JKSoundManager::GetInstance();
audio.Init();

auto& config = jk::JKConfigManager::GetInstance();
audio.SetMasterVolume(config.GetFloat("MasterVolume", 1.0f));
audio.SetBusVolume("Tetris", config.GetFloat("TetrisVolume", 1.0f));
audio.SetBusVolume("Mine", config.GetFloat("MineVolume", 1.0f));
audio.SetBGMVolume(config.GetFloat("BGMVolume", 1.0f));
```

#### 검증 기준

- [ ] `CMakeLists.txt`에 `SDL2_mixer` 링크 추가 후 빌드 성공.
- [ ] 런처 버튼 클릭 시 클릭음 재생.
- [ ] 지뢰찾기 칸 클릭/폭발 시 효과음 재생.
- [ ] 테트리스 블록 이동/라인 클리어/게임오버 시 효과음 재생.
- [ ] 테트리스 게임 시작 시 BGM 재생.
- [ ] 설정 파일에서 마스터/버스/BGM 볼륨이 다음 실행 시 유지.

---

### Phase 2: 자체 커스텀 믹서

#### 목표

SDL_mixer를 걷어내고, 부동소수점(AUDIO_F32SYS) 기반 자체 믹서를 구현합니다. UI 스레드와 오디오 콜백 스레드를 락프리 큐로 연결하고, 더킹/보이스 스틸링/디바운싱 등 고급 믹싱 정책을 프레임워크 수준에서 제어합니다.

#### 추가 파일

```
prototype/sdl2_jkwindow/
├── include/jkaudio/
│   ├── JKAudioMixer.h
│   ├── JKAudioSource.h
│   ├── JKAudioBuffer.h
│   ├── JKAudioCommand.h
│   ├── LockFreeQueue.h
│   └── JKAudioPriority.h
├── src/jkaudio/
│   ├── JKAudioMixer.cpp
│   ├── JKAudioSource.cpp
│   └── JKAudioBuffer.cpp
└── include/
    └── JKSoundManager.h      // SDL_mixer 의존 제거, JKAudioMixer 기반으로 재작성
```

#### 핵심 구조

```cpp
enum class JKAudioPriority {
    Background = 0,   // BGM, 앰비언트
    GameSFX = 1,      // 게임 효과음
    SystemAlert = 2,  // 배터리 부족, 치명적 에러
};

struct JKAudioCommand {
    uint32_t assetId;
    JKAudioPriority priority;
    float volume;
    bool triggerDucking;
    uint32_t duckingDurationMs;
};

class JKAudioMixer {
public:
    bool Init();
    void Quit();
    void Play(const JKAudioCommand& cmd);
    void SetMasterVolume(float volume);
    void Update(float deltaTime);  // 메인 루프에서 매 프레임 호출

private:
    LockFreeQueue<JKAudioCommand, 256> commandQueue_; // UI -> Audio
    std::vector<JKAudioSource> activeSources_;        // Audio Thread Only

    float masterVolume_ = 1.0f;
    float bgmVolume_ = 1.0f;
    float duckingMultiplier_ = 1.0f;

    static void SDLAudioCallback(void* userdata, Uint8* stream, int len);
};
```

#### 믹싱 파이프라인

1. **명령 수집**: 오디오 콜백 시작 시 `commandQueue_.Pop()`으로 모든 대기 명령을 꺼냄.
2. **소스 믹싱**: 각 `JKAudioSource`로부터 PCM 샘플을 읽어 출력 버퍼에 볼륨 곱 후 합산.
3. **클리핑 방지**: 출력을 `-1.0f ~ 1.0f` 범위로 제한.
4. **BGM 더킹**: 메인 루프의 `Update()`에서 BGM 볼륨에 `duckingMultiplier_` 적용.

#### 락프리 큐

```cpp
template <typename T, size_t Size>
class LockFreeQueue {
    std::array<T, Size> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};

public:
    bool Push(const T& item);
    bool Pop(T& item);
};
```

#### 정책 구현

| 기능 | 설명 |
|------|------|
| 디바운싱 | 동일 사운드는 50ms 이내 재생 무시. |
| 인스턴스 제한 | 동일 사운드는 최대 3개까지만 동시 재생. |
| 보이스 스틸링 | 채널이 꽉 찼을 때 우선순위가 낮은 채널을 `Mix_HaltChannel`로 강제 중지. |
| 오디오 더킹 | `SystemAlert` → BGM 10%, `GameSFX` → BGM 30%, 부드러운 Lerp 전환. |

#### 검증 기준

- [ ] SDL_mixer 완전 제거 후에도 사운드 재생 동작.
- [ ] 창 드래그 중에도 소리 끊김 없음.
- [ ] 10개 이상의 폭발음이 동시에 요청되어도 클리핑/샷건 노이즈 없음.
- [ ] 시스템 알림 발생 시 BGM이 즉시 줄어듦.

---

### Phase 3: Windows IPC 멀티프로세스 오디오 컴포지터

#### 목표

런처(부모 프로세스)가 중앙 오디오 서버가 되고, 테트리스/지뢰찾기(자식 프로세스)가 공유 메모리를 통해 오디오 명령을 전달하는 구조를 구현합니다. Phase 2의 락프리 큐/명령 구조를 공유 메모리에 올립니다.

#### 추가 파일

```
prototype/sdl2_jkwindow/
├── include/jkaudio/
│   ├── JKIpcAudioTransport.h
│   ├── SharedAudioQueue.h
│   └── JKAudioServer.h
└── src/jkaudio/
    ├── JKIpcAudioTransport.cpp
    ├── SharedAudioQueue.cpp
    └── JKAudioServer.cpp
```

#### 공유 메모리 큐 구조

```cpp
struct IpcAudioCommand {
    uint32_t assetId;
    float volume;
    bool loop;
    JKAudioPriority priority;
};

struct SharedAudioQueue {
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    IpcAudioCommand buffer[256];
};
```

#### Windows IPC 구현

| 항목 | API |
|------|-----|
| 공유 메모리 생성 | `CreateFileMapping`, `MapViewOfFile` |
| 자식 프로세스 연결 | `OpenFileMapping`, `MapViewOfFile` |
| 동기화 | 뮤텍스 없이 `std::atomic` + Placement New 기반 락프리 큐 |

#### 아키텍처

```
[Launcher Process]
  └── JKAudioServer
       ├── SharedMemory: TetrisQueue
       ├── SharedMemory: MineQueue
       └── SDL Audio Callback -> 믹싱 -> 스피커

[Tetris Process]              [Mine Process]
  └── Push IpcAudioCommand      └── Push IpcAudioCommand
       to TetrisQueue                to MineQueue
```

#### BGM 스트리밍 정책

- 대용량 BGM은 자식이 파일 데이터를 복사하지 않음.
- 자식은 `"TETRIS_THEME"` 같은 에셋 ID만 전송.
- 런처(오디오 서버)가 에셋을 직접 디스크에서 스트리밍 디코딩.

#### 검증 기준

- [ ] `jkproto_sdl2_jkwindow.exe --child=tetris` 형태로 자식 프로세스 실행.
- [ ] 자식에서 사운드 재생 요청 시 런처에서 실제로 소리 출력.
- [ ] 런처 창을 닫으면 자식 프로세스와 공유 메모리가 정리됨.

---

### Phase 4: DAW 개발 인프라

#### 목표

JKWindow 위에서 전문적인 오디오 작업(가상 악기, 이펙터, MIDI, 시퀀서)이 가능하도록 인프라를 구축합니다. 이 단계는 게임용 믹서를 넘어 DAW 핵심 기술로 확장합니다.

#### 추가 파일

```
prototype/sdl2_jkwindow/
├── include/jkaudio/
│   ├── JKAudioDevice.h         // WASAPI/ASIO 추상화
│   ├── JKAudioGraph.h          // DAG 기반 오디오 그래프
│   ├── JKAudioNode.h           // 노드 베이스
│   ├── JKAudioPluginHost.h     // CLAP/VST3 호스트
│   ├── JKMidiInput.h           // MIDI 입력
│   └── JKAudioClock.h          // A/V 동기화 클럭
└── src/jkaudio/
    ├── JKAudioDevice.cpp
    ├── JKAudioGraph.cpp
    ├── JKAudioPluginHost.cpp
    └── JKMidiInput.cpp
```

#### 핵심 컴포넌트

| 컴포넌트 | 역할 |
|----------|------|
| `JKAudioDevice` | SDL2 기본 드라이버 외에 WASAPI Exclusive/ASIO로 저지연 출력. |
| `JKAudioGraph` | 트랙/이펙터/버스를 노드 그래프로 연결. 위상 정렬 후 1차원 렌더 시퀀스 컴파일. |
| `JKAudioNode` | 입력/출력 버스, `processBlock()` 인터페이스. |
| `JKAudioPluginHost` | CLAP(C ABI) 플러그인 로드 및 호스팅. VST3은 Phase 4 후반 옵션. |
| `JKMidiInput` | Windows MIDI API 연동, 가상 악기 트리거. |

#### 하드 리얼타임 제약

- 오디오 콜백 내부에서 `new`/`malloc`/`std::mutex` 사용 금지.
- 모든 메모리는 UI 스레드에서 사전 할당.
- 그래프 변경은 UI 스레드에서 새 시퀀스를 컴파일한 뒤 `std::atomic` 포인터 교체(RCU).

#### 캐시 친화적 버퍼 관리

| 기법 | 설명 |
|------|------|
| 인플레이스 처리 | EQ, 게인 등은 입력 버퍼에 직접 덮어씀. |
| 캐시 라인 정렬 | `alignas(64)` 버퍼 사용. |
| LIFO 스크래치 풀 | 락프리 스택으로 최근에 쓴 버퍼를 우선 재사용. |
| False Sharing 방지 | 버퍼 끝에 패딩을 두어 캐시 라인 격리. |

#### 검증 기준

- [ ] WASAPI Exclusive 모드에서 5ms 이하 지연 시간 측정.
- [ ] 간단한 오디오 그래프(사인파 생성 → 게인 → 마스터)에서 소리 출력.
- [ ] CLAP 플러그인 하나를 JKWindow 창 내부에 로드하고 소리 출력.

---

## 5. 파일 및 빌드 변경 예정

### Phase 1 변경

```cmake
# prototype/sdl2_jkwindow/CMakeLists.txt
target_link_libraries(jkproto_sdl2_jkwindow PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    SDL2_mixer::SDL2_mixer   # 추가
)
```

### Phase 2~4 구조

```
prototype/sdl2_jkwindow/
├── include/
│   ├── JKSoundManager.h          # Phase 1~2 (싱글톤 API 유지)
│   └── jkaudio/
│       ├── JKAudioMixer.h        # Phase 2
│       ├── JKAudioSource.h
│       ├── JKAudioBuffer.h
│       ├── JKAudioCommand.h
│       ├── LockFreeQueue.h
│       ├── JKAudioPriority.h
│       ├── JKIpcAudioTransport.h # Phase 3
│       ├── SharedAudioQueue.h
│       ├── JKAudioServer.h
│       ├── JKAudioDevice.h       # Phase 4
│       ├── JKAudioGraph.h
│       ├── JKAudioNode.h
│       ├── JKAudioPluginHost.h
│       └── JKMidiInput.h
├── src/
│   ├── JKSoundManager.cpp
│   └── jkaudio/
│       ├── JKAudioMixer.cpp
│       ├── JKAudioSource.cpp
│       ├── JKAudioBuffer.cpp
│       ├── JKIpcAudioTransport.cpp
│       ├── SharedAudioQueue.cpp
│       ├── JKAudioServer.cpp
│       ├── JKAudioDevice.cpp
│       ├── JKAudioGraph.cpp
│       ├── JKAudioPluginHost.cpp
│       └── JKMidiInput.cpp
└── assets/sounds/
    └── ...
```

---

## 6. 다음 즉시 행동

1. `prototype/sdl2_jkwindow/CMakeLists.txt`에 `SDL2_mixer` 의존성 추가.
2. `include/JKSoundManager.h` 및 `src/JKSoundManager.cpp` 작성 (Phase 1).
3. `JKApplication::Init()` / `Quit()`에 `JKSoundManager` 연동.
4. 지뢰찾기/테트리스 앱의 이벤트 핸들러에 효과음 트리거 포인트 삽입.
5. `assets/sounds/` 디렉터리 생성 및 기본 WAV/OGG 리소스 준비.

---

## 7. 참고 문서

- `12_sdl2_prototype_roadmap.md` — SDL2 JKWindow 프로토타입 단계
- `13_sdl2_top_level_apps.md` — 최상위 앱 후보 및 우선순위
- `20_sdl2_jango_porting_plan.md` — 원본 비즈니스 앱 포팅 현황
- 외부 참고: SDL_mixer, JUCE, Tracktion Engine, CLAP, PipeWire
