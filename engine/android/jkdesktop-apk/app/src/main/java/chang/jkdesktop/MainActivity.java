// #126 jkdesktop-apk — SDLActivity 서브클래스: lib 로드 순서·서버 인자·
// 에셋 추출(폰트/배경/런처 아이콘)·srv.err 내보내기(로그 수취 채널 — docs/103).

// 이식 원문: SDL2 2.32 SDLActivity 계약 — getLibraries()가 로드 순서를,
// getArguments()가 SDL_main argv를 정한다(기본값은 빈 배열 = 엔진 단일프로세스
// 데모 모드. --server를 넣어야 서버가 뜬다). getMainSharedObject는 getLibraries
// 마지막 원소의 lib<name>.so — jksrv가 끝이므로 libjkdesk.so가 진입물.
package chang.jkdesktop;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileInputStream;

import android.app.Activity;
import android.content.ContentValues;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.MediaStore;

public class MainActivity extends SDLActivity {
    private final Handler exporter = new Handler(Looper.getMainLooper());

    @Override
    protected String[] getLibraries() {
        // [P0 횡 — docs/106 §1-8] 빌드 지문(APK 실행본 추적관계 —
        // 스텔일 APK를 내용으로 판별하는 물음: 로그캐트에서 발견).
        android.util.Log.i("jkdesk", "BUILD_SHA=" + BuildConfig.BUILD_SHA);
        return new String[] { "SDL2", "jkdesk" };
    }

    @Override
    protected String[] getArguments() {
        // 엔진 main 인자 — 서버 모드 강제(#126는 클라만 exec로 겸용).
        return new String[] { "--server" };
    }

    @Override
    protected void onStart() {
        extractAssets();
        super.onStart();
    }

