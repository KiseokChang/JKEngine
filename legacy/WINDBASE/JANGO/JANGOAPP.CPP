#include "joglobal.h"
#include "okdialog.h"
#include "popupstc.h"
#include "resbttn.h"
#include "fltbttn.h"
#include "passdlg.h"
#include "joappabt.h"
#include "budaedlg.h"
#include "insarc.h"
#include "equipwin.h"
#include "eqp24win.h"
//#include "add24dlg.h"
//#include "addeqwin.h"
#include "eqseldlg.h"
#include "insawin.h"
#include "adddlg.h"
#include "jangoapp.h"

#define JOAppMessageCount 6

static char *ButtonMessage[JOAppMessageCount]={
	"·¡ ¤áËe·i ’Ÿa¡e\n  <·¥¬a>µA ‰ÅÐe ¸b´ó·i Ðs“¡”a.\n\n"
	"    - Ñe ¦”¶¥· ¶w ®¸÷\n"
	"    - Ñe ¦”¶¥· ˆñ¬‚\n"
	"    - Ñe ¦”¶¥· ¬b¹A\n"
	"    - ¯¥‹A ·¥¶¥· —w¢\n"
	"    - ÑeÑ× Ìa´b\n"
	"      <¬a‰¡¸a ÑeÑ×>       <º Ëb ‹¡ ÑeÑ×>\n"
	"      <º­¡»¡ ÑeÑ×>       <Ðb    b ÑeÑ×>\n"
	"      <¹·Ša ÑeÑ×>         <Ñi ´‚ Ñw ÑeÑ×>\n"
	"      <‰‹s ÑeÑ×>         <»¢    À‚ ÑeÑ×>\n"
	"      <Ëb‹¡ ÑeÑ×>         <¸a ‰b »w ÑeÑ×>\n"
	"      <‰iÑ¥‰Å‰ ÑeÑ×>     <»¡Òá‰Å¯± ÑeÑ×>\n"
	"      <¶©¥i ¬—·©¸a ÑeÑ×>  <¸åµbµ¸÷ ÑeÑ×>\n"
	"    - ¸åµb¸a µiœq\n"
	"—w·i Ði ® ·¶¯s“¡”a.",
	"·¥¬a •A·¡Èa ÑÁ·©·i ‰ÅŸ¡Ðs“¡”a.",
	"\n\n·¡ ¤áËe·i ’Ÿa¡e\n  <¢‰¸a>µA ‰ÅÐe ¸b´ó·i Ðs“¡”a.\n",
	"\n\n·¡ ¤áËe·i ’Ÿa¡e\n  <Èe´¢>µA ‰ÅÐe ¸b´ó·i Ðs“¡”a.\n",
 // "\n\n·¡ ¤áËe·i ’Ÿa¡e\n  <µ¤w¸÷§¡>µA ‰ÅÐe ¸b´ó·i Ðs“¡”a.\n",
	"",
	"\n\n·¡ ¤áËe·i ’Ÿa¡e\n  ¸b´ó·i {““¡”a.\n"
};

JangoApplication::JangoApplication():
	JKApplication()
{
	BackColor=LtBlue;
}

JangoApplication::~JangoApplication()
{
}

void JangoApplication::InitResourceMan()
{
	JKApplication::InitResourceMan();
	if(ResMan)	{
		uint16 i;
		for(i=0;i<JOAppMessageCount;i++)
			ResMan->AddResource(new ResourceBase(i+1,RES_STRING,(BYTE *)ButtonMessage[i]));
		for(i=0;i<InsaIconCount;i++)
			ResMan->AddResource(new ResourceBase(InsaIconID[i],RES_IMAGE,(BYTE *)"insaimg.dat",RT_FILE|RT_PACKFILE,i));
	}
	::InitGlobal();
}

