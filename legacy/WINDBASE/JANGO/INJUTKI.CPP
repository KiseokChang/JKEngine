#include "joglobal.h"
#include "fltbttn.h"
#include "scrllist.h"
#include "insamsg.h"
#include "ingadlg.h"
#include "injutki.h"

JutkiCondWin::JutkiCondWin(JKRect rect,WORD ctrlid,PersonManager& recman,WORD *& srcharray,uint16& srchcount):
	CommonCondWin(rect,ctrlid,4,recman,srcharray,srchcount)
{}

void JutkiCondWin::SetupWindow()
{
	CommonCondWin::SetupWindow();

	JKControl *ctrl=GetControl(ID_BUTTON+3);
	if(ctrl) {
		JKRect rect = ctrl->WinRect;
		ctrl->WinRect.Offset(JKPoint(0,50));

		JKButton *bttn=new JKFloatingButton(rect,ID_BUTTON+4);
		bttn->SetText("·¥ˆa ®¸÷");
		AddControl(bttn);
 }
}

void JutkiCondWin::EvButtonClick(WORD ctrlid)
{
	CommonCondWin::EvButtonClick(ctrlid);
	if(ctrlid==ID_BUTTON+4) ModifyInga();
}


void JutkiCondWin::CreatePresentCondition()
{
	BYTE fname[13];
	strcpy(fname,BudaeName);
	strcat(fname,"jtk.dat");
	IngaMan.GetInwon(fname);
	CommonCondWin::CreatePresentCondition();
}

void JutkiCondWin::ModifyInga()
{
	if(IngaDialog(InwonTextLen).Run()==ID_OK)
		InitList();
}