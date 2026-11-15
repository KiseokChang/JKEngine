// VideoPlayerActivity.java — 폰/DeX 동영상 재생 MVP (2026-11-15 사용자 판정)
//
// 왜 이 방식인가: 데스크톱 vplayer(jkapp_vplayer)는 FFmpeg 의존이라 Android NDK
// 빌드에 아직 없다(CMake 주석 "vplayer 제외 — 스펙 §6 v2"). 그동안 "TV에서
// 영상이 안 나온다"를 없애기 위해 **Android 기본 재생기(MediaPlayer/VideoView)**
// 로 위임한다 — 오디오·코덱·자막을 플랫폼이 처리하고, 우리는 **어느 화면(DeX)에
// 띄울지**만 책임진다. FFmpeg 탑재는 후속(인앱 재생·OSD 통합).
//
// 계약:
//  - intent extra "path" = 재생할 파일(비었으면 최근 동영상 자동 선택)
//  - 기동 표면 = **외부(DeX) 디스플레이** 우선(없으면 폰 화면)
//  - 화면 꺼짐 방지 + 전체화면 + 하단 기본 컨트롤(탭하면 나타남)
package chang.jkdesktop;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.hardware.display.DisplayManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.view.Display;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.MediaController;
import android.widget.TextView;
import android.widget.VideoView;

import java.io.File;
import java.util.Arrays;
import java.util.Comparator;

public final class VideoPlayerActivity extends Activity {
    private VideoView video;
    private TextView hint;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.BLACK);

        video = new VideoView(this);
        MediaController ctl = new MediaController(this);
        ctl.setAnchorView(video);
        video.setMediaController(ctl);
        root.addView(video, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        hint = new TextView(this);
        hint.setTextColor(Color.WHITE);
        hint.setTextSize(18f);
        hint.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams hp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT);
        hp.gravity = Gravity.CENTER;
        root.addView(hint, hp);

        setContentView(root);

        pendingPath = getIntent() != null ? getIntent().getStringExtra("path") : null;
        if (pendingPath == null || pendingPath.isEmpty()) pendingPath = null;
        // [권한 UX 2026-11-15] 외부 저장소 접근이 없으면 MediaPlayer가 what=1로
        // 조용히 죽는다 → 이유와 설정 이동을 보여주고, 허용 후 돌아오면 자동 재시도.
        if (!hasStorageAccess()) {
            askStorageAccess();
            return;
        }
        startPending();
    }

    // 재생 대기 경로(null = 최근 동영상 자동) / 설정 왕복 표식.
    private String pendingPath;
    private boolean resumeTried;

    private boolean hasStorageAccess() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            return android.os.Environment.isExternalStorageManager();
        }
        return checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE)
                == android.content.pm.PackageManager.PERMISSION_GRANTED;
    }

    private void askStorageAccess() {
        hint.setText("동영상을 열려면 파일 접근 권한이 필요합니다.");
        android.app.AlertDialog.Builder b = new android.app.AlertDialog.Builder(this);
        b.setTitle("파일 접근 권한");
        b.setMessage("동영상을 읽으려면 \"모든 파일 접근\" 권한이 필요합니다.\n"
                + "설정에서 Jkdesktop을 허용한 뒤 이 화면으로 돌아오면 자동으로 재생합니다.");
        b.setPositiveButton("설정 열기", (d, w) -> {
            resumeTried = true;
            try {
                startActivity(new android.content.Intent(
                        android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())));
            } catch (Exception e) {
                try {
                    startActivity(new android.content.Intent(
                            android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                } catch (Exception e2) {
                    android.util.Log.e("jkdesk", "storage settings failed", e2);
                }
            }
        });
        b.setNegativeButton("닫기", (d, w) -> finish());
        b.show();
        android.util.Log.i("jkdesk", "video: storage permission required");
    }

    @Override
    protected void onResume() {
        super.onResume();
        // 설정에서 허용하고 돌아오면 자동 재시도 — 사용자가 같은 화면에 다시 오게.
        if (resumeTried && hasStorageAccess()) {
            resumeTried = false;
            startPending();
        }
    }

    private void startPending() {
        String path = pendingPath != null ? pendingPath : pickLatestVideo();
        if (path == null) {
            hint.setText("재생할 동영상이 없습니다\n(Download/Movies 폴더에 영상을 넣어 주세요)");
            return;
        }
        final String shown = path;
        hint.setText("여는 중: " + new File(path).getName());
        video.setVideoURI(Uri.fromFile(new File(path)));
        video.setOnPreparedListener((MediaPlayer mp) -> {
            hint.setVisibility(View.GONE);
            video.start();
            android.util.Log.i("jkdesk", "video playing: " + shown);
        });
        video.setOnErrorListener((mp, what, extra) -> {
            hint.setText("재생 실패 (code " + what + "/" + extra + ")\n" + shown);
            android.util.Log.e("jkdesk",
                    "video error what=" + what + " extra=" + extra + " " + shown);
            return true;
        });
        video.setOnCompletionListener(mp -> finish());
    }

    // 최근 동영상 자동 선택 — Movies/Download/DCIM 순, 디렉터리별 최신.
    private String pickLatestVideo() {
        String[] dirs = { "/sdcard/Movies", "/sdcard/Download", "/sdcard/DCIM/Camera" };
        String[] exts = { ".mp4", ".mkv", ".webm", ".m4v", ".3gp" };
        File best = null;
        for (String d : dirs) {
            File dir = new File(d);
            File[] files = dir.listFiles();
            if (files == null || files.length == 0) continue;
            Arrays.sort(files, Comparator.comparingLong(File::lastModified));
            for (int i = files.length - 1; i >= 0; i--) {
                File f = files[i];
                String n = f.getName().toLowerCase();
                boolean isVideo = false;
                for (String e : exts) {
                    if (n.endsWith(e)) { isVideo = true; break; }
                }
                if (!isVideo || !f.isFile()) continue;
                if (best == null || f.lastModified() > best.lastModified()) best = f;
                break;
            }
        }
        return best != null ? best.getAbsolutePath() : null;
    }

    // 외부(셸 런처/JNI 훅/오버레이)에서 부르는 진입점. path 빈 문자열 = 최신 동영상.
    // 외부(DeX) 디스플레이가 있으면 그쪽에 띄운다 — "TV에서 봐야 한다"는 목적.
    static void launchFrom(Context ctx, String path) {
        try {
            Intent i = new Intent(ctx, VideoPlayerActivity.class);
            i.putExtra("path", path == null ? "" : path);
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            Bundle opts = null;
            try {
                DisplayManager dm = (DisplayManager) ctx.getSystemService(
                        Context.DISPLAY_SERVICE);
                int displayId = Display.DEFAULT_DISPLAY;
                if (dm != null) {
                    for (Display d : dm.getDisplays()) {
                        if (d.getDisplayId() != Display.DEFAULT_DISPLAY) {
                            displayId = d.getDisplayId();
                            break;
                        }
                    }
                }
                opts = ActivityOptions.makeBasic()
                        .setLaunchDisplayId(displayId).toBundle();
                android.util.Log.i("jkdesk", "video launch display=" + displayId);
            } catch (Exception ignored) {
            }
            if (opts != null) ctx.startActivity(i, opts);
            else ctx.startActivity(i);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "video launch failed", e);
        }
    }
}
