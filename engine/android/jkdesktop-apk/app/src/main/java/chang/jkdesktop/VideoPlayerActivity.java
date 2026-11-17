// VideoPlayerActivity.java — 폰/DeX 동영상 재생기 (2026-11-15 시작, 2026-11-18 완성)
//
// 왜 이 방식인가: 데스크톱 vplayer(jkapp_vplayer)는 FFmpeg 의존이라 Android NDK
// 빌드에 아직 없다(CMake 주석 "vplayer 제외 — 스펙 §6 v2"). 그동안 "영상이 안
// 나온다"를 없애기 위해 **Android 기본 재생기(MediaPlayer/VideoView)** 로 위임한다
// — 오디오·코덱·자막을 플랫폼이 처리하고, 우리는 **어느 화면에 띄울지**와
// **조작 UI**를 책임진다.
//
// 계약(2026-11-18 사용자 판정 "재생화면만 있네요" 반영 — 컨트롤 신설):
//  - intent extra "path" = 재생할 파일(비었으면 최근 동영상 자동 선택)
//  - 기동 표면 = **셸이 떠 있는 디스플레이**(사용자가 누른 화면) → PRESENTATION → 기본
//  - 화면 꺼짐 방지 + 전체화면
//  - 상단 바: 파일명 · [목록] · [✕ 닫기]
//  - 하단 바: [▶/⏸] · 진행 바(드래그 탐색) · "00:03 / 00:06"
//  - 화면 탭 = 하단 바 표시/숨김, 끝나면 "다시 재생" 안내(창은 유지)
package chang.jkdesktop;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.hardware.display.DisplayManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.VideoView;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

public final class VideoPlayerActivity extends Activity {
    private VideoView video;
    private FrameLayout rootView;
    private TextView hint;
    private TextView titleView;
    private TextView timeView;
    private Button playPause;
    private SeekBar seek;
    private LinearLayout bottomBar;
    private LinearLayout topBar;

    private String pendingPath;
    private boolean resumeTried;
    private boolean userSeeking = false;   // 드래그 중에는 진행 바를 덮어쓰지 않는다
    private final Handler ui = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        rootView = new FrameLayout(this);
        rootView.setBackgroundColor(Color.BLACK);

        video = new VideoView(this);
        rootView.addView(video, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        hint = new TextView(this);
        hint.setTextColor(Color.WHITE);
        hint.setTextSize(18f);
        hint.setGravity(Gravity.CENTER);
        rootView.addView(hint, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER));

        // 상단 바 — 파일명 · 목록 · 닫기
        topBar = new LinearLayout(this);
        topBar.setOrientation(LinearLayout.HORIZONTAL);
        topBar.setBackgroundColor(0xAA000000);
        topBar.setPadding(dp(16), dp(10), dp(16), dp(10));

