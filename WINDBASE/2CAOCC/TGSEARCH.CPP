#include "jkbutton.h"
#include "plntglst.h"
#include "tmptglst.h"
#include "tgsearch.h"

TargetSearchDialog::TargetSearchDialog(PlanTargetManager& plntgman,TempTargetManager& tmptgman):
	JKDialog(JKRect(560,25,850,160)),PlanTargetMan(plntgman),TempTargetMan(tmptgman)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("ÑÁb ‰ÒB");
}

TargetSearchDialog::~TargetSearchDialog()
{
}

void TargetSearchDialog::SetupWindow()
{
	JKButton *bttn;
	bttn=new JKButton(JKRect(20,20,150,50),ID_BUTTON+1);
	bttn->SetText("Îa¸â ·©œqÎa");
	AddControl(bttn);
	bttn=new JKButton(JKRect(20,60,150,90),ID_BUTTON+2);
	bttn->SetText("‰ÒB¬a‰b Îa¸â");
	AddControl(bttn);

}

void TargetSearchDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.PutIMG("log100.img",JKPoint(170,10));
}

void TargetSearchDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void TargetSearchDialog::EvButtonClick(WORD ctrlid)
{
	JKRect rect=WinRect.SetLeftTopBy();
	rect.Offset(-ClientRect.LeftTop());
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+560;
	rect.left=rect.right-600;
	JKControl *ctrl1=GetControl(ID_SUBWINDOW+1);
	JKControl *ctrl2=GetControl(ID_SUBWINDOW+2);
	if(ctrlid==ID_BUTTON+1){
		if(!ctrl1&&!ctrl2){
			AddControl(new TempTargetListWindow(rect,TempTargetMan));
		}
	}
	else if(ctrlid==ID_BUTTON+2){
		if(!ctrl1&&!ctrl2) {
			rect.left-=200;
			rect.right-=200;
			AddControl(new PlanTargetListWindow(rect,PlanTargetMan));
		}
	}

}

void TargetSearchDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+2;
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
