// scriptdemo — docs/27 단계 1의 첫 스크립트 앱 (계약: engine/scripts/jk.d.ts).
// 라벨 시계(setInterval) + 클릭 카운터 버튼 + edit 에코. 단일/서버 모드
// 공통으로 .jkx 하나만으로 구동된다(공용 jkapp_script.dll 모듈).

var clickCount = 0;

var clockLabel = createLabel({ x: 20, y: 20, w: 140, h: 26 }, "--:--:--");
var clickBtn = createButton({ x: 20, y: 60, w: 150, h: 38 }, "Clicked: 0");
var edit = createEdit({ x: 20, y: 112, w: 200, h: 30 }, "type here");
var echoLabel = createLabel({ x: 20, y: 152, w: 260, h: 26 }, "");

function pad2(n) {
    return n < 10 ? "0" + n : "" + n;
}

function onCreate() {
    log("scriptdemo onCreate: clickBtn id=" + clickBtn + ", edit id=" + edit);
    setInterval(1000, function () {
        var d = new Date();
        setText(clockLabel,
            pad2(d.getHours()) + ":" + pad2(d.getMinutes()) + ":" + pad2(d.getSeconds()));
    });
}

function onClick(id) {
    if (id === clickBtn) {
        clickCount++;
        setText(clickBtn, "Clicked: " + clickCount);
        setText(echoLabel, "edit: " + getText(edit));
    }
}

function onExit() {
    log("scriptdemo onExit");
}