        titleView = new TextView(this);
        titleView.setTextColor(Color.WHITE);
        titleView.setTextSize(16f);
        titleView.setSingleLine(true);
        titleView.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);
        topBar.addView(titleView, new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));

        Button listBtn = barButton("목록");
        listBtn.setOnClickListener(v -> showPickDialog());
        topBar.addView(listBtn);

        Button close = barButton("닫기");
        close.setOnClickListener(v -> finish());
        topBar.addView(close);

        FrameLayout.LayoutParams tp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP);
        rootView.addView(topBar, tp);

        // 하단 바 — 재생/일시정지 · 진행 바 · 시간
        bottomBar = new LinearLayout(this);
        bottomBar.setOrientation(LinearLayout.HORIZONTAL);
        bottomBar.setGravity(Gravity.CENTER_VERTICAL);
        bottomBar.setBackgroundColor(0xAA000000);
        bottomBar.setPadding(dp(12), dp(8), dp(16), dp(8));

        playPause = barButton("일시정지");
        playPause.setOnClickListener(v -> togglePlay());
        bottomBar.addView(playPause);

        seek = new SeekBar(this);
        seek.setMax(1000);
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                if (!fromUser) return;
                final int dur = video.getDuration();
                if (dur > 0) {
                    video.seekTo((int) ((long) dur * progress / 1000));
                    updateTime();
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar sb) { userSeeking = true; }

            @Override
            public void onStopTrackingTouch(SeekBar sb) { userSeeking = false; }
        });
        bottomBar.addView(seek, new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));

        timeView = new TextView(this);
        timeView.setTextColor(Color.WHITE);
        timeView.setTextSize(14f);
        timeView.setPadding(dp(12), 0, 0, 0);
        timeView.setText("00:00 / 00:00");
        bottomBar.addView(timeView);

        FrameLayout.LayoutParams bp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM);
        rootView.addView(bottomBar, bp);

        // 화면 탭 = 컨트롤 표시/숨김(끝난 뒤에는 다시 재생).
        rootView.setOnClickListener(v -> {
            if (!video.isPlaying() && video.getDuration() > 0
                    && hint.getVisibility() == View.VISIBLE) {
                hint.setVisibility(View.GONE);
                video.seekTo(0);
                video.start();
                syncPlayButton();
                return;
            }
            toggleBars();
        });

        setContentView(rootView);

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

    private int dp(int v) {
        return (int) (v * getResources().getDisplayMetrics().density);
    }

    private Button barButton(String label) {
        Button b = new Button(this);
        b.setText(label);
        b.setTextSize(15f);
        b.setAllCaps(false);
        b.setTextColor(Color.WHITE);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0x33FFFFFF);
        bg.setCornerRadius(dp(10));
        b.setBackground(bg);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, dp(40));
        lp.setMargins(dp(6), 0, 0, 0);
        b.setLayoutParams(lp);
        return b;
    }

    private void toggleBars() {
        final boolean show = topBar.getVisibility() != View.VISIBLE;
        topBar.setVisibility(show ? View.VISIBLE : View.GONE);
        bottomBar.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    private void togglePlay() {
        if (video.isPlaying()) video.pause();
        else video.start();
        syncPlayButton();
    }

    private void syncPlayButton() {
        if (playPause != null) playPause.setText(video.isPlaying() ? "일시정지" : "재생");
    }

    private static String fmt(int ms) {
        final int s = Math.max(0, ms / 1000);
        return String.format(Locale.US, "%02d:%02d", s / 60, s % 60);
    }

    private void updateTime() {
        if (timeView == null) return;
        final int dur = video.getDuration();
        timeView.setText(fmt(video.getCurrentPosition()) + " / " + fmt(dur));
        if (!userSeeking && dur > 0) {
            final int p = (int) ((long) video.getCurrentPosition() * 1000 / dur);
            seek.setProgress(Math.max(0, Math.min(1000, p)));
        }
    }

    // 0.5초 틱 — 진행 바·시간 갱신(정지 상태면 갱신만, 비용 국소).
    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            if (!isFinishing()) {
                updateTime();
                syncPlayButton();
                ui.postDelayed(this, 500);
            }
        }
    };

    private boolean hasStorageAccess() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            return android.os.Environment.isExternalStorageManager();
        }
        return checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE)
                == android.content.pm.PackageManager.PERMISSION_GRANTED;
    }

    private void askStorageAccess() {
        hint.setText("동영상을 열려면 파일 접근 권한이 필요합니다.");
        AlertDialog.Builder b = new AlertDialog.Builder(this);
        b.setTitle("파일 접근 권한");
        b.setMessage("동영상을 읽으려면 \"모든 파일 접근\" 권한이 필요합니다.\n"
                + "설정에서 Jkdesktop을 허용한 뒤 이 화면으로 돌아오면 자동으로 재생합니다.");
        b.setPositiveButton("설정 열기", (d, w) -> {
            resumeTried = true;
            try {
                startActivity(new Intent(
                        android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())));
            } catch (Exception e) {
                try {
                    startActivity(new Intent(
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
        if (resumeTried && hasStorageAccess()) {
            resumeTried = false;
            startPending();
        }
        ui.removeCallbacks(tick);
        ui.post(tick);
    }

    @Override
    protected void onPause() {
        super.onPause();
        ui.removeCallbacks(tick);
        if (video != null && video.isPlaying()) video.pause();
        syncPlayButton();
    }

    private void startPending() {
        String path = pendingPath != null ? pendingPath : pickLatestVideo();
        if (path == null) {
            hint.setText("재생할 동영상이 없습니다\n(Download/Movies 폴더에 영상을 넣어 주세요)");
            return;
        }
        pendingPath = path;
        open(path);
    }

    private void open(String path) {
        openUri(Uri.fromFile(new File(path)), new File(path).getName());
    }

    private void openUri(Uri uri, String displayName) {
        final String shown = displayName;
        hint.setText("여는 중: " + displayName);
        hint.setVisibility(View.VISIBLE);
        if (titleView != null) titleView.setText(displayName);
        video.setVideoURI(uri);
        video.setOnPreparedListener((MediaPlayer mp) -> {
            hint.setVisibility(View.GONE);
            video.start();
            syncPlayButton();
            updateTime();
            android.util.Log.i("jkdesk", "video playing: " + shown +
                    " size=" + mp.getVideoWidth() + "x" + mp.getVideoHeight() +
                    " dur=" + mp.getDuration() + "ms");
        });
        video.setOnErrorListener((mp, what, extra) -> {
            hint.setText("재생 실패 (code " + what + "/" + extra + ")\n" + shown);
            hint.setVisibility(View.VISIBLE);
            android.util.Log.e("jkdesk",
                    "video error what=" + what + " extra=" + extra + " " + shown);
            return true;
        });
        // [2026-11-18] 완료 = 창 유지 + 안내(탭하면 처음부터). 컨트롤은 그대로 남는다.
        video.setOnCompletionListener(mp -> {
            android.util.Log.i("jkdesk", "video ended: " + shown);
            hint.setText("재생이 끝났습니다\n화면을 누르면 다시 재생합니다");
            hint.setVisibility(View.VISIBLE);
            syncPlayButton();
            updateTime();
        });
    }

    // [2026-11-18 사용자 판정 "목록 기능에서 파일 선택해야 하는 거 아닌가요?"]
    // 목록 = ① 기기 파일 선택(시스템 문서 선택기, video/*) + ② 최근 동영상 빠른 선택.
    private static final int REQ_PICK_FILE = 1001;

    private void showPickDialog() {
        final List<File> files = listVideos();
        final String[] names = new String[files.size() + 1];
        names[0] = "다른 파일 선택… (기기 둘러보기)";
        for (int i = 0; i < files.size(); ++i) {
            names[i + 1] = files.get(i).getName();
        }
        new AlertDialog.Builder(this)
                .setTitle("동영상 선택")
                .setItems(names, (d, which) -> {
                    if (which == 0) pickWithSystemChooser();
                    else open(files.get(which - 1).getAbsolutePath());
                })
                .setNegativeButton("닫기", null)
                .show();
    }

    // 시스템 문서 선택기 — 다운로드/드라이브/문서 앱 포함. content:// URI 그대로 재생.
    private void pickWithSystemChooser() {
        try {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("video/*");
            i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivityForResult(i, REQ_PICK_FILE);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "file chooser failed", e);
            hint.setText("파일 선택기를 열 수 없습니다");
            hint.setVisibility(View.VISIBLE);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK_FILE || resultCode != RESULT_OK || data == null) return;
        final Uri uri = data.getData();
        if (uri == null) return;
        try {
            getContentResolver().takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception ignored) {
        }
        String name = uri.getLastPathSegment();
        if (name == null || name.isEmpty()) name = "선택한 동영상";
        openUri(uri, name);
    }

    private List<File> listVideos() {
        String[] dirs = { "/sdcard/Movies", "/sdcard/Download", "/sdcard/DCIM/Camera" };
        String[] exts = { ".mp4", ".mkv", ".webm", ".m4v", ".3gp" };
        List<File> out = new ArrayList<>();
        for (String d : dirs) {
            File[] files = new File(d).listFiles();
            if (files == null) continue;
            for (File f : files) {
                if (!f.isFile()) continue;
                String n = f.getName().toLowerCase(Locale.US);
                for (String e : exts) {
                    if (n.endsWith(e)) {
                        out.add(f);
                        break;
                    }
                }
            }
        }
        out.sort(Comparator.comparingLong(File::lastModified).reversed());
        return out;
    }

    // 최근 동영상 자동 선택 — Movies/Download/DCIM 순, 디렉터리별 최신.
    private String pickLatestVideo() {
        List<File> all = listVideos();
        return all.isEmpty() ? null : all.get(0).getAbsolutePath();
    }

    @Override
    protected void onDestroy() {
        ui.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    // [2026-11-18 사용자 지적] "폰 화면에는 일부러 안 띄웁니다"는 잘못된 설명이었다.
    // 실제 동작은 첫 비기본/PRESENTATION 후보로 **가상 Desktop(display 16)** 을 골라
    // 아무도 안 보는 화면에 그리는 것이었다(실측 로그: video launch display=16).
    // 정본 = **사용자가 누른 화면과 같은 디스플레이**(셸 액티비티의 디스플레이).
    private static int pickDisplayId(Context ctx) {
        try {
            if (ctx instanceof Activity) {
                Display d = ((Activity) ctx).getWindowManager().getDefaultDisplay();
                if (d != null) return d.getDisplayId();
            }
        } catch (Throwable ignored) {
        }
        try {
            DisplayManager dm = (DisplayManager) ctx.getSystemService(
                    Context.DISPLAY_SERVICE);
            if (dm != null) {
                Display[] pres = dm.getDisplays(
                        DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
                if (pres != null && pres.length > 0) return pres[0].getDisplayId();
            }
        } catch (Throwable ignored) {
        }
        return Display.DEFAULT_DISPLAY;
    }

    // 외부(셸 런처/JNI 훅/오버레이)에서 부르는 진입점. path 빈 문자열 = 최신 동영상.
    static void launchFrom(Context ctx, String path) {
        try {
            Intent i = new Intent(ctx, VideoPlayerActivity.class);
            i.putExtra("path", path == null ? "" : path);
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            final int displayId = pickDisplayId(ctx);
            android.util.Log.i("jkdesk", "video launch display=" + displayId);
            Bundle opts = ActivityOptions.makeBasic()
                    .setLaunchDisplayId(displayId).toBundle();
            ctx.startActivity(i, opts);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "video launch failed", e);
        }
    }
}
