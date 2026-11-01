// #125 스파이크-B — SDLActivity 서브클래스 (폐기 코드).
// 이식 원문: SDL2 2.32 SDLActivity 계약 — getLibraries()가 로드 순서를,
// getArguments()가 SDL_main argv를 정한다(기본값은 빈 배열 = 엔진 단일프로세스
// 데모 모드. --server를 넣어야 서버가 뜬다). getMainSharedObject는 getLibraries
// 마지막 원소의 lib<name>.so — jksrv가 끝이므로 libjkdesk.so가 진입물.
package chang.spikeb;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "jkdesk" };
    }

    @Override
    protected String[] getArguments() {
        // 엔진 main 인자 — 서버 모드 강제(스파이크는 클라만 exec로 겸용).
        return new String[] { "--server" };
    }

    @Override
    protected void onStart() {
        extractAssets();
        super.onStart();
    }

    // 컴파일 정의 상수 경로(/data/data/chang.spikeb/files/fonts)에 폰트를
    // 미리 풀어 둔다 — JKVectorFont/JKTextAtlas가 그 절대 경로로 읽는다.
    private void extractAssets() {
        String[] fontFiles = {
                "ENGLISH.FNT", "ENGLISH.VFT", "HANGUL.FNT", "HANJA.FNT",
                "HANMOON.VFT", "SPECIAL.FNT" };
        File fontsDir = new File(getFilesDir(), "fonts");
        fontsDir.mkdirs();
        for (String name : fontFiles) {
            File dst = new File(fontsDir, name);
            if (dst.isFile() && dst.length() > 0) continue;
            try (InputStream in = getAssets().open("fonts/" + name);
                 OutputStream out = new FileOutputStream(dst)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            } catch (Exception e) {
                android.util.Log.e("spikeB", "extractAssets failed: " + name, e);
            }
        }
    }
}