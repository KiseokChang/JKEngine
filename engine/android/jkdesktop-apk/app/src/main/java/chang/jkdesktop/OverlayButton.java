// OverlayButton.java — 시스템 오버레이 미니 태스크바(UI v2 MVP, docs/106 §3)
//
// 사용자 확정 범위(2026-11-14): "다른 앱 위에 뜨는 최소 버전" — 다른 앱을 쓰다
// 잃어버린 jkdesktop으로 돌아가는 **떠 있는 버튼** 하나. 전체 태스크바(창 목록·
// 미니 프리뷰)는 후속: 네이티브 셸 렌더를 오버레이 서피스에 합성해야 한다.
//
// 권한: SYSTEM_ALERT_WINDOW — 없으면 설정 화면(ACTION_MANAGE_OVERLAY_PERMISSION)만
// 연다(자동 부여 금지 — 사용자가 직접 허용).
// 동작(2026-11-15 사용자 판정 반영 — "실행 말고 다른 기능"):
//   탭        = 음성 듣기 시작(JkVoice.begin) — 다른 앱을 쓰다 바로 말로 조작
//   길게 누름 = jkdesktop을 **DeX(외부) 디스플레이**로 전면 복귀
//              (종전엔 폰 화면(display 0)에 떠서 사용자 의도와 어긋났다)
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
            remove();
            return false;
        }
        if (!canDraw(ctx)) {
            openPermissionSettings(ctx);
            return false;
        }
        show(ctx);
        return view != null;
    }

    public static boolean isShowing() {
        return view != null;
    }

    private static void show(Context ctx) {
        try {
            Context app = ctx.getApplicationContext();
            wm = (WindowManager) app.getSystemService(Context.WINDOW_SERVICE);
            if (wm == null) return;

            int size = Math.round(TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, 44,
                    app.getResources().getDisplayMetrics()));
            TextView v = new TextView(app);
            v.setText("JK");
            v.setTextColor(Color.WHITE);
            v.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
            v.setGravity(Gravity.CENTER);
            GradientDrawable bg = new GradientDrawable();
            bg.setShape(GradientDrawable.OVAL);
            bg.setColor(0xE61565C0);   // 반투명 파랑 — 어디서나 보이게
            bg.setStroke(2, 0xFFFFFFFF);
            v.setBackground(bg);
            v.setAlpha(0.92f);

            params = new WindowManager.LayoutParams(
                    size, size,
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                            ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                            : WindowManager.LayoutParams.TYPE_PHONE,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                            | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                    android.graphics.PixelFormat.TRANSLUCENT);
            params.gravity = Gravity.TOP | Gravity.START;
            SharedPreferences sp = app.getSharedPreferences("overlay", 0);
            params.x = sp.getInt("x", 24);
            params.y = sp.getInt("y", 240);

            v.setOnTouchListener((cv, ev) -> {
                switch (ev.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
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
                        if (Math.abs(dx) > 8 || Math.abs(dy) > 8) moved = true;
                        params.x = Math.max(0, startParamX + dx);
                        params.y = Math.max(0, startParamY + dy);
                        try {
                            wm.updateViewLayout(view, params);
                        } catch (Exception ignored) {
                        }
                        return true;
                    case MotionEvent.ACTION_UP:
                        dragging = false;
                        if (!moved) {
                            final long heldMs = android.os.SystemClock.uptimeMillis()
                                    - downAt;
                            if (heldMs >= 500) {
                                bringAppToFront(app);   // 길게 = DeX로 복귀
                            } else {
                                startVoice(app);        // 탭 = 음성 듣기
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
            });

            view = v;
            wm.addView(v, params);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "overlay add failed", e);
            view = null;
        }
    }

    private static void remove() {
        try {
            if (wm != null && view != null) wm.removeView(view);
        } catch (Exception ignored) {
        }
        view = null;
        wm = null;
        params = null;
    }

    // 외부(DeX) 디스플레이 찾기 — 없으면 기본 디스플레이.
    private static int externalDisplayId(Context ctx) {
        try {
            DisplayManager dm = (DisplayManager) ctx.getSystemService(
                    Context.DISPLAY_SERVICE);
            if (dm == null) return Display.DEFAULT_DISPLAY;
            for (Display d : dm.getDisplays()) {
                if (d.getDisplayId() != Display.DEFAULT_DISPLAY) {
                    return d.getDisplayId();   // DeX/HDMI 등 첫 외부 화면
                }
            }
        } catch (Exception ignored) {
        }
        return Display.DEFAULT_DISPLAY;
    }

    private static void bringAppToFront(Context app) {
        try {
            Intent i = app.getPackageManager()
                    .getLaunchIntentForPackage(app.getPackageName());
            if (i == null) return;
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
            final int displayId = externalDisplayId(app);
            ActivityOptions opts =
                    ActivityOptions.makeBasic().setLaunchDisplayId(displayId);
            app.startActivity(i, opts.toBundle());
            android.util.Log.i("jkdesk",
                    "overlay bring-to-front display=" + displayId);
        } catch (Exception e) {
            android.util.Log.e("jkdesk", "overlay bring-to-front failed", e);
        }
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
