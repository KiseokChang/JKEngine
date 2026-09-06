#include "eqpinput.h"
#include "combobox.h"
#include "jkbutton.h"
#include "eqpplace.h"
#include "eqplist.h"
#include "extnval.h"

EqpInput::EqpInput(WORD ctrlid,JKControl *parent):
	JKWindow(JKRect(100,70,500,350),parent,ctrlid),MODIFY(FALSE)
{
	SetText("Èe´¢ ·³b Àw");
	BackColor=LtGray;
	WinForm=0x03;
}

EqpInput::~EqpInput()
{
}

void EqpInput::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=65;
	rect.top+=20;
	rect.right=rect.left+102;
	rect.bottom=rect.top+120;
	Combobox *Name=new Combobox(rect,ID_EDIT,8);
	AddControl(Name);
	Name->HuboMan->Add(0,new HuboBase(DAEDAEHQ));
	Name->HuboMan->Add(1,new HuboBase(DAEDAEA));
	Name->HuboMan->Add(2,new HuboBase(DAEDAEB));
	Name->HuboMan->Add(3,new HuboBase(DAEDAEC));

	rect.top+=40;
	rect.bottom=rect.top+120;

	Combobox *Kind=new Combobox(rect,ID_EDIT+1,8);
	AddControl(Kind);
	Kind->HuboMan->Add(0,new HuboBase("HE"));
	Kind->HuboMan->Add(1,new HuboBase("WP"));
	Kind->HuboMan->Add(2,new HuboBase("ICM"));
	Kind->HuboMan->Add(3,new HuboBase("DP-ICM"));
	Kind->HuboMan->Add(4,new HuboBase("ILL"));
	Kind->HuboMan->Add(5,new HuboBase("HEAT"));
	Kind->HuboMan->Add(6,new HuboBase("RAP"));
	Kind->HuboMan->Add(7,new HuboBase("RA-FAS"));
	Kind->HuboMan->Add(8,new HuboBase("5.56MM"));
	Kind->HuboMan->Add(9,new HuboBase("¸w´¢"));
	Kind->HuboMan->Add(10,new HuboBase("®…¤i¯¥‰Å"));
	Kind->HuboMan->Add(11,new HuboBase("¯¡Ðe¯¥‰Å"));
	Kind->HuboMan->Add(12,new HuboBase("¸ó‹e¯¥‰Å"));
	Kind->HuboMan->Add(13,new HuboBase("‰ÅÉ·¯¥‰Å"));
	Kind->HuboMan->Add(14,new HuboBase("’A‰Å"));

	rect.top+=40;
	rect.bottom=rect.top+20;
	JKEdit *edit=new JKEdit(rect,ID_EDIT+2,Bomb.Dod,10,FALSE);
	AddControl(edit);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+20;
	edit=new JKEdit(rect,ID_EDIT+3,Bomb.Lot,21,FALSE);
	AddControl(edit);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+20;
	edit=new JKEdit(rect,ID_EDIT+8,Bomb.HaveYear,10,FALSE);
	AddControl(edit);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=250;
	rect.top+=20;
	rect.right=rect.left+102;
	rect.bottom=rect.top+20;
	edit=new JKEdit(rect,ID_EDIT+4,Bomb.Born,10,FALSE);
	AddControl(edit);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+150;
	Combobox *place=new Combobox(rect,ID_EDIT+5,14);
	AddControl(place);
	PlaceAdd(*place);

	rect.top+=40;
	rect.bottom=rect.top+20;
	edit=new JKEdit(rect,ID_EDIT+6,Bomb.Count,4,FALSE);
	AddControl(edit);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+70;
	Combobox *use=new Combobox(rect,ID_EDIT+7,4);
	AddControl(use);
	use->HuboMan->Add(0,new HuboBase("B/L"));
	use->HuboMan->Add(1,new HuboBase("CSR"));
	use->HuboMan->Add(2,new HuboBase("ŠaÈe"));

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=ClientRect.Width()/2-100;
	rect.right=rect.left+60;
	rect.top=rect.bottom-40;
	rect.bottom=rect.top+30;

	JKButton *btn=new JKButton(rect,ID_BUTTON);
	btn->SetText("¸á  ¸w");
	AddControl(btn);

	rect.left=rect.right+80;
	rect.right=rect.left+60;
	btn=new JKButton(rect,ID_BUTTON+1);
	btn->SetText("Âá  ­¡");
	AddControl(btn);
}

