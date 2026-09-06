#include "joclock.h"
#include "sollucvt.h"
#include "statbar.h"

StatusBar::StatusBar(JKRect rect,WORD ctrlid,JKControl *parent):
	JKControl(rect,ctrlid,parent)
{
	BackColor=LtGray;
	TextColor=LtBlue;
	WinAttr|=WA_FIRSTSHOW|WA_NOINCLIENT;
}

StatusBar::~StatusBar()
{
}

JKRect StatusBar::CalcClientRect()
{
	JKRect rect =WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-5);
	rect.right-=370;
	return rect;
}

void StatusBar::InitWindow()
{
	JKControl::InitWindow();
	ChangeDate();
}

void StatusBar::SetupWindow()
{
	JKStatic *stc;
	JKRect rect=ClientRect.SetLeftTopBy();
	rect.top=0;
	rect.bottom=19;
	rect.left=rect.right+20;
	rect.right=rect.left+8*10;
	stc=new JKStatic(rect,ID_STATIC+1);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=LtGray;
	stc->TextColor=LtBlue;
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	stc->SetText("    .  .  ");
	AddControl(stc);

	rect.left=rect.right+20;
	rect.right=rect.left+8*16;
	stc=new JKStatic(rect,ID_STATIC+2);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=LtGray;
	stc->TextColor=LtBlue;
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	stc->SetText("(·q)     .  .  ");
	AddControl(stc);

	rect.left=rect.right+20;
	rect.right=rect.left+8*8;
	stc=new JOClock(rect,0);
	stc->WinAttr|=WA_NOINCLIENT;
	stc->BackColor=LtGray;
	stc->TextColor=Yellow;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);
}

void StatusBar::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect =WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.Box3D(rect,1);
	rect.Expand(-5);
	JKRect temp;
	temp=rect;
	temp.right-=370;
	dc.Rectangle3D(temp.ExpandBy(1),-1);
	temp.left=temp.right+5;
	temp.right=rect.right;
	dc.Rectangle3D(temp.ExpandBy(1),-1);
}

void StatusBar::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect =ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(Black);
	dc.TextOutX(rect.OffsetBy(JKPoint(2,2)),WinText,ADJ_YCENTER);
	dc.SetTextColor(TextColor);
	dc.TextOutX(rect,WinText,ADJ_YCENTER);
}


void StatusBar::ChangeDate()
{
	struct date Solar,Lunar;
	char buffer[15];
	JKControl *ctrl=GetControl(ID_STATIC+1);
	if(ctrl){
		getdate(&Solar);
		sprintf(buffer,"%4d.%2d.%2d",Solar.da_year,Solar.da_mon,Solar.da_day);
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}

	ctrl=GetControl(ID_STATIC+2);
	if(ctrl){
		getdate(&Solar);
		SolLuCvt().SolToLuna(Solar,Lunar);
		sprintf(buffer,"(·q) %4d.%2d.%2d",Lunar.da_year,Lunar.da_mon,Lunar.da_day);
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}

ProcessResult StatusBar::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case CLOCK_CHANGEDATE:
			ChangeDate();
			break;
		default:
			break;
	}
	return JKControl::RespondMessage(msg);
}
