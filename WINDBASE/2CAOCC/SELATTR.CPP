#include "jkbutton.h"
#include "jkedit.h"
#include "typectrl.h"
#include "chkctrl.h"
#include "selcolor.h"
#include "selattr.h"

SelectAttributeDialog::SelectAttributeDialog(JKPoint p,MetaTextOut& mttext,JKControl *parent):
	JKDialog(JKRect(0,0,360,300).OffsetBy(p),parent),
	MtText(mttext),
  XSizeEntry(0,"ì·¡",0,10000,10,MtText.FontSize.x),
  YSizeEntry(1,"‘¼·¡",0,10000,10,MtText.FontSize.y),
  TextAlignEntry(2,"µwµb",MtText.TextAlign),
  RotateAngleEntry(3,"ÒA¸åˆb",0,359,10,MtText.RotateAngle),
  SlantAngleEntry(4,"¶¬aÁA",0,359,10,MtText.SlantAngle)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("‹i¸a ­¢¬÷ »¡¸÷");
}

SelectAttributeDialog::~SelectAttributeDialog()
{
}

void SelectAttributeDialog::SetupWindow()
{
	JKControl *ctrl;
	AddControl(new TypeEntryControl( JKRect(100, 20,310, 40),ID_USERCONTROL+1,XSizeEntry));
	AddControl(new TypeEntryControl( JKRect(100, 45,310, 65),ID_USERCONTROL+2,YSizeEntry));
	AddControl(new CheckEntryControl(JKRect(100, 70,310, 90),ID_USERCONTROL+3,TextAlignEntry));
	AddControl(new TypeEntryControl( JKRect(100, 95,310,115),ID_USERCONTROL+4,RotateAngleEntry));
	AddControl(new TypeEntryControl( JKRect(100,120,310,140),ID_USERCONTROL+5,SlantAngleEntry));
	ctrl=new JKButton(JKRect(100,145,310,160),ID_USERCONTROL+7);
	ctrl->WinAttr|=WA_NOHASFOCUS;
	ctrl->BackColor=MtText.Color;
	AddControl(ctrl);
	ctrl=new JKEdit(JKRect(100,170,310,190),ID_USERCONTROL+6,0,40,FALSE);
	ctrl->SetText(MtText.TextBuffer);
	AddControl(ctrl);

	JKButton *bttn;
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=rect.bottom-30;
	rect.left=rect.right-100;
	bttn=new JKButton(rect,ID_BUTTON+2);
	bttn->SetText("Âá­¡");
	AddControl(bttn);

	rect.right=rect.left-10;
	rect.left=rect.right-100;
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("ÑÂ·¥");
	AddControl(bttn);
}

void SelectAttributeDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(50, 20,95, 40),"ì  ·¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 45,95, 65),"‘¼  ·¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 70,95, 90),"µw  µb",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50, 95,95,115),"ÒA¸åˆb",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,120,95,140),"¶¬aÁA",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,145,95,165),"¬‚  ¬w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,170,95,190),"¢…  ¸w",ADJ_RIGHT|ADJ_YCENTER);
}

void SelectAttributeDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->SetFocus();
}

void SelectAttributeDialog::EvButtonClick(WORD ctrlid)
{
	JKControl *ctrl;
	if(ctrlid==ID_USERCONTROL+7) {
		ctrl=GetControl(ID_USERCONTROL+7);
		if(ctrl){
			BYTE color=ctrl->BackColor;
			if(SelectColorDialog(JKPoint(360,100),color,this).Run()==ID_OK)
				ctrl->BackColor=color;
				ctrl->RedrawClient();
		}
	}
	else if(ctrlid==ID_BUTTON+1) {
		BOOL bool;
		int32 num;
		XSizeEntry.GetNumber(num);
		MtText.FontSize.x=num;
		YSizeEntry.GetNumber(num);
		MtText.FontSize.y=num;
		MtText.TextAlign=TextAlignEntry.GetBoolean();
		RotateAngleEntry.GetNumber(num);
		MtText.RotateAngle=num;
		SlantAngleEntry.GetNumber(num);
		MtText.SlantAngle=num;
		ctrl=GetControl(ID_USERCONTROL+6);
		if(ctrl) MtText.SetText(ctrl->GetText());
		ctrl=GetControl(ID_USERCONTROL+7);
		if(ctrl) MtText.Color=ctrl->BackColor;
		SendMessage(ID_OK);
	}
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void SelectAttributeDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_USERCONTROL+6;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_USERCONTROL+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) ctrlid=ID_BUTTON+1;
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_USERCONTROL+1;
			else ctrlid=ID_USERCONTROL+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_USERCONTROL+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_USERCONTROL+1)	ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+1) ctrlid=LastCtrlID;
			else if(ctrlid==ID_BUTTON+2) ctrlid=ID_BUTTON+1;
			else ctrlid=ID_BUTTON+2;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}
