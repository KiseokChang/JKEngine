#include "joglobal.h"
#include "jkbutton.h"
#include "jkedit.h"
#include "combobox.h"
#include "okdialog.h"
#include "budaedlg.h"
#include "persnman.h"
#include "adddlg.h"

#define JOAppMessageCount 6

static char *DestFileSuffix  = "main";
static char *DestObsvSuffix  = "obsv";
static char *SrcFileSuffix[4]= {
	"main0","main1","main2","main3"
};
static char *SrcObsvSuffix[4]= {
	"obsv0","obsv1","obsv2","obsv3"
};

AddFileDialog::AddFileDialog():
	JKDialog(JKRect(200,100,800,400)),HuboMan(0),AddMode(0)
{
	BackColor=LtGray;
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("·¥¬a •A·¡Èa Ìa·©  ‰ÅŸ¡ Ïa¡‹aœ‘");
	HuboMan = new HuboManager();
	if(!HuboMan) EventHandler::CreationError=JKControl::CreationError=TRUE;
}

AddFileDialog::~AddFileDialog()
{
	if(HuboMan) delete HuboMan;
}

void AddFileDialog::InitWindow()
{
	InitBudae();
	JKDialog::InitWindow();
	InitAddMode();
}

void AddFileDialog::SetupWindow()
{

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-20);

	rect.top+=20;
	rect.bottom=rect.top+20;
	rect.right=rect.left+250;

	JKButton *bttn;

	rect.left=rect.right+150;
	rect.right=rect.left+150;
	rect.bottom+=70;
	Combobox *combo=new Combobox(rect,ID_EDIT+1,10,HuboMan,FALSE);
	AddControl(combo);
	rect.bottom-=70;

	JKStatic *stc;
	rect.top=rect.bottom+20;
	rect.bottom=rect.top+20;
	rect.left=20;

	rect.right=rect.left+100;
	stc=new JKStatic(rect,0);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->BackColor=BackColor;
	stc->SetText("º •A·¡Èa");
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,0);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->BackColor=BackColor;
	stc->SetText("º ÉA·¡§i");
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,0);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->BackColor=BackColor;
	stc->SetText("¡e”q •A·¡Èa");
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,0);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->BackColor=BackColor;
	stc->SetText("¡e”q ÉA·¡§i");
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+20;
	rect.left=20;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+1);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtRed;
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+2);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtRed;
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+3);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtRed;
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+4);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtRed;
	AddControl(stc);

	rect.top=rect.bottom+10;
	rect.bottom=rect.top+20;
	rect.left=20;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+5);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtBlue;
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+6);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtBlue;
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+7);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtBlue;
	AddControl(stc);
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	stc=new JKStatic(rect,ID_STATIC+8);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->TextColor=LtBlue;
	AddControl(stc);

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+25;
	rect.left=20;
	rect.right=rect.left+150;
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("¯©   Ð—");
	AddControl(bttn);

	rect.left=rect.right+150;
	rect.right=rect.left+130;
	bttn=new JKButton(rect,ID_BUTTON+2);
	bttn->SetText("¹·   ža");
	AddControl(bttn);
}

void AddFileDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	SetFocus();
}

void AddFileDialog::EvButtonClick(WORD ctrlid)
{
	JKControl *ctrl;
	switch(ctrlid){
		case ID_BUTTON+1:
			ExecuteAdd();
			break;
		case ID_BUTTON+2:
			CloseWindow();
			break;
		default:
			break;
	}
}

void AddFileDialog::PaintClient(JKDC& dc,JKRect& /*paintrect*/)
{
	dc.IsOverWrite=FALSE;
}

