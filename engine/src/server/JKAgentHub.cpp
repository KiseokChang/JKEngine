// [P1 -- docs/108 3단계] 에이전트 허브 -- HandleAgentQuery(도구 44종 분기) 독립 TU.
// 서버 TU 원문 이동(동작 무변)·조력자 static 사본(interim -- 통합은 docs/106 §2-5 후속)·
// 권한 매트릭스는 공유 헤더(단일 진실원).
#include <server/JKWindowServer.h>
#include <server/JKServerState.h>
#include <server/JKAgentPermissions.h>
#include <jkhooks_android.h>  // JNI 훅 단일 ABI(docs/106 §4)

#include <agent/JKAgentJson.h>
#include <apps/MusicDirStore.h>
#include <fs/JKFs.h>
#include <theme/JKTheme.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace jk {
namespace server {

// JsonEsc -- JKWindowServer.cpp 정의 공유 함수(헤더 표도 후속).
std::string JsonEsc(const std::string& s);

struct NoteRow {
    int id = 0;
    std::string text;        // note 본문 / item 제목
    int win = 0;             // note: 타깃 창 id (0=범용)
    int state = 0;           // item: 0대기/1진행/2완료
    std::string src;         // "agent"|"user"
    std::string tsRaw;       // "ts":<원문 숫자> — 재직렬화용
    bool isItem = false;
};

static void LoadSettingsKv(bool& mute, int& volume, int& retention,
                           std::string& fontPath, std::string& fontFallback,
                           std::string& fontScale);

// exe-dir 흡수 (docs/68 W5): 이 TU에 복각돼 있던 GetModuleFileNameA 블록
// (buf→find_last_of→substr — WritePermissionsEntry/SettingsKvPath/NotesPath/
// FilesPermRaw/RevokeTrustRecord/AgentToolAllowed/AppToolAllowed/agent_permissions
// 퍼미션 읽기)을 1헬퍼로 통일. 원문 절단 규약 = 뒤 구분자 없음(dir,
// 소비자 결합이 '/'로 조립 — 플랜 F1 전역 규약, docs/64) — 그대로 보존.
// 절단 취약점(MAX_PATH/1024 후단 절단)은 어댑터의 동적 재시도로 동시 소거.
// [T9-브라우저] JNI 훅 선언 = include/jkhooks_android.h 단일 ABI 헤더
// (docs/106 §4 통합 — weak 계약은 헤더 주석 참조).

// [P1 — docs/108 1단계] 원문 이동 → server/JKServerState.cpp
static std::string ExeDirNoSlash() { return state::ExeDirNoSlash(); }
// [P1 -- docs/108 3단계] 경로·지문 알리스 -- 서버 TU의 동명 static 알리스와 수형(원문 모두 state:: 단일 진실원).
static std::string SettingsKvPath() { return state::SettingsKvPath(); }
static std::string NotesPath() { return state::NotesPath(); }
static bool ValidFingerprint(const std::string& fp) { return state::ValidFingerprint(fp); }
// trust.json 레코드 제거 -- 서버 TU 정의(extern 승계).
std::string RevokeTrustRecord(const std::string& fingerprint);
// 파일 허브·토픽 표·프레임 인코더 -- 서버 TU 정의가 단일 진실원(static 해제 승계).
std::string FilesPermRaw(const std::string& tool);
bool ValidFilePath(const std::string& path);
std::string FilesListOpJson(const std::string& path);
std::string FilesReadOpJson(const std::string& path, int maxBytes);
std::string FilesAuditOpJson(int limit);
extern const char* kReservedTopicPrefixes[7];  // 개수 고정(range-for 완감 형).
bool WritePng(const std::string& path, int w, int h, const uint8_t* px);
bool EncodeLayerJpegB64(const std::vector<uint8_t>& px, int w, int h,
                        int maxw, std::string* b64Out, int* encW, int* encH);

static std::string WritePermissionsEntry(const std::string& permTool,
                                         const std::string& decision) {
    const std::string dir = ExeDirNoSlash();
    const std::string path = dir + "/permissions.json";

    std::map<std::string, std::string> values;
    if (std::FILE* f = std::fopen(path.c_str(), "rb")) {
        // 전체 읽기 (docs/53 §9 잔여): 4KB 스택 버프는 파일 뒤쪽의 알려진
        // 도구 행을 잘라내 RMW가 기본값으로 되돌렸다. 256KiB 상한 = 이상
        // 파일 메모리 가드 — 초과 시 잘린 JSON이 파싱 실패하면 전재기록이
        // 기본값으로 복원한다(자기 치유, 알려진 키만 기록되는 RMW 원래 의미).
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        if (sz < 0) {
            // ftell 실패에도 RMW를 진행하면 기존 파일 전체가 기본값으로
            // 되돌려진다(opus 최종리뷰 n3) — 지문 없는 not-found RMW보다
            // 나은 정직 오류로 파킹 전 즉답 경로에 표면화한다.
            std::fclose(f);
            return "permissions_unreadable";
        }
        std::fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            const size_t cap =
                std::min<size_t>(static_cast<size_t>(sz), 256 * 1024);
            std::vector<char> buf(cap + 1, '\0');
            const size_t n = std::fread(buf.data(), 1, cap, f);
            buf[n] = '\0';
            jk::agent::AgentJson json(buf.data());
            std::string v;
            if (json.ok()) {
                for (const AgentPermRow& r : kPermMatrix) {
                    if (json.GetStr(r.tool, v) &&
                        (v == "allow" || v == "ask" || v == "deny")) {
                        values[r.tool] = v;
                    }
                }
            }
        }
        std::fclose(f);
    }
    values[permTool] = decision;
    std::string out = "{";
    bool first = true;
    for (const AgentPermRow& r : kPermMatrix) {
        if (std::string(r.gate) == "server(fixed)") continue;
        if (!first) out += ",";
        first = false;
        const auto it = values.find(r.tool);
        out += "\"" + std::string(r.tool) + "\":\"" +
               (it != values.end() ? it->second : std::string(r.deflt)) + "\"";
    }
    out += "}";
    std::FILE* w = std::fopen(path.c_str(), "wb");
    if (!w) return "write_failed";
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), w);
    std::fclose(w);
    return wrote == out.size() ? std::string() : std::string("write_failed");
}

static bool TrustRecordText(const std::string& text,
                            const std::string& fingerprint,
                            std::string& recOut) {
    const std::string needle = "\"fingerprint\":\"" + fingerprint + "\"";
    const size_t hit = text.find(needle);
    if (hit == std::string::npos) return false;
    const size_t begin = text.rfind('{', hit);
    const size_t end = text.find('}', hit);
    if (begin == std::string::npos || end == std::string::npos) return false;
    recOut = text.substr(begin, end - begin + 1);
    return true;
}

static std::string Utf8TrimTo(const std::string& s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
        --cut;   // 연속 바이트(10xxxxxx)만큼 물러난다
    }
    if (cut > 0) --cut;   // 리드 바이트도 제거(잘린 시퀀스)
    return s.substr(0, cut);
}

static bool ValidFontScale(const std::string& v) {
    if (v.empty()) return false;
    char* end = nullptr;
    const double d = std::strtod(v.c_str(), &end);
    return end != v.c_str() && *end == '\0' && d >= 1.0 && d <= 3.0;
}

static void NotesArrayRows(const std::string& body, const char* name,
                           bool isItem, std::vector<NoteRow>& rows) {
    const std::string key = std::string("\"") + name + "\":[";
    const size_t arr = body.find(key);
    if (arr == std::string::npos) return;
    size_t pos = body.find('[', arr);
    if (pos == std::string::npos) return;
    ++pos;
    while (pos < body.size()) {
        // 다음 비공백 문자가 '{'가 아니면 배열 끝 — find('{')로는 ']'를
        // 건너뛰어 다음 배열(역방향 오염: backlog 행이 notes로 흡수됨)까지
        // 긁는다. RMW가 그 오염을 재직렬화로 굳히므로 반드시 여기서 끊는다.
        while (pos < body.size() &&
               std::isspace(static_cast<unsigned char>(body[pos]))) {
            ++pos;
        }
        if (pos >= body.size() || body[pos] != '{') break;
        const size_t obj = pos;
        const size_t close = body.find('}', obj);
        if (close == std::string::npos) break;
        const std::string row = body.substr(obj, close - obj + 1);
        jk::agent::AgentJson j(row);
        NoteRow r;
        r.isItem = isItem;
        if (j.ok()) {
            j.GetInt("id", r.id);
            j.GetStr("src", r.src);
            if (isItem) {
                j.GetStr("title", r.text);
                j.GetInt("state", r.state);
            } else {
                j.GetStr("text", r.text);
                j.GetInt("win", r.win);
            }
            const size_t tp = row.find("\"ts\":");
            if (tp != std::string::npos) {
                size_t d = tp + 5;
                while (d < row.size() &&
                       (row[d] == '-' || (row[d] >= '0' && row[d] <= '9'))) {
                    r.tsRaw += row[d];
                    ++d;
                }
            }
            if (r.id > 0 && !r.tsRaw.empty()) rows.push_back(r);
        }
        pos = close + 1;
        // 다음 행은 쉼표 뒤 — ','/공백을 걷어내고 '{'를 다시 검사한다.
        while (pos < body.size() &&
               (body[pos] == ',' ||
                std::isspace(static_cast<unsigned char>(body[pos])))) {
            ++pos;
        }
    }
}
static bool WriteSettingsKv(bool mute, int volume, int retention,
                            const std::string& fontPath,
                            const std::string& fontFallback,
                            const std::string& fontScale,
                            std::string* errOut = nullptr) {
    // 관리 3키(audio/retention/text)의 새 값 원문 슬라이스 — C1 보존 합성과
    // 신설(무원문) 폴백 양쪽이 소비한다.
    const std::string audioJson =
        std::string("{\"mute\":") + (mute ? "1" : "0") +
        ",\"volume\":" + std::to_string(volume) + "}";
    const std::string retentionJson =
        "{\"days\":" + std::to_string(retention) + "}";
    const std::string textJson =
        std::string("{\"font_path\":\"") + JsonEsc(fontPath) +
        "\",\"font_fallback\":\"" + JsonEsc(fontFallback) +
        "\",\"font_scale\":\"" + JsonEsc(fontScale) + "\"}";
    // C1 (T2 — T1 리뷰 C1 교차 작성자 소각 봉합, 플랜 2026-10-10-music-dirs-ui
    // 필수 인계): 기존까지의 메모리 KV 재조립은 이 파일의 **미관리 상위 키**
    // (music.dirs·gallery.dirs·장래 키)를 전량 소각했다 — music가 제2작성자가
    // 된 순간(폴더 관리 T2) 볼륨 변경 1회가 폴더 목록을 지운다. 쓰기 직전
    // 원문을 상위 스캐너(MusicDirStore.h — 원문 슬라이스 규약, CP949/비UTF-8
    // 바이트 투명)로 읽어 ComposeKeyed 재부착: **미관리 키는 원문 슬라이스
    // 그대로**, 관리 3키는 같은 자리 재기입, 부재 관리 키는 말미 신설. 부재·
    // 빈 파일·부적합 원문은 보존 원문이 없어 관리 3키 신설(기존 동작 — 파손
    // 문서에는 보존 원문이 없고 설정 허브 정문은 쓰기 성립해야 한다 — 정직
    // 부기; 원본 파손 상태도 봉합하지 못한다).
    std::string out;
    std::vector<jk::music::store::JsonField> fields;
    std::string original;
    const bool hadFile =
        jk::music::store::ReadAll(SettingsKvPath(), original);
    // I-1 (T2 fix r1 — C1 보존의 구조적 부작용 봉합, 리뷰 필수): 디스크 원문이
    // **quickjs 부적합**이면(부팅 리더 LoadSettingsKv와 같은 판정 — 같은 파일
    // 을 부팅도 settings_set도 같은 눈으로 봐야 하는 대칭 계약) 보존 재합성을
    // 거부하고 **쓰지 않는다**. 거부 근거: C1 보존이 CP949 혼입 문서를 영구
    // 잔존시키고(스캐너는 수용), 그 문서 위의 매번 settings_set 재합성은 부팅
    // 리더의 기본값 무음 리셋을 계속 재생산한다 — 회복 경로(music 패널에서
    // CP949 항목 제거 → 문서 순수 회귀)는 열려 있음. 정직 err 1행을
    // settings_set 답신에 실어 사용자에게 보인다(무음 계약). 빈 파일은 관리
    // 3키 신설(치유)로 통과 — 튕겨낼 데이터가 없다. 전각 치유(CP949 변환)는
    // 별도 라인(원장 부기).
    if (hadFile && !original.empty() && !jk::agent::AgentJson(original).ok()) {
        if (errOut) *errOut = "settings.json 파싱 실패 — 수기 치유 필요";
        std::fprintf(stderr,
                     "JKWindowServer: settings_set 거부 — %s: %s\n",
                     "settings.json 파싱 실패 — 수기 치유 필요",
                     SettingsKvPath().c_str());
        return false;
    }
    if (hadFile && !original.empty() &&
        jk::music::store::ScanTopLevelFields(original, fields)) {
        const std::vector<jk::music::store::JsonField> managed = {
            {"audio", audioJson},
            {"retention", retentionJson},
            {"text", textJson},
        };
        out = jk::music::store::ComposeKeyed(fields, managed);
    } else {
        out = "{\"audio\":" + audioJson + ",\"retention\":" + retentionJson +
              ",\"text\":" + textJson + "}";
    }
    const std::string kvPath = SettingsKvPath();
    // 파일 교체는 T1 WriteSettingsAtomic 재용(tmp 완성본+rename 사다리 —
    // 기존 fopen("wb")의 비원자 쓰기 중간 절단 창 소각; T1 리뷰 C1 "교차 중
    // 레이스 소각 지문도 같은 봉투에" 지시의 부분-상태 봉합 몫). 기존 .bak
    // dance(선삭제+선대피)를 펼치지 않은 이유: 사다리가 원문 자체를 보수
    // (복원)하는 형태라 선대피 뒤 3연속 실패 시 유일 잔존(.bak)이 사다리의
    // remove(bak)에서 소각되는 창이 생긴다 — 대신 아래에서 직전 원문 사본을
    // 성공 세대에만 쓴다(opus 리뷰 MINOR-1의 .bak 1세대 복구 원본 계약 유지;
    // 사본 쓰기 실패는 무해 잔산 — 정직 정정(M-4): Windows 사다리 경로는
    // 구 .bak 존재 시 remove/rename으로 .bak.bak를 소각하므로 사본 재작성
    // 실패 시 **.bak 부재**가 남을 수 있고(구세대 사본 잔존은 posix 1세대
    // 교체 rename 경로에만), 본 쓰기는 이미 성립한 뒤인 안전측 — 방위 사본
    // 실종은 방위 목적상 무해. 이 앱의 .bak은 직전 원문 스냅샷 사본이다).
    // 남는 한계(정직 부기): 두 작성자(서버 settings_set ↔ 클라 music 폴더
    // 관리) 사이의 프로세스 간 파일 락 시설이 부재라 교차 최종-승자 정합은
    // 읽고-고쳐쓰기 스냅샷 몫 — 미관리 키는 마지막 승자가 읽은 스냅샷이
    // 남는다. 관리 3키는 메모리 진실원이라 어떤 경로의 재쓰기에서도 회복되고;
    // 미관리 키 교차 소각 창은 쓰기 직전 재독(위 C1 배치)으로 스캔·전체
    // 쓰기 ms 단위까지 축소되었다(임시파일 프로세스 접점은 pid 접미로 소각
    // — MusicDirStore.h fix r1 M-1).
    const bool hadOriginal = !original.empty();  // 직전 원문 사본(.bak) 근거
    std::string werr;
    const bool ok = jk::music::store::WriteSettingsAtomic(kvPath, out, werr);
    if (!ok) {
        // 실패도 조용한 소실 없이(문서 존재 로그 — return은 기존 계약 ok=false;
        // err는 settings_set 답신 detail로 표면화 — I-1의 정직 규약 승계)
        if (errOut) *errOut = werr;
        std::fprintf(stderr,
                     "JKWindowServer: settings.json write failed — %s\n",
                     werr.c_str());
        return ok;
    }
    if (hadOriginal) {  // .bak 1세대 — 직전 원문 사본(사용 원문 = 위 읽는 스냅샷)
        std::string bwerr;
        jk::music::store::WriteSettingsAtomic(kvPath + ".bak", original, bwerr);
    }
    return ok;
}

static bool ReadNotes(std::vector<NoteRow>& notes, std::vector<NoteRow>& items,
                      int& next) {
    std::FILE* f = std::fopen(NotesPath().c_str(), "rb");
    if (!f) return true;   // 파일 없음 = 빈 상태(정상)
    std::string body;
    char chunk[8192];
    size_t n = 0;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) body.append(chunk, n);
    std::fclose(f);
    // 이상 수확 방어(opus NIT-2): 8MiB 초과 파일은 파싱/수확을 거부 —
    // docs/53 state 파일 캡과 같은 상한. 쓰기 캡(256KiB)보다 큰 정상 상태는
    // 있을 수 없으므로(캡 거부) 이 경계는 심어진 파일만 걸러낸다.
    if (body.size() > 8u * 1024 * 1024) return false;
    jk::agent::AgentJson j(body);
    if (!j.ok()) return false;   // 손상 — 호출자가 빈 상태 시작(정직 로그)
    NotesArrayRows(body, "notes", false, notes);
    NotesArrayRows(body, "backlog", true, items);
    // 역오염 방어(opus MINOR-4): JSON은 통과하는데 행 스캔이 행을 놓치면
    // (pretty-print 재포맷, 키 오탈자, drop된 행) RMW가 빈 배열/축소 배열을
    // "정상"으로 재직렬화해 사용자 노트를 소각한다. 읽기 오염 = 쓰기 오염 —
    // 수확 수와 파서 수가 다르면 파산 파일로 판정(RMW 거부, read는 빈 시작).
    {
        int cntN = 0, cntI = 0;
        const bool shapeOk =
            j.GetArraySize("notes", cntN) && j.GetArraySize("backlog", cntI);
        if (!shapeOk || cntN != static_cast<int>(notes.size()) ||
            cntI != static_cast<int>(items.size())) {
            return false;
        }
    }
    if (!j.GetInt("next", next)) {
        // 구형/손상 파일 — 채번기 복구(최대 id + 1).
        for (const NoteRow& r : notes) next = std::max(next, r.id + 1);
        for (const NoteRow& r : items) next = std::max(next, r.id + 1);
    }
    return true;
}

static bool WriteNotesFile(const std::vector<NoteRow>& notes,
                           const std::vector<NoteRow>& items, int next) {
    std::string out = "{\"notes\":[";
    for (size_t i = 0; i < notes.size(); ++i) {
        if (i) out += ",";
        // 행 크기가 가변(text 512바이트 × 이스케이프 확장)이라 스트링 빌더 —
        // 고정 snprintf 버퍼는 잘림 함정(docs/38 레슨 계열).
        out += "{\"id\":" + std::to_string(notes[i].id) +
               ",\"text\":\"" + JsonEsc(notes[i].text) +
               "\",\"win\":" + std::to_string(notes[i].win) +
               ",\"ts\":" + notes[i].tsRaw +
               ",\"src\":\"" + JsonEsc(notes[i].src) + "\"}";
    }
    out += "],\"backlog\":[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += "{\"id\":" + std::to_string(items[i].id) +
               ",\"title\":\"" + JsonEsc(items[i].text) +
               "\",\"state\":" + std::to_string(items[i].state) +
               ",\"ts\":" + items[i].tsRaw +
               ",\"src\":\"" + JsonEsc(items[i].src) + "\"}";
    }
    out += "],\"next\":" + std::to_string(next) + "}";
    if (out.size() > 262144) return false;   // 256KiB 캡 (docs/53 3d 선례)
    const std::string path = NotesPath();
    std::remove((path + ".bak").c_str());
    // rename 실패(브로커/프로브가 읽기 잠금) → fopen "wb"로 원본이 잘리는
    // receipts M5 선례의 동일 벡터 — 실패 시 절단 없이 중단. 첫 쓰기(원본
    // 부재)는 통과.
    if (std::FILE* probe = std::fopen(path.c_str(), "rb")) {
        std::fclose(probe);
        if (std::rename(path.c_str(), (path + ".bak").c_str()) != 0) {
            return false;
        }
    }
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::rename((path + ".bak").c_str(), path.c_str());
        return false;
    }
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (wrote != out.size()) {
        std::remove(path.c_str());
        std::rename((path + ".bak").c_str(), path.c_str());
        return false;
    }
    return true;
}

