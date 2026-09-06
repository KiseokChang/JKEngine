// docs/27 단계 2 시나리오 — 좌표 클릭 probe 대체 스모크.
// UI를 짓고 findControl/click/injectMouse/setText/getText로 구동한 뒤
// assert/assertEq로 상태를 검증한다. 실패는 [script] ASSERT FAIL 로그 +
// 실패 카운트로 남고, `jkdesktop test-script` 러너가 종료 코드로 승화한다.
// 시나리오는 동기형으로 유지한다 (러너의 타이머 서비스는 no-op).
var btnId = 0;
var labelId = 0;
var editId = 0;
var clicks = 0;

function onClick(id) {
    if (id === btnId) {
        clicks++;
        setText(labelId, "clicked:" + clicks);
    }
}

function onCreate() {
    btnId = createButton({ x: 10, y: 10, w: 80, h: 24 }, "Push", 10);
    labelId = createLabel({ x: 10, y: 40, w: 200, h: 20 }, "ready", 11);
    editId = createEdit({ x: 10, y: 70, w: 120, h: 22 }, "hello", 12);

    assert(findControl(btnId) === btnId, "findControl by id");
    assert(findControl("Push") === btnId, "findControl by visible text");
    assert(findControl("no-such-thing") === null, "findControl miss returns null");

    // 구조적 클릭 — 버튼의 OnClick을 직접 호출한다.
    click(btnId);
    assertEq(getText(labelId), "clicked:1", "structural click drives onClick");

    // 행동 주입 — RespondMessage 라우팅(히트테스트)을 통과하는 실제 입력 경로.
    // 버튼 {10,10,80,24}의 중심.
    injectMouse(50, 22);
    assertEq(getText(labelId), "clicked:2", "injected mouse routes to the button");

    setText(editId, "world");
    assertEq(getText(editId), "world", "setText/getText roundtrip");

    log("uiauto scenario done: " + clicks + " click(s)");
}