    @Override
    protected void onPostCreate(Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);
        // 수명 계약(스펙 §3): 데스크탑 서비스 — Back 이후 서버·음성 유지.
        try {
            startForegroundService(new android.content.Intent(this, ForegroundService.class));
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "fgs start failed", e);
        }
        // SDL_main이 freopen한 files/srv.log를 MediaStore.Downloads로 내보낸다
        // — Android 15 타앱 logcat 필터(14:36 실측)의 대체 수취 채널. 기동 후
        // 8초 간격 6회 — 서버 부팅 로그를 Termux scp로 수취할 수 있게 한다.
        for (int i = 1; i <= 6; i++) {
            exporter.postDelayed(this::exportSrvLog, 8000L * i);
        }
    }

    @Override
    protected void onPause() {
        exportSrvLog();
        super.onPause();
    }

    // [2026-11-13 실측 원존] 종전 = MediaStore insert/update에 "jk-err-N.log"
    // 고정 이름 → MediaStore가 text/plain에 .txt를 덧붙여 "jk-err-1.log.txt"를
    // 만들고, exportSeq가 부팅마다 1로 리셋돼 같은 이름과 충돌 →
    // "Failed to build unique file" + update 예외가 매 내보내기마다 반복됐다
    // (로그 수출 채널 자체가 죽어 있었음). 수형 = Downloads에 **고정 파일로
    // 직접 덮어쓰기**(MANAGE_EXTERNAL_STORAGE 보유 — 최신 1개만 유지, 충돌
    // 원천 소각) + 권한/경로 실패 시 MediaStore 폴백(이름에 타임스탬프 = 유일).
    private void exportSrvLog() {
        exportFile(new File(getFilesDir(), "srv.err"), "jk_srv.err.log");
        exportFile(new File(getFilesDir(), "srv.log"), "jk_srv.out.log");
    }

    private void exportFile(File src, String name) {
        if (!src.isFile() || src.length() == 0) return;
        try {
            File dir = android.os.Environment.getExternalStoragePublicDirectory(
                    android.os.Environment.DIRECTORY_DOWNLOADS);
            File dst = new File(dir, name);
            try (FileInputStream in = new FileInputStream(src);
                 FileOutputStream out = new FileOutputStream(dst, false)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
            android.media.MediaScannerConnection.scanFile(
                    this, new String[] { dst.getAbsolutePath() }, null, null);
            return;
        } catch (Exception directErr) {
            android.util.Log.w("jkdesk", "direct download write failed: " + directErr);
        }
        // 폴백 — MediaStore(타임스탬프 이름 = 충돌 불가).
        try {
            String stamp = new java.text.SimpleDateFormat("MMdd-HHmmss",
                    java.util.Locale.US).format(new java.util.Date());
            ContentValues cv = new ContentValues();
            cv.put(MediaStore.Downloads.DISPLAY_NAME,
                   name.replace(".log", "-" + stamp + ".log"));
            cv.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
            cv.put(MediaStore.Downloads.IS_PENDING, 1);
            Uri uri = getContentResolver().insert(
                    MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
            if (uri == null) return;
            try (FileInputStream in = new FileInputStream(src);
                 OutputStream out = getContentResolver().openOutputStream(uri)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while (out != null && (n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
            ContentValues done = new ContentValues();
            done.put(MediaStore.Downloads.IS_PENDING, 0);
            getContentResolver().update(uri, done, null, null);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "exportSrvLog fallback failed", e);
        }
    }

    // 컴파일 정의 상수 경로(/data/data/chang.jkdesktop/files/…)에 자산을 미리
    // 풀어 둔다 — SDL_main이 files/로 chdir해 상대 "assets/…" 경로를 해석한다
    // (r11 원문: 서버가 CWD에서 assets/backgrounds·icons 로드 실패).
    private void extractAssets() {
        // [P0 — docs/106 §3] 자산 버전 스탬프: 1판은 versionCode 고정 → APK 업데이트 후에도
        // 샘이름 자산이 스텔일 유지 — 스탬프 일치 시 전체 열거 생략(부팅 가속).
        // 스탬프 = 버전 + 자산 트리 사이즈 합(개수 변화 감지 — 추가 자산은 재추출).
        try {
            java.io.File stamp = new java.io.File(getFilesDir(), "assets.stamp");
            String want = assetStamp();
            if (stamp.isFile()) {
                java.io.BufferedReader br = new java.io.BufferedReader(
                    new java.io.FileReader(stamp));
                String have = br.readLine();
                br.close();
                java.io.File first = new java.io.File(getFilesDir(),
                    "assets/icons/launcher_browser@1x.png");
                if (want.equals(have) && first.isFile() && first.length() > 0) return;
            }
        } catch (Exception ignored) { }
        // 전체 동적 추출 — assets 트리를 files/assets로(런처 아이콘 30종@1x·
        // 바탕 전부 — CWD(files) 상대 "assets/…" 해석점 정합). 폰트만 files/fonts
        // (컴파일 정의 절대 경로 계약 — JKENGINE_FONT_DIR "/data/data/…/fonts").
        try {
            java.util.ArrayList<String> all = new java.util.ArrayList<>();
            collectAssets("", all, 3);
            for (String path : all) {
                String dstRel = path.startsWith("fonts/") ? path : "assets/" + path;
                File dst = new File(getFilesDir(), dstRel);
                // [2026-11-13 원존] 종전 "이미 있고 크기>0 = 건너뜀"은 **갱신된
                // 아이콘을 영원히 교체하지 않았다**(스탬프가 불일치해 전체 추출을
                // 돌아도 기존 파일은 옛 세대 유지). 스탬프 불일치 경로 = 전체
                // 덮어쓰기가 계약 — 이 분기에 도달했다는 것 자체가 재추출 신호다.
                // [2026-11-14 성능] 단 **크기가 같은 파일은 복사 생략**(비압축
                // 자산만 openFd로 확인 — 압축 자산/실패 = 종전대로 복사). 매
                // 업데이트마다 수십 MB를 UI 스레드로 다시 쓰던 비용을 실제 변경분
                // 으로 줄인다.
                try {
                    long assetLen = -1;
                    try {
                        assetLen = getAssets().openFd(path).getLength();
                    } catch (Exception notFd) {
                        assetLen = -1;   // 압축 자산 — 크기 조회 불가
                    }
                    if (assetLen >= 0 && dst.isFile() && dst.length() == assetLen) {
                        continue;
                    }
                } catch (Exception ignored) {
                }
                // [P0] 파일당 try/catch — 1건 실패가 전체를 쓰러뜼리지 않게.
                try {
                    dst.getParentFile().mkdirs();
                    try (InputStream in = getAssets().open(path);
                         OutputStream out = new FileOutputStream(dst)) {
                        byte[] buf = new byte[64 * 1024];
                        int n;
                        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                    }
                    // [lf/helix 폰 조달 leg — 11-07] 추출 기본 0660 → 실행 비트 보증.
                    if (dstRel.startsWith("assets/bin/")) dst.setExecutable(true, false);
                } catch (Exception fileErr) {
                    android.util.Log.e("jkdesk", "asset " + path + " failed", fileErr);
                }
            }
            // 스탬프 기록(완전 추출 성공 — 다음 부팅부터 생략 가능).
            try {
                java.io.File stamp = new java.io.File(getFilesDir(), "assets.stamp");
                String v = assetStamp();
                try (java.io.FileWriter fw = new java.io.FileWriter(stamp)) {
                    fw.write(v);
                }
            } catch (Exception ignored) { }
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "extractAssets failed", e);
        }
    }

    // [2026-11-13 실측 원존] 종전 스탬프 = versionCode + getAssets().list("").length
    // = **최상위 항목 수**뿐이라 하위 폴더에 파일을 추가해도 값이 그대로 —
    // 새 아이콘이 APK에는 있는데 폰 files/assets에는 옛 세대가 남았다(launcher_chat
    // 부재 원문). 수형 = 패키지 갱신 시각(lastUpdateTime: 재설치마다 변함) +
    // 아이콘/폰트 하위 개수(부분 갱신 감지). 열거 비용은 깊이 1회뿐.
    private String assetStamp() {
        try {
            android.content.pm.PackageInfo pi = getPackageManager()
                .getPackageInfo(getPackageName(), 0);
            return pi.versionCode + ":" + pi.lastUpdateTime
                + ":" + getAssets().list("icons").length
                + ":" + getAssets().list("fonts").length;
        } catch (Exception e) {
            return "unknown:" + System.currentTimeMillis();
        }
    }

    private void collectAssets(String dir, java.util.List<String> out, int depth) {
        try {
            String[] items = getAssets().list(dir);
            for (String name : items) {
                String rel = dir.isEmpty() ? name : dir + "/" + name;
                // 쉘 상대 로드 경로 = "assets/…" (fonts 제외 — 컴파일 정의 절대 경로)
                if (depth > 0 && getAssets().list(rel).length > 0) {
                    collectAssets(rel, out, depth);
                } else if (rel.startsWith("assets/")) {
                    out.add(rel);   // dst = files/assets/… (CWD 해석점 그대로)
                } else {
                    out.add(rel);   // fonts 등 직접 경로
                }
            }
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "collectAssets failed: " + dir, e);
        }
    }
}