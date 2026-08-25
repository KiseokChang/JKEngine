#include "joglobal.h"
#include "joclock.h"
#include "jkbutton.h"
#include "maintitl.h"
#include "sollucvt.h"

MainTitle::MainTitle(JKRect& rect,WORD ctlid,JKControl *parent):
	JKControl(rect,ctlid,parent)
{
	BackColor=LtBlue;
}

JKRect MainTitle::CalcClientRect()
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-3);
	return rect;
}

void MainTitle::InitWindow()
{
	JKControl::InitWindow();
	ChangeDate();
}

void MainTitle::SetupWindow()
{
	JKRect rect;
	JKStatic *stc;

	rect = ClientRect.SetLeftTopBy(JKPoint(0,0));

	int16 middle=rect.Height()/2;

	rect.left+=5;
	rect.right=rect.left+8*5;
	stc=new JKStatic(rect,ID_STATIC+1);
	stc->BackColor=BackColor;
	stc->TextColor=LtRed;
	stc->AdjustFlag=ADJ_YCENTER;
	stc->SetText("·¥ ¬a");
	AddControl(stc);

	rect.left=rect.right+13;
	rect.right=rect.left+8*10;
	stc=new JKStatic(rect,ID_STATIC+2);
	stc->BackColor=BackColor;
	stc->TextColor=Black;
	stc->AdjustFlag=ADJ_YCENTER;
	BYTE buffer[15]="¹A    ””";
	memcpy(buffer+6-strlen(BudaeName),BudaeName,strlen(BudaeName));
	stc->SetText(buffer);
	AddControl(stc);

	rect.top+=2;
	rect.left=153;
	rect.right=rect.left+8*16;
	rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+3);
	stc->BackColor=BackColor;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_YCENTER;
	stc->SetText("Á·  ¶¥:    /    ");
	AddControl(stc);

	rect.left=rect.right+10;
	rect.right=rect.left+8*16;
	stc=new JKStatic(rect,ID_STATIC+4);
	stc->BackColor=BackColor;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_YCENTER;
	stc->SetText("¬a  ‰¡:    /    ");
	AddControl(stc);

	rect.left=450;
	rect.right=rect.left+8*10;
	stc=new JKStatic(rect,ID_STATIC+5);
	stc->BackColor=BackColor;
	stc->TextColor=LtGreen;
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	stc->SetText("    .  .  ");
	AddControl(stc);

	rect.left=rect.right+20;
	rect.right=rect.left+8*8;
	stc=new JOClock(rect,0);
	stc->BackColor=BackColor;
	stc->TextColor=Yellow;
	stc->AdjustFlag=ADJ_XYCENTER;
	AddControl(stc);

	rect.left=153;
	rect.right=rect.left+8*16;
	rect.top=middle+4;
	rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+6);
	stc->BackColor=BackColor;
	stc->TextColor=LtGreen;
	stc->AdjustFlag=ADJ_YCENTER;
	stc->SetText("Ñe¸¶¥:    /    ");
	AddControl(stc);

	rect.left=rect.right+1+20;
	rect.right=400;
	JKButton *bttn=new JKButton(rect,ID_STATIC+7);
	bttn->SetEnable(FALSE);
	bttn->Depth=-1;
	bttn->BackColor=LtGray;
	bttn->TextColor=LtRed;
	bttn->AdjustFlag=ADJ_XYCENTER;
	bttn->SetText("Ñeµb¸a");
	AddControl(bttn);

	rect.left=450;
	rect.right=rect.left+8*15;
	stc=new JKStatic(rect,ID_STATIC+8);
	stc->BackColor=BackColor;
	stc->TextColor=LtGreen;
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	stc->SetText("(·q)     .  .  ");
	AddControl(stc);

}

void MainTitle::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
	dc.Rectangle3D(rect,1);
	rect.Expand(-2);
	dc.Rectangle3D(rect,-1);
}

void MainTitle::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	int16 middle=rect.Height()/2;

	//dc.Rectangle3D(JKRect(0,0,150,53),1);
	dc.VLine(JKPoint(145,0),54,Gray);
	dc.VLine(JKPoint(146,0),54,White);

	dc.HLine(JKPoint(147,middle),rect.Width()-146,Gray);
	dc.HLine(JKPoint(147,middle+1),rect.Width()-146,White);

	dc.VLine(JKPoint(425,0),54,Gray);
	dc.VLine(JKPoint(426,0),54,White);
}

ProcessResult MainTitle::RespondMessage(JKMSG& msg)
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

void MainTitle::ChangeWholeAmount(uint16 ganbu,uint16 sabyeong)
{
	char buffer[15];
	sprintf(buffer,"Á·  ¶¥:%4d/%4d",ganbu,sabyeong);
	JKControl *ctrl=GetControl(ID_STATIC+3);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}

void MainTitle::ChangeSagoAmount(uint16 ganbu,uint16 sabyeong)
{
	char buffer[15];
	sprintf(buffer,"¬a  ‰¡:%4d/%4d",ganbu,sabyeong);
	JKControl *ctrl=GetControl(ID_STATIC+4);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}

void MainTitle::ChangeCurrentAmount(uint16 ganbu,uint16 sabyeong)
{
	char buffer[15];
	sprintf(buffer,"Ñe¸¶¥:%4d/%4d",ganbu,sabyeong);
	JKControl *ctrl=GetControl(ID_STATIC+6);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}

void MainTitle::ChangeManagerMode(BOOL mode)
{
	char buffer[7];
	strcpy(buffer,mode? "Ñeµb¸a":"¸åµb¸a");
	JKControl *ctrl=GetControl(ID_STATIC+7);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}                                                                                                                                                                                                                                                             

void MainTitle::ChangeDate()
{
	struct date Solar,Lunar;
	char buffer[15];
	getdate(&Solar);
	sprintf(buffer,"%4d.%2d.%2d",Solar.da_year,Solar.da_mon,Solar.da_day);
	JKControl *ctrl=GetControl(ID_STATIC+5);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}

	SolLuCvt da;
	da.SolToLuna(Solar,Lunar);
	sprintf(buffer,"(·q) %4d.%2d.%2d",Lunar.da_year,Lunar.da_mon,Lunar.da_day);
	ctrl=GetControl(ID_STATIC+8);
	if(ctrl){
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}