void EqpInput::EvShowWindow()
{
	JKWindow::EvShowWindow();
	JKControl *win=GetControl(ID_EDIT);
	if(win) win->SetFocus();
}

void EqpInput::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=10;
	rect.top+=20;
	rect.right=rect.left+50;
	rect.bottom=rect.top+22;
	dc.BackColor=LtGray;
	dc.TextOutX(rect,"­¡­¢",ADJ_RIGHT);

	rect.top=rect.bottom+22;
	rect.bottom=rect.top+120;
	dc.TextOutX(rect,"Ðw¡¢",ADJ_RIGHT);

	rect.top+=40;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"D.O.D",ADJ_RIGHT|ADJ_YCENTER);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"LOT-NO",ADJ_RIGHT|ADJ_YCENTER);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"®w·©",ADJ_RIGHT|ADJ_YCENTER);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=182;
	rect.top+=20;
	rect.right=rect.left+65;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¹A¸b‘e•¡",ADJ_RIGHT|ADJ_YCENTER);

	rect.top=rect.bottom+22;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¥¡‰Å¸w­¡",ADJ_RIGHT);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¤i®",ADJ_RIGHT|ADJ_YCENTER);

	rect.top=rect.bottom+22;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¬a¶w¶w•¡",ADJ_RIGHT);
}

void EqpInput::EvCharDown(uint16 key,WORD )
{
	switch(key) {
		case ESC:
			SendMessage(ID_CANCEL);
			break;
		case ENTER:
			SendMessage(ID_OK);
			break;
	}
}

void EqpInput::EvEditFinish(WORD ctrlid)
{
	char ct[6];
	JKEdit *edit;

	switch(ctrlid) {
		case ID_EDIT+6:
			edit=(JKEdit *) GetControl(ID_EDIT+6);
			if(edit) {
				edit->GetText(ct);
				if(ct!=""&&!atoi(ct)) {
					edit->SetText("");
					edit->SetFocus();;
				}
			}
			break;
		case ID_EDIT+5:
			PlaceWrite();
			break;
	}
}

void EqpInput::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid) {
		case ID_BUTTON:
			GetValue();
			if(!IsEmpty()) SendControlMessage(ID_SAVE);
			break;
		case ID_BUTTON+1:
			ClearAllText();
			HideWindow();
			break;
	}
}

BOOL EqpInput::IsEmpty()
{
	JKEdit *win=(JKEdit *)GetControl(ID_EDIT);
	if(!win||win->GetTextSize()==0) return TRUE;
	win=(JKEdit *)GetControl(ID_EDIT+1);
	if(!win||win->GetTextSize()==0) return TRUE;
	win=(JKEdit *)GetControl(ID_EDIT+5);
	if(!win||win->GetTextSize()==0) return TRUE;
	win=(JKEdit *)GetControl(ID_EDIT+7);
	if(!win||win->GetTextSize()==0) return TRUE;
	if(strlen(Bomb.Name)==0) return TRUE;
	if(strlen(Bomb.Dod)==0) return TRUE;
	if(strlen(Bomb.Lot)==0) return TRUE;
	if(strlen(Bomb.Born)==0) return TRUE;
	if(strlen(Bomb.Count)==0) return TRUE;
	if(strlen(Bomb.HaveYear)==0) return TRUE;
	return FALSE;
}


