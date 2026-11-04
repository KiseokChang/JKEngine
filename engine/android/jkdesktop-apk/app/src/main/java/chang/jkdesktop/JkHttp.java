// T7 — LLM HTTP 어댑터의 cloud TLS leg (플랜 2026-11-04-jkapk-t7-llm-http T3):
// 플랫폼 HTTPS(HttpsURLConnection) — TLS 조달 0(스펙 §5 (b) 계약). 네이티브가
// JNI로 부른다(jkvoice_bridge.cpp의 JkHttp 클래스 캐시·GetStaticMethodID
// 원존 계약). 실패(비200·예외) = null 반환 — 네이티브의 정직 실패 계약과
// 동형(소비자 stub 폴백). 민감 키는 네이티브가 Bearer에 결합 전달(이 TU는
// 저장하지 않는다).
package chang.jkdesktop;

public final class JkHttp {
    private JkHttp() {}

    // @return 응답 본문(UTF-8) 또는 null(실패). 호출 = 네이티브 워커 스레드.
    public static String httpPost(String url, String body, String bearer,
                                  int timeoutMs) {
        try {
            java.net.URL u = new java.net.URL(url);
            java.net.HttpURLConnection conn =
                (java.net.HttpURLConnection) u.openConnection();
            conn.setRequestMethod("POST");
            conn.setConnectTimeout(timeoutMs);
            conn.setReadTimeout(timeoutMs);
            conn.setDoOutput(true);
            conn.setInstanceFollowRedirects(false);
            conn.setRequestProperty("Content-Type", "application/json");
            if (bearer != null && !bearer.isEmpty()) {
                conn.setRequestProperty("Authorization", bearer);
            }
            byte[] out = body.getBytes("UTF-8");
            conn.setFixedLengthStreamingMode(out.length);
            java.io.OutputStream os = conn.getOutputStream();
            os.write(out);
            os.flush();
            os.close();
            int code = conn.getResponseCode();
            if (code != 200) {
                return null;
            }
            java.io.InputStream in = conn.getInputStream();
            java.io.ByteArrayOutputStream baos =
                new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            for (int n; (n = in.read(buf)) > 0; ) {
                baos.write(buf, 0, n);
            }
            in.close();
            return baos.toString("UTF-8");
        } catch (Exception e) {
            return null;
        }
    }
}