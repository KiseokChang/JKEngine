// OverlayButton.java — 시스템 오버레이 미니 태스크바(UI v2 MVP, docs/106 §3)
//
// 사용자 확정 범위(2026-11-14): "다른 앱 위에 뜨는 최소 버전" — 다른 앱을 쓰다
// 잃어버린 jkdesktop으로 돌아가는 **떠 있는 버튼** 하나. 전체 태스크바(창 목록·
// 미니 프리뷰)는 후속: 네이티브 셸 렌더를 오버레이 서피스에 합성해야 한다.
//
// 권한: SYSTEM_ALERT_WINDOW — 없으면 설정 화면(ACTION_MANAGE_OVERLAY_PERMISSION)만
// 연다(자동 부여 금지 — 사용자가 직접 허용).
// 동작(2026-11-15 사용자 판정 반영 — "실행 말고 다른 기능", 전략 docs/111 P2):
//   길게      = 상태판 — /talk "창 목록 보여줘" 회신을 화면 패널 + TTS로.
//              (더블탭은 폰 기본 제스처인 **화면 잠금**과 충돌해 폐기 — 사용자
//               실측 2026-11-16. 명령 채널은 /talk 단일.)
//   탭        = 음성 듣기 시작(JkVoice.begin) — 다른 앱을 쓰다 바로 말로 조작
//   (DeX 복귀 = TV의 jkdesktop 아이콘 탭 — 버튼에서 제거: 같은 제스처를
//    상태판에 양보했다)
//   드래그    = 위치 이동(위치는 재시작 후에도 복원)

package chang.jkdesktop;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.hardware.display.DisplayManager;
import android.view.Display;
import android.app.ActivityOptions;
import android.widget.TextView;

public final class OverlayButton {
    private static View view;
    private static WindowManager.LayoutParams params;
    private static WindowManager wm;
    private static WindowManager.LayoutParams spec;
    private static boolean dragging;
    private static float dragStartX;
    private static float dragStartY;
    private static int startParamX;
    private static int startParamY;
    private static boolean moved;
    private static long downAt;
    private static final android.os.Handler UI =
            new android.os.Handler(android.os.Looper.getMainLooper());

    private OverlayButton() {}

    public static boolean canDraw(Context ctx) {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.M
                || Settings.canDrawOverlays(ctx);
    }

