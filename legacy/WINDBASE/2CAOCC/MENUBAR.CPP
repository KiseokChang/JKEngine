#include "jkbutton.h"
#include "moonctrl.h"
#include "occmsg.h"
#include "menubar.h"

MenuBar::MenuBar(JKRect rect,JKControl *parent):
	JKControl(rect,0,parent)
{
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW;
}

MenuBar::~MenuBar()
{
}

JKRect MenuBar::CalcClientRect()
{
	JKRect rect =WinRect.SetLeftTopBy();
	rect.Expand(-5);
	return rect;
}

void MenuBar::SetupWindow()
{
	JKButton *bttn;
	JKRect rect=ClientRect.SetLeftTopBy();
	rect.left+=5;
	rect.right-=5;
	rect.top=100+15;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("H¯¡ˆe¬é¸÷");
	AddControl(bttn);

	rect.top=rect.bottom+31;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+2);
	bttn->SetText("¸ñ Îa ¸â");
	AddControl(bttn);
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+3);
	bttn->SetText("”ÑwÎa¸â");
	AddControl(bttn);

	rect.top=rect.bottom+16;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+4);
	bttn->SetText("‰·µbÉ·¹A");
	AddControl(bttn);

	rect.top=rect.bottom+16;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+5);
	bttn->SetText("¬a‰bÑeÑ×");
	AddControl(bttn);

	rect.top=rect.bottom+16;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+6);
	bttn->SetText("´aŠ…Í¡¥w");
	AddControl(bttn);

	rect.top=rect.bottom+16;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+7);
	bttn->SetText("‹¡Èa¦”");
	AddControl(bttn);

	rect.top=rect.bottom+16;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+8);
	bttn->SetText("Îa    ¸â");
	AddControl(bttn);

	rect.top=rect.bottom+16;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+9);
	bttn->SetText("Îa¸âÀáŸ¡");
	AddControl(bttn);


	rect.top=rect.bottom+16;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+10);
	bttn->SetText("Š‰w¹A¶¥");
	AddControl(bttn);

	rect.top=rect.bottom+21;
	rect.bottom=rect.top+25;
	bttn= new JKButton(rect,ID_BUTTON+11);
	bttn->SetText("¹·    ža");
	AddControl(bttn);

	rect=ClientRect.SetLeftTopBy();
	rect.top=rect.bottom-34;
	bttn= new MoonControl(rect,ID_BUTTON+12);
	AddControl(bttn);
}

void MenuBar::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect =WinRect.SetLeftTopBy();
	dc.Box3D(rect,1);
	rect.Expand(-4);
	dc.Rectangle3D(rect,-1);
}

void MenuBar::PaintClient(JKDC& dc,JKRect&)
{
	dc.PutIMG("log100.img",JKPoint(-3,5));

	JKRect rect =ClientRect.SetLeftTopBy();
	rect.left+=5;
	rect.right-=5;

	rect.top=100+15;
	rect.bottom=rect.top+26*1;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+10;
	rect.bottom=rect.top+20+26*2;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+15;
	rect.bottom=rect.top+26;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+15;
	rect.bottom=rect.top+26;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+15;
	rect.bottom=rect.top+26;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+15;
	rect.bottom=rect.top+26;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+15;
	rect.bottom=rect.top+26;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+15;
	rect.bottom=rect.top+26;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);
	rect.top=rect.bottom+15;
	rect.bottom=rect.top+26;
	dc.Rectangle3D(rect.ExpandBy(2),-2,Black,Black);

	dc.SetTextColor(LtBlue);
	dc.SetBackColor(BackColor);
	rect.top=100+15+26*1+10;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"·±· ¬a‰b",ADJ_XYCENTER);

	rect=ClientRect.SetLeftTopBy();
	rect.bottom-=35;
	rect.top=rect.bottom-20;
	dc.TextOutX(rect,"< ¶©  ‰× >",ADJ_XYCENTER);
}

void MenuBar::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			SendControlCommand(OCC_COMMAND+1);
			break;
		case ID_BUTTON+2:
			SendControlCommand(OCC_COMMAND+2);
			break;
		case ID_BUTTON+3:
			SendControlCommand(OCC_COMMAND+3);
			break;
		case ID_BUTTON+4:
			SendControlCommand(OCC_COMMAND+4);
			break;
		case ID_BUTTON+5:
			SendControlCommand(OCC_COMMAND+5);
			break;
		case ID_BUTTON+6:
			SendControlCommand(OCC_COMMAND+6);
			break;
		case ID_BUTTON+7:
			SendControlCommand(OCC_COMMAND+7);
			break;
		case ID_BUTTON+8:
			SendControlCommand(OCC_COMMAND+8);
			break;
		case ID_BUTTON+9:
			SendControlCommand(OCC_COMMAND+9);
			break;
		case ID_BUTTON+10:
			SendControlCommand(OCC_COMMAND+10);
			break;
		case ID_BUTTON+11:
			SendControlMessage(CLOSEWINDOW);
			break;
	}
}

void MenuBar::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+11;
	if(key==PGDNKEY||key==DOWNARROW){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_BUTTON+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else return;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY||key==UPARROW){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_BUTTON+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_BUTTON+1)	ctrlid=LastCtrlID;
			else return;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}


