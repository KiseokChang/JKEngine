#include "eqpstat.h"
#include "jkbutton.h"

EqpStat::EqpStat(WORD ctrlid,JKControl *parent):
	JKWindow(JKRect(150,70,450,300),parent,ctrlid)
{
	WinForm=0x03;
	SetText("Ñe¸ ¬wÈ ¥¡‹¡");
	WinAttr&=~WA_FIRSTSHOW;
	BackColor=LtGray;
}
EqpStat::~EqpStat()
{}

void EqpStat::SetupWindow()
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

void EqpStat::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.SetBackColor(BackColor);
	rect.left+=40;
	rect.right=rect.left+60;
	rect.top+=20;
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
	dc.TextOutX(rect,"¬a¶w¶w•¡ : ",ADJ_YCENTER|ADJ_RIGHT);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=110;
	rect.right=rect.left+140;
	rect.top+=20;
	rect.bottom=rect.top+20;

	if(*Bomb.Kind==0) dc.TextOutX(rect,"¸åÁA",ADJ_YCENTER|ADJ_LEFT);
	else dc.TextOutX(rect,Bomb.Kind,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	if(*Bomb.Dod==0) dc.TextOutX(rect,"¸åÁA",ADJ_YCENTER|ADJ_LEFT);
	else dc.TextOutX(rect,Bomb.Dod,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	if(*Bomb.Lot==0) dc.TextOutX(rect,"¸åÁA",ADJ_YCENTER|ADJ_LEFT);
	else dc.TextOutX(rect,Bomb.Lot,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	if(*Bomb.Born==0) dc.TextOutX(rect,"¸åÁA",ADJ_YCENTER|ADJ_LEFT);
	else dc.TextOutX(rect,Bomb.Born,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	if(*Bomb.Place==0) dc.TextOutX(rect,"¸åÁA",ADJ_YCENTER|ADJ_LEFT);
	else dc.TextOutX(rect,Bomb.Place,ADJ_YCENTER|ADJ_LEFT);

	rect.top=rect.top+20;
	rect.bottom=rect.top+20;
	if(*Bomb.Use==0) dc.TextOutX(rect,"¸åÁA",ADJ_YCENTER|ADJ_LEFT);
	else dc.TextOutX(rect,Bomb.Use,ADJ_YCENTER|ADJ_LEFT);

}

void EqpStat::EvButtonClick(WORD ctrlid)
{
	switch (ctrlid) {
		case ID_BUTTON:
			HideWindow();
			break;
	}
}

void EqpStat::SetBomb(BOMB& bm)
{
	Bomb=bm;
}

