#!/usr/bin/env bash
# install_stt_models.sh — STT 구두(#112) ggml 모델 조달기 (T1 — 스펙
# 2026-10-25-stt-voice-design §1.3 표·§4 판정·R4). install_lf_helix_posix.sh
# 선형(멱등 조달기 — "기계만 커밋" 원칙 docs/44·81·84: 모델 파일·whisper-cli
# 바이너리는 커밋 금지, 본 스크립트가 그 기계). 산출물은 무추적 dir로 내려놓음:
#   Win(Git Bash/MSYS)     → <engine>/build/stt-models      (small 466 MB)
#   WSL                    → <engine>/buildwsl/stt-models   (small 466 MB)
#   폰(Termux/Termux:API)  → <engine>/buildterm/stt-models  (tiny-ko 74 MB)
# 축별 모델 핀 = 스펙 §1.3·R4(2026-10-25 확정): 폰(komixv2 tiny-ko·CER ~11.6%)
# /Win·WSL(ggml small·CER ~17.5%). 전부 HF 공개 변환본(§7 출처 — 원본/변환본
# 링크 승계):
#   small   ggerganov/whisper.cpp/ggml-small.bin
#           (https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin)
#   tiny-ko wabisabisocial/whisper-tiny-korean-ggml/ggml-tiny-ko.bin
#           (https://huggingface.co/wabisabisocial/whisper-tiny-korean-ggml/resolve/main/ggml-tiny-ko.bin)
#   small-komixv2(EYESfix Step B — 3순위 상향 후보·CER ~7.4% — 폰 지연 재판정
#   대상): leuconoe/whisper-komixv2-ggml/ggml-small-komixv2.bin
#           (https://huggingface.co/leuconoe/whisper-komixv2-ggml/resolve/main/ggml-small-komixv2.bin)
#           핀 실측 원장(2026-10-25): 원본 seastar105/whisper-small-komixv2
#           HF repo에는 ggml이 **없다**(siblings 전수 — safetensors/msgpack만)
#           → 변환본 = komixv2 변환 가족 repo. 교차 등호: jaewontop/
#           whisper-small-komixv2-ggml의 whisper-komixv2-ggml.bin sha256 =
#           본 핀과 동일(별도 repo의 동일 blob — 2repo 교차 실측). 부기: 본
#           조달기의 기존 tiny-ko 핀(dd6d9f46…) = leuconoe repo의
#           ggml-tiny-komixv2.bin과 sha256 등호(같은 변환 가족 blob — 기존
#           핀의 계보 규명 — 무변조).
#           채용 = STT_SMALLKO=1 환경변수 명시(전 축 — 기본 핀 무변조 — 오버
#           라이드 계열과 같은 축 무반영 명시 조달 관행).
# 멱등: 파일이 있고 sha256이 핀과 일치하면 네트워크 없이 영수증만 인쇄한다.
# sha256 불일치(부분 다운로드/변경) = 재조달(정직 — 조용한 대체 금지).
# 오버라이드(축 무반영 명시 조달): STT_MODEL_DIR·STT_MODEL_URL·STT_MODEL_NAME.
set -u

FAIL() { echo "STT-MODELS-SETUP-FAIL: $*"; exit 1; }

ENGINE=$(cd "$(dirname "$0")/../.." && pwd)

# --- 축 판정(폰 aarch64 포함 — T5에서 동일 스크립트를 폰에서 그대로 돌린다)
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) AXIS=win ;;
    *) if grep -qi microsoft /proc/version 2>/dev/null; then AXIS=wsl
       else AXIS=termux
       fi ;;
esac

# --- 축별 dir+모델 핀
SMALL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"
SMALL_SHA="1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b"
SMALL_SIZE=487601967
TINYKO_URL="https://huggingface.co/wabisabisocial/whisper-tiny-korean-ggml/resolve/main/ggml-tiny-ko.bin"
TINYKO_SHA="dd6d9f46a7d292bb2ee4725ccb132bedde690ce2ee83e437a7786f8c67c41d08"
TINYKO_SIZE=77691730
# EYESfix Step B — 3순위 상향 핀 (small-komixv2 ggml 변환본 — 위 원장).
SMALLKO_URL="https://huggingface.co/leuconoe/whisper-komixv2-ggml/resolve/main/ggml-small-komixv2.bin"
SMALLKO_SHA="a428f6462d65c8f8942a2e1b9684910e5ff3a6873cc2b80856d549739627bba4"
SMALLKO_SIZE=487601984

