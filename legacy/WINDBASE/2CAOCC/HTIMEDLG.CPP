#include "checkbox.h"
#include "jkbutton.h"
#include "typectrl.h"
#include "plntgman.h"
#include "htimedlg.h"

HTimeDialog::HTimeDialog(JKPoint p,JKControl *parent):
	JKDialog(JKRect(0,0,375,250).OffsetBy(p),parent)
{
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("H ¯¡ˆe ¸w·³");

	struct time currtime;
	gettime(&currtime);
	for(uint16 i=0;i<6;i++){
		if(PlanTargetMan->EnableSet[i])
			TimeEnt[i]=new TimeEntry(i,"TimeEntry",&PlanTargetMan->HTimeSet[i]);
		else
			TimeEnt[i]=new TimeEntry(i,"TimeEntry",&currtime);
	}
}

HTimeDialog::~HTimeDialog()
{
}

void HTimeDialog::SetupWindow()
{
	JKButton *bttn;
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=rect.bottom-30;
	rect.left=rect.right-100;
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("ÑÂ·¥");
	AddControl(bttn);


	char *butttext[6]={
    "‰·Ìa¬a", "Îa¸âŠ…", "Îa¸â”", "ÑÁb‰b¡i", "ÑÁbÈáé", "‰·º…¬a"
	};

	JKPoint p(10,10);
	for(uint16 i=0;i<6;i++){
		JKCheckBox *chkbox=new JKCheckBox(JKRect(6,38+20*i,21,38+20*(i+1)-1).OffsetBy(p),ID_CHECKBOX+1+i);
		AddControl(chkbox);
		chkbox->Status=PlanTargetMan->EnableSet[i];

		bttn=new JKButton(JKRect(22,38+20*i,138,38+20*(i+1)-1).OffsetBy(p),ID_BUTTON+2+i);
		bttn->SetText(butttext[i]);
		bttn->SetEnable(FALSE);
		AddControl(bttn);

		AddControl(new TypeEntryControl(JKRect(146,38+i*20,300,38+20*(i+1)-1).OffsetBy(p),ID_USERCONTROL+1+i,*TimeEnt[i]));
	}
}

void HTimeDialog::PaintClient(JKDC& dc,JKRect&)
{
	JKPoint p(10,10);
	JKRect rect(0,0,303,160);
	dc.Rectangle3D(rect.OffsetBy(p),1,LtGray,White);
	rect.Expand(-1);
	dc.Rectangle(rect.OffsetBy(p),Black);
	rect.Expand(-1);
	dc.Rectangle3D(rect.OffsetBy(p),-1,LtGray,White);

	dc.VLine(JKPoint(140+1,2)+p,rect.Height(),LtGray);
	dc.VLine(JKPoint(140+2,1)+p,rect.Height()+1,Black);
	dc.VLine(JKPoint(140+3,2)+p,rect.Height(),White);

	dc.HLine(JKPoint(2,30+1)+p,rect.Width(),LtGray);
	dc.HLine(JKPoint(1,30+2)+p,rect.Width()+1,Black);
	dc.HLine(JKPoint(2,30+3)+p,rect.Width(),White);

	rect.Expand(-3);
	rect.top+=32;
	JKRect temp;
	temp=rect; temp.right=temp.left+134;
	dc.Box3D(temp.OffsetBy(p),-1,FindNearestColor(55,55,55));
	temp=rect; temp.left+=141;
	dc.Box3D(temp.OffsetBy(p),-1,FindNearestColor(55,55,55));

	dc.IsOverWrite=FALSE;
	dc.SetTextColor(Magenta);
	dc.TextOutX(JKRect(0  ,0,140,33).OffsetBy(p),"‰·‰b ¤w¤ó",ADJ_XYCENTER);
	dc.TextOutX(JKRect(146,0,300,33).OffsetBy(p),"H  ¯¡ ˆe",ADJ_XYCENTER);
}

void HTimeDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void HTimeDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1)	SendMessage(ID_OK);
}

void HTimeDialog::EvDlgOk()
{
	for(uint16 i=0;i<6;i++){
		JKCheckBox *chkbox=(JKCheckBox *)GetControl(ID_CHECKBOX+1+i);
		if(chkbox){
			PlanTargetMan->EnableSet[i]=chkbox->Status;
			TimeEnt[i]->StrToTime(TimeEnt[i]->DataBuffer,PlanTargetMan->HTimeSet[i]);
		}
	}
}
