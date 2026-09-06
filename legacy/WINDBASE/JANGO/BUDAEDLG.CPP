#include <ctype.h>
#include "joglobal.h"
#include "jkbutton.h"
#include "jkedit.h"
#include "budaedlg.h"

ChangeBudaeDialog::ChangeBudaeDialog(JKRect rect):
	JKDialog(rect)
{
	WinForm=0x03;
	BackColor=Black;
	TextColor=White;
	SetText("¸b´ó ¦”    ¥e‰w");
}

ChangeBudaeDialog::~ChangeBudaeDialog()
{}

void ChangeBudaeDialog::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-10);

	rect.bottom=rect.top+20;

	JKStatic *stc=new JKStatic(JKRect(rect.left,rect.top,rect.left+80,rect.bottom),ID_STATIC+1);
	stc->SetText("¦ ” ¡w :");
	stc->BackColor=BackColor;
	stc->TextColor=TextColor;
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	JKEdit *edit=new JKEdit(JKRect(rect.right-70,rect.top,rect.right,rect.bottom),ID_EDIT+1,0,3,FALSE,FALSE);
	edit->SetText(BudaeName);
	AddControl(edit);

	rect.left=10;
	JKButton *bttn;
	rect.top=rect.bottom+10;
	rect.bottom=rect.top+25;
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("¥e     ‰w");
	AddControl(bttn);
	rect.top=rect.bottom+10;
	rect.bottom=rect.top+25;
	bttn=new JKButton(rect,ID_BUTTON+2);
	bttn->SetText("Âá     ­¡");
	AddControl(bttn);
}

void ChangeBudaeDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		JKEdit *edit=(JKEdit *)GetControl(ID_EDIT+1);
		if(edit){
			char buffer[4];
			edit->GetText(buffer);
			::DeleteWhiteSpace(buffer);
			uint16 len=strlen(buffer);
			if(len&&len<4&&!ispunct(*buffer)&&strcmp(BudaeName,buffer)){
				strcpy(BudaeName,buffer);
				SendMessage(ID_OK);
			}
			else edit->SetText(BudaeName);
		}
	}
	else if(ctrlid==ID_BUTTON+2){
		SendMessage(ID_CANCEL);
	}
}


void ChangeBudaeDialog::EvDlgOk()
{
}