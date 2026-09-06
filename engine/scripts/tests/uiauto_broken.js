// 의도적으로 틀린 시나리오 — test-script 러너의 결함 검출력을 검증한다
// (docs/27 §5 단계 2 검증: "의도적 버그 주입 1건"). 이 스크립트는 반드시
// 실패해야 하며, 러너는 종료 코드 1을 내야 한다.
function onCreate() {
    createButton({ x: 10, y: 10, w: 80, h: 24 }, "Push", 10);
    assertEq(2 + 2, 5, "injected defect: arithmetic must fail");
}