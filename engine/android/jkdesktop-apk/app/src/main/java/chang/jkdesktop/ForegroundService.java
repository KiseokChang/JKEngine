// #126 jkdesktop-apk — 포그라운드 서비스 (스펙 2026-11-01 §3 수명 계약):
// 서버 표면·음성 세션의 소멸 방지 — Back 이후에도 데스크탑 유지.
package chang.jkdesktop;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;

public class ForegroundService extends Service {
    static final String CHANNEL = "jkdesktop";
    static final int NOTIF_ID = 126;

    @Override
    public void onCreate() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationManager nm = getSystemService(NotificationManager.class);
            NotificationChannel ch = new NotificationChannel(
                    CHANNEL, "JKDesktop", NotificationManager.IMPORTANCE_LOW);
            nm.createNotificationChannel(ch);
            Notification n = new Notification.Builder(this, CHANNEL)
                    .setSmallIcon(android.R.drawable.ic_media_play)
                    .setContentTitle("JKDesktop")
                    .setContentText("데스크탑 실행 중")
                    .setOngoing(true)
                    .build();
            // 원장 2건(logcat 원문): ①3-인수 명시 SPECIAL_USE → Samsung
            // CannotPostForegroundServiceNotification 사망 ②smallIcon 없는
            // 알림 → "Bad notification for startForeground" 사망 — smallIcon
            // 필수 + 매니페스트 타입 추론 2-인수 계약.
            startForeground(NOTIF_ID, n);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;  // 시스템 회수 후 재기동 — 수명 계약(§3)
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}