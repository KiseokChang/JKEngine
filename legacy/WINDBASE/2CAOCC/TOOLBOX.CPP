#include "jkbutton.h"
#include "occmsg.h"
#include "toolbox.h"

ToolBox::ToolBox(JKRect& rect,WORD ctlid,JKControl *parent):
	JKWindow(rect,parent,ctlid)
{
  WinForm=0x03;
	WinAttr|=WA_NORESTRICT|WA_TITLEMOVEABLE|WA_CHANGEPLACE;
	SetText("‹¡“w ¬w¸a");
}

ToolBox::~ToolBox()
{}


void ToolBox::SetupWindow()
{
	JKButton *bttn;
	JKRect rect=ClientRect.SetLeftTopBy();
	rect.left+=5;
	rect.right-=5;

	rect.top=5;
	rect.bottom=rect.top+20;
	bttn= new JKButton(rect,ID_BUTTON+1);
  bttn->SetText("¬å  ¬‚¬w");
	AddControl(bttn);
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+20;
	bttn= new JKButton(rect,ID_BUTTON+2);
  bttn->SetText("¬å  ŠŠ‹¡");
	AddControl(bttn);
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+20;
	bttn= new JKButton(rect,ID_BUTTON+3);
  bttn->SetText("¢…¸a­¢¬÷");
  AddControl(bttn);
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+20;
	bttn= new JKButton(rect,ID_BUTTON+4);
	AddControl(bttn);
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+20;
	bttn= new JKButton(rect,ID_BUTTON+5);
	AddControl(bttn);
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+20;
	bttn= new JKButton(rect,ID_BUTTON+6);
  bttn->SetText("·ª´áµ¡‹¡");
	AddControl(bttn);
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+20;
	bttn= new JKButton(rect,ID_BUTTON+7);
  bttn->SetText("¸á¸wÐa‹¡");
	AddControl(bttn);
}

void ToolBox::EvButtonClick(WORD ctrlid)
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
	}
}

void ToolBox::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+7;
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


