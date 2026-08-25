#include "eqpview.h"
#include "jkbutton.h"
#include "extnval.h"

EqpView::EqpView(JKControl *parent):
	JKDialog(JKRect(150,70,450,390),parent)
{
	WinForm=0x03;
	SetText("¸a­AÓ¡ ¥¡‹¡ Àw");
}
EqpView::~EqpView()
{}

void EqpView::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=ClientRect.Width()/2-50;
	rect.top=rect.bottom-50;
	rect.right=rect.left+100;
	rect.bottom=rect.top+30;
	JKButton *btn=new JKButton(rect,ID_BUTTON);
	btn->SetText("ÑÂ ·¥");
	AddControl(btn);
}

void EqpView::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=40;
	rect.right=rect.left+60;
	rect.top+=20;
	rect.bottom=rect.top+20;
	dc.BackColor=LtGray;
	dc.TextOutX(rect,"­¡  ­¢ : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"Èe  ¹· : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"D.O.D : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"LOT-NO : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¹A¸b‘e•¡ : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¥¡‰Å¸w­¡ : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¤i  ® : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¬a¶w¶w•¡ : ",ADJ_YCENTER|ADJ_RIGHT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"®w·© : ",ADJ_YCENTER|ADJ_RIGHT);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=110;
	rect.right=rect.left+140;
	rect.top+=20;
	rect.bottom=rect.top+20;

	dc.TextOutX(rect,Bomb.Name,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.Kind,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.Dod,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.Lot,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.Born,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.Place,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.Count,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.Use,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,Bomb.HaveYear,ADJ_YCENTER|ADJ_LEFT);
}

void EqpView::EvButtonClick(WORD ctrlid)
{
	switch (ctrlid) {
		case ID_BUTTON:
			CloseWindow();
			break;
	}
}

void EqpView::SetBomb(int16 index)
{
	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	if(fm) {
		fm->Read(Bomb,index);
		delete fm;
	}
}

