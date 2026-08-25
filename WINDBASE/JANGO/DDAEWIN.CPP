#include "ddaewin.h"
#include "combobox.h"
#include "jkbutton.h"

DaedaeDialog::DaedaeDialog(char* daedae,WORD ctrlid,JKControl *parent):
	JKDialog(JKRect(150,100,450,350),parent,ctrlid),DaeDaeName(daedae)
{
	WinForm=0x03;
	SetText("”” ¬åÈ‚");
	WinAttr|=WA_FIRSTSHOW;
	BackColor=LIGHTGRAY;
	TextColor=BLACK;
}

DaedaeDialog::~DaedaeDialog()
{}

void DaedaeDialog::SetupWindow()
{
  JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	Combobox *combobox=new Combobox(JKRect(rect.left+150,rect.top+30,rect.right-20,rect.bottom-30),ID_EDIT+1,3);
  combobox->WinAttr|=WA_FIRSTSHOW;
  AddControl(combobox);
  combobox->HuboMan->Add(0,new HuboBase("2"));
  combobox->HuboMan->Add(1,new HuboBase("92"));
  combobox->HuboMan->Add(2,new HuboBase("100"));
  combobox->HuboMan->Add(3,new HuboBase("332"));
  combobox->HuboMan->Add(4,new HuboBase("365"));
  combobox->HuboMan->Add(5,new HuboBase("755"));
  combobox->HuboMan->Add(6,new HuboBase("855"));
  combobox->HuboMan->Add(7,new HuboBase("899"));

  rect.left=ClientRect.Width()/2-80;
  rect.right=rect.left+60;
  rect.top=rect.bottom-50;
	rect.bottom=rect.top+30;
  JKButton *btn=new JKButton(rect,ID_BUTTON+1);
  btn->SetText("ÑÂ ·¥");
  AddControl(btn);

  rect.left=rect.right+40;
  rect.right=rect.left+60;
  btn=new JKButton(rect,ID_BUTTON+2);
  btn->SetText("Âá ­¡");
  AddControl(btn);
}

void DaedaeDialog::PaintClient(JKDC& dc,JKRect& rect1)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
  rect.left+=20;
	rect.top+=30;
	rect.right-=170;
	rect.bottom-=30;
  dc.IsOverWrite=FALSE;
  dc.TextColor=BLUE;
  dc.TextOutX(rect,"””Ÿi\n¬åÈ‚Ðaˆáa\n·³bÐa­A¶a.\n",ADJ_LEFT);
}


void DaedaeDialog::EvCharDown(uint16 key,WORD modifier)
{
	if(key==ESC) HideWindow();
}

void DaedaeDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid) {
  	case ID_BUTTON+1:
			if(DaeDaeName) {
				SendMessage(ID_OK);
			}
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
  }
}

ProcessResult DaedaeDialog::RespondMessage(JKMSG& msg)
{
	return JKDialog::RespondMessage(msg);
}

void DaedaeDialog::EvEditFinish(WORD ctrlid)
{
  Combobox *edit;
	switch(ctrlid) {
  	case ID_EDIT+1:
    	edit=(Combobox *)GetControl(ctrlid);
			if(edit) {
				strncpy(DaeDaeName,edit->GetText(),3);
				DaeDaeName[3]=NULL;
			}
    	break;
  }
}

void DaedaeDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
  SetFocus();
  JKControl *ctrl=GetControl(ID_EDIT+1);
  if(ctrl) ctrl->SetFocus();
}