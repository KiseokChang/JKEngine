#include "jkbutton.h"
#include "logodlg.h"

LogoDialog::LogoDialog(JKRect rect,JKControl *parent):
	JKDialog(rect,parent)
{
	BackColor=FindNearestColor(62,62,62);
	WinForm=0x01;
	Depth=1;
	BorderSize=5;
}

LogoDialog::~LogoDialog()
{}

void LogoDialog::InitWindow()
{
	JKDialog::InitWindow();
	SetTimer(10000);
}

void LogoDialog::PaintClient(JKDC& dc,JKRect& rect)
{
	dc.PutIMG("orglog.img",JKPoint(60,0),rect);
}

void LogoDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void LogoDialog::EvCharDown(uint16 key,WORD)
{
	if(key==ENTER) SendMessage(ID_OK);
}

void LogoDialog::EvTimerHappen(DWORD)
{
	SendMessage(ID_OK);
}

void LogoDialog::EvOnlyMove(JKPoint&)
{
	SendMessage(ID_OK);
}
void LogoDialog::EvNcOnlyMove(JKPoint&)
{
	SendMessage(ID_OK);
}

void LogoDialog::EvLeftRelease(JKPoint& p)
{
	char Message[]="»¥¼a¡  e—e ·¡—i:\n\n"
								 "259FA 3BA µA¬á Ìa‰e µ¥  ¸w‹¡¬â ¬w¥w\n\n"
								 "855FA ¥¥¦µA¬á ia µ¥  µ¡µq¢… ·©¥w\n\n"
								 "µa”e(2CA) ¸å¬e¯©µA¬á µ¥ ¢…µwŠ ·©¥w\n\n"
								 "Š…”  g·¡ ¹½´a¹vŠ e!\n"
								 "”a “a‹a—i ”â·¡µa!\n\n";

	if(JKRect(285,100,290,105).IsContain(p)){
		MsgFilter->NopaintRegion.ClearNode();
		JKDC *dc=CreateClientDC();
		if(dc){
			dc->IsOverWrite=FALSE;
			JKRect rect=ClientRect.SetLeftTopBy();
			dc->SetTextColor(Black);
			dc->TextOutX(rect.OffsetBy(JKPoint(1,1)),Message,ADJ_XYCENTER);
			dc->SetTextColor(LtBlue);
			dc->TextOutX(rect,Message,ADJ_XYCENTER);
		}
	}
}