void EqpInput::GetValue()
{
	JKEdit *win=(JKEdit *)GetControl(ID_EDIT);
	if(win) win->GetText(Bomb.Name,8);
	win=(JKEdit *)GetControl(ID_EDIT+1);
	if(win) win->GetText(Bomb.Kind,8);
	win=(JKEdit *)GetControl(ID_EDIT+5);
	if(win) win->GetText(Bomb.Place,14);
	win=(JKEdit *)GetControl(ID_EDIT+7);
	if(win) win->GetText(Bomb.Use,4);
}

void EqpInput::ClearAllText()
{
	JKEdit *edit;
	for(int16 i=0;i<9;i++) {
		edit=(JKEdit *)GetControl(ID_EDIT+i);
		if(edit) {
			edit->SetText("");
			edit->RedrawClient();
		}
		strcpy(Bomb.SomoYear,"");
	}
}

void EqpInput::SetValue(int16 index)
{
	BOMB Bomb;
	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	if(fm) {
		fm->Read(Bomb,index);
		JKEdit *win=(JKEdit *)GetControl(ID_EDIT);
		if(win) win->SetText(Bomb.Name);
		win=(JKEdit *)GetControl(ID_EDIT+1);
		if(win) win->SetText(Bomb.Kind);
		win=(JKEdit *)GetControl(ID_EDIT+2);
		if(win) win->SetText(Bomb.Dod);
		win=(JKEdit *)GetControl(ID_EDIT+3);
		if(win) win->SetText(Bomb.Lot);
		win=(JKEdit *)GetControl(ID_EDIT+4);
		if(win) win->SetText(Bomb.Born);
		win=(JKEdit *)GetControl(ID_EDIT+5);
		if(win) win->SetText(Bomb.Place);
		win=(JKEdit *)GetControl(ID_EDIT+6);
		if(win) win->SetText(Bomb.Count);
		win=(JKEdit *)GetControl(ID_EDIT+7);
		if(win) win->SetText(Bomb.Use);
		win=(JKEdit *)GetControl(ID_EDIT+8);
		if(win) win->SetText(Bomb.HaveYear);
		strcpy(Bomb.SomoYear,"");
	}
	if(fm) delete fm;
}

void EqpInput::CheckBomb()
{
	if((strcmp(Bomb.Name,DAEDAEHQ)!=0)&&(strcmp(Bomb.Name,DAEDAEA)!=0)
		&&(strcmp(Bomb.Name,DAEDAEB)!=0)&&(strcmp(Bomb.Name,DAEDAEC)!=0))
		strcpy(Bomb.Name,DAEDAEHQ);
	uint16 i=atoi(Bomb.Count);
	if(!i||i>9999) {
		i=9999;
		sprintf(Bomb.Count,"%4d",i);
	}
}

void EqpInput::PlaceAdd(Combobox &com)
{
	int16 i=0;
	char buf[15];
	EqpPlace *place=new EqpPlace();
	while(1) {
		if(!place->Read(*buf,i)) break;
		com.HuboMan->Add(i,new HuboBase(buf,i));
		i++;
	}
	if(place) delete place;
}

int16 EqpInput::PlaceWrite()
{
	int16 i=0;
	char buf[15];

	Combobox *edit=(Combobox *) GetControl(ID_EDIT+5);
	if(edit) {
		edit->GetText(buf,14);
		if(edit->GetTextSize()!=0) {
			HuboManager *huboman=edit->HuboMan;
			HuboBase *hubo;
			for(i=0;i<huboman->NodeCount;i++) {
				hubo=huboman->GetHuboBase(i);
				if(strstr(buf,hubo->TextBuff)!=0&&strstr(hubo->TextBuff,buf)!=0) return 0;
			}
			EqpPlace *place=new EqpPlace();
			if(place)place->Write(*buf);
			if(place) delete place;
			huboman->DestroyAll();
			PlaceAdd(*edit);
		}
	}
	return TRUE;
}