case "$AXIS" in
    win)    DEF_DIR="build/stt-models"      DEF_URL="$SMALL_URL"
            DEF_SHA="$SMALL_SHA"   DEF_SIZE="$SMALL_SIZE"   AXIS_BIN=win ;;
    wsl)    DEF_DIR="buildwsl/stt-models"   DEF_URL="$SMALL_URL"
            DEF_SHA="$SMALL_SHA"   DEF_SIZE="$SMALL_SIZE"   AXIS_BIN=win ;;
    termux) DEF_DIR="buildterm/stt-models"  DEF_URL="$TINYKO_URL"
            DEF_SHA="$TINYKO_SHA"  DEF_SIZE="$TINYKO_SIZE"  AXIS_BIN=phone ;;
esac

# EYESfix Step B — 3순위 명시 채용(기본 핀 무변조 — STT_SMALLKO=1 만).
if [ "${STT_SMALLKO:-0}" = "1" ]; then
    DEF_URL="$SMALLKO_URL"
    DEF_SHA="$SMALLKO_SHA"
    DEF_SIZE="$SMALLKO_SIZE"
    echo "SMALLKO-SELECTED: small-komixv2 핀 채용 (3순위 — 명시 조달)"
fi

MODEL_DIR="${STT_MODEL_DIR:-$ENGINE/$DEF_DIR}"
MODEL_URL="${STT_MODEL_URL:-$DEF_URL}"
MODEL_SHA="${STT_MODEL_SHA:-$DEF_SHA}"
MODEL_SIZE="${STT_MODEL_SIZE:-$DEF_SIZE}"

echo "=== install_stt_models (T1 — ggml 모델 조달기, idempotent) ==="
echo "axis=$AXIS dir=$MODEL_DIR"
echo "url=$MODEL_URL"
echo "pin-sha256=$MODEL_SHA size=$MODEL_SIZE"

mkdir -p "$MODEL_DIR" || FAIL "mkdir $MODEL_DIR failed"
MODEL_FILE="$MODEL_DIR/$(basename "$MODEL_URL")"

# --- 영수증(sha256.txt) — 축·URL·핀 기록(무추적 dir에만 남는다)
RECEIPT="$MODEL_DIR/sha256.txt"
receipt() {
    { echo "axis=$AXIS"
      echo "url=$MODEL_URL"
      echo "file=$(basename "$MODEL_URL") size=$MODEL_SIZE sha256=$MODEL_SHA"
      echo "local-sha256=$(sha256sum "$MODEL_FILE" | cut -d' ' -f1)"
    } > "$RECEIPT" || FAIL "receipt write failed"
}

# --- 멱등: 이미 있고 sha가 핀과 일치 = 네트워크 0
if [ -s "$MODEL_FILE" ]; then
    GOT=$(sha256sum "$MODEL_FILE" | cut -d' ' -f1)
    if [ "$GOT" = "$MODEL_SHA" ]; then
        echo "ALREADY-PRESENT (no network use) sha256=$GOT"
        receipt
        echo "STT-MODELS-SETUP-OK (cached)"
        exit 0
    fi
    echo "sha mismatch (local=$GOT pin=$MODEL_SHA) — re-fetching (정직: 조용한 대체 금지)"
fi

# --- 조달(부재 시에만 네트워크)
timeout 1800 curl -fsSL -o "$MODEL_FILE.part" "$MODEL_URL" \
    || FAIL "model download failed (rc=$?, url=$MODEL_URL)"
mv "$MODEL_FILE.part" "$MODEL_FILE" || FAIL "rename failed"

GOT=$(sha256sum "$MODEL_FILE" | cut -d' ' -f1)
[ "$GOT" = "$MODEL_SHA" ] \
    || FAIL "sha256 mismatch after download (local=$GOT pin=$MODEL_SHA)"
GOT_SIZE=$(wc -c < "$MODEL_FILE")
[ "$GOT_SIZE" = "$MODEL_SIZE" ] \
    || FAIL "size mismatch after download (local=$GOT_SIZE pin=$MODEL_SIZE)"
receipt

echo "downloaded+verified file=$(basename "$MODEL_FILE") size=$GOT_SIZE sha256=$GOT"
echo "STT-MODELS-SETUP-OK"
exit 0