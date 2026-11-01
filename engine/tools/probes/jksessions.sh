#!/usr/bin/env bash
# jksessions.sh — 대화 세션 돌아보기 도구 (스펙 2026-11-01-chat-session-log-design D7).
#
# 세션 파일: <state>/sessions/<YYYYMMDD-HHMMSS>_<source>_<id4>.jsonl
# (ChatSessionLog — engine/include/apps/ChatSessionLog.h 가 줄 형식 계약 원문).
#
# 사용:
#   jksessions.sh [-s STATE_DIR] list [N]            최근 N개 세션(기본 10)
#   jksessions.sh [-s STATE_DIR] show <파일|latest>   한 세션 transcript
#   jksessions.sh [-s STATE_DIR] summary [N]          최근 N개 세션 요약(기본 전부)
#
# state 경로: -s 인자 > env JK_STATE_DIR > 실행 위치 기준 ./state
# 의존: bash + awk + sed + ls/sort 뿐(jq 가정 금지 — 폰 Termux 공통).
# 폰은 ssh로 이 스크립트를 실행한다(주소는 기록하지 않는다).

set -u

STATE="${JK_STATE_DIR:-./state}"
if [ "${1:-}" = "-s" ]; then
    [ $# -ge 2 ] || { echo "usage: $0 [-s STATE_DIR] list|show|summary ..." >&2; exit 2; }
    STATE="$2"; shift 2
fi
SDIR="${STATE%/}/sessions"
CMD="${1:-list}"; [ $# -gt 0 ] && shift

usage() {
    sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
}

# 이름 오름차순 = 시간 오름차순(파일명이 시각으로 시작).
session_files() {
    [ -d "$SDIR" ] || return 0
    ls -1 "$SDIR" 2>/dev/null | grep '\.jsonl$' | sort
}

# 공통 awk 라이브러리: JSON 한 줄에서 문자열/숫자 필드 추출(평면 객체 전용).
AWK_LIB='
function jstr(line, key,    p, i, c, n, out, h) {
    p = index(line, "\"" key "\":\"")
    if (p == 0) return ""
    i = p + length(key) + 4
    n = length(line); out = ""
    while (i <= n) {
        c = substr(line, i, 1)
        if (c == "\"") break
        if (c == "\\") {
            i++; c = substr(line, i, 1)
            if (c == "n") out = out " "
            else if (c == "t") out = out " "
            else if (c == "r") out = out ""
            else if (c == "u") { h = substr(line, i + 1, 4); i += 4; out = out "\\u" h }
            else out = out c
        } else out = out c
        i++
    }
    return out
}
function jnum(line, key,    p, rest, m) {
    p = index(line, "\"" key "\":")
    if (p == 0) return ""
    rest = substr(line, p + length(key) + 3)
    if (substr(rest, 1, 1) == "\"") return ""
    m = match(rest, /^[-0-9a-z.]+/)
    if (m == 0) return ""
    return substr(rest, 1, RLENGTH)
}
'

cmd_list() {
    local n="${1:-10}"
    local files; files=$(session_files)
    if [ -z "$files" ]; then echo "(세션 없음: $SDIR)"; return 0; fi
    echo "$files" | tail -n "$n" | while IFS= read -r f; do
        awk -v Q="'" "$AWK_LIB"'
            /"type":"user"/ { turns++; if (first == "") first = jstr($0, "text") }
            /"type":"action"/ && /"ok":false/ { fails++ }
            END { printf "%s  turns=%d fail=%d  %s\n", FN, turns, fails, first }
        ' FN="$f" "$SDIR/$f"
    done
}

resolve_file() {
    local arg="$1"
    if [ "$arg" = "latest" ]; then
        local last; last=$(session_files | tail -n 1)
        [ -n "$last" ] || return 1
        echo "$SDIR/$last"; return 0
    fi
    if [ -f "$arg" ]; then echo "$arg"; return 0; fi
    if [ -f "$SDIR/$arg" ]; then echo "$SDIR/$arg"; return 0; fi
    return 1
}

cmd_show() {
    local f; f=$(resolve_file "${1:-latest}") || { echo "세션 파일 없음: ${1:-latest} ($SDIR)" >&2; return 1; }
    echo "== $(basename "$f")"
    awk -v Q="'" "$AWK_LIB"'
        function flush() {
            if (cur == "") return
            print cur
            cur = ""
        }
        /"type":"user"/ {
            flush()
            t = jnum($0, "turn"); stt = jstr($0, "stt"); sm = jnum($0, "stt_ms")
            src = jstr($0, "source")
            cur = "[" t "] 나"
            if (stt != "") cur = cur "(" stt (sm != "" ? " " sm "ms" : "") ")"
            cur = cur ": " jstr($0, "text")
            app = ""
            next
        }
        /"type":"route"/ {
            path = jstr($0, "path"); m = jstr($0, "matched"); app = jstr($0, "app")
            seg = path
            if (m != "" && path != "exact") seg = seg " " Q m Q
            k = jstr($0, "kind")
            if (k != "") seg = seg " " k
            cur = cur " → (" seg ")"
            next
        }
        /"type":"action"/ {
            tool = jstr($0, "tool"); ok = jnum($0, "ok"); err = jstr($0, "err")
            what = (app != "") ? app : jstr($0, "args")
            seg = tool (what != "" ? " " what : "") " " (ok == "true" ? "ok" : "FAIL")
            if (err != "") seg = seg "(" err ")"
            seg = seg " " jnum($0, "ms") "ms"
            cur = cur " " seg
            next
        }
        /"type":"reply"/ {
            cur = cur " → 답: " jstr($0, "text") "  [" jnum($0, "total_ms") "ms]"
            flush()
            next
        }
        END { flush() }
    ' "$f"
}

cmd_summary() {
    local files; files=$(session_files)
    if [ -z "$files" ]; then echo "(세션 없음: $SDIR)"; return 0; fi
    [ -n "${1:-}" ] && files=$(echo "$files" | tail -n "$1")
    local paths=()
    while IFS= read -r f; do paths+=("$SDIR/$f"); done <<< "$files"
    awk -v Q="'" "$AWK_LIB"'
        FNR == 1 { nfiles++; fn = FILENAME; sub(/.*\//, "", fn) }
        /"type":"user"/ { turns++; key = fn "#" jnum($0, "turn"); utext[key] = jstr($0, "text") }
        /"type":"route"/ {
            key = fn "#" jnum($0, "turn"); p = jstr($0, "path"); if (p == "") p = "?"
            if (!(p in pc)) porder[++np] = p
            pc[p]++
            rapp[key] = jstr($0, "app")
        }
        /"type":"action"/ && /"ok":false/ {
            key = fn "#" jnum($0, "turn"); err = jstr($0, "err")
            fails[++nf] = key "  " jstr($0, "tool") " " jstr($0, "args") " err=" err "  — " utext[key]
            if (err == "unknown_app") {
                a = rapp[key]; if (a == "") a = jstr($0, "args")
                unk[++nu] = a "  — " utext[key] "  (" key ")"
            }
        }
        /"type":"reply"/ {
            ms = jnum($0, "total_ms") + 0
            if (ms > 3000) { key = fn "#" jnum($0, "turn"); slow[++ns] = key "  " ms "ms  — " utext[key] }
        }
        END {
            printf "세션 %d · 턴 %d\n", nfiles, turns
            print "-- 경로별 턴 수"
            for (i = 1; i <= np; i++) printf "  %-6s %d\n", porder[i], pc[porder[i]]
            printf "-- action ok=false 턴 (%d)\n", nf
            for (i = 1; i <= nf; i++) print "  " fails[i]
            printf "-- err=unknown_app (%d)\n", nu
            for (i = 1; i <= nu; i++) print "  " unk[i]
            printf "-- total_ms > 3000 (%d)\n", ns
            for (i = 1; i <= ns; i++) print "  " slow[i]
        }
    ' "${paths[@]}"
}

case "$CMD" in
    list)    cmd_list "$@" ;;
    show)    cmd_show "$@" ;;
    summary) cmd_summary "$@" ;;
    -h|--help|help) usage ;;
    *) echo "알 수 없는 명령: $CMD" >&2; usage >&2; exit 2 ;;
esac
