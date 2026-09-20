// myapp.js — 워크숍 진실원 템플릿 (docs/60 §2.1)
// 이 파일이 곧 앱입니다. 메모장에서 고치거나, 에이전트에게 말로
// 고치게 하세요 (폰 채팅: "할일 판 만들어줘"). 저장하면 즉시 리로드됩니다.
//
// 런타임 진짜 복사본은 <exeDir>/state/scripts/myapp.js — 워크숍 앱 기동 시
// 이 내용으로 자동 생성된다(JKAppModule_script.cpp kTemplateScript).
// 이 소스 트리 사본은 참고용(SCRI에 실려 배포되지만 workshop 모드는 무시).

var hello = createLabel({ x: 20, y: 20, w: 220, h: 26 }, "안녕하세요!");
var helloBtn = createButton({ x: 20, y: 56, w: 140, h: 34 }, "눌러 보세요");

function onClick(id) {
    if (id === helloBtn) {
        setText(hello, "반가워요!");
    }
}