    // 설정 화면 열기(사용자가 직접 허용 — 앱이 대신 켜지 않는다).
    public static void openPermissionSettings(Context ctx) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        try {
            Intent i = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + ctx.getPackageName()));
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(i);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "overlay settings failed", e);
        }
    }

    // 토글: 켜짐 → 끄기, 꺼짐 → (권한 있으면) 띄우기 / (없으면) 설정 화면.
    // 반환 = 토글 후 표시 중 여부.
    public static boolean toggle(Context ctx) {
        if (view != null) {
            remove("toggle-off");
            setWanted(ctx, false);
            return false;
        }
        if (!canDraw(ctx)) {
            openPermissionSettings(ctx);
            return false;
        }
        show(ctx);
        setWanted(ctx, view != null);
        return view != null;
    }

    // [2026-11-15 실측 "탭했더니 사라졌어요"] 의도 저장 — 액티비티가 재생성되거나
    // 프로세스가 새로 뜰 때 **원래 켜져 있었으면 다시 띄운다**. 종전엔 켜짐 상태가
    // 메모리에만 있어 조용히 사라진 것처럼 보였다.
    private static void setWanted(Context ctx, boolean wanted) {
        try {
            ctx.getApplicationContext().getSharedPreferences("overlay", 0)
                    .edit().putBoolean("wanted", wanted).apply();
        } catch (Exception ignored) {
        }
    }

    public static boolean wanted(Context ctx) {
        try {
            return ctx.getApplicationContext().getSharedPreferences("overlay", 0)
                    .getBoolean("wanted", false);
        } catch (Exception e) {
            return false;
        }
    }

    // 앱 기동 시 호출 — 켜져 있었으면 복원(권한 상실 시엔 조용히 포기).
    public static void restoreIfWanted(Context ctx) {
        if (view != null || !wanted(ctx)) return;
        if (!canDraw(ctx)) {
            android.util.Log.i("jkdesk", "overlay restore skipped (no permission)");
            return;
        }
        show(ctx);
        android.util.Log.i("jkdesk", "overlay restored -> " + (view != null));
    }

    public static boolean isShowing() {
        return view != null;
    }

    // 칩(원형) 하나 만들기 — 색만 다르고 규격 동일.
    private static TextView makeChip(Context app, int size, String label, int color) {
        TextView t = new TextView(app);
        t.setText(label);
        t.setTextColor(Color.WHITE);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        t.setGravity(Gravity.CENTER);
        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.OVAL);
        bg.setColor(color);
        bg.setStroke(2, 0xFFFFFFFF);
        t.setBackground(bg);
        t.setAlpha(0.92f);
        return t;
    }

    private static void show(Context ctx) {
        try {
            Context app = ctx.getApplicationContext();
            // [실측 원존 2026-11-16] 앱 리소스는 **DeX 표면 밀도(1.0)** 를 물고 있어
            // 44dp가 44px로 계산됐다 → 폰 화면에서 칩이 손톱만 하고 터치도 어긋남
            // ("버튼 이동도 안 되고"). 오버레이가 붙는 **디스플레이의 컨텍스트**로
            // 치수를 계산한다(폰 기본 디스플레이 = 2.625 배율).
            Context uiCtx = app;
            try {
                DisplayManager dm0 = (DisplayManager) app.getSystemService(
                        Context.DISPLAY_SERVICE);
                Display def = dm0 != null ? dm0.getDisplay(Display.DEFAULT_DISPLAY) : null;
                if (def != null) uiCtx = app.createDisplayContext(def);
            } catch (Exception ignored) {
            }
            wm = (WindowManager) uiCtx.getSystemService(Context.WINDOW_SERVICE);
            if (wm == null) return;

            int size = Math.round(TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, 44,
                    uiCtx.getResources().getDisplayMetrics()));
            // [2026-11-16 사용자 실측 2건] 더블탭 = 폰 화면 잠금, 길게 = 시스템
            // 앱 선택 창 — 두 제스처 모두 OS가 선점한다. 그래서 **버튼을 둘로**
            // 나눈다: JK(탭=음성), ≡(탭=상태판). 제스처는 탭/드래그뿐.
            android.widget.LinearLayout bar = new android.widget.LinearLayout(uiCtx);
            bar.setOrientation(android.widget.LinearLayout.HORIZONTAL);

            TextView mic = makeChip(uiCtx, size, "JK", 0xE61565C0);
            TextView stat = makeChip(uiCtx, size, "≡", 0xE63F4754);
            android.widget.LinearLayout.LayoutParams c1 =
                    new android.widget.LinearLayout.LayoutParams(size, size);
            c1.rightMargin = Math.round(size * 0.18f);
            bar.addView(mic, c1);
            bar.addView(stat, new android.widget.LinearLayout.LayoutParams(size, size));

            View v = bar;
            mic.setTag("mic");
            stat.setTag("status");
            params = new WindowManager.LayoutParams(
                    // [실측 원존 2026-11-16 "버튼 이동도 안 되고"] 두 칩을 담는
                    // 컨테이너인데 창 폭을 **칩 1개 크기(size)**로 고정해 잘렸다
                    // (≡가 크롭돼 안 보이고 터치도 어긋남) → 양방향 WRAP_CONTENT.
                    WindowManager.LayoutParams.WRAP_CONTENT,
                    WindowManager.LayoutParams.WRAP_CONTENT,
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                            ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                            : WindowManager.LayoutParams.TYPE_PHONE,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                            | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                            | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                    android.graphics.PixelFormat.TRANSLUCENT);
            params.gravity = Gravity.TOP | Gravity.START;
            SharedPreferences sp = app.getSharedPreferences("overlay", 0);
            params.x = sp.getInt("x", 24);
            params.y = sp.getInt("y", 240);

            final View.OnTouchListener handler = (cv, ev) -> {
                switch (ev.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        android.util.Log.i("jkdesk", "overlay touch down @" +
                                (int) ev.getRawX() + "," + (int) ev.getRawY() +
                                " tag=" + (cv != null ? cv.getTag() : "?"));
                        dragging = true;
                        moved = false;
                        downAt = android.os.SystemClock.uptimeMillis();
                        dragStartX = ev.getRawX();
                        dragStartY = ev.getRawY();
                        startParamX = params.x;
                        startParamY = params.y;
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        if (!dragging) return false;
                        int dx = (int) (ev.getRawX() - dragStartX);
                        int dy = (int) (ev.getRawY() - dragStartY);
                        // [실측 원존 2026-11-16] 임계 8px는 폰 실제 탭의 손떨림에도
                        // 걸려 **모든 탭이 드래그로 판정**됐다(로그: tap → moved=true,
                        // 상태판·음성 미실행). 손가락 탭 허용 오차 = 24px.
                        if (Math.abs(dx) > 24 || Math.abs(dy) > 24) moved = true;
                        params.x = Math.max(0, startParamX + dx);
                        params.y = Math.max(0, startParamY + dy);
                        try {
                            wm.updateViewLayout(view, params);
                        } catch (Exception ignored) {
                        }
                        return true;
                    case MotionEvent.ACTION_UP:
                        android.util.Log.i("jkdesk", "overlay touch up moved=" +
                                moved + " tag=" + (cv != null ? cv.getTag() : "?"));
                        dragging = false;
                        if (!moved) {
                            if (cv != null && cv.getTag() != null &&
                                "status".equals(cv.getTag().toString())) {
                                statusBriefing(app);    // ≡ 탭 = 상태판
                            } else {
                                startVoice(app);        // JK 탭 = 음성
                            }
                        } else {
                            app.getSharedPreferences("overlay", 0).edit()
                                    .putInt("x", params.x)
                                    .putInt("y", params.y)
                                    .apply();
                        }
                        return true;
                    default:
                        return false;
                }
            };

            mic.setOnTouchListener(handler);
            stat.setOnTouchListener(handler);

            view = v;
            wm.addView(v, params);
            // 진단: 창이 실제로 어떤 크기로 붙었는지 1회 기록(터치 불능 신고 대응).
            v.post(() -> android.util.Log.i("jkdesk",
                    "overlay added w=" + v.getWidth() + " h=" + v.getHeight() +
                    " at=" + params.x + "," + params.y));
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "overlay add failed", e);
            view = null;
        }
    }

    private static void remove(String reason) {
        android.util.Log.i("jkdesk", "overlay remove: " + reason);
        try {
            if (wm != null && view != null) wm.removeView(view);
        } catch (Exception ignored) {
        }
        view = null;
        wm = null;
        params = null;
    }

    // 길게 = 상태판 — 전략 docs/111 P2 "폰 = 조종간 + 상태판"(더블탭 폐기).
    // 명령은 /talk 단일 채널로 보내고 회신을 TTS로 읽는다(별도 상태 API 없음).
    static void statusBriefing(Context app) {
        new Thread(() -> {
            try {
                java.net.URL u = new java.net.URL("http://127.0.0.1:8091/talk");
                java.net.HttpURLConnection c =
                        (java.net.HttpURLConnection) u.openConnection();
                c.setRequestMethod("POST");
                c.setDoOutput(true);
                c.setConnectTimeout(3000);
                c.setReadTimeout(8000);
                c.setRequestProperty("Content-Type", "application/json");
                byte[] body = "{\"text\":\"창 목록 보여줘\"}"
                        .getBytes(java.nio.charset.StandardCharsets.UTF_8);
                c.getOutputStream().write(body);
                java.io.InputStream in = c.getInputStream();
                java.io.ByteArrayOutputStream bo = new java.io.ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                int n;
                while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
                in.close();
                String json = new String(bo.toByteArray(),
                        java.nio.charset.StandardCharsets.UTF_8);
                String reply = extractJsonString(json, "reply");
                final java.util.List<String> titles = extractTitles(json);
                android.util.Log.i("jkdesk", "overlay status -> " + reply +
                        " / windows=" + titles.size());
                // [P2 상태판] 눈으로 보는 목록 + 귀로 듣는 요약(둘 다 같은 회신 기반).
                UI.post(() -> showStatusPanel(app, titles));
                if (reply != null && !reply.isEmpty()) JkTts.speak(reply);
            } catch (Exception e) {
                android.util.Log.e("jkdesk", "overlay status failed", e);
            }
        }, "jk-overlay-status").start();
    }

    // list_windows 회신에서 창 제목만 뽑는다(스키마: {"windows":[{"title":..}]}).
    private static java.util.List<String> extractTitles(String json) {
        java.util.List<String> out = new java.util.ArrayList<>();
        // /talk 회신은 server 필드 안에 JSON을 **한 번 더 이스케이프**해 담는다
        // (\"title\":\"...\"). 먼저 풀지 않으면 제목을 0건으로 센다(실측
        // 2026-11-15: 창 2개인데 windows=0).
        json = json.replace("\\\"", "\"").replace("\\\\", "\\");
        int at = 0;
        final String key = "\"title\":\"";
        while ((at = json.indexOf(key, at)) != -1) {
            at += key.length();
            StringBuilder sb = new StringBuilder();
            for (int i = at; i < json.length(); i++) {
                char ch = json.charAt(i);
                if (ch == '\\') { i++; if (i < json.length()) sb.append(json.charAt(i)); }
                else if (ch == '"') break;
                else sb.append(ch);
            }
            if (sb.length() > 0) out.add(sb.toString());
        }
        return out;
    }

    // [P2 상태판] 다른 앱 위에 뜨는 목록 패널 — 탭하면 닫힌다.
    private static View statusPanel;

    private static void showStatusPanel(Context ctx, java.util.List<String> titles) {
        try {
            dismissStatusPanel();
            Context app = ctx.getApplicationContext();
            // [2026-11-15] 상태판은 **DeX(외부) 디스플레이 우선**에 띄운다 —
            // TV로 보는 사용자에게 폰 화면 패널은 보이지 않는다(눈확인 원문).
            // 외부 화면이 없으면 기본 디스플레이(폰).
            Context panelCtx = app;
            try {
                DisplayManager dm = (DisplayManager) app.getSystemService(
                        Context.DISPLAY_SERVICE);
                if (dm != null) {
                    // [2026-11-17] 첫 비기본 표면은 **가상 Desktop**(비가시)일 수
                    // 있다 — 실제로 보이는 외부 화면(PRESENTATION)만 고른다.
                    Display[] pres = dm.getDisplays(
                            DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
                    if (pres != null && pres.length > 0) {
                        panelCtx = app.createDisplayContext(pres[0]);
                        android.util.Log.i("jkdesk", "status panel display=" +
                                pres[0].getDisplayId());
                    }
                }
            } catch (Exception ignored) {
            }
            WindowManager mgr = (WindowManager) panelCtx.getSystemService(
                    Context.WINDOW_SERVICE);
            if (mgr == null) return;

            TextView tv = new TextView(panelCtx);
            StringBuilder sb = new StringBuilder();
            sb.append("떠 있는 창 ").append(titles.size()).append("개\n\n");
            if (titles.isEmpty()) sb.append("(없음)");
            for (int i = 0; i < Math.min(titles.size(), 12); i++) {
                sb.append(i + 1).append(". ").append(titles.get(i)).append('\n');
            }
            sb.append("\n(탭하면 닫힙니다)");
            tv.setText(sb.toString());
            tv.setTextColor(0xFFF0F0F5);
            // [실측 2026-11-15 "상태창 글씨 작아서 못 읽겠다"] TV 거리 가독 —
            // 표면 높이의 비례(≈3%)로 키우고 최소 22sp를 보장한다.
            final android.util.DisplayMetrics dm = panelCtx.getResources().getDisplayMetrics();
            float sp = Math.max(22f, (dm.heightPixels / dm.density) * 0.030f);
            tv.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, sp);
            tv.setLineSpacing(0f, 1.25f);
            final int padPx = Math.round(sp * dm.density * 0.9f);
            tv.setPadding(padPx, Math.round(padPx * 0.75f), padPx,
                          Math.round(padPx * 0.75f));

            GradientDrawable bg = new GradientDrawable();
            bg.setColor(0xF016171A);
            bg.setCornerRadius(padPx);
            bg.setStroke(2, 0x60FFFFFF);
            tv.setBackground(bg);

            WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                    WindowManager.LayoutParams.WRAP_CONTENT,
                    WindowManager.LayoutParams.WRAP_CONTENT,
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                            ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                            : WindowManager.LayoutParams.TYPE_PHONE,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                    android.graphics.PixelFormat.TRANSLUCENT);
            lp.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
            lp.y = Math.round(dm.heightPixels * 0.10f);
            // 긴 제목이 화면을 넘지 않게 폭 상한(80%).
            lp.width = Math.round(dm.widthPixels * 0.80f);
            tv.setOnClickListener(v -> dismissStatusPanel());
            mgr.addView(tv, lp);
            statusPanel = tv;
            statusPanelMgr = mgr;
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "status panel failed", e);
        }
    }

    private static WindowManager statusPanelMgr;

    private static void dismissStatusPanel() {
        if (statusPanel != null && statusPanelMgr != null) {
            try {
                statusPanelMgr.removeView(statusPanel);
            } catch (Exception ignored) {
            }
        }
        statusPanel = null;
        statusPanelMgr = null;
    }

    // 최소 JSON 문자열 추출 — {"reply":"..."} 한 키면 충분(이스케이프 해제).
    private static String extractJsonString(String json, String key) {
        String needle = "\"" + key + "\":\"";
        int at = json.indexOf(needle);
        if (at < 0) return null;
        StringBuilder sb = new StringBuilder();
        for (int i = at + needle.length(); i < json.length(); i++) {
            char ch = json.charAt(i);
            if (ch == '\\') { i++; if (i < json.length()) sb.append(json.charAt(i)); }
            else if (ch == '"') break;
            else sb.append(ch);
        }
        return sb.toString();
    }

    // 탭 = 음성 듣기 시작(푸시투토크) — 다른 앱 위에서도 말로 조작.
    private static void startVoice(Context app) {
        try {
            boolean started = JkVoice.begin(app);
            android.util.Log.i("jkdesk", "overlay voice begin -> " + started);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "overlay voice begin failed", e);
        }
    }
}
