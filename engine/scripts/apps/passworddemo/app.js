// passworddemo — docs/27 단계 3 레거시 포팅 파일럿.
// 원본: src/apps/JangoUI.cpp의 PasswordDialog와 그 진입 흐름. JangoUI의
// "Personnel" 메뉴 버튼 → PasswordDialog 모달(라벨 + 입력란 + OK/Cancel) →
// 결과 처리(성공: 환영 표시 / 실패: Error 메시지 박스)를 스크립트로 옮긴 것.
// 좌표는 원본 MakeRect(left, top, right, bottom) 값을 x/y/w/h로 환산한 것 —
// 나란히 실행해 레이아웃 등가를 확인한다 (`jkdesktop jango` vs 이 앱).

var ID_BTN_PERSONNEL = 101;   // 원본 ID_BTN_INSA
var ID_LABEL_STATUS = 110;

var ID_EDIT_PASSWORD = 200;   // 원본 PasswordDialog 내부 id
var ID_BTN_OK = 201;
var ID_BTN_CANCEL = 202;

// 원본 StubPassword와 동일.
var StubPassword = "1234";

var dlg = 0;

function onCreate() {
    // 진입 버튼 + 결과 표시(원본은 성공 시 InsaDialog를 열지만 이 파일럿의
    // 범위는 다이얼로그 1개 — 상태 라벨로 대체).
    createButton({ x: 20, y: 20, w: 150, h: 30 }, "Personnel", ID_BTN_PERSONNEL);
    createLabel({ x: 20, y: 70, w: 280, h: 24 }, "", ID_LABEL_STATUS);
}

function onClick(id) {
    if (id === ID_BTN_PERSONNEL) {
        openPasswordDialog();
        return;
    }
    if (dlg === 0) return;
    if (id === ID_BTN_OK) {
        dialogClose(dlg, 1);   // JKDialog::ResultOk
    } else if (id === ID_BTN_CANCEL) {
        dialogClose(dlg, 2);   // JKDialog::ResultCancel
    }
}

function openPasswordDialog() {
    if (dlg === 0) {
        // 원본 PasswordDialog: JKDialog("Password"), MakeRect(220,170,420,300),
        // WA_TITLEMOVEABLE — createDialog가 같은 플래그를 건다.
        // 컨트롤 좌표도 원본 그대로.
        dlg = createDialog("Password", { x: 220, y: 170, w: 200, h: 130 }, onDialogClose);
        dialogAddLabel(dlg, { x: 10, y: 10, w: 180, h: 20 }, "Enter password:");
        dialogAddEdit(dlg, { x: 10, y: 45, w: 180, h: 25 }, "", ID_EDIT_PASSWORD);
        dialogAddButton(dlg, { x: 20, y: 80, w: 70, h: 30 }, "OK", ID_BTN_OK);
        dialogAddButton(dlg, { x: 110, y: 80, w: 70, h: 30 }, "Cancel", ID_BTN_CANCEL);
    }
    // 원본 OnMenuButton(ID_BTN_INSA): ClearPassword() 후 Show() — 다이얼로그는
    // 한 번 만들고 매번 재사용한다.
    setText(ID_EDIT_PASSWORD, "");
    dialogShow(dlg);
}

function onDialogClose(result) {
    // 원본 OnPasswordDone: OK + 비밀번호 일치 → 진입, 불일치 → Error 메시지 박스.
    if (result === 1) {
        var entered = getText(ID_EDIT_PASSWORD);
        if (entered === StubPassword) {
            setText(ID_LABEL_STATUS, "Access granted");
        } else {
            messageBox("Error", "Incorrect password.");
        }
    } else {
        setText(ID_LABEL_STATUS, "");
    }
}