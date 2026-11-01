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
    private long exportSeq = 0;

    @Override
    protected String[] getLibraries() {
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

    private void exportSrvLog() {
        // stdout(srv.log)+stderr(srv.err) — r11 분리 수체 규약
        exportOne(new File(getFilesDir(), "srv.err"), "jk-err-" + (++exportSeq) + ".log");
        exportOne(new File(getFilesDir(), "srv.log"), "jk-out-" + (++exportSeq) + ".log");
    }

    private void exportOne(File src, String name) {
        try {
            if (!src.isFile() || src.length() == 0) return;
            ContentValues cv = new ContentValues();
            cv.put(MediaStore.Downloads.DISPLAY_NAME, name);
            cv.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
            cv.put(MediaStore.Downloads.IS_PENDING, 1);
            Uri uri = getContentResolver().insert(
                    MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
            if (uri == null) return;
            try (FileInputStream in = new FileInputStream(src);
                 OutputStream out = getContentResolver().openOutputStream(uri)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while (in != null && out != null && (n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
            ContentValues done = new ContentValues();
            done.put(MediaStore.Downloads.IS_PENDING, 0);
            getContentResolver().update(uri, done, null, null);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "exportSrvLog failed", e);
        }
    }

    // 컴파일 정의 상수 경로(/data/data/chang.jkdesktop/files/…)에 자산을 미리
    // 풀어 둔다 — SDL_main이 files/로 chdir해 상대 "assets/…" 경로를 해석한다
    // (r11 원문: 서버가 CWD에서 assets/backgrounds·icons 로드 실패).
    private void extractAssets() {
        String[] assetPaths = {
                "fonts/ENGLISH.FNT", "fonts/ENGLISH.VFT", "fonts/HANGUL.FNT",
                "fonts/HANJA.FNT", "fonts/HANMOON.VFT", "fonts/SPECIAL.FNT",
                "assets/backgrounds/desktop@1x.png",
                "assets/icons/launcher_mine@1x.png",
        };
        for (String path : assetPaths) {
            File dst = new File(getFilesDir(), path);
            if (dst.isFile() && dst.length() > 0) continue;
            dst.getParentFile().mkdirs();
            // getAssets() 계약: open 경로는 assets 루트 상대 — APK 안의 배치가
            // "assets/…"면 open은 "…"다(r13 원문 — launcher_mine 0건 추출).
            String openPath = path.startsWith("assets/")
                    ? path.substring("assets/".length()) : path;
            try (InputStream in = getAssets().open(openPath);
                 OutputStream out = new FileOutputStream(dst)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            } catch (Exception e) {
                android.util.Log.e("jkdesk", "extractAssets failed: " + path, e);
            }
        }
    }
}