void JangoApplication::SetupWindow()
{
	JKPoint p;
	uint16 height=105;
	int16 ButtonColor=LtGray;

	JKButton *bttn;
	JKRect rect(50,50,300,120);
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("·¥    ¬a");
	bttn->BackColor=ButtonColor;
	bttn->Depth=2;
	AddControl(bttn);

	rect.top+=height;
	rect.bottom+=height;
	bttn=new JKButton(rect,ID_BUTTON+2);
	bttn->SetText("2.4¹· ‰ÅŸ¡");
	bttn->BackColor=ButtonColor;
	bttn->Depth=2;
	AddControl(bttn);

	rect.top+=height;
	rect.bottom+=height;
	bttn=new JKButton(rect,ID_BUTTON+3);
	bttn->SetText("Èe    ´¢");
	bttn->BackColor=ButtonColor;
	bttn->Depth=2;
	AddControl(bttn);

	rect.top+=height;
	rect.bottom+=height;
	bttn=new JKButton(rect,ID_BUTTON+4);
  bttn->SetText("°w¶wÏa¡‹aœ‘ ÑÁ·© ‰ÅŸ¡‹¡");
	bttn->BackColor=ButtonColor;
	bttn->Depth=2;
	AddControl(bttn);

	rect.top+=height;
	rect.bottom+=height;
	bttn=new JKButton(rect,ID_BUTTON+5);
	bttn->SetText("¦” ¬å¸÷");
	bttn->BackColor=ButtonColor;
	bttn->TextColor=LtBlue;
	bttn->Depth=2;
	AddControl(bttn);

	rect.top+=height;
	rect.bottom+=height;
	bttn=new JKButton(rect,ID_BUTTON+6);
	bttn->SetText("{  ‹¡");
	bttn->BackColor=ButtonColor;
	bttn->Depth=2;
	AddControl(bttn);


	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=360;
	rect.right-=50;
	rect.top=50;
	rect.bottom=649;
	JOAppAboutControl *aboutwin = new JOAppAboutControl(rect,ID_USERCONTROL+1);
	AddControl(aboutwin);


	CtrlResMan.Add(new CtrlResBase(ID_BUTTON+1,0x0001,JKRect()));
	CtrlResMan.Add(new CtrlResBase(ID_BUTTON+2,0x0002,JKRect()));
	CtrlResMan.Add(new CtrlResBase(ID_BUTTON+2,0x0003,JKRect()));
	CtrlResMan.Add(new CtrlResBase(ID_BUTTON+3,0x0004,JKRect()));
	CtrlResMan.Add(new CtrlResBase(ID_BUTTON+5,0x0005,JKRect()));
	CtrlResMan.Add(new CtrlResBase(ID_BUTTON+5,0x0006,JKRect()));
	CtrlResMan.Add(new CtrlResBase(ID_BUTTON+6,0x0007,JKRect()));
}

BOOL JangoApplication::CanClose()
{
	if(JKApplication::CanClose()){
		::WriteGlobal();
		return TRUE;
	}
	return FALSE;
}

void JangoApplication::PaintClient(JKDC& dc,JKRect&)
{
	char CopyRightMessage[]=
		"Copyright   ”¶á  ‹±Ñw­A(89-00186)   ¬w¥w  ¸w‹¡¬â(94-71022279)   ·©¥w  µ¡µq¢…(95-71001873)";
	JKRect rect(0,getmaxy()-30,getmaxx(),getmaxy());
	dc.Box3D(rect,-1,LtGray);
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(Gray);
	dc.TextOutX(rect.OffsetBy(JKPoint(2,2)),CopyRightMessage,ADJ_XYCENTER);
	dc.SetTextColor(White);
	dc.TextOutX(rect,CopyRightMessage,ADJ_XYCENTER);

}

ProcessResult JangoApplication::RespondMessage(JKMSG& msg)
{
	if(!msg.winid){
		JOAppAboutControl *ctrl = (JOAppAboutControl *)GetControl(ID_USERCONTROL+1);
		if(ctrl){
			if(msg.main==ONLYMOVE||msg.main==ELONLYMOVE ||
					msg.main==CONTROL_LEFTCLICK || msg.main==CONTROL_RIGHTCLICK)
				ctrl->SetResourceID(0);
		}
	}
	return JKApplication::RespondMessage(msg);
}

