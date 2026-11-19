# 113 — Android 동영상 재생기 (2026-11-18)

데스크톱 셸의 "비디오" 아이콘은 Android에서 `VideoPlayerActivity`로 위임한다
(엔진 `jkapp_vplayer`는 FFmpeg 의존이라 NDK 빌드에 없음). 이 문서는 그 재생기의
계약과, 실측으로 뽑은 **디스플레이 선택 교훈**을 남긴다.

## 1. 표시 디스플레이 — "누른 화면"이 정본

**실측(2026-11-18)**: 이 폰의 디스플레이는 4개다.

| id | 이름 | 성격 |
|---|---|---|
| 0 | 폰 화면 | 내부 |
| 1 | 덮은 화면(폴드) | 내부, OFF |
| 2 | **HDMI(TV)** | EXTERNAL, `FLAG_PRESENTATION`, ON |
| 16 | **가상 Desktop** | `FLAG_DESKTOP_DISPLAY`, **화면에 안 보임** |

시행착오 3단:
1. "기본이 아닌 첫 화면" → **16(가상 Desktop)** 선택 → 영상이 아무도 안 보는
   화면에 그려지고 **소리만 남았다**(사용자: "앱은 안 뜨고 소리만 남").
2. `DISPLAY_CATEGORY_PRESENTATION` 우선 → 이 기기에서는 **역시 16이 먼저** 왔다.
3. **정본 = 셸 액티비티의 디스플레이**(`getWindowManager().getDefaultDisplay()`)
   → 사용자가 누른 그 화면에 뜬다. 폴백 = PRESENTATION → 기본.

> 교훈: "외부 화면"을 이름/순서로 추론하지 말고 **사용자가 보고 있는 표면**을
> 묻는다. 가상 디스플레이는 외부처럼 보이지만 시청자에게 도달하지 않는다.

## 2. 수명 — 짧은 클립이 앱을 죽인다

테스트 클립 `jk_test.mp4` = **6.0초**. 종전 `onCompletion → finish()` 때문에
재생 직후 창이 사라져 "앱이 안 뜬다"로 보였다. 수형: 완료 시 **창 유지** +
"재생이 끝났습니다 / 화면을 누르면 다시 재생" 안내.

## 3. 조작 UI (2026-11-18 "재생화면만 있네요" 반영)

- 상단 바: **파일명** · [목록] · [닫기]
- 하단 바: **[일시정지/재생]** · 진행 바(드래그 탐색) · `00:03 / 00:06`
- 0.5초 틱으로 진행 갱신(드래그 중에는 덮어쓰지 않음)
- 화면 탭 = 컨트롤 토글 / 완료 후 탭 = 처음부터 재생
- **목록** = ① "다른 파일 선택…"(`ACTION_OPEN_DOCUMENT`, `video/*` — 시스템
  문서 선택기, `content://` 그대로 재생) + ② Movies/Download/DCIM 최신순
- 글리프(⏸▶✕) 대신 **한글 라벨** — 폰트 미탑재 시 두부(□) 방지

## 4. 구두 진입 (2026-11-20)

폰 실측: *"비디오 플레이어 켜줘"* → `unknown_app: "플레이어"`.
- `ChatRouter` 별명 3종: **비디오 · 플레이어 · 영상 → vplayer** (셀프테스트 1n-v1/v2)
- `launch_app` 안드로이드 분기: `vplayer`/`video` = `JkPlayVideo("")` 위임 →
  `{"ok":true,"via":"platform"}` (종전 unknown_app).
  **주의**: `HandleAgentQuery`는 함수 말미에 저널 append + 회신 송신이 있으므로
  조기 `return` 금지 — `platformHandled` 분기로 스폰만 건너뛴다.

## 5. 이어보기 · 배속 (2026-11-20)

- **이어보기**: 영상별 마지막 위치를 SharedPreferences(`video`)에 기억.
  3초 미만/끝-5초 이내는 저장하지 않고, 끝까지 보면 키를 삭제. 전환·일시정지·
  onPause에서 저장, onPrepared에서 `seekTo`.
- **배속**: 하단 바 버튼 = 1.0x → 1.25x → 1.5x → 2.0x 순환
  (`MediaPlayer.setPlaybackParams`, API 23+; 실패는 경고 로그로 열화).

## 6. 남은 일

- 자막(외부 .srt/.vtt, `addTimedTextSource`), 볼륨 제스처
- 엔진 vplayer(FFmpeg) 탑재 시 이 액티비티로 대체
