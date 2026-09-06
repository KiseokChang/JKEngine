#include "eqpsmip.h"
#include "eqfilemn.h"
#include "jkbutton.h"
#include "listbox.h"
#include "extnval.h"

EquipSomoInputWindow::EquipSomoInputWindow(JKControl *parent=0):
	JKDialog(JKRect(150,70,450,290),parent),
	TempCount1(0),BombCount(0),TempCount2(0)
{
	WinForm=0x03;
  SetText("­¡¡¡ÑeÑ× ·³bÀw");
	InitSomo();
}

EquipSomoInputWindow::~EquipSomoInputWindow()
{}

void EquipSomoInputWindow::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.right-=20;
	rect.top+=30;
	rect.left=rect.right-200;
	rect.bottom=rect.top+20;
	JKEdit *edit=new JKEdit(rect,ID_EDIT+1,Count,4,FALSE);
	AddControl(edit);

	rect.top=rect.bottom+30;
	rect.bottom=rect.top+20;
	edit=new JKEdit(rect,ID_EDIT+2,SomoYear,10,FALSE);
	AddControl(edit);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=ClientRect.Width()/2-100;
	rect.right=rect.left+70;
	rect.top=rect.bottom-50;
	rect.bottom=rect.top+30;
	JKButton *btn=new JKButton(rect,ID_BUTTON+1);
  btn->SetText("ÑÂ ·¥");
	AddControl(btn);
	rect.left=rect.right+60;
	rect.right=rect.left+70;
	btn=new JKButton(rect,ID_BUTTON+2);
  btn->SetText("Âá ­¡");
	AddControl(btn);
}

void EquipSomoInputWindow::PaintClient(JKDC& dc,JKRect&)
{
	char buf[30];
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.IsOverWrite=FALSE;
	rect.left+=70;
	rect.right=rect.left+40;
	rect.top+=10;
	rect.bottom=rect.top+20;
	sprintf(buf,"1 - %u  Œa»¡ ˆa“wÐs“¡”a.",BombCount);
	dc.TextOutX(rect,buf,ADJ_YCENTER|ADJ_LEFT);

	rect.left-=50;
	rect.top+=20;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"¤i  ®",ADJ_YCENTER|ADJ_LEFT);
	rect.top=rect.bottom+30;
	rect.bottom=rect.top+20;
	dc.TextOutX(rect,"­¡¡¡·©",ADJ_YCENTER|ADJ_LEFT);
}

void EquipSomoInputWindow::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid) {
		case ID_BUTTON+1:
			if(TempCount1) Modify();
			CloseWindow();
			break;
		case ID_BUTTON+2:
			CloseWindow();
			break;
	}
}

void EquipSomoInputWindow::EvEditFinish(WORD ctrlid)
{
	JKEdit *edit;
	uint16 temp;

	switch(ctrlid) {
		case ID_EDIT+1:
			edit=(JKEdit *)GetControl(ctrlid);
			temp=atoi(Count);
			if(temp&&edit&&BombCount>=temp) {
				TempCount1=atoi(Count);
				TempCount2=BombCount-TempCount1;
				TempCount1=BombCount-TempCount2;
			}
			else {
				edit->SetText("");
				edit->SetFocus();
			}
			break;
		case ID_EDIT+2:
			edit=(JKEdit *)GetControl(ctrlid);
			if(!strlen(SomoYear)&&edit) {
				edit->SetFocus();
			}
			break;
	}
}

void EquipSomoInputWindow::Modify()
{
	char buf[5];
	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	EqiupFileManage *somo=new EqiupFileManage(SOMONAME);

	if(fm&&somo&&strlen(SomoYear)) {
		if(TempCount2>0) {
			//itoa(TempCount2,SomoBomb.Count,10);
			sprintf(SomoBomb.Count,"%u",TempCount2);
			fm->Modify(SomoBomb,Index);
		}
		else if(!TempCount2) fm->Delete(Index);
		//itoa(TempCount1,SomoBomb.Count,10);
		sprintf(SomoBomb.Count,"%u",TempCount1);
		strcpy(SomoBomb.SomoYear,SomoYear);
		somo->Append(SomoBomb);
		delete fm;
		delete somo;
	}
}

void EquipSomoInputWindow::InitSomo()
{
	EqiupFileManage *fm=new EqiupFileManage(EQUIPNAME);
	if(fm) {
		fm->Read(SomoBomb,Index);
		BombCount=atoi(SomoBomb.Count);
//		if(BombCount<=0||BombCount>=10000) CloseWindow();
		delete fm;
	}
}


