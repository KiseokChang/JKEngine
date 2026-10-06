#!/bin/bash
# CPU 소등 실측 (docs/78 TX5 폰 관측 — 서버 ~99%/클라 ~90% 스핀) —
# 프레임 페이싱+활동 게이트(서버/클라) 적용 후 영수증. Δ측정= /proc utime+stime
# 3초 간격 대차 (ps %cpu는 수명 평균이라 idle 판정 불가 — 렛슨).
# 표준: WSLg :0, setsid 부팅, 셀프테스트 RC, 측정 후 정리.
set -u
cd /mnt/i/progwork/JKENGINE/engine

echo "=== selftest ==="
./buildwsl/jkdesktop test >/tmp/pac_test.log 2>&1
echo "test rc=$?"; grep -aE "failure" /tmp/pac_test.log | head -1

echo "=== boot server (setsid detached) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null; sleep 1
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >/tmp/pac_srv.log 2>&1 &
sleep 5

echo "=== launch client terminal ==="
timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"terminal"}}' 2>/dev/null | grep -a '{' | head -1
sleep 8

echo "=== cpu delta over 3s (idle, no typing) ==="
for pid in $(pgrep -f 'buildwsl/jkdesktop'); do
    cmd=$(tr '\0' ' ' < /proc/$pid/cmdline 2>/dev/null)
    t1=$(awk '{print $14+$15}' /proc/$pid/stat 2>/dev/null) || continue
    [ -z "$t1" ] && continue
    sleep 3
    t2=$(awk '{print $14+$15}' /proc/$pid/stat 2>/dev/null) || continue
    [ -z "$t2" ] && continue
    awk -v d=$((t2-t1)) -v c="$cmd" 'BEGIN{printf "%5.1f%% %s\n", d/300*100, c}'
done

echo "=== cleanup ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
echo "=== PACING-CHECK-END ==="