static bool PruneReceipts(int retentionDays) {
    std::string stateDir = SettingsKvPath();
    stateDir = stateDir.substr(0, stateDir.find_last_of("\\/") + 1);
    const std::string path = stateDir + "receipts.jsonl";
    const std::string newPath = path + ".new";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return true;  // 파일 없음 = 정리할 것도 없음 (정상)
    // 스트리밍 프룬 (docs/54 §10 레저): 전체 파일+kept를 메모리에 올리지
    // 않고 행 단위로 .new에 복사한다 — 행당 메모리 = 최대 행 길이. 원본은
    // .new가 완성될 때까지 건드리지 않으므로 실패 시 전 단계 무손상.
    std::FILE* w = std::fopen(newPath.c_str(), "wb");
    if (!w) {
        std::fclose(f);
        return false;
    }
    const long long cutoff =
        static_cast<long long>(std::time(nullptr)) * 1000 -
        static_cast<long long>(retentionDays) * 86400 * 1000;
    auto writeKept = [&](const std::string& line) -> bool {
        if (line.empty()) return true;
        const size_t tp = line.find("\"ts\":");
        const long long ts =
            (tp == std::string::npos) ? 0 : std::atoll(line.c_str() + tp + 5);
        if (ts < cutoff) return true;
        if (std::fwrite(line.data(), 1, line.size(), w) != line.size())
            return false;
        if (std::fputc('\n', w) == EOF) return false;
        return true;
    };
    std::vector<char> buf(65536);
    std::string line;  // 청크 경계에 걸린 미완성 행
    bool wfail = false;
    size_t nread;
    while (!wfail && (nread = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
        size_t pos = 0;
        while (pos < nread) {
            const char* nl =
                static_cast<const char*>(std::memchr(buf.data() + pos, '\n',
                                                     nread - pos));
            if (!nl) {
                line.append(buf.data() + pos, nread - pos);
                break;
            }
            line.append(buf.data() + pos,
                        static_cast<size_t>(nl - (buf.data() + pos)));
            pos += static_cast<size_t>(nl - (buf.data() + pos)) + 1;
            if (!writeKept(line)) {
                wfail = true;
                break;
            }
            line.clear();
        }
    }
    if (!wfail && !line.empty() && !writeKept(line))  // 개행 없는 마지막 행
        wfail = true;
    std::fclose(f);
    std::fclose(w);
    if (wfail) {
        std::remove(newPath.c_str());
        return false;
    }
    // opus 리뷰 M5 (docs/54 §11): rename 실패(예: 브로커가 append용으로
    // 열어둔 공유 위반 창)에도 진행하면 원본 절단 + .bak은 한 세대 전
    // 임파일러가 된다 — 실패 시 중단, 원본은 그대로(다음 set에서 재시도).
    std::remove((path + ".bak").c_str());
    if (std::rename(path.c_str(), (path + ".bak").c_str()) != 0) {
        std::remove(newPath.c_str());
        return false;
    }
    if (std::rename(newPath.c_str(), path.c_str()) != 0) {
        std::rename((path + ".bak").c_str(), path.c_str());  // 복원
        return false;
    }
    // 성공 — .bak은 한 세대 전 원본 보존(1세대 규약).
    return true;
}

void JKWindowServer::HandleAgentQuery(JKClientConnection& client,
                                      uint32_t queryId, const std::string& json) {
    // 배지 스냅샷 수집기 (#118 T2): overlay_numbers(도구 reply count)와
    // focus_window의 "num" 판정이 **같은 필터·순서**를 먹는 1본 — 입력 양식 =
    // 순수 부품 OverlayBadgeOrder의 그것(3a 원존). 필터 = 배지 렌더 패스
    // (DrawWindowNumberBadges)와 동기 — list_windows의 단절·control-only.
    // shell 제외에 **캡처 오버레이 면제가 포함**된다(T1 리뷰 Minor 수형 —
    // 도구 분기가 렌더 패스보다 넓어 순간 어긋나던 것 소각: "번호의 진실원 =
    // 화면 배지" 계약). 락 보유 중(HandleAgentQuery 전제)이라 무락 순회 —
    // 잠금 헬퍼 재획득 교착 원존(주석 3779행 지대) 준수는 그대로.
    const auto badgeTargets = [this]() {
        std::vector<jk::server::OverlayBadgeTarget> ts;
        for (auto& c : clients_) {
            if (!c || c->IsDisconnected() || c->IsControlOnly() ||
                c->IsShell() || c->Title() == kCaptureOverlayTitle) {
                continue;
            }
            JKCompositorLayer* lay =
                compositor_ ? compositor_->FindLayerById(c->Id()) : nullptr;
            ts.push_back(jk::server::OverlayBadgeTarget{
                c->Id(), lay && lay->IsVisible()});
        }
        return ts;
    };
    jk::agent::AgentJson req(json);
    std::string tool, reply;
    bool replied = true;
    if (!req.ok() || !req.GetStr("tool", tool)) {
        reply = "{\"ok\":false,\"error\":\"bad_request\"}";
    } else if (tool == "ping") {
        reply = "{\"ok\":true,\"pong\":true}";
    } else if (tool == "list_windows") {
        std::string out = "{\"ok\":true,\"windows\":[";
        bool first = true;
        for (auto& c : clients_) {
            if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
            // dw/dh (폰 미러 스펙 2026-09-27): 표시 크기 = 서면×layer scale.
            // 캡처(window_frame)는 서면 px이므로 폰 탭은 x + fx*dw로 데스크톱
            // 점을 얻는다 — 1:1 레이어에서 dw==w, fit-scale에서도 정확.
            char item[760];
            JKCompositorLayer* lay =
                compositor_ ? compositor_->FindLayerById(c->Id()) : nullptr;
            const int dw = lay ? static_cast<int>(std::llround(
                                     static_cast<double>(c->Width()) *
                                     lay->ScaleX()))
                               : c->Width();
            const int dh = lay ? static_cast<int>(std::llround(
                                     static_cast<double>(c->Height()) *
                                     lay->ScaleY()))
                               : c->Height();
            std::snprintf(item, sizeof(item),
                "%s{\"id\":%u,\"title\":\"%s\",\"pid\":%u,\"x\":%d,\"y\":%d,"
                "\"w\":%d,\"h\":%d,\"dw\":%d,\"dh\":%d,"
                "\"focused\":%s,\"minimized\":%s}",
                first ? "" : ",", c->Id(), JsonEsc(c->Title()).c_str(), c->Pid(),
                c->X(), c->Y(), c->Width(), c->Height(), dw, dh,
                focusedClientId_ == c->Id() ? "true" : "false",
                (compositor_ && !compositor_->IsLayerVisible(c->Id()))
                    ? "true" : "false");
            out += item;
            first = false;
        }
        reply = out + "]}";
    } else if (tool == "focus_window") {
        int id = 0;
        JKClientConnection* target = nullptr;
        // close_window와 동형 해소(F1): args.id 직접호출 계약 보존 + args.app
        // 제목 매칭 폴백(라우터 Focus.app) + argless = 포커스 창. 자기 창을
        // 포커스하는 것도 ok:true(id 직접호출 계약과 동일 — 부작용 없음).
        std::string appArg;
        const bool hasId = req.GetObjInt("args", "id", id);
        req.GetObjStr("args", "app", appArg);
        // #118 T2 (스펙 D3) — args.num = 배지 숫자 화법("창 3"/"3번 창"의
        // 라우터 Focus.winNum 릴레이): 화면 배지의 번호→창 판정. 진실원 =
        // 배지 렌더 패스(DrawWindowNumberBadges)와 동일 필터+순서(배지 스냅샷
        // 수집기 — list_windows 등호 z등호 T1 원존 —
        // OverlayBadgeWindowForNumber가 1기반 재용·3a-⑤). id(app)·num 동시
        // 지정은 없는 계약(라우터 어휘가
        // 상호배타) — hasId가 앞선다(기존 계약 보존). 범위 밖 = 0 → 아래
        // window_not_found 정직 회신(D5 — overlay_numbers no_window와 같은
        // 계열·가짜 ok 금지).
        int numArg = 0;
        const bool hasNum = req.GetObjInt("args", "num", numArg);
        if (hasNum && !hasId && appArg.empty()) {
            const std::vector<jk::server::OverlayBadgeTarget> targets =
                badgeTargets();
            const uint32_t wanted =
                jk::server::OverlayBadgeWindowForNumber(targets, numArg);
            for (auto& c : clients_) {
                if (wanted != 0 && c && c->Id() == wanted) {
                    target = c.get();
                    break;
                }
            }
            // 범위 밖(wanted == 0) = target 남김 — 아래 window_not_found.
            // **포커스 창 폴백 금지**(원존 위반 방지): argless 원존
            // (focusedClientId_ 폴백)은 발화가 대상을 지정하지 못한 나머지
            // 형이고, num은 "지정했는데 화면에 없다"의 정직 실패다.
        } else if (hasId || !appArg.empty() || focusedClientId_ != 0) {
            std::vector<AgentWindowRef> wins;
            for (auto& c : clients_) {
                if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) {
                    continue;
                }
                wins.push_back(AgentWindowRef{
                    c->Id(), c->Title(), focusedClientId_ == c->Id()});
            }
            const uint32_t resolved = ResolveAgentWindowTarget(
                wins, hasId, hasId ? static_cast<uint32_t>(id) : 0, appArg);
            if (resolved != 0) {
                for (auto& c : clients_) {
                    if (c && c->Id() == resolved) { target = c.get(); break; }
                }
            }
        }
        if (target && !target->IsControlOnly()) {
            // Same restore-on-activate semantics as WindowActivate (docs/28).
            if (compositor_) {
                compositor_->SetLayerVisible(target->Id(), true);
            }
            FocusClient(target->Id());
            PushWindowListUnsafe();
            reply = "{\"ok\":true}";
        } else {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        }
    } else if (tool == "window_fullscreen") {
        // vplayer 전체화면(스펙 2026-09-17 vplayer-fullscreen-osd §2.2): id
        // 생략 = 호출자 자기 창(창 클라이언트 — vplayer 경로), 명시 id =
        // 임의 창(스크립트/프로브). control-only 호출자의 생략형은 no_window.
        // on 생략 = 현재 상태 반전(AgentJson에 bool 접근자 없음 — GetObjInt
        // 0/1로 읽는다). 화면 상태 변경일 뿐 승인 행위가 아니어서 none gate
        // (focus_window 분류). reply의 fullscreen은 토글 후 실제 플래그 —
        // 클라 플래그의 유일 신뢰원.
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        } else if (!client.IsControlOnly()) {
            target = &client;
        }
        JKCompositorLayer* fsLayer = nullptr;
        if (target && !target->IsControlOnly() && !target->IsShell() && compositor_) {
            // opus 리뷰 MINOR-2: 명시 id로 shell(태스크바) 레이어를 노리면
            // 전체화면 토글이 데스크탑 셸을 덮는다 — shell은 '창'이 아니므로
            // window_not_found로 거절한다(생략형 경로는 애초 shell 불가).
            fsLayer = compositor_->FindLayerById(target->Id());
        }
        if (!fsLayer) {
            reply = (target ? "{\"ok\":false,\"error\":\"window_not_found\"}"
                            : "{\"ok\":false,\"error\":\"no_window\"}");
        } else {
            int onArg = -1;
            bool on = fsLayer->IsFullscreen() ? false : true;
            if (req.GetObjInt("args", "on", onArg) &&
                (onArg == 0 || onArg == 1)) {
                on = (onArg == 1);
            }
            ToggleFullscreen(*target, *fsLayer, on);
            reply = std::string("{\"ok\":true,\"fullscreen\":") +
                    (fsLayer->IsFullscreen() ? "true" : "false") + "}";
        }
    } else if (tool == "window_move" || tool == "window_resize") {
        // 폰 실전 개선 태스크 2 (스펙 2026-09-21-phone-practical-improvements):
        // 창 기하 서버 조작 2종 — 폰 채팅 LLM이 창을 옮기고 크기를 바꾼다.
        // 대상 선정/거절 뼈대는 window_fullscreen 복제: id 생략 = 호출자 자기
        // 창(윈도 클라), control-only 생략형 = no_window. 명시 id로 shell
        // (태스크바)/캡처 오버레이/control-only 연결을 노리면 window_not_found
        // (shell은 '창'이 아니라는 opus MINOR-2 분류 승계).
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        } else if (!client.IsControlOnly()) {
            target = &client;
        }
        // 가드가 읽는 값은 int64 리더로 (docs/57 §13.3 — int32 축소 캐스트는
        // 2^32+100 → 100 랩어라웃으로 ±32768/[80,8192] 가드를 우회시킨다).
        int64_t x = 0, y = 0, w = 0, h = 0;
        const bool hasXY = req.GetObjInt64("args", "x", x) &&
                           req.GetObjInt64("args", "y", y);
        const bool hasWH = req.GetObjInt64("args", "w", w) &&
                           req.GetObjInt64("args", "h", h);
        // 인자 유효성을 대상 판정보다 먼저 — 인자가 잘못된 호출은 대상이
        // 있든 없든 bad_args가 정직한 답.
        if (tool == "window_move" && !hasXY) {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
        } else if (tool == "window_resize" && !hasWH) {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
        } else if (!target) {
            reply = "{\"ok\":false,\"error\":\"no_window\"}";
        } else if (target->IsControlOnly() || target->IsShell() ||
                   target->Title() == kCaptureOverlayTitle || !compositor_) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            JKCompositorLayer* layer = compositor_->FindLayerById(target->Id());
            if (!layer) {
                reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
            } else if (preMaxRects_.count(layer->Id()) != 0) {
                // 최대화 중 기하 조작은 preMaxRects_ 진실원(저장 rect)과
                // 충돌한다 — 복원 후 조작하라는 거절.
                reply = "{\"ok\":false,\"error\":\"window_maximized\"}";
            } else if (layer->IsFullscreen()) {
                // 전체화면 레이어는 상태 전용(위치 0,0/출력 전체 크기) —
                // 복원 후 조작(preFsRects_ 저장 rect 보호, window_fullscreen
                // 토글로 복원).
                reply = "{\"ok\":false,\"error\":\"window_fullscreen_state\"}";
            } else if (tool == "window_move") {
                // 좌표 클램프 없음(Windows 동작 — 화면 밖 허용), int 범위만.
                if (x < -32768 || x > 32768 || y < -32768 || y > 32768) {
                    reply = "{\"ok\":false,\"error\":\"bad_args\"}";
                } else {
                    // BOTH required (docs/28 lesson): input mapping reads
                    // client->X()/Y() while the draw path reads the layer.
                    compositor_->SetLayerPosition(target->Id(),
                                                  static_cast<int>(x),
                                                  static_cast<int>(y));
                    target->SetPosition(static_cast<int>(x), static_cast<int>(y));
                    PushWindowListUnsafe();  // focus_window 선례 — 태스크바 동기
                    reply = "{\"ok\":true}";
                }
            } else {
                // resize: 인자 클램프가 아니라 범위 검사 — 범위 밖은 bad_args.
                if (w < 80 || w > 8192 || h < 80 || h > 8192) {
                    reply = "{\"ok\":false,\"error\":\"bad_args\"}";
                } else if (w == layer->Width() && h == layer->Height()) {
                    reply = "{\"ok\":true}";  // no-op — 동일 픽셀 크기
                } else {
                    // CommitChromeResize의 ResizeLayer가 layer scale을 1로
                    // 리셋한다(JKWindowServer.cpp 주석 참조). fit-scaled
                    // 레이어(픽셀 크기≠표시 크기)는 기존 ScaleX/Y를 반영한
                    // 표시 크기를 넘겨 scale이 보존되게 한다 — 스케일 1 레이어는
                    // dispW/H=자기(정규 경로와 동일, SetLayerScale 스킵).
                    int dispW = static_cast<int>(w), dispH = static_cast<int>(h);
                    const float sx = layer->ScaleX(), sy = layer->ScaleY();
                    if (sx != 1.0f || sy != 1.0f) {
                        dispW = static_cast<int>(std::llround(w * sx));
                        dispH = static_cast<int>(std::llround(h * sy));
                    }
                    CommitChromeResize(*target, target->Id(), static_cast<int>(w),
                                       static_cast<int>(h), dispW, dispH);
                    PushWindowListUnsafe();  // 태스크바 크기 표기 동기
                    reply = "{\"ok\":true}";
                }
            }
        }
    } else if (tool == "window_minimize") {
        // 최소화/복원 (docs/57 §14.16, 사용자 보고 "지뢰찾기 최소화" 무도구):
        // 태스크바 WindowMinimizeToggle과 동일 의미론 — 레이어 가시성 토글 +
        // 숨김 시 포커스 인계(다음 최상위로). 복원은 활성화까지(태스크바 클릭
        // 착지감). 대상 선정 뼈대는 window_fullscreen 복제 — 명시 id /
        // 생략 = 자기 창(control-only 생략형 no_window), shell은 창이 아니라
        // window_not_found(opus MINOR-2 승계). on=1 최소화 / on=0 복원 /
        // 생략 = 반전.
        int id = 0;
        JKClientConnection* target = nullptr;
        if (req.GetObjInt("args", "id", id)) {
            for (auto& c : clients_) {
                if (c && c->Id() == static_cast<uint32_t>(id)) { target = c.get(); break; }
            }
        } else if (!client.IsControlOnly()) {
            target = &client;
        }
        JKCompositorLayer* mnLayer = nullptr;
        if (target && !target->IsControlOnly() && !target->IsShell() && compositor_) {
            mnLayer = compositor_->FindLayerById(target->Id());
        }
        if (!mnLayer) {
            reply = (target ? "{\"ok\":false,\"error\":\"window_not_found\"}"
                            : "{\"ok\":false,\"error\":\"no_window\"}");
        } else {
            int onArg = -1;
            bool minimize = mnLayer->IsVisible();   // 생략 = 현재 상태 반전
            if (req.GetObjInt("args", "on", onArg) &&
                (onArg == 0 || onArg == 1)) {
                minimize = (onArg == 1);
            }
            if (minimize) {
                compositor_->SetLayerVisible(target->Id(), false);
                // 숨김이 포커스를 잃는다 — 다음 최상위 앱 창에 인계
                // (WindowMinimizeToggle 셸 경로와 동일 폴백).
                if (focusedClientId_ == target->Id()) {
                    FocusClient(compositor_->TopmostLayerId());
                }
            } else {
                compositor_->SetLayerVisible(target->Id(), true);
                FocusClient(target->Id());   // 복원 = 활성화(restore-on-activate)
            }
            PushWindowListUnsafe();  // minimized flag follows visibility
            reply = std::string("{\"ok\":true,\"minimized\":") +
                    (mnLayer->IsVisible() ? "false" : "true") + "}";
        }
    } else if (tool == "list_app_tools") {
        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.4): 평면 행 카탈로그
        // (레슨 39 — AgentJson에 중첩 배열 접근이 없어 브로커 tools/list
        // 합성과 agentctl face 양쪽이 평면 행을 먹는다). inputSchema는 등록
        // 원문 그대로 임베드(부재 시 {}), 나머지 필드는 JsonEsc.
        std::string out = "{\"ok\":true,\"tools\":[";
        bool first = true;
        for (const auto& kv : appToolManifests_) {
            const AppToolManifest& m = kv.second;
            for (const AppToolDef& t : m.tools) {
                if (!first) out += ",";
                first = false;
                out += "{\"app\":\"" + JsonEsc(m.app) + "\",\"name\":\"" +
                       JsonEsc(t.name) + "\",\"description\":\"" +
                       JsonEsc(t.description) + "\",\"inputSchema\":" +
                       (t.inputSchema.empty() ? "{}" : t.inputSchema) +
                       ",\"windowId\":" + std::to_string(m.windowId) +
                       ",\"title\":\"" + JsonEsc(m.title) +
                       "\",\"connId\":" + std::to_string(m.connId) +
                       // 스펙 §4: modal은 false도 명시 — 소비자 파싱 단순화.
                       ",\"modal\":" + (m.modal ? "true" : "false") + "}";
            }
        }
        reply = out + "]}";
    } else if (tool == "app_tool") {
        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.4): 중계. args는
        // 원문 패스스루(서버 스키마 검증 안 함 — 앱 계약). allow/ask 모두
        // 응답이 지연된다 — 기존 파킹 경로의 replied=false 기계를 그대로 쓴다
        // (allow 중계 = 앱의 AgentToolResult 대기, ask 파킹 = 승인 resolve가
        // 중계를 시작). deny만 즉답.
        std::string app, toolName, argsRaw;
        int windowIdArg = 0;
        const bool hasWindowId = req.GetObjInt("args", "windowId", windowIdArg);
        req.GetObjStr("args", "app", app);
        req.GetObjStr("args", "tool", toolName);
        req.GetObjRaw("args", "args", argsRaw);
        // 의미 커서 (스펙 2026-09-22-semantic-cursor §3): 커서 선언 앱의
        // <app>.move/read는 플랫폼 구현 — 릴레이 체인 전에 인터셉트한다
        // (move 동기 에코 / read는 snapshot 중계+커서 헤더 조립 지연응답).
        // act도 인터셉트해 사전 검증(kind enum/인자/격자)만 하고 — 유효
        // 요청은 false를 돌려 기존 릴레이 경로가 그대로 간다(kind/row/col
        // 원문, 게이트 ask 기본). 미선언 앱은 false(기존 릴레이).
        if ((toolName == "move" || toolName == "read" || toolName == "act") &&
            HandleCursorAppTool(client, queryId, app, toolName, argsRaw,
                                reply, replied)) {
            // 인터셉트됨 — 응답 완료(move) 또는 중계 지연(read).
        } else if (argsRaw.size() > 256 * 1024) {
            // 스펙 §4.1 호출 경로 상한 (result 16KiB는 HandleToolResult가).
            // 256KiB로 상향 (docs/60 §5 백로그 소각, docs/57 §12.6): 워크숍
            // set_script가 256KiB 백스톱을 두고 있어 8KiB 앞단이 병목이었다 —
            // 앱 측 백스톱과 정렬. result 16KiB 캡은 별도 백로그로 유지.
            reply = "{\"ok\":false,\"error\":\"args_too_large\"}";
        } else if (app.empty() || toolName.empty()) {
            reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
        } else {
            // 후보 수집 (스펙 §4.2): 등록된 (app, tool) 조합 역매칭 — 접두
            // 추측 금지. 같은 app의 다중 인스턴스(연결간 중복 등록 허용 —
            // HandleToolRegister 러링)가 복수 후보를 낸다: windowId 지정 =
            // 직행, 미지정+복수 = ambiguous+후보 목록(자기교정).
            std::vector<const AppToolManifest*> cands;
            for (const auto& kv : appToolManifests_) {
                for (const AppToolDef& t : kv.second.tools) {
                    if (kv.second.app == app && t.name == toolName) {
                        cands.push_back(&kv.second);
                        break;
                    }
                }
            }
            // 슬롯 리바인드 (docs/59 §13, 2026-09-20 폰 플로우 실측): 요청자
            // (jkagentd)는 CLI가 턴마다 재스폰하므로 연결 id가 턴마다 바뀌고,
            // 턴 종료의 요청자 회수기(final-review MINOR-2)가 슬롯을 전량
            // 비운다. 생존한 다이얼로그는 모달 가드(§5)에 영구 tool_gone —
            // 폰 플로우의 턴 2 내비게이션(filedlg_navigate/list/choose)이
            // 전면 파산했다. 슬롯이 "완전히" 비어 있고(회수/만료 — 소유
            // 다이얼로그도 기록 없음) 목표 모달 후보의 연결이 생존해 있으면,
            // 제어 채널 호출자를 새 요청자로 리바인드한다. 슬롯에 소유
            // 다이얼로그가 기록돼 있는 진짜 모달 경합은 기존 tool_gone 유지.
            // paramsTaken=true — 원 다이얼로그가 이미 소진했으므로 재전달
            // 금지. waitAsync=true — 리바인드 슬롯은 파킹 승인 항목이 없어
            // 해소는 file.open_result 이벤트 방송으로만 통지(아래 해소 경로).
            if (!cands.empty() && client.IsControlOnly() &&
                pendingFileDialog_.requesterConnId == 0 &&
                pendingFileDialog_.dialogConnId == 0 &&
                pendingFileDialog_.requestId == 0) {
                for (const AppToolManifest* c : cands) {
                    if (!c->modal) continue;
                    bool alive = false;
                    for (const auto& cc : clients_) {
                        if (cc && cc->Id() == c->connId &&
                            !cc->IsDisconnected()) {
                            alive = true;
                            break;
                        }
                    }
                    if (alive) {
                        pendingFileDialog_.requesterConnId = client.Id();
                        pendingFileDialog_.dialogConnId = c->connId;
                        pendingFileDialog_.paramsTaken = true;
                        pendingFileDialog_.waitAsync = true;
                        std::fprintf(stderr,
                                     "filedlg slot rebound: requester=%u dialog=%u\n",
                                     client.Id(), c->connId);
                        break;   // 슬롯 1개 — 첫 생존 모달 후보에 바인드
                    }
                }
            }
            // 모달 후보 선별 (스펙 §5 케이스 ③ — 고아+슬롯 재사용): 슬롯
            // 만료/요청자 회수/재사용 후에도 살아 있는 고아 다이얼로그가 후보에
            // 끼면 정상 슬롯 소유 다이얼로그의 호출까지 ambiguous로 봉쇄된다
            // (아래 단일 분기의 가드는 cands.size()==1에만 도달 — 복수 후보
            // 경로는 미커버였음). 수집 직후 모달 후보를 슬롯 소유로 선별해
            // 뿌리에서 걷어낸다. 판정은 앱이 아니라 슬롯 진실원
            // pendingFileDialog_.dialogConnId가 한다 — 단일 분기 가드와 동일
            // 조건(중복 제거해도 무방하나 최소 변경으로 유지).
            bool orphanPurgedAll = false;
            if (!cands.empty()) {
                std::vector<const AppToolManifest*> owned;
                for (const AppToolManifest* c : cands) {
                    if (!c->modal ||
                        pendingFileDialog_.dialogConnId == c->connId) {
                        owned.push_back(c);
                    }
                }
                orphanPurgedAll = owned.empty();
                cands.swap(owned);
            }
            // windowId 지정 = 그 창 직행 (스펙 §4.2 인스턴스 변별).
            if (hasWindowId && windowIdArg > 0) {
                std::vector<const AppToolManifest*> filtered;
                for (const AppToolManifest* c : cands)
                    if (c->windowId == static_cast<uint32_t>(windowIdArg))
                        filtered.push_back(c);
                cands.swap(filtered);
            }
            if (cands.empty()) {
                // 후보가 있었지만 모달 선별에서 전부 걷어졌으면 tool_gone(기존
                // 가드와 동일 페이로드) — 애초 후보가 없었던 경우는 기존
                // unknown_app_tool 유지(windowId 불일치로 비는 경로 포함).
                reply = (orphanPurgedAll
                             ? "{\"ok\":false,\"error\":\"tool_gone\"}"
                             : "{\"ok\":false,\"error\":\"unknown_app_tool\"}");
            } else if (cands.size() > 1) {
                // 스펙 §4.2: 묵시적 추측 라우팅 금지 — 후보 제시(자기교정).
                std::string list = "[";
                for (size_t i = 0; i < cands.size(); ++i) {
                    if (i) list += ",";
                    list += "{\"windowId\":" +
                            std::to_string(cands[i]->windowId) +
                            ",\"title\":\"" + JsonEsc(cands[i]->title) + "\"}";
                }
                reply = "{\"ok\":false,\"error\":\"ambiguous\",\"candidates\":" +
                        list + "]}";
            } else {
                const AppToolManifest* m = cands.front();
                // 모달 가드 (스펙 §5): 모달 도구는 "다이얼로그가 슬롯을
                // 소유한 동안만" 존재한다. 슬롯 만료(600s)/해소/재사용 후에도
                // 살아 있는 고아 다이얼로그의 도구 호출을 중계 전에 차단한다
                // (docs/48 MAJOR-1 교차결함 동류 — 판정은 앱이 아니라 슬롯
                // 진실원 pendingFileDialog_.dialogConnId가 한다).
                if (m->modal && pendingFileDialog_.dialogConnId != m->connId) {
                    reply = "{\"ok\":false,\"error\":\"tool_gone\"}";
                } else {
                    JKClientConnection* conn = nullptr;
                    for (const auto& c : clients_) {
                        if (c && c->Id() == m->connId && !c->IsDisconnected()) {
                            conn = c.get();
                            break;
                        }
                    }
                    if (!conn) {
                        // 매니페스트는 살아 있지만 연결이 끊김(정리 대기) —
                        // tool_gone과 동일 수명 사건 (스펙 §9).
                        reply = "{\"ok\":false,\"error\":\"unknown_app_tool\"}";
                    } else {
                        // 의미 커서 act (스펙 §3): 커서 선언 앱의 act는 게이트
                        // 기본값 ask(파킹) — permissions.json의 app_tool.* 키
                        // (allow/ask/deny)가 있으면 그 값이 이긴다(스펙 §8
                        // "앱별 act allow 선택지는 permissions.json 운용").
                        AgentDecision gate = AppToolAllowed(
                            app, toolName,
                            (m->cursor.valid && toolName == "act")
                                ? AgentDecision::Ask
                                : AgentDecision::Allow);
                        switch (gate) {
                            case AgentDecision::Deny:
                                // 스펙 §9 게이트 표면 — capture_ask 선례의 "denied".
                                reply = "{\"ok\":false,\"error\":\"denied\"}";
                                break;
                            case AgentDecision::Ask: {
                                // 스펙 §4.3: 기존 승인 파이프라인 재사용 —
                                // close_window ask 패턴 그대로 (구독자 체크 →
                                // approval_unavailable, PendingApproval push +
                                // agent.approval_request 방송, replied=false).
                                // kind="app_tool", name=app+"."+tool,
                                // targetId=windowId(Task 8 하이라이트 소비).
                                bool subscriber = false;
                                for (const auto& c : clients_) {
                                    if (c && c->AgentEventSubscriber() &&
                                        !c->IsDisconnected()) {
                                        subscriber = true;
                                        break;
                                    }
                                }
                                if (!subscriber) {
                                    reply = "{\"ok\":false,"
                                            "\"error\":\"approval_unavailable\"}";
                                    break;
                                }
                                if (ApprovalParkingFull(client.Id())) {
                                    reply = "{\"ok\":false,"
                                            "\"error\":\"approval_overflow\"}";
                                    break;
                                }
                                PendingApproval p;
                                p.kind = "app_tool";
                                p.requestId = nextApprovalId_++;
                                p.queryId = queryId;
                                p.requesterId = client.Id();
                                p.targetId = m->windowId;
                                p.expiresAt = std::time(nullptr) + 60;
                                // 배너 표기(스펙 §5 1단)는 p.name 하나를 소스로
                                // 삼는다 — 그리는 쪽에서 app/tool을 다시 조립하지
                                // 않게 app+"."+tool을 파킹 시점에 확정.
                                p.name = app + "." + toolName;
                                // 의미 커서 (스펙 2026-09-22-semantic-cursor
                                // §3 act, Task 2): 파킹 시점에 최신 선언으로
                                // 셀 rect를 고정한다 — 배너 문구
                                // "<app>.<kind> at (r,c)"와 승인 하이라이트의
                                // 진실원. 유효성(kind enum/인자/격자 내)은
                                // HandleCursorAppTool이 앱 도달 전 검증하므로
                                // 이 분기는 항상 성공한다(파싱 실패 시 기존
                                // <app>.<tool> 유지 — 방어적 폴백, rect 고정
                                // 없이는 승인 시점 재검증이 선언 불변으로
                                // 통과시켜 릴레이가 원문을 때리므로 안전).
                                if (m->cursor.valid && toolName == "act") {
                                    jk::agent::AgentJson a(argsRaw.empty()
                                                               ? "{}"
                                                               : argsRaw);
                                    std::string kind;
                                    int row = 0, col = 0;
                                    if (a.GetStr("kind", kind) &&
                                        a.GetInt("row", row) &&
                                        a.GetInt("col", col)) {
                                        // reset은 위치 무의미한 정의 전이
                                        // (스펙 §4 리셋=좌상단) — 앱도 좌표를
                                        // 무시하므로 셀 링/배너 좌표 표기 없음
                                        // (폰 실전 1판: "reset at (0,0)"이
                                        // 오해를 산 실측 — docs/64 §8). LLM 인자
                                        // 스키마는 계약 유지(row/col 필수 —
                                        // 자리 채움값만 배너에 안 나온다).
                                        const bool positionless =
                                            (kind == "reset");
                                        p.semCell = !positionless;
                                        p.semKind = kind;
                                        p.semRow = row;
                                        p.semCol = col;
                                        p.semRectX =
                                            m->cursor.originX +
                                            col * m->cursor.cellW;
                                        p.semRectY =
                                            m->cursor.originY +
                                            row * m->cursor.cellH;
                                        p.semRectW = m->cursor.cellW;
                                        p.semRectH = m->cursor.cellH;
                                        p.name = positionless
                                            ? app + "." + kind
                                            : app + "." + kind + " at (" +
                                                  std::to_string(row) + "," +
                                                  std::to_string(col) + ")";
                                    }
                                }
                                p.appToolApp = app;
                                p.appToolTool = toolName;
                                p.appToolArgs = argsRaw;
                                p.appToolConnId = m->connId;
                                // 스펙 §5 3단: 대상 창 "조작 전" 썸네일 — 캡처
                                // 실패는 비치명(thumb 필드만 생략, 승인 흐름은
                                // 계속). 제어 연결 매니페스트(windowId=0)는 레이어
                                // 부재로 자연 실패한다. 파일명 규약 =
                                // capture_window의 shot_<ts>_<id>.png와 동일 형식.
                                // 서버 루프 스레드에서의 GPU readback은
                                // capture_window가 이미 이 스레드에서 하는 비용과
                                // 같다(스레드 추가 없음).
                                std::string thumb;
                                {
                                    const std::string sdir =
                                        StateDir() + "/screenshots";
                                    std::error_code dirEc;
                                    std::filesystem::create_directory(
                                        std::filesystem::path(sdir), dirEc);
                                    char tbuf[512];
                                    std::snprintf(tbuf, sizeof(tbuf),
                                                  "%s/approval_%lld_%u.png",
                                                  sdir.c_str(),
                                                  static_cast<long long>(
                                                      std::time(nullptr)),
                                                  p.requestId);
                                    if (CaptureLayerToPng(m->windowId, tbuf))
                                        thumb = tbuf;
                                }
                                char buf[2048];
                                std::snprintf(buf, sizeof(buf),
                                              "{\"topic\":\"agent.approval_request\","
                                              "\"request\":%u,\"tool\":\"app_tool\","
                                              "\"kind\":\"app_tool\","
                                              "\"name\":\"%s\","
                                              "\"target_id\":%u,\"title\":\"%s\","
                                              "\"target\":{\"app\":\"%s\","
                                              "\"tool\":\"%s\",\"windowId\":%u,"
                                              "\"title\":\"%s\"}"
                                              "%s%s%s,"
                                              "\"ts\":%lld}",
                                              p.requestId, JsonEsc(p.name).c_str(),
                                              m->windowId,
                                              JsonEsc(m->title).c_str(),
                                              JsonEsc(app).c_str(),
                                              JsonEsc(toolName).c_str(),
                                              m->windowId,
                                              JsonEsc(m->title).c_str(),
                                              thumb.empty() ? "" : ",\"thumb\":\"",
                                              thumb.empty()
                                                  ? ""
                                                  : JsonEsc(thumb).c_str(),
                                              thumb.empty() ? "" : "\"",
                                              static_cast<long long>(
                                                  std::time(nullptr)) * 1000);
                                pendingApprovals_.push_back(p);
                                PushAgentEventJson(buf);
                                replied = false;  // 승인 resolve(또는 만료)가 응답
                                break;
                            }
                            case AgentDecision::Allow: {
                                // 스펙 §9 시퀀싱: 즉시 중계 + 타이머 시작. 응답은
                                // 앱의 AgentToolResult(HandleToolResult)가 queryId로
                                // 회송 — replied=false. expiresAt은 여기서 설정하므로
                                // 승인 대기 시간이 타임아웃을 갉지 않는다.
                                const uint32_t reqId = nextToolReqId_++;
                                InflightAppTool inf;
                                inf.reqId = reqId;
                                inf.queryId = queryId;
                                inf.requesterConnId = client.Id();
                                inf.targetConnId = conn->Id();
                                inf.windowId = m->windowId;
                                inf.expiresAt = std::time(nullptr) + 10;
                                // MINOR-3 — reset act 즉시 allow 경로: ok 회송
                                // 시 커서 정의 전이((0,0)) 리셋(스펙 §4).
                                if (m->cursor.valid && toolName == "act") {
                                    jk::agent::AgentJson a(argsRaw.empty()
                                                               ? "{}"
                                                               : argsRaw);
                                    std::string kind;
                                    int row = 0, col = 0;
                                    if (a.GetStr("kind", kind)) {
                                        if (kind == "reset") {
                                            inf.resetCursorOnOk = true;
                                        } else if (a.GetInt("row", row) &&
                                                   a.GetInt("col", col)) {
                                            // 폰 실전 — act 성공 시 커서가
                                            // acted 칸으로 이동(docs/64 §8).
                                            inf.semAct = true;
                                            inf.semRow = row;
                                            inf.semCol = col;
                                        }
                                    }
                                }
                                inflightAppTools_[reqId] = inf;
                                std::string callJson = "{\"app\":\"" + JsonEsc(app) +
                                    "\",\"tool\":\"" + JsonEsc(toolName) +
                                    "\",\"args\":" +
                                    (argsRaw.empty() ? "{}" : argsRaw) + "}";
                                ipc::WriteAgentToolCall(conn->Transport(), reqId,
                                                        callJson);
                                replied = false;  // HandleToolResult가 응답한다
                                break;
                            }
                        }
                    }
                }  // 모달 가드 else
            }
        }
    } else if (tool == "close_window") {
        int id = 0;
        JKClientConnection* target = nullptr;
        // 타깃 해소 확장(채팅 F1 — plan 2026-10-08-chat-close-fix): 기존
        // args.id 계약은 그대로(기존 프로브·도구의 주소 직접호출 보존)하고,
        // id 미지정 시 chat 라우터가 이미 해소해 둔 args.app 제목 매칭 →
        // 그것도 없으면 서버 포커스 창(폰 "닫아줘" — 창 부재 시 정직
        // window_not_found 유지). 해소는 여기(target 선정)만 — 아래 권한
        // 행렬(가능/평가/거부)은 원문 그대로 통과(우회 금지 계약). 후보는
        // list_windows와 동일 필터(단절·control-only·shell 제외).
        std::string appArg;
        const bool hasId = req.GetObjInt("args", "id", id);
        req.GetObjStr("args", "app", appArg);
        if (hasId || !appArg.empty() || focusedClientId_ != 0) {
            std::vector<AgentWindowRef> wins;
            for (auto& c : clients_) {
                if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) {
                    continue;
                }
                wins.push_back(AgentWindowRef{
                    c->Id(), c->Title(), focusedClientId_ == c->Id()});
            }
            const uint32_t resolved = ResolveAgentWindowTarget(
                wins, hasId, hasId ? static_cast<uint32_t>(id) : 0, appArg);
            if (resolved != 0) {
                for (auto& c : clients_) {
                    if (c && c->Id() == resolved) { target = c.get(); break; }
                }
            }
        }
        if (!target || target->IsControlOnly()) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            switch (AgentToolAllowed("close_window")) {
                case AgentDecision::Allow: {
                    // Server-initiated close: the client's read loop treats
                    // Close as quit; the disconnect cleanup path then removes
                    // the layer and fires window.destroyed.
                    ipc::WriteMessage(target->Transport(), ipc::MsgType::Close,
                                      std::vector<uint8_t>{});
                    reply = "{\"ok\":true}";
                    break;
                }
                case AgentDecision::Ask: {
                    // M2 chat inline approval: park the query and broadcast
                    // the request — the reply goes out only when the approval
                    // resolves (or the expiry scan answers with timeout).
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = target->Id();
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"close_window\","
                                  "\"target_id\":%u,\"title\":\"%s\",\"ts\":%lld}",
                                  p.requestId, p.targetId,
                                  JsonEsc(target->Title()).c_str(),
                                  static_cast<long long>(std::time(nullptr)) * 1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    // M2a: the server-side gate denied it — permissions.json
                    // is the approval act (same file the broker reads).
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "trust_request") {
        // Script trust gate (docs/37 spec): jktriggers asks before the first
        // eval of an unknown fingerprint. Default permission is "ask" — the
        // same inline-approval pipeline as close_window. The server only
        // relays the decision; the loader owns trust.json.
        std::string name, origin, fingerprint;
        req.GetObjStr("args", "name", name);
        req.GetObjStr("args", "origin", origin);
        req.GetObjStr("args", "fingerprint", fingerprint);
        // Trim the name — whitespace-only is still missing (final-review fix:
        // validate before parking an approval, so no face can feed a
        // malformed fingerprint/origin into the broadcast payload).
        {
            const size_t b = name.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) {
                name.clear();
            } else {
                name = name.substr(b, name.find_last_not_of(" \t\r\n") - b + 1);
            }
        }
        if (name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else if (name.size() > 96) {
            // docs/38: bound the display name before it reaches the approval
            // broadcast payload and the parked PendingApproval (same shape as
            // bad_fingerprint — validated before parking).
            reply = "{\"ok\":false,\"error\":\"bad_name\"}";
        } else if (!ValidFingerprint(fingerprint)) {
            reply = "{\"ok\":false,\"error\":\"bad_fingerprint\"}";
        } else if (origin != "dev" && origin != "package") {
            reply = "{\"ok\":false,\"error\":\"bad_origin\"}";
        } else {
            switch (AgentToolAllowed("trust_request")) {
                case AgentDecision::Allow:
                    // permissions.json says allow — every script is trusted.
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "trust_request";
                    p.name = name;
                    p.origin = origin;
                    p.fingerprint = fingerprint;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[1024];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"trust_request\","
                                  "\"kind\":\"trust_request\",\"name\":\"%s\","
                                  "\"origin\":\"%s\",\"fingerprint\":\"%s\","
                                  "\"ts\":%lld}",
                                  p.requestId, JsonEsc(name).c_str(),
                                  JsonEsc(origin).c_str(),
                                  JsonEsc(fingerprint).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "permission_set") {
        // 스펙 §2.2: 매트릭스 쓰기 — AgentToolAllowed("permission_set")은
        // 파일값 무시하고 항상 Ask(하드코딩, §2.2 2단 우회 봉쇄). 검증은
        // 파킹 전(trust_request의 bad_* 선례).
        std::string permTool, decision;
        req.GetObjStr("args", "tool", permTool);
        req.GetObjStr("args", "decision", decision);
        bool known = false;
        for (const AgentPermRow& r : kPermMatrix) {
            if (permTool == r.tool) { known = true; break; }
        }
        if (permTool.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_tool\"}";
        } else if (!known) {
            reply = "{\"ok\":false,\"error\":\"unknown_tool\"}";
        } else if (decision != "allow" && decision != "ask" &&
                   decision != "deny") {
            reply = "{\"ok\":false,\"error\":\"bad_decision\"}";
        } else if (permTool == "permission_set") {
            // fixed-ask 행은 WritePermissionsEntry가 스킵하므로 승인해도
            // 파일이 불변 — written:true 거짓 보고 대신 즉답 거절(리뷰 MINOR).
            reply = "{\"ok\":false,\"error\":\"bad_target\"}";
        } else {
            switch (AgentToolAllowed("permission_set")) {
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,"
                                "\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "permission_set";
                    p.permTool = permTool;
                    p.permDecision = decision;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"permission_set\","
                                  "\"kind\":\"permission_set\","
                                  "\"target_tool\":\"%s\","
                                  "\"decision\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(permTool).c_str(),
                                  decision.c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;   // 해소 시 답신
                    break;
                }
                case AgentDecision::Allow:
                case AgentDecision::Deny:
                default:
                    // 도달하지 않는다(고정 Ask) — 방어선으로 deny 유지.
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "trust_revoke") {
        // 스펙 §2.3: 신뢰 해지 — 안전 방향이지만 무게이트는 신뢰 저장소
        // 전면 소각 DoS 통로. 기본 Ask, 파일로 allow/deny 변경 가능(해지는
        // 권한 부여가 아니라 2단 우회 위험이 없다). 파킹 전 검증+실측.
        std::string fingerprint;
        req.GetObjStr("args", "fingerprint", fingerprint);
        std::string rec, name;
        if (fingerprint.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_fingerprint\"}";
        } else if (!ValidFingerprint(fingerprint)) {
            reply = "{\"ok\":false,\"error\":\"bad_fingerprint\"}";
        } else {
            std::string trustText;
            const std::string dir = ExeDirNoSlash();
            if (std::FILE* f = std::fopen(
                    (dir + "/state/trust.json").c_str(), "rb")) {
                // 전체 읽기 (docs/53 §9 잔여 — RevokeTrustRecord와 동일 근거):
                // 64KB 캡이면 뒤쪽 레코드가 not_found로 미끄러져 해지가
                // RMW까지 못 간다. 8MiB 상한 = 이상 파일 가드.
                std::fseek(f, 0, SEEK_END);
                const long tsz = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);
                if (tsz > 0) {
                    const size_t tcap = std::min<size_t>(
                        static_cast<size_t>(tsz), 8u * 1024 * 1024);
                    std::vector<char> tbuf(tcap + 1, '\0');
                    const size_t tn = std::fread(tbuf.data(), 1, tcap, f);
                    tbuf[tn] = '\0';
                    trustText = tbuf.data();
                }
                std::fclose(f);
            }
            if (!TrustRecordText(trustText, fingerprint, rec)) {
                reply = "{\"ok\":false,\"error\":\"not_found\"}";
            } else {
                jk::agent::AgentJson(rec.c_str()).GetStr("name", name);
                switch (AgentToolAllowed("trust_revoke")) {
                    case AgentDecision::Ask: {
                        bool subscriber = false;
                        for (auto& c : clients_) {
                            if (c && c->AgentEventSubscriber() &&
                                !c->IsDisconnected()) {
                                subscriber = true;
                                break;
                            }
                        }
                        if (!subscriber) {
                            reply = "{\"ok\":false,"
                                    "\"error\":\"approval_unavailable\"}";
                            break;
                        }
                        if (ApprovalParkingFull(client.Id())) {
                            reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                            break;
                        }
                        PendingApproval p;
                        p.kind = "trust_revoke";
                        p.name = name.empty() ? "script" : name;
                        p.fingerprint = fingerprint;
                        p.requestId = nextApprovalId_++;
                        p.queryId = queryId;
                        p.requesterId = client.Id();
                        p.targetId = 0;
                        p.expiresAt = std::time(nullptr) + 60;
                        char buf[1024];
                        std::snprintf(buf, sizeof(buf),
                                      "{\"topic\":\"agent.approval_request\","
                                      "\"request\":%u,\"tool\":\"trust_revoke\","
                                      "\"kind\":\"trust_revoke\",\"name\":\"%s\","
                                      "\"fingerprint\":\"%s\",\"ts\":%lld}",
                                      p.requestId, JsonEsc(p.name).c_str(),
                                      fingerprint.c_str(),
                                      static_cast<long long>(
                                          std::time(nullptr)) * 1000);
                        pendingApprovals_.push_back(p);
                        PushAgentEventJson(buf);
                        replied = false;
                        break;
                    }
                    case AgentDecision::Allow:
                        // 파일이 allow로 명시한 설치 — 즉시 해지(사용자가
                        // 매트릭스에서 그렇게 정한 것).
                        {
                            const std::string err =
                                RevokeTrustRecord(fingerprint);
                            reply = err.empty()
                                ? "{\"ok\":true,\"written\":true,"
                                  "\"restart_needed\":true}"
                                : "{\"ok\":false,\"error\":\"" + err + "\"}";
                        }
                        break;
                    case AgentDecision::Deny:
                    default:
                        reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                        break;
                }
            }
        }
    } else if (tool == "launch_app") {
        std::string app, jkx;
        req.GetObjStr("args", "app", app);
        req.GetObjStr("args", "jkx", jkx);
        // 런치 진실성 (2026-09-24 LLM 실전 발각): 스폰 전 존재 검증. 스폰은
        // 비동기 사망한다 — app은 jkapp_<app><접미>(win32 .dll / posix .so —
        // AppModuleSuffix())가 없으면 자식이 즉시 exit,
        // jkx는 경로가 없으면 jkx.Open 실패 exit — 그런데도 무조건 ok:true는
        // "accepted 후 침묵"(docs/59 §13 폰 플로우 실측) 동형 결함이었다.
        // LLM이 스키마의 jkx 키를 골라 죽은 스폰을 ok:true로 받고 침묵하는
        // 실측이 정확히 이 구멍이다.
        //   app: jkapp_<app><접미>(win32 .dll / posix .so — AppModuleSuffix())
        //        존재 검사(런처와 같은 exeDir 기준) — 단
        //        terminal:/filedlg: 접두 관례(스폰 전용 경로)는 검사 면제.
        //   jkx: 주어진 경로 우선, 없으면 exeDir/apps/<bare>.jkx 폴백 해석
        //        (런처의 apps/ 열거와 같은 기준 — bare 이름을 쓰는 LLM을
        //        살리는 쪽). 둘 다 없으면 unknown_jkx.
        // jk::fs::GetExecutablePath 흡수 (docs/68 W5). 원문 규약: 추출 실패/
        // 구분자 없음 시 exeDir 빈값 유지.
        std::string exeDir;
        const std::string exePathLaunch = jk::fs::GetExecutablePath();
        if (!exePathLaunch.empty()) {
            const size_t launchCut = exePathLaunch.find_last_of("\\/");
            if (launchCut != std::string::npos)
                exeDir = exePathLaunch.substr(0, launchCut);
        }
        auto fileExistsFn = [](const std::string& p) {
            // 원문 관측: GetFileAttributesA는 속성 비트를 소비하지 않는다 —
            // 존재 bool만(INVALID_FILE_ATTRIBUTES가 아니면 참). std::
            // filesystem::exists 관측 동형(존재가 아닌 모든 stat 실패=false).
            // ec 중립형 필수 — p는 클라 문자열이 들어오고 이 TU는 무 try/catch
            // (review r1 HIGH).
            std::error_code existsEc;
            return std::filesystem::exists(std::filesystem::path(p), existsEc);
        };
        if (!app.empty()) {
            const bool prefixed = app.find(':') != std::string::npos;
            // 접미만 플랫폼 값(JKWindowServer.h AppModuleSuffix) — 존재 bool 관측
            // 동일. 상단 주석의 접미 표기는 win32 .dll / posix .so로 정규화
            // (G1 리뷰 승계).
            const std::string dllPath =
#if defined(__ANDROID__)
                // jkdesktop-apk: 모듈이 nativeLibraryDir의 lib 접두로 패키징된다
                // (jniLibs 규약) — 존재 검사도 lib 접두로.
                exeDir + "/libjkapp_" + app + AppModuleSuffix();
#else
                exeDir + "/jkapp_" + app + AppModuleSuffix();
#endif
            // [2026-11-20] 구두 "비디오 틀어줘" leg — 엔진 vplayer 모듈은 FFmpeg
            // 의존이라 APK에 없다. 플랫폼 재생기(Activity)로 위임하고 정직하게
            // ok:true(via=platform)를 돌려준다(종전 = unknown_app).
            // 주의: 함수 말미에 저널 append + 실제 회신 송신이 있으므로 **조기
            // return 금지**(회신 유실) — 분기로만 스폰을 건너뛴다.
            bool platformHandled = false;
#if defined(__ANDROID__)
            if (app == "vplayer" || app == "video") {
                if (JkPlayVideo) {
                    (void)JkPlayVideo("");
                    reply = "{\"ok\":true,\"via\":\"platform\"}";
                    platformHandled = true;
                }
            }
#endif
            if (platformHandled) {
                // 재생기 위임 완료 — 모듈 스폰/폴백 생략(reply 확정).
            } else if (prefixed || exeDir.empty() || fileExistsFn(dllPath)) {
                // docs/35: pair the capture overlay with the client that asked
                // for it (see overlaySpawner_ member comment).
                pendingSnapSpawnerConnId_ = (app == "snap") ? client.Id() : 0;
                SpawnClient(app.c_str(), false);
                reply = "{\"ok\":true}";
            } else {
                // app→jkx 폴백 (2026-09-24 폰 눈확인 발각): 워크숍 등 .jkx
                // 패키지 앱을 스키마의 내장 앱 이름 목록과 혼동해
                // {"app":"workshop"}으로 부르는 실측이 있다 — jkapp_<app><접미>
                // (win32 .dll / posix .so — AppModuleSuffix())가 없어도
                // apps/<app>.jkx가 있으면 컨테이너로 스폰해 두 호출
                // 형태를 모두 살린다(설명 드리프트가 LLM을 막히게 하지 않는다).
                const std::string jkxCandidate =
                    exeDir + "/apps/" + app + ".jkx";
                if (fileExistsFn(jkxCandidate)) {
                    const bool spawned =
                        SpawnClient(jkxCandidate.c_str(), true);
#ifdef _WIN32
                    (void)spawned;     // win32 관측 바이트 보존 — 스폰 실패도
                    reply = "{\"ok\":true,\"via\":\"jkx\"}";
#else
                    // posix(플랜 F5/F7): fromJkx는 v1 프리게이트로 false —
                    // 거짓 ok:true 대신 실패를 보고한다(posix만).
                    reply = spawned ? "{\"ok\":true,\"via\":\"jkx\"}"
                        : std::string("{\"ok\":false,\"error\":\"spawn_failed\","
                                      "\"jkx\":\"") + jkxCandidate + "\"}";
#endif
                } else {
                    reply = "{\"ok\":false,\"error\":\"unknown_app\",\"app\":\"" +
                            app +
                            "\",\"hint\":\"not a built-in app and no apps/<app>"
                            ".jkx package exists — use jkx with a path or a "
                            "bare package name\"}";
                }
            }
        } else if (!jkx.empty()) {
            // 경로 표기 변주 수용 (2026-09-24 폰 실전 2차 발각): 같은 패키지를
            // LLM이 "workshop" / "workshop.jkx" / "apps/workshop.jkx"로 번갈아
            // 불렀다 — bare 이름만 해석하던 첫 폴백은 나머지 두 형태를
            // unknown_jkx로 죽였다(원본 트랜스크립트 실측). 정규화:
            // '/'→'\\', 끝 .jkx 탈락, 선두 apps\ 탈락 후 후보 4종 순차 검사.
            std::string norm = jkx;
            for (char& ch : norm) {
                if (ch == '/') ch = '\\';
            }
            const size_t n = norm.size();
            if (n > 4 && norm[n - 4] == '.' &&
                (norm[n - 3] == 'j' || norm[n - 3] == 'J') &&
                (norm[n - 2] == 'k' || norm[n - 2] == 'K') &&
                (norm[n - 1] == 'x' || norm[n - 1] == 'X')) {
                norm = norm.substr(0, n - 4);
            }
            if (norm.rfind("apps\\", 0) == 0) norm = norm.substr(5);
            std::string resolved;
            const std::string candidates[4] = {
                jkx,                                   // 원문 (절대 경로 등)
                exeDir + "/" + jkx,                    // exeDir 기준 원문
                exeDir + "/apps/" + norm + ".jkx",     // 정규화 이름
                exeDir + "/" + norm + ".jkx",
            };
            for (const auto& c : candidates) {
                if (!c.empty() && fileExistsFn(c)) {
                    resolved = c;
                    break;
                }
            }
            if (resolved.empty()) {
                reply = "{\"ok\":false,\"error\":\"unknown_jkx\",\"jkx\":\"" +
                        jkx + "\"}";
            } else {
#ifdef _WIN32
                bool spawned = SpawnClient(resolved.c_str(), true);
                (void)spawned;     // win32 관측 바이트 보존 — 스폰 실패도
                                   // 기존 ok:true 유지(G4 계약)
                reply = "{\"ok\":true}";
#else
                bool spawned = SpawnClient(resolved.c_str(), true);
                // posix(플랜 F5/F7): fromJkx는 v1 프리게이트로 false — 거짓
                // ok:true 대신 도구에 실패를 보고한다(posix만).
                if (spawned) {
                    reply = "{\"ok\":true}";   // 기존 성공 reply 보존
                } else {
                    reply = std::string(
                        "{\"ok\":false,\"error\":\"spawn_failed\","
                        "\"jkx\":\"") + resolved + "\"}";
                }
#endif
            }
        } else {
            reply = "{\"ok\":false,\"error\":\"missing_app\"}";
        }
    } else if (tool == "send_input") {
        // 앱 정복 사다리 (스펙 2026-09-21-conquest-ladder §3.1): 트랙 B 조작
        // 수단 — 대상 창에 합성 입력. ask 기본 게이트(run_console_app와 같은
        // inline-approval 파이프라인, 승인 시점 원 요청 재실행 = files_access
        // 선례). 원문 args는 승인 재실행을 위해 파킹에 함께 저장한다.
        std::string rawArgs;
        req.GetRaw("args", rawArgs);
        jk::agent::AgentJson args(rawArgs);
        SendInputOp op;
        const std::string buildErr =
            args.ok() ? BuildSendInputOp(args, &op) : "bad_args";
        if (!buildErr.empty()) {
            reply = "{\"ok\":false,\"error\":\"" + buildErr + "\"}";
        } else {
            switch (AgentToolAllowed("send_input")) {
                case AgentDecision::Allow: {
                    const std::string ex = ExecuteSendInputOp(op);
                    reply = ex.empty() ? "{\"ok\":true,\"sent\":true}"
                                       : "{\"ok\":false,\"error\":\"" + ex + "\"}";
                    break;
                }
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "send_input";
                    p.name = op.op;           // 승인 스트립 표시용 조작명
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = op.target;
                    p.sendArgs = rawArgs;     // 승인 시점 원 요청 재실행 원문
                    p.expiresAt = std::time(nullptr) + 60;
                    // 대상 식별 (최종리뷰 Important 1): close_window 선례 형태
                    // (:3262) — 승인 스트립이 조작(op)만이 아니라 어느 창에
                    // 가해지는지 보여야 한다. 해소 실패 시 빈 문자열.
                    std::string targetTitle;
                    for (auto& c : clients_) {
                        if (c && c->Id() == op.target &&
                            !c->IsDisconnected()) {
                            targetTitle = c->Title();
                            break;
                        }
                    }
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"send_input\","
                                  "\"name\":\"%s\",\"target_id\":%u,"
                                  "\"title\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(op.op).c_str(),
                                  p.targetId, JsonEsc(targetTitle).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // 승인 해소 시 응답
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "run_console_app") {
        // P4 SDK §5: 에이전트가 콘솔 앱(apps/<name>/manifest.json)을 스폰.
        // ask 기본 게이트 — close_window/trust_request와 같은 inline-approval
        // 파이프라인. target 창이 없으므로 targetId=0, 식별자는 앱 name.
        std::string name;
        req.GetObjStr("args", "name", name);
        std::string cmd, dir, fp;
        if (name.empty() || name.size() > 64) {
            reply = "{\"ok\":false,\"error\":\"bad_name\"}";
        } else if (!shell_ ||
                   !shell_->ConsoleAppInfo(name, cmd, dir, fp)) {
            reply = "{\"ok\":false,\"error\":\"unknown_app\"}";
        } else {
            switch (AgentToolAllowed("run_console_app")) {
                case AgentDecision::Allow:
                    SpawnConsoleApp(cmd, dir, name);
                    reply = "{\"ok\":true}";
                    break;
                case AgentDecision::Ask: {
                    bool subscriber = false;
                    for (auto& c : clients_) {
                        if (c && c->AgentEventSubscriber() &&
                            !c->IsDisconnected()) {
                            subscriber = true;
                            break;
                        }
                    }
                    if (!subscriber) {
                        reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                        break;
                    }
                    if (ApprovalParkingFull(client.Id())) {
                        reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                        break;
                    }
                    PendingApproval p;
                    p.kind = "run_console_app";
                    p.name = name;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[512];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"run_console_app\","
                                  "\"name\":\"%s\",\"ts\":%lld}",
                                  p.requestId, JsonEsc(name).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                    break;
                }
                case AgentDecision::Deny:
                default:
                    reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
                    break;
            }
        }
    } else if (tool == "theme_set") {
        // P3 hot-swap (docs/52): write theme.json (the same truth the boot
        // loader and every client's 500ms mtime poll read) then swap
        // in-process. Paint-time consumers (shell, JKDC default args) follow
        // instantly; ctor-captured widget tokens re-capture via the client
        // poll's ApplyTheme walk. Allow by default — appearance only.
        std::string preset;
        req.GetObjStr("args", "preset", preset);
        const jk::theme::JKTheme* t = nullptr;
        if (preset == "light") t = &jk::theme::kLight;
        else if (preset == "classic") t = &jk::theme::kClassic;
        else if (preset == "dark") t = &jk::theme::kDefault;
        if (!t) {
            reply = "{\"ok\":false,\"error\":\"bad_preset\"}";
        } else {
            jk::theme::setTheme(t);
            // (T2 fix r1 NT2-1 가문) 테마 핫스왑 = 셸 색 재칠 — rect 사건이
            // 없는 변화라 다음 합성이 스킵 분기로 먹는 회귀 봉합. 드문 사건이라
            // 전체 프레젠트(리뷰 처방: "색 전체 재칠"). 클라 재칠은 각 poll
            // ApplyTheme 몫(원문 유지).
            if (compositor_) {
                compositor_->RequestFullPresent();
            }
            if (!jk::theme::WriteThemePresetFile(preset)) {
                // Swap happened in-process but the truth file didn't land —
                // clients' mtime poll would never follow. Fail loudly
                // instead of replying ok (docs/52 review MINOR).
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                char buf[96];
                std::snprintf(buf, sizeof(buf),
                              "{\"ok\":true,\"preset\":\"%s\"}",
                              preset.c_str());
                reply = buf;
            }
        }
    } else if (tool == "settings_read") {
        // 스펙 2026-09-18-settings-hub §2.2: 현재 설정 수집 — B 진화 씨앗
        // 봉투(key/kind/value). bool은 int 0/1로 직렬화(AgentJson 파서
        // 계약), ts는 epoch 초. 수집 원천: theme.json(exe 옆),
        // state/triggers.json, state/idle_minutes, 서버 KV 멤버,
        // state/layout_*.json, receipts.jsonl 꼬리.
        std::string out = "{\"ok\":true,\"settings\":[";
        // 2048: text.font_path 300자가 JsonEsc 제어문자 6배 확장({ 등)까지
        // 갈 수 있다 — 640은 조용한 절단으로 봉투 전체를 파산시킨다.
        char item[2048];
        // theme.current: theme.json의 preset(부트 로더 진실원). 파일은 exe
        // 옆 — jk::theme::DefaultThemePath() (StateDir 아님).
        {
            std::string preset = "dark";
            std::FILE* f =
                std::fopen(jk::theme::DefaultThemePath().c_str(), "rb");
            if (f) {
                std::string text;
                char chunk[4096];
                size_t n;
                while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
                    text.append(chunk, n);
                std::fclose(f);
                jk::agent::AgentJson json(text);
                std::string p;
                if (json.ok() && json.GetStr("preset", p) && !p.empty())
                    preset = p;
            }
            std::snprintf(item, sizeof(item),
                          "{\"key\":\"theme.current\",\"kind\":\"string\","
                          "\"value\":\"%s\"}",
                          JsonEsc(preset).c_str());
            out += item;
        }
        // text.font_path (docs/63 §4): 서버 멤버 — 기동 시 KV+기본값 합성
        // (빈 문자열 = 오버라이드 없음, GUI가 기본 경로를 표시).
        std::snprintf(item, sizeof(item),
                      ",{\"key\":\"text.font_path\",\"kind\":\"string\","
                      "\"value\":\"%s\"}",
                      JsonEsc(textFontPath_).c_str());
        out += item;
        // text.font_fallback (docs/63 §6 2단계): 보조 폰트 체인 — 빈 문자열 =
        // 미설정(체인 없음).
        std::snprintf(item, sizeof(item),
                      ",{\"key\":\"text.font_fallback\",\"kind\":\"string\","
                      "\"value\":\"%s\"}",
                      JsonEsc(textFontFallback_).c_str());
        out += item;
        // text.font_scale (docs/63 §6 Task 3): 셀 확대 배율 문자열 float —
        // 미설정은 기본 "1.0"(멤버 초기값).
        std::snprintf(item, sizeof(item),
                      ",{\"key\":\"text.font_scale\",\"kind\":\"string\","
                      "\"value\":\"%s\"}",
                      JsonEsc(textFontScale_).c_str());
        out += item;
        // triggers: state/triggers.json 플래그(trigger_toggle의 진실원).
        {
            std::FILE* f =
                std::fopen((StateDir() + "/triggers.json").c_str(), "rb");
            if (f) {
                std::string text;
                char chunk[8192];
                size_t n;
                while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
                    text.append(chunk, n);
                std::fclose(f);
                jk::agent::AgentJson json(text);
                int cnt = 0;
                if (json.ok() && json.GetArraySize("triggers", cnt)) {
                    for (int i = 0; i < cnt && i < 64; ++i) {
                        std::string nm;
                        int en = 1;
                        if (!json.GetArrStr("triggers", i, "name", nm)) continue;
                        json.GetArrInt("triggers", i, "enabled", en);
                        std::snprintf(item, sizeof(item),
                                      ",{\"key\":\"trigger.%s\",\"kind\":"
                                      "\"bool\",\"value\":%d}",
                                      JsonEsc(nm).c_str(), en ? 1 : 0);
                        out += item;
                    }
                }
            }
        }
        // idle_minutes: state/idle_minutes 파일(trig_idle이 읽는 진실원).
        {
            int idle = 30;
            if (std::FILE* f =
                    std::fopen((StateDir() + "/idle_minutes").c_str(), "rb")) {
                char buf[32] = {};
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                buf[n] = '\0';
                const int v = std::atoi(buf);
                if (v >= 0) idle = v;
            }
            std::snprintf(item, sizeof(item),
                          ",{\"key\":\"idle_minutes\",\"kind\":\"int\","
                          "\"value\":%d}",
                          idle);
            out += item;
        }
        // 서버 KV 멤버(부팅 로드 — Init의 LoadSettingsKv).
        std::snprintf(
            item, sizeof(item),
            ",{\"key\":\"receipt_retention_days\",\"kind\":\"int\","
            "\"value\":%d},{\"key\":\"audio_master_mute\",\"kind\":\"bool\","
            "\"value\":%d},{\"key\":\"audio_master_volume\",\"kind\":\"int\","
            "\"value\":%d}",
            receiptRetentionDays_, audioMasterMute_ ? 1 : 0, audioMasterVolume_);
        out += item;
        // layouts: state/layout_*.json 열거 — key/value = 레이아웃 이름.
        // stage-3 task 5: 수기 FindFirstFileA 와일드카드 열거 → directory_
        // iterator + 접두/접미 필터. 원문 와일드카드 "layout_*.json"의 의미는
        // "layout_"로 시작하고 ".json"으로 끝나는 파일명이다(가운데 '*'은 0자
        // 이상 — win32 규약) → starts_with/ends_with 관측 동형, 소비자의
        // name.size() > 12 게이트(빈 이름 컷)는 그대로 남는다.
        {
            const std::string dir = StateDir();
            std::error_code openEc;
            std::filesystem::directory_iterator it(
                std::filesystem::path(dir), openEc);
            const std::filesystem::directory_iterator end;
            while (!openEc && it != end) {
                const std::string name = it->path().filename().string();
                // platform note: the win32 find-data wildcard match was
                // case-insensitive; starts_with is case-SENSITIVE. Real
                // layouts all write "layout_" lowercase, so accepted
                // divergence (docs/70 §6 #9).
                if (name.starts_with("layout_") && name.ends_with(".json")) {
                    // "layout_<name>.json" → <name> (7자 접두, 5자 확장자).
                    if (name.size() > 12) {
                        std::string bare = name.substr(7, name.size() - 12);
                        std::snprintf(item, sizeof(item),
                                      ",{\"key\":\"layout.%s\",\"kind\":\"string\","
                                      "\"value\":\"%s\"}",
                                      JsonEsc(bare).c_str(), JsonEsc(bare).c_str());
                        out += item;
                    }
                }
                std::error_code incEc;
                it.increment(incEc);
                if (incEc) break;  // 원문: FindNextFile 실패 = 열거 종료
            }
        }
        // receipts 통계: 총 행수 + 최근 ts(초). 꼬리 256KiB만 읽는다
        // (read_receipts의 상한 스캔과 같은 규모 — 이상 파일 가드).
        {
            long long rows = 0, lastTs = 0;
            const std::string path = StateDir() + "/receipts.jsonl";
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (f) {
                std::fseek(f, 0, SEEK_END);
                const long size = std::ftell(f);
                const long start = size > 262144 ? size - 262144 : 0;
                std::fseek(f, start, SEEK_SET);
                std::vector<char> buf(static_cast<size_t>(size - start) + 1);
                const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
                std::fclose(f);
                buf[n] = '\0';
                size_t pos = 0;
                while (pos < n) {
                    const char* begin = buf.data() + pos;
                    const char* nl = static_cast<const char*>(
                        std::memchr(begin, '\n', n - pos));
                    const size_t len =
                        nl ? static_cast<size_t>(nl - begin) : (n - pos);
                    if (len > 2) ++rows;
                    const std::string line(begin, len);
                    const size_t tp = line.find("\"ts\":");
                    if (tp != std::string::npos)
                        lastTs = std::atoll(line.c_str() + tp + 5);
                    if (!nl) break;
                    pos += len + 1;
                }
            }
            std::snprintf(item, sizeof(item),
                          "],\"receipts\":{\"rows\":%lld,\"last_ts\":%lld}",
                          rows, lastTs / 1000);
            out += item;
        }
        out += "}";
        reply = out;
    } else if (tool == "settings_set") {
        // 스펙 §2.2: 화이트리스트 KV. 응답에 적용 후 값(applied) — 클라
        // 단일 신뢰원(fullscreen 응답 규약). capture_allow만 키별 Ask:
        // 파킹(kind "capture_allow") — 에이전트/GUI 불문 전 경로, 승인은
        // jkchat 스트립(§2.2 키별 게이트 — 도구별 askCapable과 달리).
        std::string key;
        // 가드가 읽는 값은 int64 리더로 (docs/57 §13.3 파서 계약).
        int64_t val64 = 0;
        const bool hasInt = req.GetObjInt64("args", "value", val64);
        const int valInt = static_cast<int>(val64);
        req.GetObjStr("args", "key", key);
        if (key.empty()) {
            reply = "{\"ok\":false,\"error\":\"bad_key\"}";
        } else if (key == "idle_minutes") {
            if (!hasInt || val64 < 0 || val64 > 1440) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else {
                std::FILE* f = std::fopen(
                    (StateDir() + "/idle_minutes").c_str(), "wb");
                if (!f) {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                } else {
                    std::fprintf(f, "%d", valInt);
                    std::fclose(f);
                    reply = "{\"ok\":true,\"applied\":{\"idle_minutes\":" +
                            std::to_string(valInt) + "}}";
                }
            }
        } else if (key == "receipt_retention_days") {
            // 하한 7 (docs/54 §11 opus M4): receipts.jsonl은 에이전트 도구
            // 호출의 감사 증적 — 어느 경로에서든 하한 아래로 절단 불가.
            // (0 = 무기한은 현재값(관행) — set 불가(§2.2 표). GUI 콤보가
            // 제시하는 최소 프리셋도 7이므로 GUI 기능 손실 없음.)
            if (!hasInt || val64 < 7) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (std::string kvErr;
                       !WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        valInt, textFontPath_,
                                        textFontFallback_, textFontScale_,
                                        &kvErr)) {
                // I-1 (T2 fix r1) — 정직 err 1행: 거부 사유(detail)를 답신에
                // 실어 사용자에게 보인다(무음 리셋 체인의 "보이게 하기" 몫).
                reply = kvErr.empty()
                            ? "{\"ok\":false,\"error\":\"write_failed\"}"
                            : "{\"ok\":false,\"error\":\"write_failed\","
                              "\"detail\":\"" + JsonEsc(kvErr) + "\"}";
            } else {
                receiptRetentionDays_ = valInt;
                // KV 성공 후 즉시 정리 — 정리 실패는 reply에 표면화(KV는
                // 이미 적용됐지만 다음 set에서 재정리된다 — 정직 응답).
                reply = PruneReceipts(valInt)
                            ? ("{\"ok\":true,\"applied\":{"
                               "\"receipt_retention_days\":" +
                               std::to_string(valInt) + "}}")
                            : "{\"ok\":true,\"applied\":{\"receipt_"
                              "retention_days\":" + std::to_string(valInt) +
                              "},\"warning\":\"prune_failed\"}";
            }
        } else if (key == "audio_master_mute" || key == "audio_master_volume") {
            if (key == "audio_master_mute" && hasInt &&
                (val64 == 0 || val64 == 1)) {
                audioMasterMute_ = (val64 == 1);
            } else if (key == "audio_master_volume" && hasInt &&
                       val64 >= 0 && val64 <= 100) {
                audioMasterVolume_ = static_cast<int>(val64);
                audioMasterMute_ = false;  // 볼륨 조작은 음소거 해제 의미
            } else {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            }
            if (reply.empty()) {
                if (std::string kvErr;
                    !WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                     receiptRetentionDays_, textFontPath_,
                                     textFontFallback_, textFontScale_,
                                     &kvErr)) {
                    // I-1 (T2 fix r1) — 정직 err 1행(거부 사유 detail 실음).
                    reply = kvErr.empty()
                                ? "{\"ok\":false,\"error\":\"write_failed\"}"
                                : "{\"ok\":false,\"error\":\"write_failed\","
                                  "\"detail\":\"" + JsonEsc(kvErr) + "\"}";
                } else {
                    char ev[160];
                    std::snprintf(ev, sizeof(ev),
                                  "{\"topic\":\"audio.master\",\"data\":{"
                                  "\"mute\":%d,\"volume\":%d},\"ts\":%lld}",
                                  audioMasterMute_ ? 1 : 0, audioMasterVolume_,
                                  static_cast<long long>(
                                      std::time(nullptr)) * 1000);
                    PushAgentEventJson(ev);
                    reply = "{\"ok\":true,\"applied\":{\"audio_master_mute\":" +
                            std::string(audioMasterMute_ ? "1" : "0") +
                            ",\"audio_master_volume\":" +
                            std::to_string(audioMasterVolume_) + "}}";
                }
            }
        } else if (key == "text_font_path") {
            // docs/63 §4: 데스크탑 벡터 폰트 경로. 값은 문자열 — 재시작 적용
            // (아틀라스는 기동 시 Init이 JKTextAtlas::ResolveDesktopFontPath로
            // 합성한다). 상한 300자 — LoadSettingsKv·ResolveDesktopFontPath와
            // 같은 캡. 빈 값은 기본 폰트로의 폴백이 아니라 기각(오타 방어 —
            // 되돌리려면 파일 수동 편집이 아니라 기본값 명시를 쓴다).
            std::string valStr;
            if (!req.GetObjStr("args", "value", valStr) || valStr.empty() ||
                valStr.size() > 300) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (std::string kvErr;
                       !WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        receiptRetentionDays_, valStr,
                                        textFontFallback_, textFontScale_,
                                        &kvErr)) {
                // I-1 (T2 fix r1) — 정직 err 1행(거부 사유 detail 실음).
                reply = kvErr.empty()
                            ? "{\"ok\":false,\"error\":\"write_failed\"}"
                            : "{\"ok\":false,\"error\":\"write_failed\","
                              "\"detail\":\"" + JsonEsc(kvErr) + "\"}";
            } else {
                textFontPath_ = valStr;
                reply = std::string("{\"ok\":true,\"applied\":{"
                                    "\"text_font_path\":\"") +
                        JsonEsc(valStr) + "\"},\"note\":\"applies_on_restart\"}";
            }
        } else if (key == "text_font_fallback") {
            // docs/63 §6 2단계: 보조 폰트 체인 경로. 재시작 적용(text_font_path와
            // 동일 — 아틀라스는 기동 시 InitFallback이
            // ResolveDesktopFallbackPath로 합성). 상한 300자 동일. **빈 값은
            // 해제 허용**(font_path와 반대 — 체인의 기본 상태가 "없음"이라
            // 빈 값이 유효한 목표 상태다).
            std::string valStr;
            if (!req.GetObjStr("args", "value", valStr) || valStr.size() > 300) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (std::string kvErr;
                       !WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        receiptRetentionDays_, textFontPath_,
                                        valStr, textFontScale_, &kvErr)) {
                // I-1 (T2 fix r1) — 정직 err 1행(거부 사유 detail 실음).
                reply = kvErr.empty()
                            ? "{\"ok\":false,\"error\":\"write_failed\"}"
                            : "{\"ok\":false,\"error\":\"write_failed\","
                              "\"detail\":\"" + JsonEsc(kvErr) + "\"}";
            } else {
                textFontFallback_ = valStr;
                reply = std::string("{\"ok\":true,\"applied\":{"
                                    "\"text_font_fallback\":\"") +
                        JsonEsc(valStr) + "\"},\"note\":\"applies_on_restart\"}";
            }
        } else if (key == "text_font_scale") {
            // docs/63 §6 Task 3: 셀 확대 배율(옵트인) — 문자열 float. 범위
            // [1.0, 3.0] 밖·숫자 아님·빈 값은 전부 bad_value(하한 미달을
            // "1.0으로 클램프"하지 않는다 — 오타 방어, font_path 선례).
            // 재시작 적용(CellMetrics()는 기동 시 settings.json 직독, 함수
            // 로컬 static — 실행 중 반영 불가).
            std::string valStr;
            if (!req.GetObjStr("args", "value", valStr) ||
                !ValidFontScale(valStr)) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else if (std::string kvErr;
                       !WriteSettingsKv(audioMasterMute_, audioMasterVolume_,
                                        receiptRetentionDays_, textFontPath_,
                                        textFontFallback_, valStr, &kvErr)) {
                // I-1 (T2 fix r1) — 정직 err 1행(거부 사유 detail 실음).
                reply = kvErr.empty()
                            ? "{\"ok\":false,\"error\":\"write_failed\"}"
                            : "{\"ok\":false,\"error\":\"write_failed\","
                              "\"detail\":\"" + JsonEsc(kvErr) + "\"}";
            } else {
                textFontScale_ = valStr;
                reply = std::string("{\"ok\":true,\"applied\":{"
                                    "\"text_font_scale\":\"") +
                        JsonEsc(valStr) + "\"},\"note\":\"applies_on_restart\"}";
            }
        } else if (key == "capture_allow") {
            // §2.2 키별 Ask: value 1=allow, 0=ask. 승인 시점에 두 캡처 도구를
            // 함께 쓴다(approve 분기의 kind "capture_allow" 해소).
            if (!hasInt || (valInt != 0 && valInt != 1)) {
                reply = "{\"ok\":false,\"error\":\"bad_value\"}";
            } else {
                bool subscriber = false;
                for (auto& c : clients_) {
                    // opus 리뷰 M3 (docs/54 §11): 코어가 모든 ImGui 클라를
                    // 구독시키므로 "구독자 존재" 검사는 요청자 자신까지
                    // true가 되어 공허하다. 승인 표면(jkchat류)은 제어
                    // 전용 연결 — 그것만 센다.
                    if (c && c->IsControlOnly() &&
                        c->AgentEventSubscriber() && !c->IsDisconnected()) {
                        subscriber = true;
                        break;
                    }
                }
                if (!subscriber) {
                    reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                } else if (ApprovalParkingFull(client.Id())) {
                    reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
                } else {
                    const std::string d = valInt ? "allow" : "ask";
                    PendingApproval p;
                    p.kind = "capture_allow";
                    p.permDecision = d;
                    p.requestId = nextApprovalId_++;
                    p.queryId = queryId;
                    p.requesterId = client.Id();
                    p.targetId = 0;
                    p.expiresAt = std::time(nullptr) + 60;
                    char buf[640];
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"settings_set\","
                                  "\"kind\":\"capture_allow\","
                                  "\"target_tool\":\"capture_window\","
                                  "\"decision\":\"%s\",\"ts\":%lld}",
                                  p.requestId, d.c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // 해소 시 답신
                }
            }
        } else {
            reply = "{\"ok\":false,\"error\":\"bad_key\"}";
        }
    } else if (tool == "open_notify") {
        // docs/33: toggle the notification center — safe UI command, no
        // permission gate (same tier as launch_app).
        if (ToggleClientByTitleUnsafe("Notifications", "notify")) {
            PushWindowListUnsafe();  // taskbar highlight follows the refocus
        }
        reply = "{\"ok\":true}";
    } else if (tool == "save_layout") {
        std::string name;
        if (!req.GetObjStr("args", "name", name) || name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            std::string snapshot = "{\"name\":\"" + JsonEsc(name) + "\",\"windows\":[";
            bool first = true;
            int count = 0;
            for (auto& c : clients_) {
                if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
                char item[640];
                std::snprintf(item, sizeof(item),
                    "%s{\"title\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
                    first ? "" : ",", JsonEsc(c->Title()).c_str(),
                    c->X(), c->Y(), c->Width(), c->Height());
                snapshot += item;
                first = false;
                ++count;
            }
            snapshot += "]}";
            const std::string path = StateDir() + "/layout_" + name + ".json";
            std::FILE* f = std::fopen(path.c_str(), "wb");
            if (!f) {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            } else {
                std::fwrite(snapshot.data(), 1, snapshot.size(), f);
                std::fclose(f);
                reply = "{\"ok\":true,\"count\":" + std::to_string(count) + "}";
            }
        }
    } else if (tool == "restore_layout") {
        std::string name;
        if (!req.GetObjStr("args", "name", name) || name.empty()) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            const std::string path = StateDir() + "/layout_" + name + ".json";
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (!f) {
                reply = "{\"ok\":false,\"error\":\"layout_not_found\"}";
            } else {
                char buf[65536];
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                buf[n] = '\0';
                jk::agent::AgentJson layout(buf);
                int arrLen = 0, restored = 0;
                std::string unmatched;
                if (!layout.ok() || !layout.GetArraySize("windows", arrLen)) {
                    reply = "{\"ok\":false,\"error\":\"bad_layout\"}";
                } else {
                    for (int i = 0; i < arrLen; ++i) {
                        std::string title;
                        int x = 0, y = 0;
                        layout.GetArrStr("windows", i, "title", title);
                        layout.GetArrInt("windows", i, "x", x);
                        layout.GetArrInt("windows", i, "y", y);
                        if (title.empty()) continue;
                        // Match by title — the stable key a layout snapshot
                        // has (surface ids change across restarts).
                        bool matched = false;
                        for (auto& c : clients_) {
                            if (!c || c->IsDisconnected() || c->IsControlOnly() || c->IsShell()) continue;
                            if (c->Title() != title) continue;
                            c->SetPosition(x, y);
                            if (compositor_) {
                                compositor_->SetLayerPosition(c->Id(), x, y);
                            }
                            matched = true;
                            ++restored;
                            break;
                        }
                        if (!matched) {
                            if (!unmatched.empty()) unmatched += ",";
                            unmatched += "\"" + JsonEsc(title) + "\"";
                        }
                    }
                    reply = "{\"ok\":true,\"restored\":" + std::to_string(restored) +
                            ",\"unmatched\":[" + unmatched + "]}";
                }
            }
        }
    } else if (tool == "publish_event") {
        // M2b trigger scripts: client→subscriber event publishing. Any
        // client may publish; the server stamps the envelope and relays it
        // to every agent-event subscriber. "data" passes through as raw
        // JSON so trigger filters can shape arbitrary payloads.
        std::string topic, data;
        // opus 리뷰 NIT-3 (docs/54 §11): 서버 네임스페이스 토픽은 스크립트
        // publish로 흉내 낼 수 없다 — window.*는 vplayer 전체화면 미러가
        // 자기 id 니들로 수용하고, audio.*는 코어 펌프가 마스터 게인에
        // 적용한다. 스크립트 커스텀 토픽은 이 접두어 밖의 것으로.
        // 리뷰 Task 2 Important-1 수술: 접두 검사가 topic 파싱 전(빈 문자열
        // 상태)에 돌아 c9ba8bf 이래 데드코드 — 검사를 파싱 후 평가하는
        // 람다로 이동해 게이트를 살린다 (file.open_result 스푸핑 봉쇄 —
        // 스펙 2026-09-19-filedlg-voice-nav 결정 6이 이 게이트에 의존).
        auto topicReserved = [](const std::string& t) {
            // 앱 발행 카탈로그 행의 정확-토픽 면제 — 접두 일괄 봉쇄는
            // source:"app" 행의 정당 발행자까지 죽인다(표 주석의 함정 기록).
            // 면제 목록 = 예약 접두 아래 사는 "app" 행: terminal.output(터미널
            // 앱 M2b), agent.notify(jktriggers desktop.notify — 트리거 액션).
            // 접두 예약 자체는 유지: window./audio. 등 특권 서버 소비자
            // (vplayer 미러/코어 펌프)의 스푸핑 봉쇄가 이 게이트의 존재 이유.
            if (t == "terminal.output" || t == "agent.notify") return false;
            for (const char* p : kReservedTopicPrefixes) {
                if (t.compare(0, std::strlen(p), p) == 0) return true;
            }
            return false;
        };
        if (!req.GetObjStr("args", "topic", topic) || topic.empty() ||
            topic.size() > 96 || JsonEsc(topic).size() > 96 ||
            !req.GetObjRaw("args", "data", data) || data.empty() ||
            data.size() > 4096) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else if (topicReserved(topic)) {
            reply = "{\"ok\":false,\"error\":\"reserved_topic\"}";
        } else {
            char ev[4352];
            std::snprintf(ev, sizeof(ev),
                          "{\"topic\":\"%s\",\"data\":%s,\"ts\":%lld}",
                          JsonEsc(topic).c_str(), data.c_str(),
                          static_cast<long long>(
                              std::chrono::duration_cast<
                                  std::chrono::milliseconds>(
                                  std::chrono::system_clock::now()
                                      .time_since_epoch())
                                  .count()));
            // docs/38: connection-level cap — 60 events / 10s (same figure as
            // the local handler cap so the local cap binds first for
            // self-loops — deterministic notify/stop point; spec §2 interaction
            // note). Over-cap events are dropped but answered ok so senders
            // don't turn into retry bombs. The budget counts allowed events
            // only (a drop consumes nothing — same semantics as the jktriggers
            // source cap) and must stay lock-free — this branch runs with
            // clientsMutex_ already held.
            constexpr int kPublishCap = 60;
            constexpr uint64_t kPublishWindowMs = 10000;
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            PublishBudget& b = publishBudgets_[client.Id()];
            if (nowMs - b.windowStartMs >= kPublishWindowMs) {
                b.windowStartMs = nowMs;
                b.count = 0;
                b.logged = false;
            }
            if (b.count >= kPublishCap) {
                // Over cap: no budget consumption on the drop path.
                if (!b.logged) {
                    b.logged = true;
                    // One log line per connection per window — stdout is the
                    // server log (run_test.sh redirects it, read_log tails it).
                    std::printf("[server] publish_event rate-capped (conn %u)\n",
                                client.Id());
                    std::fflush(stdout);
                }
                reply = "{\"ok\":true,\"dropped\":1}";
            } else {
                ++b.count;
                PushAgentEventJson(ev);
                reply = "{\"ok\":true}";
            }
        }
    } else if (tool == "capture_window") {
        // docs/35: read the client's shm surface (RGBA32) directly — the
        // app framebuffer, no screen DPI involvement. Safe tier (same as
        // launch_app).
        // 게이트 (docs/54 §11 opus M2 픽스): permissions.json 파일값이
        // 서버에서 강제된다 — "ask"면 거부(capture_ask; 설정 허브 캡처
        // 스위치의 승인 파킹이 "allow"로 뒤집을 때까지), "deny"면
        // permission_denied, 없음/allow는 docs/35 안전 계층 그대로.
        const AgentDecision gate = AgentToolAllowed("capture_window");
        if (gate == AgentDecision::Ask) {
            reply = "{\"ok\":false,\"error\":\"capture_ask\"}";
        } else if (gate == AgentDecision::Deny) {
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else {
        int id = 0;
        req.GetObjInt("args", "id", id);
        // 사전 검사는 원본 그대로 유지 — 레이어 부재는 window_not_found,
        // PNG 기록 실패만 write_failed(스펙 §5 3단 헬퍼 추출, 동작 불변).
        JKCompositorLayer* layer =
            compositor_ ? compositor_->FindLayerById(static_cast<uint32_t>(id))
                        : nullptr;
        if (!layer || !layer->Pixels() || layer->Width() <= 0 ||
            layer->Height() <= 0) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            const std::string dir = StateDir() + "/screenshots";
            std::error_code dirEc;  // 실패 무시 — 원문 bool 무시 동형, non-throwing
            std::filesystem::create_directory(std::filesystem::path(dir), dirEc);
            const long long ts =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            const std::string path = dir + "/shot_" + std::to_string(ts) +
                                     "_" + std::to_string(id) + ".png";
            if (CaptureLayerToPng(static_cast<uint32_t>(id), path)) {
                reply = "{\"ok\":true,\"path\":\"" + JsonEsc(path) + "\"}";
            } else {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            }
        }
        }
    } else if (tool == "capture_region") {
        // docs/35: composited-frame readback (SDL_RenderReadPixels), then
        // crop. The requester's own layer is hidden for the readback so a
        // rubber-band overlay does not appear in its own screenshot. Args
        // are logical desktop points — the framebuffer is physical pixels.
        // 게이트: capture_window와 동일 (docs/54 §11 opus M2 픽스).
        const AgentDecision gateR = AgentToolAllowed("capture_region");
        if (gateR == AgentDecision::Ask) {
            reply = "{\"ok\":false,\"error\":\"capture_ask\"}";
        } else if (gateR == AgentDecision::Deny) {
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else {
        int x = 0, y = 0, w = 0, h = 0;
        req.GetObjInt("args", "x", x);
        req.GetObjInt("args", "y", y);
        req.GetObjInt("args", "w", w);
        req.GetObjInt("args", "h", h);
        const int outW = compositor_ ? compositor_->OutputWidth() : 0;
        const int outH = compositor_ ? compositor_->OutputHeight() : 0;
        if (outW <= 0 || outH <= 0 || w <= 0 || h <= 0) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else {
            // Intersect with the output bounds (logical points).
            int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
            int x1 = x + w > outW ? outW : x + w;
            int y1 = y + h > outH ? outH : y + h;
            if (x0 >= x1 || y0 >= y1) {
                reply = "{\"ok\":false,\"error\":\"bad_request\"}";
            } else {
                const float scale = compositor_->OutputScale();
                const int fx = static_cast<int>(x0 * scale);
                const int fy = static_cast<int>(y0 * scale);
                const int fw = static_cast<int>((x1 - x0) * scale);
                const int fh = static_cast<int>((y1 - y0) * scale);
                const int fullW =
                    static_cast<int>(outW * scale);
                const int fullH = static_cast<int>(outH * scale);

                JKCompositorLayer* self =
                    compositor_->FindLayerById(client.Id());
                const bool hideSelf = self && self->IsVisible();
                if (hideSelf) self->SetVisible(false);
                // Hide the viewer that launched this overlay too (docs/35):
                // it is the capture UI and must not appear in its own shot.
                JKCompositorLayer* spawner = nullptr;
                auto sp = overlaySpawner_.find(client.Id());
                if (sp != overlaySpawner_.end() && sp->second != client.Id()) {
                    spawner = compositor_->FindLayerById(sp->second);
                }
                const bool hideSpawner = spawner && spawner->IsVisible();
                if (hideSpawner) spawner->SetVisible(false);
                Composite(false);   // clear + draw, no present
                std::vector<uint8_t> full(
                    static_cast<size_t>(fullW) * fullH * 4);
                SDL_Rect fullRect{0, 0, fullW, fullH};
                const int got = SDL_RenderReadPixels(
                    renderer_, &fullRect, SDL_PIXELFORMAT_RGBA32,
                    full.data(), fullW * 4);
                if (hideSpawner) spawner->SetVisible(true);
                if (hideSelf) self->SetVisible(true);
                Composite(false);
                if (got != 0) {
                    reply = "{\"ok\":false,\"error\":\"read_failed\"}";
                } else {
                    // Row-wise crop into the capture buffer.
                    std::vector<uint8_t> crop(
                        static_cast<size_t>(fw) * fh * 4);
                    for (int row = 0; row < fh; ++row) {
                        std::memcpy(crop.data() +
                                        static_cast<size_t>(row) * fw * 4,
                                    full.data() +
                                        (static_cast<size_t>(fy + row) *
                                             fullW +
                                         fx) * 4,
                                    static_cast<size_t>(fw) * 4);
                    }
                    const std::string dir = StateDir() + "/screenshots";
                    std::error_code dirEc;  // 실패 무시 — non-throwing (review r1)
                    std::filesystem::create_directory(
                        std::filesystem::path(dir), dirEc);
                    const long long ts =
                        std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            std::chrono::system_clock::now()
                                .time_since_epoch())
                            .count();
                    const std::string path =
                        dir + "/shot_" + std::to_string(ts) + "_region.png";
                    if (WritePng(path, fw, fh, crop.data())) {
                        reply = "{\"ok\":true,\"path\":\"" + JsonEsc(path) +
                                "\"}";
                    } else {
                        reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                    }
                }
            }
        }
        }
    } else if (tool == "window_frame") {
        // 폰 미러 (스펙 2026-09-27-phone-mirror): capture_window와 같은 shm
        // 서면 readback이지만 디스크 기록 대신 JPEG b64를 답한다 — 폰 폴링이
        // screenshots 폴더를 쓰레기로 채우지 않는다. 레이어 fit-scale은 무시
        // (서면 px) — 폰은 dw/dh 비율로 좌표를 되돌린다. 게이트는 캡처 쌍과
        // 동일 server(flip) 분류 (docs/54 §11 opus M2 선례).
        const AgentDecision gateF = AgentToolAllowed("window_frame");
        if (gateF == AgentDecision::Ask) {
            reply = "{\"ok\":false,\"error\":\"capture_ask\"}";
        } else if (gateF == AgentDecision::Deny) {
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else {
        int id = 0, maxw = 0;
        req.GetObjInt("args", "id", id);
        req.GetObjInt("args", "maxw", maxw);
        JKCompositorLayer* layer =
            compositor_ ? compositor_->FindLayerById(static_cast<uint32_t>(id))
                        : nullptr;
        if (id == 0) {
            // 데스크톱 와이드 뷰 (docs/57 §14.15, 사용자 요청): 미선택 미러의
            // 기본 화면 — 합성 프레임버퍼 전체 readback(capture_region 파이프
            // 라인의 크롭 없음 변형). 폰은 dw/dh(논리 데스크톱) 비율로
            // id=0 send_input을 되돌리고 서버가 히트테스트로 최상위 클라이언트
            // 에 전달한다(ExecuteSendInputOp 데스크톱 분기).
            const int outW = compositor_ ? compositor_->OutputWidth() : 0;
            const int outH = compositor_ ? compositor_->OutputHeight() : 0;
            if (outW <= 0 || outH <= 0) {
                reply = "{\"ok\":false,\"error\":\"bad_request\"}";
            } else {
                const float dsc = compositor_->OutputScale();
                const int fw = static_cast<int>(outW * dsc);
                const int fh = static_cast<int>(outH * dsc);
                Composite(false);   // clear + draw, no present (capture_region 선례)
                std::vector<uint8_t> px(static_cast<size_t>(fw) * fh * 4);
                SDL_Rect fr{0, 0, fw, fh};
                const int got = SDL_RenderReadPixels(
                    renderer_, &fr, SDL_PIXELFORMAT_RGBA32, px.data(), fw * 4);
                Composite(false);
                std::string b64;
                int ew = 0, eh = 0;
                if (got != 0) {
                    reply = "{\"ok\":false,\"error\":\"read_failed\"}";
                } else if (EncodeLayerJpegB64(px, fw, fh, maxw, &b64, &ew, &eh)) {
                    reply = "{\"ok\":true,\"desktop\":1,\"w\":" +
                            std::to_string(ew) + ",\"h\":" + std::to_string(eh) +
                            ",\"dw\":" + std::to_string(outW) +
                            ",\"dh\":" + std::to_string(outH) +
                            ",\"data\":\"" + b64 + "\"}";
                } else {
                    reply = "{\"ok\":false,\"error\":\"frame_too_large\"}";
                }
            }
        } else if (id < 0) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else if (!layer || !layer->Pixels() || layer->Width() <= 0 ||
                   layer->Height() <= 0) {
            reply = "{\"ok\":false,\"error\":\"window_not_found\"}";
        } else {
            // 서면 px 1회 복사 (capture_window 선례) — 클라가 인코딩 중에
            // shm에 커밋할 수 있다.
            std::vector<uint8_t> px(
                layer->Pixels(),
                layer->Pixels() + static_cast<size_t>(layer->Width()) *
                                       layer->Height() * 4);
            std::string b64;
            int ew = 0, eh = 0;
            if (EncodeLayerJpegB64(px, layer->Width(), layer->Height(), maxw,
                                   &b64, &ew, &eh)) {
                reply = "{\"ok\":true,\"w\":" + std::to_string(ew) +
                        ",\"h\":" + std::to_string(eh) +
                        ",\"sw\":" + std::to_string(layer->Width()) +
                        ",\"sh\":" + std::to_string(layer->Height()) +
                        ",\"data\":\"" + b64 + "\"}";
            } else {
                reply = "{\"ok\":false,\"error\":\"frame_too_large\"}";
            }
        }
        }
    } else if (tool == "trigger_toggle") {
        // docs/34: write state/triggers.json (single source of truth) then
        // publish triggers.reload — jktriggers re-reads the file on the
        // event. Safe tier (no gate), same as launch_app.
        std::string name;
        int on = -1;
        req.GetObjStr("args", "name", name);
        req.GetObjInt("args", "on", on);
        if (name.empty() || on < 0) {
            reply = "{\"ok\":false,\"error\":\"missing_name\"}";
        } else {
            const std::string path = StateDir() + "/triggers.json";
            std::map<std::string, int> flags;
            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (f) {
                char buf[4096] = {};
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                jk::agent::AgentJson json(buf);
                int cnt = 0;
                if (json.ok() && json.GetArraySize("triggers", cnt)) {
                    for (int i = 0; i < cnt && i < 64; ++i) {
                        std::string nm;
                        int en = 1;
                        if (json.GetArrStr("triggers", i, "name", nm)) {
                            json.GetArrInt("triggers", i, "enabled", en);
                            flags[nm] = en;
                        }
                    }
                }
            }
            flags[name] = on;
            std::string out = "{\"triggers\":[";
            bool first = true;
            for (const auto& kv : flags) {
                if (!first) out += ",";
                first = false;
                out += "{\"name\":\"" + JsonEsc(kv.first) + "\",\"enabled\":" +
                       std::to_string(kv.second) + "}";
            }
            out += "]}";
            reply = "{\"ok\":true}";
            if (std::FILE* w = std::fopen(path.c_str(), "wb")) {
                std::fwrite(out.data(), 1, out.size(), w);
                std::fclose(w);
            } else {
                reply = "{\"ok\":false,\"error\":\"write_failed\"}";
            }
            if (reply.find("\"ok\":true") != std::string::npos) {
                char ev[128];
                std::snprintf(ev, sizeof(ev),
                              "{\"topic\":\"triggers.reload\",\"data\":{},"
                              "\"ts\":%lld}",
                              static_cast<long long>(
                                  std::chrono::duration_cast<
                                      std::chrono::milliseconds>(
                                      std::chrono::system_clock::now()
                                          .time_since_epoch())
                                      .count()));
                PushAgentEventJson(ev);
            }
        }
    } else if (tool == "trigger_list") {
        // docs/34: flat rows from jktriggers' loaded manifest merged with
        // the flags file (missing entry = enabled).
        std::map<std::string, std::pair<std::vector<std::string>, int>> merged;
        auto readState = [&](const char* file, bool isManifest) {
            std::FILE* f =
                std::fopen((StateDir() + "/" + file).c_str(), "rb");
            if (!f) return;
            char buf[8192] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (!json.ok() || !json.GetArraySize("triggers", cnt)) return;
            for (int i = 0; i < cnt && i < 256; ++i) {
                std::string nm, topic;
                if (!json.GetArrStr("triggers", i, "name", nm)) continue;
                auto& entry = merged[nm];
                if (isManifest) {
                    // operator[] default is 0 — absent flags mean enabled.
                    entry.second = 1;
                    if (json.GetArrStr("triggers", i, "topic", topic) &&
                        !topic.empty()) {
                        entry.first.push_back(topic);
                    }
                } else {
                    int en = 1;
                    json.GetArrInt("triggers", i, "enabled", en);
                    entry.second = en;
                }
            }
        };
        readState("triggers_loaded.json", true);
        readState("triggers.json", false);
        std::string out = "{\"ok\":true,\"triggers\":[";
        bool first = true;
        for (const auto& kv : merged) {
            if (!first) out += ",";
            first = false;
            out += "{\"name\":\"" + JsonEsc(kv.first) + "\",\"topics\":[";
            for (size_t i = 0; i < kv.second.first.size(); ++i) {
                if (i) out += ",";
                out += "\"" + JsonEsc(kv.second.first[i]) + "\"";
            }
            out += "],\"enabled\":" + std::to_string(kv.second.second) + "}";
        }
        reply = out + "]}";
    } else if (tool == "trust_list") {
        // Script trust store (docs/37 spec): the loader's trust.json records,
        // fingerprints truncated to 15 chars ("sha256:"+8hex) for display.
        // docs/38: a missing/corrupt store is distinguished from an empty one
        // — trust_store_unreadable on fopen or parse failure; a valid file
        // with zero records still answers ok with an empty array.
        std::FILE* f =
            std::fopen((StateDir() + "/trust.json").c_str(), "rb");
        if (!f) {
            reply = "{\"ok\":false,\"error\":\"trust_store_unreadable\"}";
        } else {
            char buf[65536] = {};
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            buf[n] = '\0';
            jk::agent::AgentJson json(buf);
            int cnt = 0;
            if (!json.ok() || !json.GetArraySize("records", cnt)) {
                reply = "{\"ok\":false,\"error\":\"trust_store_unreadable\"}";
            } else {
                std::string out = "{\"ok\":true,\"records\":[";
                bool first = true;
                for (int i = 0; i < cnt && i < 512; ++i) {
                    std::string fp, name, source;
                    // ts is epoch ms (int64 in the loader's writer) but the
                    // AgentJson accessor has no int64 getter — display-only
                    // int truncation is accepted (jktriggers reads ts via a
                    // direct QuickJS reader instead).
                    int ts = 0;
                    if (!json.GetArrStr("records", i, "fingerprint", fp))
                        continue;
                    json.GetArrStr("records", i, "name", name);
                    json.GetArrStr("records", i, "source", source);
                    json.GetArrInt("records", i, "ts", ts);
                    if (!first) out += ",";
                    first = false;
                    // 에이전트 관리자 해지용 전체 지문 (스펙 §2.3 전제) — 15자
                    // 절단 fingerprint는 표시용으로 유지.
                    out += "{\"fingerprint\":\"" +
                           (fp.size() > 15 ? fp.substr(0, 15) : fp) +
                           "\",\"name\":\"" + JsonEsc(name) +
                           "\",\"source\":\"" + JsonEsc(source) +
                           "\",\"fp\":\"" + JsonEsc(fp) +
                           "\",\"ts\":" + std::to_string(ts) + "}";
                }
                out += "]}";
                reply = out;
            }
        }
    } else if (tool == "events_list") {
        // docs/32: the structured event catalog. Static rows describe the
        // system topics (source + payload shape); fired/last_ts come from
        // the runtime stats and subscribers from the live connections.
        // Topics published via publish_event that are not cataloged are
        // appended as dynamic "app" rows.
        size_t subscribers = 0;
        for (auto& c : clients_) {
            if (c && c->AgentEventSubscriber() && !c->IsDisconnected()) {
                ++subscribers;
            }
        }
        static const struct {
            const char* topic;
            const char* source;  // server | app
            const char* desc;
            const char* fields;  // JSON array literal of payload paths
        } kCatalog[] = {
            {"window.created", "server", "클라 창(레이어) 생성",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.focused", "server", "창 포커스 이동",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.destroyed", "server", "창 소멸(연결 종료, 정상 종료 포함)",
             "[\"id\",\"title\",\"pid\"]"},
            {"window.maximized", "server",
             "창 최대화 (크롬 최대화 버튼 / 제목 더블클릭, docs/39)",
             "[\"id\",\"title\"]"},
            {"window.restored", "server",
             "창 복원 (최대화 해제 — 버튼/더블클릭/제목·가장자리 드래그, docs/39)",
             "[\"id\",\"title\"]"},
            {"window.fullscreen", "server",
             "창 전체화면 진입 (window_fullscreen 도구/vplayer F11·더블클릭, "
             "docs/50 §11)",
             "[\"id\",\"title\"]"},
            {"window.fullscreen_exit", "server",
             "창 전체화면 해제 (window_fullscreen 도구/vplayer F11·더블클릭·OSD "
             "버튼, docs/50 §11)",
             "[\"id\",\"title\"]"},
            {"app.crashed", "server", "앱 비정상 종료(exit code 0/259 외)",
             "[\"id\",\"title\",\"pid\"]"},
            {"agent.approval_request", "server",
             "ask 권한 승인 요청 (docs/31) + trust_request (docs/37) + "
             "capture_allow 키별 Ask (docs/54) + send_input (docs/62 정복 사다리)",
             "\"request,tool,kind,title/target_id/name,origin,fingerprint\""},
            {"agent.approval_resolved", "server",
             "승인 결정: allow/deny/timeout", "[\"request\",\"decision\"]"},
            {"terminal.output", "app", "터미널 출력(250ms 코얼레싱, VT 제거)",
             "[\"data.text\"]"},
            {"agent.notify", "app",
             "알림 방송 — desktop.notify()가 발행, 채팅 [알림] 줄 + 알림 센터가 소비",
             "[\"data.title\",\"data.body\"]"},
            {"triggers.reload", "server", "트리거 플래그 변경 재적재 신호", "[]"},
            {"audio.master", "server",
             "마스터 볼륨/뮤트 변경 방송 (settings_set, docs/54) — 코어 펌프가 "
             "JKSoundManager에 적용",
             "\"data.mute,data.volume\""},
            // 앱 도구 허브 (스펙 §4.1): 등록/소멸 시 publish. 레슨 ⑧ 신규
            // 토픽은 카탈로그 즉시 등록.
            {"agent.app_tools_changed", "server",
             "앱 도구 등록/소멸(연결 수명)", "[\"topic\"]"},
            // file_open wait:event 비동기 모드 (스펙 §6 결정 5): filedlg가
            // 해소한 결과/취소와 만료 스캔의 expired를 방송. 레슨 ⑧ 신규
            // 토픽은 카탈로그 즉시 등록.
            {"file.open_result", "server",
             "file_open(wait:event) 해소 — ok=true path / 취소 ok=false / "
             "만료 expired (docs/48 파킹 파이프라인의 이벤트 통지)",
             "[\"ok\",\"path\",\"error\"]"},
        };
        std::string out = "{\"ok\":true,\"subscribers\":" +
                          std::to_string(subscribers) + ",\"events\":[";
        bool first = true;
        auto appendEntry = [&](const std::string& topic, const char* source,
                               const char* desc, const char* fields) {
            const auto it = topicStats_.find(topic);
            const uint64_t fired =
                it == topicStats_.end() ? 0 : it->second.fired;
            const long long lastTs =
                it == topicStats_.end() ? 0 : it->second.lastTs;
            if (!first) out += ",";
            first = false;
            out += "{\"topic\":\"" + JsonEsc(topic) + "\",\"source\":\"" +
                   source + "\",\"desc\":\"" + JsonEsc(desc) +
                   "\",\"fields\":" + fields + ",\"fired\":" +
                   std::to_string(fired) + ",\"last_ts\":" +
                   std::to_string(lastTs) + "}";
        };
        for (const auto& e : kCatalog) {
            appendEntry(e.topic, e.source, e.desc, e.fields);
        }
        for (const auto& kv : topicStats_) {
            bool inCatalog = false;
            for (const auto& e : kCatalog) inCatalog |= (e.topic == kv.first);
            if (!inCatalog) {
                appendEntry(kv.first, "app",
                            "publish_event로 발행된 동적 토픽", "[]");
            }
        }
        reply = out + "]}";
    } else if (tool == "agent_permissions") {
        // 스펙 §2.1: permissions.json + 기본값 병합. gate 뱃지로 게이트
        // 소비처를 정직 표기 — "none" 행의 파일값은 서버 무력(브로커만).
        // file은 "" = 오버라이드 없음 (AgentJson이 null을 못 읽는다).
        const std::string dir = ExeDirNoSlash();
        const std::string permPath = dir + "/permissions.json";
        char pbuf[4096] = {};
        bool fileExists = false;
        if (std::FILE* f = std::fopen(permPath.c_str(), "rb")) {
            fileExists = true;
            std::fread(pbuf, 1, sizeof(pbuf) - 1, f);
            std::fclose(f);
        }
        // AgentJson은 복사/대입 불가(docs/38) — 파일이 없으면 "{}"로 생성.
        jk::agent::AgentJson perm(fileExists ? pbuf : "{}");
        if (fileExists && !perm.ok()) {
            // 파일 없음(기본값)과 파싱 실패(수동 편집 실수)를 구분 —
            // trust_list의 docs/38 선례.
            reply = "{\"ok\":false,\"error\":\"permissions_unreadable\"}";
        } else {
            std::string out = "{\"ok\":true,\"perms\":[";
            bool first = true;
            for (const AgentPermRow& row : kPermMatrix) {
                std::string fileVal;
                const bool hasFile = perm.ok() &&
                    perm.GetStr(row.tool, fileVal) &&
                    (fileVal == "allow" || fileVal == "ask" ||
                     fileVal == "deny");
                std::string effective = row.deflt;
                if (std::string(row.gate) == "server(fixed)") {
                    effective = "ask";
                } else if (std::string(row.gate) == "server") {
                    if (hasFile) effective = fileVal;
                } else if (std::string(row.gate) == "server(flip)") {
                    // docs/54 §11 opus M2: 캡처 쌍 — 파일값이 서버 강제
                    // ("ask" = capture_ask 거부, flip 승인이 해소).
                    if (hasFile) effective = fileVal;
                } else if (std::string(row.gate) == "server(files)") {
                    // 파일 허브 (스펙 2026-09-18-file-hub §2.2): 파일값이
                    // 서버 강제. 파일 없음 = 소스 분리 — 표시는 에이전트
                    // 쪽 최악값(ask)을 쓴다(사용자 window 연결은 무승인).
                    if (hasFile) effective = fileVal;
                    else effective = "ask";
                } else if (std::string(row.gate) == "server(audit)") {
                    // files_audit (opus 리뷰 MINOR-3): 파일값 강제(deny/ask
                    // 파킹) — 파일 없음 = 기본 allow.
                    if (hasFile) effective = fileVal;
                } else {
                    effective = "allow";   // broker/none — 서버 미게이트
                }
                if (!first) out += ",";
                first = false;
                out += "{\"tool\":\"" + std::string(row.tool) +
                       "\",\"gate\":\"" + row.gate + "\",\"file\":\"" +
                       (hasFile ? fileVal : std::string()) +
                       "\",\"effective\":\"" + effective +
                       "\",\"default\":\"" + row.deflt + "\"}";
            }
            reply = out + "]}";
        }
    } else if (tool == "installed_list") {
        // 스펙 §2.4: 셸 런처 스캔의 이름/kind. shell_ 미기동 = 빈 배열(정상).
        std::vector<std::pair<std::string, const char*>> rows;
        if (shell_) shell_->ListInstalled(rows);
        std::string out = "{\"ok\":true,\"installed\":[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ",";
            out += "{\"name\":\"" + JsonEsc(rows[i].first) +
                   "\",\"kind\":\"" + rows[i].second + "\"}";
        }
        reply = out + "]}";
    } else if (tool == "read_receipts") {
        // 스펙 §2.5: 브로커 receipts.jsonl 꼬리 — ts/tool/ok만 반환
        // (result 전문은 args에 경로/명령어가 실릴 수 있다).
        int limit = 50;
        req.GetObjInt("args", "limit", limit);
        if (limit <= 0) limit = 50;
        if (limit > 200) limit = 200;
        const std::string path = StateDir() + "/receipts.jsonl";
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            reply = "{\"ok\":true,\"rows\":[]}";   // 브로커 미사용 = 정상
        } else {
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            const long start = size > 262144 ? size - 262144 : 0;
            std::fseek(f, start, SEEK_SET);
            std::vector<char> buf(static_cast<size_t>(size - start) + 1);
            const size_t n = std::fread(buf.data(), 1, buf.size() - 1, f);
            std::fclose(f);
            buf[n] = '\0';
            std::vector<std::string> lines;
            size_t pos = 0;
            while (pos < n) {
                const char* begin = buf.data() + pos;
                const char* nl = static_cast<const char*>(
                    std::memchr(begin, '\n', n - pos));
                const size_t len = nl ? (size_t)(nl - begin) : (n - pos);
                if (len > 0) lines.push_back(std::string(begin, len));
                pos += len + (nl ? 1 : 0);
            }
            std::string out = "{\"ok\":true,\"rows\":[";
            int used = 0;
            for (size_t i = lines.size(); i-- > 0 && used < limit;) {
                const std::string& line = lines[i];
                // JSONL 행의 result 중첩은 2레벨 리더의 관심사가 아니다 —
                // ts/tool은 행 파서(AgentJson) + ts는 raw 스캔(숫자 필드),
                // ok는 result 내 raw 스캔.
                jk::agent::AgentJson row(line.c_str());
                std::string toolName;
                if (!row.ok() || !row.GetStr("tool", toolName)) continue;
                long long ts = 0;
                {
                    const size_t tp = line.find("\"ts\":");
                    if (tp != std::string::npos) {
                        ts = std::atoll(line.c_str() + tp + 5);
                    }
                }
                const bool okFlag =
                    line.find("\"result\":") != std::string::npos &&
                    line.find("\"ok\":true") != std::string::npos;
                if (used) out += ",";
                // 직렬화 규약(파서 계약): ts = epoch 초(2레벨 GetArrInt 경유,
                // ms → 초 절단), ok = "0"/"1" 문자열(리더가 bool을 못 읽는다).
                out += "{\"ts\":" + std::to_string(ts / 1000) +
                       ",\"tool\":\"" + JsonEsc(toolName) +
                       "\",\"ok\":\"" + (okFlag ? "1" : "0") + "\"}";
                ++used;
            }
            reply = out + "]}";
        }
    } else if (tool == "notes_read") {
        // 노트 허브 (스펙 2026-09-18-notes-hub §2.2): state/notes.json 전체
        // 반환. ts는 epoch ms 원문 저장 — 클라 응답은 epoch 초 절단
        // (read_receipts와 동일 2레벨 규약).
        std::vector<NoteRow> notes, items;
        int next = 1;
        if (!ReadNotes(notes, items, next)) {
            std::printf("[server] notes.json unreadable — starting empty\n");
            std::fflush(stdout);
        }
        std::string out = "{\"ok\":true,\"notes\":[";
        for (size_t i = 0; i < notes.size(); ++i) {
            if (i) out += ",";
            out += "{\"id\":" + std::to_string(notes[i].id) +
                   ",\"text\":\"" + JsonEsc(notes[i].text) +
                   "\",\"win\":" + std::to_string(notes[i].win) +
                   ",\"ts\":" + std::to_string(
                       std::atoll(notes[i].tsRaw.c_str()) / 1000) +
                   ",\"src\":\"" + JsonEsc(notes[i].src) + "\"}";
        }
        out += "],\"backlog\":[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i) out += ",";
            out += "{\"id\":" + std::to_string(items[i].id) +
                   ",\"title\":\"" + JsonEsc(items[i].text) +
                   "\",\"state\":" + std::to_string(items[i].state) +
                   ",\"ts\":" + std::to_string(
                       std::atoll(items[i].tsRaw.c_str()) / 1000) +
                   ",\"src\":\"" + JsonEsc(items[i].src) + "\"}";
        }
        reply = out + "]}";
    } else if (tool == "notes_write") {
        // op 화이트리스트(§2.2): add_note/add_item/move_item/del. RMW는
        // 행 파싱 → 재직렬화(서버가 유일 쓰기자). id는 서버 채번기(next).
        std::string op, text;
        int id = 0, st = -1, win = -1;
        req.GetObjStr("args", "op", op);
        req.GetObjStr("args", "text", text);
        req.GetObjInt("args", "id", id);
        req.GetObjInt("args", "state", st);
        req.GetObjInt("args", "win", win);
        if (op != "add_note" && op != "add_item" && op != "move_item" &&
            op != "del") {
            reply = "{\"ok\":false,\"error\":\"bad_op\"}";
        } else if (op == "add_note" &&
                   (text.empty() || text.size() > 512)) {
            reply = "{\"ok\":false,\"error\":\"bad_text\"}";
        } else if (op == "add_item" &&
                   (text.empty() || text.size() > 128)) {
            reply = "{\"ok\":false,\"error\":\"bad_text\"}";
        } else if (op == "add_item" && st > 2) {
            // 스펙 §2.2: state 0..2 — 명시 out-of-range는 bad_state(opus
            // NIT-4). 생략(st==-1)은 대기(0)로 채번 시 적용.
            reply = "{\"ok\":false,\"error\":\"bad_state\"}";
        } else if (op == "move_item" && (id <= 0 || st < 0 || st > 2)) {
            reply = "{\"ok\":false,\"error\":\"bad_state\"}";
        } else if (op == "del" && id <= 0) {
            reply = "{\"ok\":false,\"error\":\"bad_id\"}";
        } else {
            std::vector<NoteRow> notes, items;
            int next = 1;
            if (!ReadNotes(notes, items, next)) {
                reply = "{\"ok\":false,\"error\":\"notes_unreadable\"}";
            } else if (op == "add_note" || op == "add_item") {
                NoteRow r;
                r.isItem = (op == "add_item");
                r.id = next;
                r.text = text;
                r.src = client.IsControlOnly() ? "agent" : "user";
                r.tsRaw = std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
                if (r.isItem) {
                    r.state = st >= 0 ? st : 0;   // 생략(-1) → 대기
                    items.push_back(r);
                } else {
                    r.win = win > 0 ? win : 0;
                    notes.push_back(r);
                }
                if (WriteNotesFile(notes, items, next + 1)) {
                    reply = "{\"ok\":true,\"id\":" + std::to_string(next) +
                            ",\"written\":true}";
                    // §2.4: 에이전트 노트는 알림 센터로 유입(agent.notify —
                    // 기존 토픽, 카탈로그 변경 없음). 사용자 추가는 방송 없음.
                    // 방송은 publish_event와 같은 연결 예산(docs/38 60/10s)을
                    // 쓴다(opus MINOR-1 — 노트 스팸 = 토스트 스팸 + 히스토리
                    // 전체 재쓰기). 캡 초과분은 **노트가 아니라 토스트만
                    // 버린다** — 노트는 데이터라 남고, senders가 재시도 폭탄
                    // 으로 변하지 않게 ok 유지.
                    if (r.src == "agent") {
                        constexpr int kPublishCap = 60;
                        constexpr uint64_t kPublishWindowMs = 10000;
                        const uint64_t nowMs = static_cast<uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                std::chrono::steady_clock::now()
                                    .time_since_epoch())
                                .count());
                        PublishBudget& b = publishBudgets_[client.Id()];
                        bool overCap = false;
                        if (nowMs - b.windowStartMs >= kPublishWindowMs) {
                            b.windowStartMs = nowMs;
                            b.count = 0;
                            b.logged = false;
                        }
                        if (b.count >= kPublishCap) {
                            overCap = true;
                            if (!b.logged) {
                                b.logged = true;
                                std::printf("[server] notes notify "
                                            "rate-capped (conn %u)\n",
                                            client.Id());
                                std::fflush(stdout);
                            }
                        }
                        if (!overCap) {
                            ++b.count;
                            char ev[896];
                            std::snprintf(
                                ev, sizeof(ev),
                                "{\"topic\":\"agent.notify\",\"data\":{"
                                "\"title\":\"[노트] %s\",\"body\":\"%s\"},"
                                "\"ts\":%lld}",
                                (r.isItem ? "백로그" : "코멘트"),
                                JsonEsc(Utf8TrimTo(r.text, 128)).c_str(),
                                static_cast<long long>(
                                    std::time(nullptr)) * 1000);
                            PushAgentEventJson(ev);
                        }
                    }
                } else {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                }
            } else if (op == "move_item") {
                bool found = false;
                for (NoteRow& r : items) {
                    if (r.id == id) { r.state = st; found = true; break; }
                }
                if (!found) {
                    reply = "{\"ok\":false,\"error\":\"id_not_found\"}";
                } else if (WriteNotesFile(notes, items, next)) {
                    reply = "{\"ok\":true,\"written\":true}";
                } else {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                }
            } else {   // del
                const size_t beforeN = notes.size(), beforeI = items.size();
                for (size_t i = notes.size(); i > 0; --i) {
                    if (notes[i - 1].id == id) {
                        notes.erase(notes.begin() +
                                    static_cast<long>(i - 1));
                    }
                }
                for (size_t i = items.size(); i > 0; --i) {
                    if (items[i - 1].id == id) {
                        items.erase(items.begin() +
                                    static_cast<long>(i - 1));
                    }
                }
                if (notes.size() == beforeN && items.size() == beforeI) {
                    reply = "{\"ok\":false,\"error\":\"id_not_found\"}";
                } else if (WriteNotesFile(notes, items, next)) {
                    reply = "{\"ok\":true,\"written\":true}";
                } else {
                    reply = "{\"ok\":false,\"error\":\"write_failed\"}";
                }
            }
        }
    } else if (tool == "files_list" || tool == "files_read") {
        // 파일 허브 (스펙 2026-09-18-file-hub §2.2): 최초의 파일 콘텐츠
        // 도구 — 읽기 전용. 게이트 "server(files)": 소스 분리 — 사용자
        // (window 연결)는 무승인(자기 파일), 에이전트(control-only)는 파킹
        // (kind files_access). permissions.json 명시 "allow"면 에이전트도
        // 무승인, "deny"면 전 소스 거부, 없음/ask가 위 기본.
        std::string path;
        int maxBytes = 0;
        req.GetObjStr("args", "path", path);
        req.GetObjInt("args", "maxBytes", maxBytes);
        const std::string perm = FilesPermRaw(tool);
        if (perm == "deny") {
            // 사용자가 영구 거부한 행위 — window 연결도 거부된다.
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else if (!ValidFilePath(path)) {
            reply = "{\"ok\":false,\"error\":\"bad_path\"}";
        } else if (perm == "allow" || !client.IsControlOnly()) {
            reply = tool == "files_list" ? FilesListOpJson(path)
                                         : FilesReadOpJson(path, maxBytes);
        } else {
            // 에이전트 ask — 승인 파킹(close_window 선례). 구독자 검사는
            // control-only만 센다(opus 리뷰 M3, docs/54 §11 — 코어가 모든
            // ImGui 클라를 구독시키므로 전체 구독자 검사는 공허하다).
            bool subscriber = false;
            for (auto& c : clients_) {
                if (c && c->IsControlOnly() && c->AgentEventSubscriber() &&
                    !c->IsDisconnected()) {
                    subscriber = true;
                    break;
                }
            }
            if (!subscriber) {
                reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
            } else if (ApprovalParkingFull(client.Id())) {
                reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
            } else {
                PendingApproval p;
                p.kind = "files_access";
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.expiresAt = std::time(nullptr) + 60;
                // 재실행 원본 — 승인 시점에 deny 재검사 + op 재실행(파킹
                // 대기 중 permissions.json이 바뀌면 최신 게이트가 강제).
                p.filesTool = tool;
                p.filesPath = path;
                p.filesMaxBytes = maxBytes;
                char buf[1024];   // path 256 + JsonEsc 확장(백슬래시 2배) 여유
                const int evLen =
                    std::snprintf(buf, sizeof(buf),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"%s\","
                                  "\"kind\":\"files_access\","
                                  "\"title\":\"%s\",\"ts\":%lld}",
                                  p.requestId, tool.c_str(),
                                  JsonEsc(path).c_str(),
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                if (evLen < 0 || static_cast<size_t>(evLen) >= sizeof(buf)) {
                    // 잘린 이벤트는 무효 JSON(opus 리뷰 MINOR-2) — 파킹하지
                    // 않고 승인 불가로 답한다(발행-후-검사 순서).
                    reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                } else {
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf);
                    replied = false;  // answered when the approval resolves
                }
            }
        }
    } else if (tool == "files_audit") {
        // 감사 열람 — 기본 allow(저위험), 단 파일값 강제(opus 리뷰 MINOR-3):
        // deny=전 소스 거부, ask+에이전트=files 도구와 동일 파킹. 브로커가
        // 쓴 receipts의 files_* 행만 — 신규 감사 파일 없음(단일 감사원).
        int limit = 0;
        req.GetObjInt("args", "limit", limit);
        const std::string auditPerm = FilesPermRaw("files_audit");
        if (auditPerm == "deny") {
            reply = "{\"ok\":false,\"error\":\"permission_denied\"}";
        } else if (!client.IsControlOnly() || auditPerm == "allow" ||
                   auditPerm == "missing") {
            reply = FilesAuditOpJson(limit);
        } else {
            // ask + 에이전트 — files_list/read와 동일한 승인 파킹.
            bool subscriber = false;
            for (auto& c : clients_) {
                if (c && c->IsControlOnly() && c->AgentEventSubscriber() &&
                    !c->IsDisconnected()) {
                    subscriber = true;
                    break;
                }
            }
            if (!subscriber) {
                reply = "{\"ok\":false,\"error\":\"approval_unavailable\"}";
            } else if (ApprovalParkingFull(client.Id())) {
                reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
            } else {
                PendingApproval p;
                p.kind = "files_access";
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.expiresAt = std::time(nullptr) + 60;
                p.filesTool = "files_audit";
                p.filesLimit = limit;   // 재실행 원본 — 승인 시점 재실행
                char buf2[256];         // 경로 없음 — tool 이름뿐, 잘림 여유
                const int evLen2 =
                    std::snprintf(buf2, sizeof(buf2),
                                  "{\"topic\":\"agent.approval_request\","
                                  "\"request\":%u,\"tool\":\"files_audit\","
                                  "\"kind\":\"files_access\","
                                  "\"title\":\"files_audit\",\"ts\":%lld}",
                                  p.requestId,
                                  static_cast<long long>(std::time(nullptr)) *
                                      1000);
                if (evLen2 < 0 ||
                    static_cast<size_t>(evLen2) >= sizeof(buf2)) {
                    reply =
                        "{\"ok\":false,\"error\":\"approval_unavailable\"}";
                } else {
                    pendingApprovals_.push_back(p);
                    PushAgentEventJson(buf2);
                    replied = false;
                }
            }
        }
    } else if (tool == "launch_chat") {
        // M2 chat: open the Win32 chat window (approval surface). One API,
        // many faces — MCP agents can open it too.
#ifdef _WIN32
        SpawnProcess("jkchat.exe", "");
        reply = "{\"ok\":true}";
#else
        // posix는 jkchat을 CMake 게이트로 빌드하지 않는다(docs/70 §2 T1 —
        // raw USER32/GDI, 전 대상 윈 전용) — 거짓 ok 대신 플랫폼을 보고한다.
        reply = "{\"ok\":false,\"error\":\"unavailable_on_platform\"}";
#endif
    } else if (tool == "approve") {
        // M2 chat: resolve one pending approval. The parked query's reply
        // goes to the ORIGINAL requester; the approver gets the ack below.
        int request = 0;
        std::string decision;
        req.GetObjInt("args", "request", request);
        req.GetObjStr("args", "decision", decision);
        const bool allow = (decision == "allow");
        bool resolved = false;
        bool selfApprove = false;
        // 앱 도구 허브 (스펙 §4.3): app_tool 승인은 응답을 즉시 쓰지 않는다 —
        // 중계를 시작하고 응답은 앱의 AgentToolResult가 회송(HandleToolResult).
        bool deferred = false;
        for (auto it = pendingApprovals_.begin();
             it != pendingApprovals_.end(); ++it) {
            if (it->requestId != static_cast<uint32_t>(request)) continue;
            // 승인 파이프라인의 2단 우회 봉쇄(스펙 §7 리뷰 후속): 파킹을
            // 자기 연결에서 approve하면 승인 없는 허가가 된다. kind를
            // 2종(permission_set/trust_revoke)으로 한정하면 trust_request/
            // run_console_app 파킹이 우회를 남긴다(opus 최종리뷰 M1 — 자기
            // 구독으로 승인 불가 상태를 스스로 해소해 지문 선기록/스폰이
            // 그대로 관통). close_window만 예외 — ask 모드에서 채팅 자신의
            // /close를 자기 승인 스트립으로 해소하는 것은 docs/31 §3의 설계된
            // UX다(non-blocking 채팅의 존재 이유). 나머지 전종에는 legit
            // 요청자-자체승인 경로가 없다. 파킹은 건드리지 않는다 — 다른
            // 표면(채팅)의 승인은 여전히 가능하다.
            if (it->requesterId == client.Id() && it->kind != "close_window") {
                selfApprove = true;
                break;
            }
            resolved = true;
            if (allow && it->kind == "close_window") {
                for (auto& c : clients_) {
                    if (c && c->Id() == it->targetId && !c->IsDisconnected()) {
                        ipc::WriteMessage(c->Transport(), ipc::MsgType::Close,
                                          std::vector<uint8_t>{});
                        break;
                    }
                }
            }
            if (allow && it->kind == "run_console_app" && shell_) {
                // P4 SDK §5: 승인 시점에 스캔에서 cmd를 다시 읽는다 — 승인
                // 대기 중 매니페스트가 바뀌었으면 최신 cmd가 스폰된다(파킹된
                // cmd를 신뢰하지 않음 — 지문 재계산이 아니라 재조회로 방어).
                std::string cmd, dir, fp;
                if (shell_->ConsoleAppInfo(it->name, cmd, dir, fp)) {
                    SpawnConsoleApp(cmd, dir, it->name);
                }
            }
            for (auto& c : clients_) {
                if (c && c->Id() == it->requesterId && !c->IsDisconnected()) {
                    std::string result;
                    if (!allow) {
                        result = "{\"ok\":false,\"error\":\"denied_by_user\"}";
                    } else if (it->kind == "permission_set") {
                        // 쓰기 실패는 요청자 reply에 표면화 (docs/52 선례).
                        const std::string err = WritePermissionsEntry(
                            it->permTool, it->permDecision);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else if (it->kind == "capture_allow") {
                        // 설정 허브 §2.2 키별 Ask 해소: 두 캡처 도구를 함께
                        // 쓴다 — 쓰기는 RMW 2회(같은 값이라 멱등 방향).
                        std::string err = WritePermissionsEntry(
                            "capture_window", it->permDecision);
                        if (err.empty()) {
                            err = WritePermissionsEntry("capture_region",
                                                        it->permDecision);
                        }
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else if (it->kind == "trust_revoke") {
                        const std::string err =
                            RevokeTrustRecord(it->fingerprint);
                        result = err.empty()
                            ? "{\"ok\":true,\"written\":true,"
                              "\"restart_needed\":true}"
                            : "{\"ok\":false,\"error\":\"" + err + "\"}";
                    } else if (it->kind == "files_access") {
                        // 승인 = 원 요청 재실행(스펙 2026-09-18-file-hub
                        // §2.2) — 승인 시점에 deny 재검사(파킹 대기 중
                        // permissions.json이 바뀌면 최신 게이트가 강제 —
                        // run_console_app의 재조회 선례) + 경로 재검증.
                        if (FilesPermRaw(it->filesTool) == "deny") {
                            result =
                                "{\"ok\":false,\"error\":\"permission_denied\"}";
                        } else if (it->filesTool == "files_audit") {
                            // 감사 열람 파킹(MINOR-3) — 경로 없음, limit 재실행.
                            result = FilesAuditOpJson(it->filesLimit);
                        } else if (!ValidFilePath(it->filesPath)) {
                            result = "{\"ok\":false,\"error\":\"bad_path\"}";
                        } else if (it->filesTool == "files_read") {
                            result = FilesReadOpJson(it->filesPath,
                                                     it->filesMaxBytes);
                        } else {   // "files_list"
                            result = FilesListOpJson(it->filesPath);
                        }
                    } else if (it->kind == "send_input") {
                        // 승인 시점 게이트 재검사(파킹 대기 중 permissions.json이
                        // 바뀌면 최신 게이트 강제 — files_access/run_console_app
                        // 선례) + 원 요청 재실행.
                        if (AgentToolAllowed("send_input") ==
                            AgentDecision::Deny) {
                            result =
                                "{\"ok\":false,\"error\":\"permission_denied\"}";
                        } else {
                            jk::agent::AgentJson args(it->sendArgs);
                            SendInputOp op;
                            const std::string buildErr =
                                args.ok() ? BuildSendInputOp(args, &op)
                                          : "bad_args";
                            const std::string ex =
                                buildErr.empty() ? ExecuteSendInputOp(op)
                                                 : buildErr;
                            result = ex.empty()
                                ? "{\"ok\":true,\"sent\":true}"
                                : "{\"ok\":false,\"error\":\"" + ex + "\"}";
                        }
                    } else if (it->kind == "app_tool") {
                        // 앱 도구 허브 (스펙 2026-09-19-app-tool-hub §4.3/§9):
                        // 승인 = 중계 시작 — files_access의 재실행형이 아니라
                        // 파킹-응답형. resolve 시점에 10s 타이머가 시작되므로
                        // 승인 대기가 tool_timeout 오발을 낳지 않는다. 대기 중
                        // 앱이 닫혔으면 승인해도 중계 불가 — tool_gone(연결
                        // 생존 재확인 + 매니페스트 재조회, run_console_app의
                        // 승인 시점 재조회 선례).
                        auto mit = appToolManifests_.find(it->appToolConnId);
                        JKClientConnection* target = nullptr;
                        if (mit != appToolManifests_.end() &&
                            mit->second.app == it->appToolApp) {
                            for (auto& c : clients_) {
                                if (c && c->Id() == it->appToolConnId &&
                                    !c->IsDisconnected()) {
                                    target = c.get();
                                    break;
                                }
                            }
                        }
                        if (!target) {
                            result = "{\"ok\":false,\"error\":\"tool_gone\"}";
                        } else if (mit->second.modal &&
                                   pendingFileDialog_.dialogConnId !=
                                       mit->second.connId) {
                            // 모달 가드 — resolve 중계 경로 (스펙 §5 케이스
                            // ③): 파킹 대기 중 슬롯이 만료/해소/재사용되고
                            // 새 file_open이 슬롯을 가져가면, 늦게 승인된
                            // 이 요청은 고아(또는 타) 다이얼로그를 때린다.
                            // 첫 중계 경로(app_tool)와 동일 조건 — 판정은
                            // 앱이 아니라 슬롯 진실원 pendingFileDialog_
                            // .dialogConnId가 한다.
                            result = "{\"ok\":false,\"error\":\"tool_gone\"}";
                        } else if (!it->semKind.empty() &&
                                   CursorActStale(mit->second, *it)) {
                            // 의미 커서 (스펙 2026-09-22-semantic-cursor §3
                            // act, Task 2): 승인 시점 재선언 재검증 — 파킹 시
                            // 고정한 셀 rect가 최신 선언과 어긋나면(재선언으로
                            // 격자/기하가 바뀜) 승인해도 배너가 보여준 칸과
                            // 다른 칸을 때린다 — 거부(files_access의 승인
                            // 시점 재검증 선례). 해소 기록은 아래 공통 경로의
                            // approval_resolved 이벤트가 계속 발행해 보인다.
                            result =
                                "{\"ok\":false,\"error\":\"bad_grid\"}";
                        } else {
                            const uint32_t reqId = nextToolReqId_++;
                            InflightAppTool inf;
                            inf.reqId = reqId;
                            inf.queryId = it->queryId;
                            inf.requesterConnId = it->requesterId;
                            inf.targetConnId = target->Id();
                            inf.windowId = mit->second.windowId;
                            inf.expiresAt = std::time(nullptr) + 10;
                            // MINOR-3 — reset act 승인 경로: ok 회송 시 커서
                            // 정의 전이((0,0)) 리셋(스펙 §4).
                            inf.resetCursorOnOk =
                                (it->semKind == "reset");
                            // 폰 실전 — 일반 act는 acted 칸 이동(위 즉시
                            // allow 경로와 동일 규약, docs/64 §8).
                            if (!it->semKind.empty() &&
                                it->semKind != "reset") {
                                inf.semAct = true;
                                inf.semRow = it->semRow;
                                inf.semCol = it->semCol;
                            }
                            inflightAppTools_[reqId] = inf;
                            ipc::WriteAgentToolCall(
                                target->Transport(), reqId,
                                "{\"app\":\"" + JsonEsc(it->appToolApp) +
                                    "\",\"tool\":\"" + JsonEsc(it->appToolTool) +
                                    "\",\"args\":" +
                                    (it->appToolArgs.empty() ? "{}"
                                                             : it->appToolArgs) +
                                    "}");
                            deferred = true;  // HandleToolResult가 응답한다
                        }
                    } else {
                        result = "{\"ok\":true}";
                    }
                    const int flag = (result.find("\"ok\":true") !=
                                      std::string::npos)
                                         ? 1 : 0;
                    if (!deferred) {
                        ipc::WriteAgentJson(c->Transport(), ipc::MsgType::AgentReply,
                                            it->queryId, flag, result);
                    }
                    break;
                }
            }
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "{\"topic\":\"agent.approval_resolved\","
                          "\"request\":%u,\"decision\":\"%s\"}",
                          it->requestId, allow ? "allow" : "deny");
            PushAgentEventJson(buf);
            pendingApprovals_.erase(it);
            reply = allow ? "{\"ok\":true,\"approved\":true}"
                          : "{\"ok\":true,\"approved\":false}";
            break;
        }
        if (selfApprove) {
            reply = "{\"ok\":false,\"error\":\"self_approve\"}";
        } else if (!resolved) {
            reply = "{\"ok\":false,\"error\":\"unknown_request\"}";
        }
    } else if (tool == "file_open") {
        // 파일 열기 대화상자 (설계 specs/2026-09-13-file-dialog §1b): 쿼리를
        // 파킹한 뒤 filedlg:<json args> appName 접두로 다이얼로그를 띄운다.
        // 파킹은 jkchat close_window/trust_request와 같은 pendingApprovals_
        // 기계를 재사용 — 해소는 다이얼로그의 file_open_result가 담당하고,
        // 요청자가 먼저 닫히면 만료 스캔이 회수한다. 권한 게이트 없음 —
        // launch_app 같은 안전 계층 (AgentToolAllowed 기본 allow).
        std::string filter, start, title, wait;
        req.GetObjStr("args", "filter", filter);
        req.GetObjStr("args", "start", start);
        req.GetObjStr("args", "title", title);
        // wait 모드 (스펙 §6 결정 5): ""/"reply" = 현행(파킹 응답이 해소 때
        // 도착), "event" = 즉시 parked ack + 해소를 file.open_result 이벤트로.
        // 폰 브로커가 최대 600s 블록되는 것을 끊는 게 존재 이유. 알 수 없는
        // 값은 오류 표(스펙 §8)대로 즉답.
        req.GetObjStr("args", "wait", wait);
        // 256자 상한 (trust_request의 bad_name 선례) — 1차 가지치기. 실제
        // cmdLine 경계는 아래의 이스케이프 후 크기 검사다 (원시 길이만으로는
        // 인용 확장을 못 잡는다).
        if (!wait.empty() && wait != "reply" && wait != "event") {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
        } else if (filter.size() > 256 || start.size() > 256 ||
                   title.size() > 256) {
            reply = "{\"ok\":false,\"error\":\"bad_request\"}";
        } else if (pendingFileDialog_.requesterConnId != 0) {
            // 설계 리스크 2: 1슬롯 선착순 — 대화상자가 열려 있으면 후발은
            // 파킹해도 해소자가 없으므로 즉시 오류 (만료 대기보다 정직).
            reply = "{\"ok\":false,\"error\":\"dialog_busy\"}";
        } else {
            // 스폰 인자: args 그대로의 json (선택 필드만 — 없으면 키 생략).
            std::string jsonArgs = "{";
            bool first = true;
            auto appendField = [&](const char* key, const std::string& v) {
                if (v.empty()) return;
                if (!first) jsonArgs += ",";
                first = false;
                jsonArgs += std::string("\"") + key + "\":\"" + JsonEsc(v) +
                            "\"";
            };
            appendField("filter", filter);
            appendField("start", start);
            appendField("title", title);
            jsonArgs += "}";
            // publish_event의 이스케이프 후 크기 검사 선례: cmdLine 잘림은
            // 조용히 일어나므로 스폰 전에 이스케이프된 크기를 검문한다. 1000
            // = SpawnProcess cmdLine 2048 − 최악 exe 경로(~1026, 인용 포함)
            // − "--filedlg \"\"" 골격 — 원시 json에서 인용 확장분까지 포함한
            // 상계(JsonEsc는 인용+백슬래시를 모두 늘리므로 보수적).
            if (JsonEsc(jsonArgs).size() > 1000) {
                reply = "{\"ok\":false,\"error\":\"bad_request\"}";
            } else if (ApprovalParkingFull(client.Id())) {
                // 파킹 플러드 상한 (docs/56 §2b) — file_open도 승인 파킹의
                // 한 종류라 동일 노출(1슬롯 dialog_busy 검사가 먼저 온다).
                reply = "{\"ok\":false,\"error\":\"approval_overflow\"}";
            } else {
                // 만료는 승인 파이프라인의 60s가 아니라 600s — 사용자가
                // 다이얼로그에서 고민하는 시간을 감안한다. 요청자 연결이 먼저
                // 닫혀도 이 만료 스캔이 회수한다 (신규 무효화 코드 없음).
                PendingApproval p;
                p.kind = "file_open";
                p.requestId = nextApprovalId_++;
                p.queryId = queryId;
                p.requesterId = client.Id();
                p.targetId = 0;  // 대상 창 없음 — 다이얼로그가 해소자
                p.expiresAt = std::time(nullptr) + 600;
                pendingApprovals_.push_back(p);
                auto parkedIt = std::prev(pendingApprovals_.end());
                if (!SpawnClient(("filedlg:" + jsonArgs).c_str(), false)) {
                    // 스폰 실패 — 파킹 즉시 해소 (오류 응답). 저장해둔
                    // 반복자로 지운다 (pop_back의 순서 가정 제거).
                    pendingApprovals_.erase(parkedIt);
                    reply = "{\"ok\":false,\"error\":\"spawn_failed\"}";
                } else {
                    pendingFileDialog_.requesterConnId = client.Id();
                    pendingFileDialog_.requestId = p.requestId;
                    pendingFileDialog_.waitAsync = (wait == "event");
                    pendingFileDialog_.filter = filter;
                    pendingFileDialog_.start = start;
                    pendingFileDialog_.title = title;
                    if (pendingFileDialog_.waitAsync) {
                        // 스펙 §6 결정 5: 즉시 parked ack — 해소는
                        // file_open_result가 이벤트로 방송한다.
                        replied = true;
                        reply = "{\"ok\":true,\"parked\":true}";
                    } else {
                        // 현행 reply 모드 — file_open_result(또는 만료)가
                        // 응답한다.
                        replied = false;
                    }
                }
            }
        }
    } else if (tool == "file_dialog_params") {
        // filedlg 앱 기동 직후 1회 — file_open이 채운 파라미터를 꺼내 간다
        // (설계 D3: 모듈 ABI 무변경, 스폰 인자 회수는 쿼리로). 선착순 1회 —
        // 재요청은 오류. requesterConnId로 요청자-다이얼로그 상관관계를
        // 전달하고, 상관관계 자체는 결과 회수까지 슬롯에 남는다.
        if (pendingFileDialog_.requesterConnId == 0 ||
            pendingFileDialog_.paramsTaken) {
            reply = "{\"ok\":false,\"error\":\"no_pending_dialog\"}";
        } else {
            std::string out = "{\"ok\":true,\"requesterConnId\":" +
                              std::to_string(pendingFileDialog_.requesterConnId);
            auto appendParam = [&](const char* key, const std::string& v) {
                out += std::string(",\"") + key + "\":\"" + JsonEsc(v) + "\"";
            };
            appendParam("filter", pendingFileDialog_.filter);
            appendParam("start", pendingFileDialog_.start);
            appendParam("title", pendingFileDialog_.title);
            reply = out + "}";
            // 스펙 §5: 슬롯이 파라미터를 준 다이얼로그 연결을 기록 — 모달
            // 가드(app_tool 중계 재검증)의 진실원. paramsTaken과 함께
            // 선착순 소진 시점에 확정된다.
            pendingFileDialog_.dialogConnId = client.Id();
            pendingFileDialog_.paramsTaken = true;  // 선착순 소진
        }
    } else if (tool == "file_open_result") {
        // filedlg 앱 종료 결과 — 요청자의 파킹 쿼리를 완료한다 (approve 도구의
        // 파킹 해소 선례). 파킹이 이미 만료/부재면 no-op + parked:false.
        int ok = 0;
        std::string path;
        req.GetObjInt("args", "ok", ok);
        req.GetObjStr("args", "path", path);
        // 발신자 상관 검증 (final-review MAJOR-1): filedlg가
        // file_dialog_params로 받은 requesterConnId를 결과에 되울린다.
        // 필드 부재(구버전 다이얼로그/수조작) 또는 슬롯의 요청자와 불일치면
        // 아무것도 해소하지 않는 parked:false no-op — 슬롯도 지우지 않는다.
        // 검증이 없으면 만료 회수 후에도 살아 있던 고아 다이얼로그의 결과가
        // 새 요청자에게 잘못 전달된다(회귀 시나리오). 수동 `--client
        // filedlg` 실행(파킹 슬롯 없음)은 requesterConnId 0으로 발신하므로
        // 어느 경로든 무해한 no-op으로 수렴한다.
        int senderConnId = -1;
        req.GetObjInt("args", "requesterConnId", senderConnId);
        // 슬롯 리바인드 (docs/59 §13): 다이얼로그는 원 요청자 id를 에코한다 —
        // 요청자 교체 후엔 에코가 슬롯의 새 requesterConnId와 영구 불일치.
        // 발신 연결이 슬롯의 dialogConnId(파라미터를 준 다이얼로그 본인)와
        // 일치하면 요청자 에코보다 강한 진실원이므로 해소를 수락한다.
        // dialogConnId==0(파라미터 수락 전) 구간은 OR가 살아 있는 연결과
        // 0을 비교할 수 없어 위변조 여지 없음(기존 불변 유지).
        const bool senderMatched =
            senderConnId >= 0 &&
            (static_cast<uint32_t>(senderConnId) ==
                 pendingFileDialog_.requesterConnId ||
             (pendingFileDialog_.dialogConnId != 0 &&
              static_cast<uint32_t>(client.Id()) ==
                  pendingFileDialog_.dialogConnId));
        bool resolved = false;
        // dialogConnId 진실원의 해소 경로 적용 (final-review Important-2):
        // senderMatched는 "요청자 에코 일치"만 검사하므로 슬롯 재사용(고아
        // 다이얼로그 + 동일 요청자의 재호출) 조합에서 고아의 결과가 "새" 슬롯을
        // 대신 해소하는 교차배달이 가능했다 — ClientFileDialogApp.h의
        // "an orphan dialog must resolve nothing" 계약 집행. 슬롯이 파라미터를
        // 준 다이얼로그 연결(dialogConnId)과 발신 연결이 일치할 때만 해소.
        // dialogConnId 미기록(params 수락 전 창)은 기존 동작 유지이며, 수동
        // 기동(`--client filedlg`)은 requesterConnId 0이라 위 조건 어디든
        // 무해한 no-op으로 수렴한다(기존 불변).
        if (senderMatched && pendingFileDialog_.requesterConnId != 0 &&
            (pendingFileDialog_.dialogConnId == 0 ||
             static_cast<uint32_t>(client.Id()) ==
                 pendingFileDialog_.dialogConnId)) {
            // waitAsync는 슬롯 소진(아래 reset) 전에 판독 — 리셋 후엔 판독
            // 불가. 상관 검증/슬롯 소진은 waitAsync 양쪽이 공유한다.
            const bool waitAsync = pendingFileDialog_.waitAsync;
            for (auto it = pendingApprovals_.begin();
                 it != pendingApprovals_.end(); ++it) {
                if (it->kind != "file_open" ||
                    it->requestId != pendingFileDialog_.requestId) {
                    continue;
                }
                resolved = true;
                if (!waitAsync) {
                    for (auto& c : clients_) {
                        if (c && c->Id() == it->requesterId &&
                            !c->IsDisconnected()) {
                            // ok=false(취소)는 ok 플래그 0 + {"ok":false}.
                            const std::string result = ok
                                ? (path.empty()
                                       ? "{\"ok\":true}"
                                       : "{\"ok\":true,\"path\":\"" +
                                             JsonEsc(path) + "\"}")
                                : "{\"ok\":false}";
                            ipc::WriteAgentJson(c->Transport(),
                                                ipc::MsgType::AgentReply,
                                                it->queryId, ok ? 1 : 0,
                                                result);
                            break;
                        }
                    }
                }
                pendingApprovals_.erase(it);
                break;
            }
            if (waitAsync) {
                // 스펙 §6 결정 5 + 리바인드 (docs/59 §13): file.open_result
                // 이벤트는 승인 항목 매칭과 독립으로 방송 — 정상 waitAsync
                // 슬롯(항목 있음)과 리바인드 슬롯(항목 없음 — 요청자 교체로
                // requestId=0) 양쪽이 이 한 경로로 방송된다. 항목 미매칭
                // waitAsync의 기존 무통지 경로(해소됐는데 구독자가 영원히 못
                // 받는)도 여기서 마감. 원래 쿼리는 parked ack로 이미 회답됐다
                // — AgentReply는 생략(이중 전달 금지). 취소(ok=false)는
                // 사용자의 정당한 행동이라 error 멤버 없음 — "expired"만이
                // 오류 문자열(스펙 §6 페이로드).
                std::string ev = ok
                    ? (path.empty()
                           ? "{\"topic\":\"file.open_result\",\"ok\":true}"
                           : "{\"topic\":\"file.open_result\",\"ok\":true,"
                             "\"path\":\"" +
                                 JsonEsc(path) + "\"}")
                    : "{\"topic\":\"file.open_result\",\"ok\":false}";
                PushAgentEventJson(ev);
            }
            pendingFileDialog_ = PendingFileDialog{};  // 슬롯 소진 (결과와 무관)
        }
        reply = resolved ? "{\"ok\":true,\"parked\":true}"
                         : "{\"tool\":\"file_open_result\","
                           "\"ok\":true,\"parked\":false}";
    } else if (tool == "overlay_numbers") {
        // 창 번호 오버레이 (#118 T1 — 스펙 2026-10-27-window-overlay-design
        // D2·D5): on 부재/1 = 켬(이중 켬 = 시작틱 갱신 — 남은 시간 10분 재시계),
        // 0 = 끔. 배지의 실렌더는 컴포지트 훅 몫(DrawWindowNumberBadges — 이
        // 핸들러는 상태 2비트+시작틱만 심는다). args 판정·순서 산출은 순수
        // 부품 승계(OverlayNumbersOnArg/OverlayBadgeOrder — selftest 3a 원존).
        int onArg = 0;
        const bool hasOn = req.GetObjInt("args", "on", onArg);
        bool wantsOn = false;
        if (!OverlayNumbersOnArg(hasOn, onArg, wantsOn)) {
            reply = "{\"ok\":false,\"error\":\"bad_args\"}";
        } else if (wantsOn) {
            // 대상 모집 = 배지 스냅샷 수집기 1본(위 HandleAgentQuery 상단 —
            // #118 T2: list_windows 동형 필터+순서, 위에 "보이는 창" 필터,
            // **캡처 오버레이 면제 — 렌더 패스와 동기** — T1 리뷰 Minor 수형).
            const std::vector<OverlayBadgeTarget> targets = badgeTargets();
            const std::vector<uint32_t> order = OverlayBadgeOrder(targets);
            if (order.empty()) {
                // D5 정직: stub 없이 window_not_found 승계 계열의 honest err —
                // "창이 없습니다"가 올바른 답(가짜 ok로 표지 행세 금지).
                reply = "{\"ok\":false,\"error\":\"no_window\"}";
            } else {
                // 켬 전이든 갱신이든 같은 전체 제시 1회 — 켬은 활성 합성을
                // 보증하지 않으므로(Run 루프 활동 게이트) 표지 화면 도달은
                // 이 전체 프레젠트 플래그(다음 합성이 소비)에 달렸다.
                OverlayNumbersTurnOn(overlayNumbers_, SDL_GetTicks());
                if (compositor_) {
                    compositor_->RequestFullPresent();
                }
                reply = "{\"ok\":true,\"count\":" +
                        std::to_string(order.size()) + "}";
            }
        } else {
            const bool wasOn = overlayNumbers_.on;
            overlayNumbers_.on = false;
            if (wasOn && compositor_) {
                // 소멸 즉시 1회 전체 제시 — 뒷화면의 마지막 배지 잔상(더티
                // 제시 사다리는 배지 영역을 커밋 rect로 갖지 않는다)을 지운다.
                compositor_->RequestFullPresent();
            }
            reply = "{\"ok\":true}";   // 끔 요청 = 멱등(이미 꺼짐도 ok — 대상
                                       // 창이 없어서도 아니다 — 끌 상태만 있다)
        }
    } else {
        reply = "{\"ok\":false,\"error\":\"not_implemented\"}";
    }
    // 사용 저널 훅 (#113 T1 — 스펙 2026-10-26-use-review-cycle-design D1·D2·
    // D5): 에이전트 도구 1콜 1행 append-only 저널. 접점은 본 dispatch
    // if/else 사슬 말미의 이 한 곳뿐(DRY — 도구 20종+ 전부 흡수, 창 관리
    // launch/close/focus도 같은 도구 채널 위로 오니 자동 동행 — D1).
    //   중복 기록 아님(D5): events_list는 구독자 브로드캐스트 이벤트 행
    //   (PushAgentEventJson 발화), read_receipts는 files_* 수취 행 — 저널은
    //   "도구 1콜 1행"의 추가 원존이며 어느 기존 파일도 만지지 않는다.
    //   bad_request(도구명 불명) 행만 제외 — 빈 도구열 기록 방지.
    //   쓰기 실패 = UseJournalAppend 내부 조용한 스킵(D1) — 회신 무영향.
    //   회전(사이즈 상한) = v1 비-목표 — append 전용 원존.
    if (!tool.empty()) {
        // args 요약 = 요청의 "args" 원문(재직렬화 — GetRaw — 중첩/문자열
        // 원형 보존). 무인자 도구는 빈 arg. 128자 상한 절단은 writer 소관
        // (스펙 D2 — 접점에서는 전달만).
        std::string jArgs;
        if (!req.GetRaw("args", jArgs)) jArgs.clear();
        // ok 판정 = 회신 raw 스캔(files_audit/receipts의 "ok":true 규약
        // 동형).
        const bool jOk = reply.find("\"ok\":true") != std::string::npos;
        std::string jErr;
        if (!jOk) {
            if (reply.empty()) {
                // parked vs allow-relay 구분(#113 최final 소견 ①): reply 빈
                // 출구는 ask 게이트 파킹과 allow-relayed 앱 도구 중계(requester
                // 콜백 대기) 양쪽에 있다 — 유계 신호 = inflightAppTools_의
                // (queryId, requesterConnId) 일치(파킹 경로는 InflightAppTool을
                // 만들지 않으므로 이 조건이 릴레이에만 통과한다 — 승인 해소
                // 전파는 본 훅 밖 경로다).
                bool jRelayed = false;
                for (const auto& kv : inflightAppTools_)
                    if (kv.second.queryId == queryId &&
                        kv.second.requesterConnId == client.Id()) {
                        jRelayed = true;
                        break;
                    }
                jErr = jRelayed ? "relayed" : "parked";
            } else {
                // 오류 열 추출 — 서버 회신의 "error"는 1레벨 규약.
                jk::agent::AgentJson repJson(reply);
                if (!repJson.ok() || !repJson.GetStr("error", jErr))
                    jErr.clear();
            }
        }
        UseJournalAppend(StateDir().c_str(),
                         static_cast<long long>(
                             std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now()
                                     .time_since_epoch())
                                 .count()),
                         tool, jArgs, jOk, jErr);
    }
    // The ask path parks the query — its reply is sent when the approval
    // resolves (approve tool), when the filedlg app answers (file_open_result)
    // or when it times out (expiry scan below).
    if (replied) {
        ipc::WriteAgentJson(client.Transport(), ipc::MsgType::AgentReply,
                            queryId, 1, reply);
    }
}

}  // namespace server
}  // namespace jk
