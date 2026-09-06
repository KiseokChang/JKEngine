// dialog.js — v3 modal dialog bindings (docs/27 단계 3). The test-script
// runner has no app host, so the dialog is not actually modal here — what is
// pinned headless is the lifecycle (create/add/show/close/reopen), the result
// codes reaching onClose, and v1/v2 bindings working on dialog controls.
var dlg = 0;
var okId = 0, cancelId = 0, editId = 0;
var closedResult = 0;
var closeCount = 0;

function onCreate() {
    dlg = createDialog("T", { x: 0, y: 0, w: 100, h: 80 }, onClosed);
    assert(dlg > 0, "createDialog returns a dialog id");

    dialogAddLabel(dlg, { x: 2, y: 2, w: 80, h: 14 }, "L");
    editId = dialogAddEdit(dlg, { x: 2, y: 20, w: 80, h: 14 }, "");
    okId = dialogAddButton(dlg, { x: 2, y: 40, w: 30, h: 16 }, "OK");
    cancelId = dialogAddButton(dlg, { x: 40, y: 40, w: 30, h: 16 }, "Cancel");
    assert(okId > 0 && cancelId > 0 && editId > 0, "dialogAdd* return control ids");

    // Dialog controls join the shared registry — v1/v2 bindings work on them.
    assert(findControl(editId) !== null, "dialog edit discoverable by id");
    assert(findControl("OK") === okId, "dialog button discoverable by text");
    setText(editId, "pw");
    assertEq(getText(editId), "pw", "dialog edit text roundtrip");

    dialogShow(dlg);
    click(okId);
    assertEq(closedResult, 1, "OK button closes with ResultOk");

    // Reuse: reopen the same dialog, close through Cancel (JangoUI pattern).
    dialogShow(dlg);
    click(cancelId);
    assertEq(closedResult, 2, "Cancel button closes with ResultCancel");

    // Code-path close passes an arbitrary result through.
    dialogShow(dlg);
    dialogClose(dlg, 4);
    assertEq(closedResult, 4, "dialogClose passes the result through");
    assertEq(closeCount, 3, "onClose fired exactly once per close");
}

function onClosed(result) {
    closedResult = result;
    ++closeCount;
}

// The ported pattern (PasswordDialog): dialog buttons route through the
// global onClick and the script maps controlId -> result -> dialogClose.
function onClick(id) {
    if (dlg === 0) return;
    if (id === okId) {
        dialogClose(dlg, 1);        // JKDialog::ResultOk
    } else if (id === cancelId) {
        dialogClose(dlg, 2);        // JKDialog::ResultCancel
    }
}