BOOL AddFileDialog::ExecuteAdd()
{
	uint16 len=strlen(BudaeName);
	char DestFileName[9],DestObsvName[9],SrcFileName[9],SrcObsvName[9];
	strcpy(DestFileName,BudaeName);
	strcpy(DestFileName+len,DestFileSuffix);
	strcpy(DestObsvName,BudaeName);
	strcpy(DestObsvName+len,DestObsvSuffix);
	strcpy(SrcFileName,BudaeName);
	strcpy(SrcFileName+len,SrcFileSuffix[AddMode]);
	strcpy(SrcObsvName,BudaeName);
	strcpy(SrcObsvName+len,SrcObsvSuffix[AddMode]);

	PersonManager *RecMan1 = new PersonManager(DestFileName,DestObsvName);
	PersonManager *RecMan2 = new PersonManager(SrcFileName,SrcObsvName);

//----------------------------------------------------------------------//
	if(RecMan1&&RecMan2)	{
		if(RecMan1->AddPersonMan(RecMan2,AddMode)){
			RecMan1->ChangeManagerMode(FALSE);
			RecMan2->ChangeManagerMode(FALSE);
			if(RecMan1->AddPersonMan(RecMan2,AddMode)){
				delete RecMan1;
				delete RecMan2;
				return TRUE;
			}
		}
	}
//----------------------------------------------------------------------//
	if(RecMan1) delete RecMan1;
	if(RecMan2) delete RecMan2;
	return FALSE;
}

void AddFileDialog::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid==ID_EDIT+1){
		if(index<4&&index!=AddMode){
			AddMode=index;
			InitAddMode();
		}
	}
}

void AddFileDialog::InitBudae()
{
	HuboMan->DestroyAll();
	uint16 len=strlen(BudaeName);
	char buffer[7];
	strcpy(buffer,BudaeName);
	strcpy(buffer+len,"-HQ");
	HuboMan->Add(-1,new HuboBase(buffer,0));
	strcpy(buffer+len,"-A");
	HuboMan->Add(-1,new HuboBase(buffer,1));
	strcpy(buffer+len,"-B");
	HuboMan->Add(-1,new HuboBase(buffer,2));
	strcpy(buffer+len,"-C");
	HuboMan->Add(-1,new HuboBase(buffer,3));

	AddMode=0;
	InitAddMode();
}

void AddFileDialog::InitAddMode()
{
	JKControl *ctrl;

	if(HuboMan){
		ctrl=GetControl(ID_EDIT+1);
		if(ctrl) {
			HuboBase *hubo=(*HuboMan)[AddMode];
			if(hubo){
				ctrl->SetText(hubo->TextBuff);
				ctrl->RedrawClient();
			}
		}
	}

	uint16 len=strlen(BudaeName);
	char DestFileName[9],DestObsvName[9],SrcFileName[9],SrcObsvName[9];
	strcpy(DestFileName,BudaeName);
	strcpy(DestFileName+len,DestFileSuffix);
	strcpy(DestObsvName,BudaeName);
	strcpy(DestObsvName+len,DestObsvSuffix);
	strcpy(SrcFileName,BudaeName);
	strcpy(SrcFileName+len,SrcFileSuffix[AddMode]);
	strcpy(SrcObsvName,BudaeName);
	strcpy(SrcObsvName+len,SrcObsvSuffix[AddMode]);

	char FileName[13];
	len=strlen(SrcFileName);
	strcpy(FileName,SrcFileName);
	ctrl=GetControl(ID_STATIC+1);
	if(ctrl){
		strcpy(FileName+len,".dat");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}
	ctrl=GetControl(ID_STATIC+2);
	if(ctrl){
		strcpy(FileName+len,".tbl");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}
	len=strlen(SrcObsvName);
	strcpy(FileName,SrcObsvName);
	ctrl=GetControl(ID_STATIC+3);
	if(ctrl){
		strcpy(FileName+len,".dat");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}
	ctrl=GetControl(ID_STATIC+4);
	if(ctrl){
		strcpy(FileName+len,".tbl");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}

	len=strlen(DestFileName);
	strcpy(FileName,DestFileName);
	ctrl=GetControl(ID_STATIC+5);
	if(ctrl){
		strcpy(FileName+len,".dat");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}
	ctrl=GetControl(ID_STATIC+6);
	if(ctrl){
		strcpy(FileName+len,".tbl");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}
	len=strlen(DestObsvName);
	strcpy(FileName,DestObsvName);
	ctrl=GetControl(ID_STATIC+7);
	if(ctrl){
		strcpy(FileName+len,".dat");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}
	ctrl=GetControl(ID_STATIC+8);
	if(ctrl){
		strcpy(FileName+len,".tbl");
		ctrl->SetText(FileName);
		ctrl->RedrawClient();
	}
}