void JangoApplication::EvShowWindow()
{
	JKApplication::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void JangoApplication::EvButtonClick(WORD ctrlid)
{
	JKControl *ctrl;
	switch(ctrlid){
		case ID_BUTTON+1:
			if(PasswordDialog(JKRect(180,150,459,329),0,"´qÑ¡Ÿi ·³bÐa¯³¯¡µ¡").Run()==ID_OK)
				SendCommand(START_INSA);
			break;
		case ID_BUTTON+2:
			SendCommand(START_EQUIP24);
			break;
		case ID_BUTTON+3:
			SendCommand(START_EQUIP);
			break;
		case ID_BUTTON+4:
      {
			FileSellectDialog *addfile=new FileSellectDialog(JKRect(200,200,530,380));
			if(addfile){
				addfile->Run();
				delete addfile;
			}
			}
			break;
		case ID_BUTTON+5:
			if(ChangeBudaeDialog(JKRect(220,170,420,300)).Run()==ID_OK){
				char Msg[50];
				sprintf(Msg,"¸b´ó ¦”¡w·¡\n%3s””¡\n¥e‰w –A´ö¯s“¡”a",BudaeName);
				OKDialog okdlg(JKRect(220,170,420,300),Msg);
				okdlg.WinForm=0x01;
				okdlg.BackColor=LtBlue;
				okdlg.Run();
			}
			break;
		case ID_BUTTON+6:
			CloseWindow();
			break;
		default:
			break;
	}
}

/*
void JangoApplication::EvCommand(WORD cmdid,WORD,DWORD,WORD)
{
	JKControl *ctrl;
	switch(cmdid){
		case START_INSA:
			MouseEvents->SetCursorID(5);
			ctrl = new InsaWindow();
			AddControl(ctrl);
			break;
		case START_EQUIP:
			MouseEvents->SetCursorID(5);
			ctrl = new EquipWindow(0);
			AddControl(ctrl);
			break;
		case START_EQUIP24:
			MouseEvents->SetCursorID(5);
			ctrl = new Equip24Window();
			AddControl(ctrl);
			break;
		case START_MOTOR:
			MouseEvents->SetCursorID(5);
			ctrl = new MotorWindow();
			AddControl(ctrl);
			break;
		default:
			break;
	}
}
*/

void JangoApplication::EvCommand(WORD cmdid,WORD,DWORD,WORD)
{
	JKDialog *dlg;
	switch(cmdid){
		case START_INSA:
			MouseEvents->SetCursorID(5);
			dlg = new InsaWindow();
			if(dlg){
				dlg->Run();
				delete dlg;
			}
			break;
		case START_EQUIP:
			MouseEvents->SetCursorID(5);
			dlg = new EquipWindow(0);
			if(dlg){
				dlg->Run();
				delete dlg;
			}
			break;
		case START_EQUIP24:
			MouseEvents->SetCursorID(5);
			dlg = new Equip24Window(0);
			if(dlg){
				dlg->Run();
				delete dlg;
			}
			break;
		default:
			break;
	}
}

void JangoApplication::EvCtrlOnlyMove(WORD ctrlid)
{
	CtrlResBase *ctrlres = CtrlResMan.Get(ctrlid);
	if(ctrlres){
		JOAppAboutControl *ctrl = (JOAppAboutControl *)GetControl(ID_USERCONTROL+1);
		if(ctrl) ctrl->SetResourceID(ctrlres->ResourceID);
	}
	else {
		JOAppAboutControl *ctrl = (JOAppAboutControl *)GetControl(ID_USERCONTROL+1);
		if(ctrl) ctrl->SetResourceID(0);
	}
}

void JangoApplication::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+6;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_BUTTON+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else ctrlid=ID_BUTTON+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_BUTTON+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_BUTTON+1)	ctrlid=LastCtrlID;
			else ctrlid=LastCtrlID;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
