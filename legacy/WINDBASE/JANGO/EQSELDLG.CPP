#include "eqseldlg.h"
#include "add24dlg.h"
#include "addeqwin.h"
#include "adddlg.h"
#include "jkbutton.h"
#include "jkedit.h"


FileSellectDialog::FileSellectDialog(JKRect rect):
	JKDialog(rect)
{
	WinForm=0x03;
	BackColor=Black;
	TextColor=White;
  SetText("°w¶w Ïa¡‹aœ‘ ÑÁ·© ‰ÅŸ¡‹¡");
}

FileSellectDialog::~FileSellectDialog()
{}

void FileSellectDialog::SetupWindow()
{
//	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

	JKButton *btn=new JKButton(JKRect(10,10,200,60),ID_BUTTON+1);
	btn->SetText("·¥¬a ÑÁ·© ‰ÅŸ¡‹¡");
	AddControl(btn);

	btn=new JKButton(JKRect(10,63,200,103),ID_BUTTON+2);
	btn->SetText("Èe´¢ ÑÁ·© ‰ÅŸ¡‹¡");
	AddControl(btn);

	btn=new JKButton(JKRect(10,106,200,146),ID_BUTTON+3);
	btn->SetText("2.4 ¹· ÑÁ·© ‰ÅŸ¡‹¡");
	AddControl(btn);

	btn=new JKButton(JKRect(203,10,310,146),ID_BUTTON+4);
	btn->SetText("Âá  ­¡");
	AddControl(btn);
}

void FileSellectDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			AddFileDialog().Run();
			break;

		case ID_BUTTON+2:
			{
			AddEquip24Dialog *eqp24dlg=new AddEquip24Dialog(0);
			if(eqp24dlg){
				eqp24dlg->Run();
				delete eqp24dlg;
			}
			}
			break;

		case ID_BUTTON+3:
      {
			AddEquipWindow *eqpdlg=new AddEquipWindow(0);
			if(eqpdlg){
				eqpdlg->Run();
				delete eqpdlg;
			}
			}
			break;

		case ID_BUTTON+4:
			SendMessage(ID_OK);
			break;
	}
}

void FileSellectDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+4;
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

void FileSellectDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

