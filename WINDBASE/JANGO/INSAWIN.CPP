#include "resbttn2.h"
#include "poplist.h"
#include "popupstc.h"
#include "combobox.h"
#include "passdlg.h"
#include "listwin.h"
#include "sagowin.h"
#include "injutki.h"
#include "inbirth.h"
#include "injunyok.h"
#include "incommon.h"
#include "maintitl.h"
#include "statbar.h"
#include "insarc.h"
#include "insamsg.h"
#include "insawin.h"

#define ViewCount 19
static WORD ViewID[ViewCount]={
	ID_SUBWINDOW+ 1,ID_SUBWINDOW+ 2,ID_SUBWINDOW+ 3,ID_SUBWINDOW+ 4,
	ID_SUBWINDOW+ 5,ID_SUBWINDOW+ 6,ID_SUBWINDOW+ 7,ID_SUBWINDOW+ 8,
	ID_SUBWINDOW+ 9,ID_SUBWINDOW+10,ID_SUBWINDOW+11,ID_SUBWINDOW+12,
	ID_SUBWINDOW+13,ID_SUBWINDOW+14,ID_SUBWINDOW+15,ID_SUBWINDOW+16,
	ID_SUBWINDOW+17,ID_SUBWINDOW+18,ID_SUBWINDOW+19
};

#define JOWinMessageCount 21

static char *ExplainMessage[JOWinMessageCount]={
	"¸åÁA ¥¡‹¡", "¬a‰¡ ·³b","ºËb‹¡\nÑeÑ×",  "º­¡»¡\nÑeÑ×","Ðbb\nÑeÑ×","Ðb‰Á¡w\nÑeÑ×",
	"¹·Ša\nÑeÑ×","Ñi´‚Ñw\nÑeÑ×","‰‹s\nÑeÑ×", "»¢À‚\nÑeÑ×","Ëb‹¡\nÑeÑ×",  "¸a‰b»w\nÑeÑ×",
	"‰iÑ¥‰Å‰\nÑeÑ×","¸åÊ¥¢\nÐa·","¸åÊ¥¢\n¬w·","¸åÊÑÁ","»¡Òá‰Å¯±\nÑeÑ×","¬—·©¸a\nÑeÑ×",
	"¸åµb·©\nÑeÑ×","¸å,Ñeµb¸a\n¥eÑÅ","º ¡A“A¡"
};

InsaWindow::InsaWindow():
	JKDialog(JKRect(0,0,getmaxx(),getmaxy()),0,ID_USERCONTROL+0x100),
	PersonManager(),SagoMan(*this),
	CurrentView(-1),ExplainID(-1)
{
	WinForm=0x03;
	Depth=1;
	BorderSize=3;
	BackColor=LtGray;
	SetText("·¥  ¬a    ‰Å  Ÿ¡");
	WinAttr|= WA_FIRSTSHOW | WA_CHANGEPLACE;
	SearchArray = new uint16[AddedRecord->NodeCount];
}

InsaWindow::~InsaWindow()
{
	if(SearchArray) delete[] SearchArray;
}

void InsaWindow::InitWindow()
{
	JKDialog::InitWindow();
	AdjustTitleAmount();
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top+=60;
	rect.bottom-=30;
	SubWinRect.Set(rect);
}

void InsaWindow::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKControl *ctrl;

	rect.right-=250;
	rect.bottom=rect.top+59;
	ctrl=new MainTitle(rect,ID_USERCONTROL+1);
	AddControl(ctrl);

	JKPoint p(800,200);
	uint16 maxwidth=60,maxheight=70;
	uint16 width=50,height=50;
	JKButton *bttn;
	WORD InsaIconArray[21]={0,2,4,6,8,8,10,12,14,16,18,20,22,26,28,30,32,34,36,38,40};
	for(uint16 i=0;i<21;i++){
		bttn=new JKResButton2(JKRect(maxwidth*(i%3),maxheight*(i/3),maxwidth*(i%3)+width-1,maxheight*(i/3)+height-1).OffsetBy(p),ID_BUTTON+1+i,InsaIconID[InsaIconArray[i]]);
		AddControl(bttn);
		CtrlResMan.Add(new CtrlResBase(ID_BUTTON+1+i,i,JKRect(maxwidth*(i%3)-15,maxheight*(i/3)+height,maxwidth*(i%3)+width+15,maxheight*(i/3)+height+45).OffsetBy(p)));
	}

//==================================================================

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top+=60;
	rect.bottom-=30;

	rect.top=rect.bottom+1;
	rect.bottom=rect.top+29;
	ctrl= new StatusBar(rect,ID_USERCONTROL+2);
	AddControl(ctrl);
	JKStatic *stc= new PopupStatic(JKRect(),ID_USERCONTROL+3);
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->BackColor=Yellow;
	AddControl(stc);
}

void InsaWindow::PaintClient(JKDC& dc,JKRect& paintrect)
{
	dc.PutIMG("bomb1.img",JKPoint(-100,0),paintrect);
	JKRect rect=ClientRect.SetLeftTopBy();
	rect.bottom-=30;
	rect.left=rect.right-249;
	dc.Box3D(rect,2,LtGray);
	rect.left+=30;
	rect.right-=30;

	JKRect temp=rect;

	rect.top+=20;
	rect.bottom=rect.top+150;
//	dc.Box3D(rect,-1,LtGray);
	dc.PutIMG("log170.img",JKPoint(rect.left+10,rect.top),rect.ExpandBy(-2));
	temp.top+=180;
	temp.bottom-=25;
	dc.Box3D(temp,-2,LtGray);
}

void InsaWindow::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void InsaWindow::EvCtrlOnlyMove(WORD ctrlid)
{
	JKControl *ctrl;
	CtrlResBase *ctrlres = CtrlResMan.Get(ctrlid);
	if(ctrlres&&ctrlres->ResourceID<JOWinMessageCount){
		if(ctrlres->ResourceID!=ExplainID){
			ctrl = GetControl(ID_USERCONTROL+3);
			if(ctrl) {
				ctrl->ChangeWindowSize(ctrlres->Rect);
				ctrl->SetText(ExplainMessage[ctrlres->ResourceID]);
				ctrl->ShowWindow();
				ExplainID=ctrlres->ResourceID;
			}
		}
	}
	else{
		if(ExplainID!=-1){
			ctrl = GetControl(ID_USERCONTROL+3);
			if(ctrl) ctrl->HideWindow();
			ExplainID=-1;
		}
	}
}

void InsaWindow::EvCtrlLeftClick(WORD ctrlid)
{
	if(ExplainID!=-1){
		JKControl *ctrl = GetControl(ID_USERCONTROL+3);
		if(ctrl) ctrl->HideWindow();
		ExplainID=-1;
	}
	JKDialog::EvCtrlLeftClick(ctrlid);
}

void InsaWindow::EvButtonClick(WORD ctrlid)
{
	JKControl *ctrl;
	switch(ctrlid){
		case ID_BUTTON+1:
			MouseEvents->SetCursorID(5);
			ListWindow *listwin;
			listwin= (ListWindow *)GetControl(ID_SUBWINDOW+1);
			if(!listwin){
				listwin= new ListWindow(SubWinRect,ID_SUBWINDOW+1,*((PersonManager *)this),SearchArray,SearchCount);
				AddControl(listwin);
			}
			listwin->InitList();
			listwin->ShowWindow();
			break;
		case ID_BUTTON+2:
			MouseEvents->SetCursorID(5);
			ctrl= new SagoWindow(SubWinRect,ID_SUBWINDOW+2,SagoMan,SearchArray,SearchCount);
			ctrl->SetText("¬a‰¡ Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			break;
		case ID_BUTTON+3:
			MouseEvents->SetCursorID(5);
			ctrl= new JutkiCondWin(SubWinRect,ID_SUBWINDOW+3,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("º Ëb ‹¡    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+4:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+4,14,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("º    ­¡    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+5:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+5,34,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("ÂA¹·Ðbb    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+6:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+6,32,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("Ðb ‰Á ¡w    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+7:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+7,50,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("¹·    Ša    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+8:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+8,43,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("Ñi ´‚ Ñw    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+9:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+9,1,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("‰    ‹s    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+10:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+10,5,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("»¢    À‚    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+11:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+11,47,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("Ëb    ‹¡    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+12:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+12,16,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("¸a ‰b »w    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+13:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+13,23,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("‰iÑ¥‰Å‰    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+14:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+14,61,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("¸åÊ¥¢ Ða·  Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+15:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+15,60,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("¸åÊ¥¢ ¬w·  Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+16:
			MouseEvents->SetCursorID(5);
			ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+16,62,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("¸åÊÑÁ ¢…®  Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=0;
			break;
		case ID_BUTTON+17:
			if(PasswordDialog(JKRect(180,150,459,329),4,"´qÑ¡Ÿi ·³bÐa¯³¯¡µ¡").Run()==ID_OK){
				MouseEvents->SetCursorID(5);
				ctrl= new CommonCondWin(SubWinRect,ID_SUBWINDOW+17,54,*((PersonManager *)this),SearchArray,SearchCount);
				ctrl->SetText("»¡Òá‰Å¯±    Ñe Ñ×");
				AddControl(ctrl);
				ctrl->ShowWindow();
				WorkingProcess.Working=TRUE;
				WorkingProcess.context=WORK_SEARCH;
				WorkingProcess.ctrlid=ID_SUBWINDOW+2;
				WorkingProcess.index=0;
			}
			break;
		case ID_BUTTON+18:
			MouseEvents->SetCursorID(5);
			ctrl= new BirthCondWin(SubWinRect,ID_SUBWINDOW+18,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("¬— ·© ¸a    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=1;
			break;
		case ID_BUTTON+19:
			MouseEvents->SetCursorID(5);
			ctrl= new JunYokCondWin(SubWinRect,ID_SUBWINDOW+19,*((PersonManager *)this),SearchArray,SearchCount);
			ctrl->SetText("¸å µb ¸a    Ñe Ñ×");
			AddControl(ctrl);
			ctrl->ShowWindow();
			WorkingProcess.Working=TRUE;
			WorkingProcess.context=WORK_SEARCH;
			WorkingProcess.ctrlid=ID_SUBWINDOW+2;
			WorkingProcess.index=1;
			break;
		case ID_BUTTON+20:
			ChangeManagerMode(!ManagerMode);
			AdjustTitleAmount();
			break;

		case ID_BUTTON+21:
			MouseEvents->SetCursorID(5);
			CloseWindow();
			break;
		default:
			break;
	}
}

ProcessResult InsaWindow::RespondMessage(JKMSG& msg)
{
	if(!msg.winid){
		if(msg.main==ONLYMOVE||msg.main==ELONLYMOVE||msg.main==CONTROL_LEFTCLICK||msg.main==CONTROL_RIGHTCLICK){
			if(ExplainID!=-1){
				JKControl *ctrl = GetControl(ID_USERCONTROL+3);
				if(ctrl) ctrl->HideWindow();
				ExplainID=-1;
			}
		}
	}
	switch(msg.main){
		case REQUIRE_ADD:
		case REQUIRE_DELETE:
		case REQUIRE_SEARCH:
		case REQUIRE_BACKUP:
		case REQUIRE_MODIFY:

		case FINISH_ADD:
		case FINISH_DELETE:
		case FINISH_SEARCH:
		case FINISH_BACKUP:
		case FINISH_MODIFY:

		case CANCEL_ADD:
		case CANCEL_DELETE:
		case CANCEL_SEARCH:
		case CANCEL_BACKUP:
		case CANCEL_MODIFY:
			MouseEvents->SetCursorID(5);
			switch(msg.main){
				case REQUIRE_ADD:
					break;
				case REQUIRE_DELETE:
					EvRequireDelete(LOWORD(msg.sub),HIWORD(msg.sub));
					break;
				case REQUIRE_SEARCH:
					break;
				case REQUIRE_BACKUP:
					EvRequireBackup(LOWORD(msg.sub),HIWORD(msg.sub));
					break;
				case REQUIRE_MODIFY:
					break;

				case FINISH_ADD:
					EvFinishAdd();
					break;
				case FINISH_DELETE:
					EvFinishDelete();
					break;
				case FINISH_SEARCH:
					EvFinishSearch();
					break;
				case FINISH_BACKUP:
					EvFinishBackup();
					break;
				case FINISH_MODIFY:
					EvFinishModify();
					break;

				case CANCEL_ADD:
					EvCancelAdd();
					break;
				case CANCEL_DELETE:
					EvCancelDelete();
					break;
				case CANCEL_SEARCH:
					EvCancelSearch();
					break;
				case CANCEL_BACKUP:
					EvCancelBackup();
					break;
				case CANCEL_MODIFY:
					EvCancelModify();
					break;
				default:
					break;
			}
			break;
		case REDRAW_TITLE:
			AdjustTitleAmount();
			break;
		default:
			break;
	}
	return JKDialog::RespondMessage(msg);
}

void InsaWindow::ViewChange(int16 index)
{
	if(CurrentView!=index&&index<(int16)ViewCount){
		MouseEvents->SetCursorID(5);
		JKControl *ctrl;
		if(index!=-1){
			ctrl = GetControl(ViewID[index]);
			if(ctrl) ctrl->EvShowWindow();
		}
		if(CurrentView!=-1){
			ctrl = GetControl(ViewID[CurrentView]);
			if(ctrl) ctrl->EvHideWindow();
		}
		CurrentView=index;

	}
}

void InsaWindow::EvRequireDelete(WORD ctrlid,int16 index)
{
	FreeIndex(index);
	JKControl *ctrl=GetControl(ctrlid);
	if(ctrl) ctrl->SendMessage(FINISH_DELETE);
	WorkingProcess.Working=FALSE;
}

void InsaWindow::EvRequireBackup(WORD ctrlid,int16 index)
{
	BackupRecord(index);
	JKControl *ctrl=GetControl(ctrlid);
	if(ctrl) ctrl->SendMessage(FINISH_DELETE);
	WorkingProcess.Working=FALSE;
}

void InsaWindow::EvFinishAdd()
{
	JKControl *ctrl;
	if(WorkingProcess.Working){
		int16 index = AddRecord(*AddedRecord);
		ctrl= GetControl(ID_SUBWINDOW+1);
		if(!ctrl){
			ctrl= new ListWindow(SubWinRect,ID_SUBWINDOW+1,*((PersonManager *)this),SearchArray,SearchCount);
			AddControl(ctrl);
		}
		ctrl->SendMessage(FINISH_ADD,index);
		ctrl->ShowWindow();

		WorkingProcess.Working=FALSE;
	}
	ctrl = GetControl(ID_USERCONTROL+2);
	if(ctrl) {
		ctrl->SetText("");
		ctrl->RedrawClient();
	}
}

void InsaWindow::EvFinishModify()
{
	JKControl *ctrl;
	if(WorkingProcess.Working){
		if(PersonManager::SetRecord(WorkingProcess.index,*AddedRecord)){
			AdjustTitleAmount();
			ctrl= GetControl(ID_SUBWINDOW+1);
			if(!ctrl){
				ctrl= new ListWindow(SubWinRect,ID_SUBWINDOW+1,*((PersonManager *)this),SearchArray,SearchCount);
				AddControl(ctrl);
			}
			ctrl->SendMessage(FINISH_MODIFY,WorkingProcess.index);
			ctrl->ShowWindow();
		}

		WorkingProcess.Working=FALSE;
	}
	ctrl = GetControl(ID_USERCONTROL+2);
	if(ctrl) {
		ctrl->SetText("");
		ctrl->RedrawClient();
	}
}

void InsaWindow::EvFinishDelete(){}
void InsaWindow::EvFinishBackup(){}

void InsaWindow::EvFinishSearch()
{
	JKControl *ctrl;
	if(WorkingProcess.Working){
		ctrl= GetControl(ID_SUBWINDOW+1);
		if(!ctrl){
			ctrl= new ListWindow(SubWinRect,ID_SUBWINDOW+1,*((PersonManager *)this),SearchArray,SearchCount);
			AddControl(ctrl);
		}
		ctrl->SendMessage(FINISH_SEARCH,WorkingProcess.index? TRUE:FALSE);
		ctrl->ShowWindow();

		WorkingProcess.Working=FALSE;
	}
	ctrl = GetControl(ID_USERCONTROL+2);
	if(ctrl) {
		ctrl->SetText("");
		ctrl->RedrawClient();
	}
}

void InsaWindow::EvCancelAdd()
{
	JKControl *ctrl;
	if(WorkingProcess.Working){
		ctrl= GetControl(ID_SUBWINDOW+1);
		if(!ctrl){
			ctrl= new ListWindow(SubWinRect,ID_SUBWINDOW+1,*((PersonManager *)this),SearchArray,SearchCount);
			AddControl(ctrl);
		}
		ctrl->SendMessage(CANCEL_ADD);
		ctrl->ShowWindow();

		WorkingProcess.Working=FALSE;
	}
	ctrl = GetControl(ID_USERCONTROL+2);
	if(ctrl) {
		ctrl->SetText("");
		ctrl->RedrawClient();
	}
}

void InsaWindow::EvCancelModify()
{
	JKControl *ctrl;
	if(WorkingProcess.Working){
		ctrl= GetControl(ID_SUBWINDOW+1);
		if(!ctrl){
			ctrl= new ListWindow(SubWinRect,ID_SUBWINDOW+1,*((PersonManager *)this),SearchArray,SearchCount);
			AddControl(ctrl);
		}
		ctrl->SendMessage(CANCEL_MODIFY,WorkingProcess.index);
		ctrl->ShowWindow();

		WorkingProcess.Working=FALSE;
	}
	ctrl = GetControl(ID_USERCONTROL+2);
	if(ctrl) {
		ctrl->SetText("");
		ctrl->RedrawClient();
	}
}

void InsaWindow::EvCancelDelete(){}
void InsaWindow::EvCancelBackup(){}
void InsaWindow::EvCancelSearch()
{
	JKControl *ctrl;
	if(WorkingProcess.Working){
		ctrl= GetControl(ID_SUBWINDOW+1);
		if(!ctrl){
			ctrl= new ListWindow(SubWinRect,ID_SUBWINDOW+1,*((PersonManager *)this),SearchArray,SearchCount);
			AddControl(ctrl);
		}
		ctrl->SendMessage(CANCEL_SEARCH,WorkingProcess.index? TRUE:FALSE);
		ctrl->ShowWindow();

		WorkingProcess.Working=FALSE;
	}
	ctrl = GetControl(ID_USERCONTROL+2);
	if(ctrl) {
		ctrl->SetText("");
		ctrl->RedrawClient();
	}
}



//===========================================================

void InsaWindow::InitByFile()
{
	PersonManager::InitByFile();
	AdjustTitleAmount();
}


BOOL InsaWindow::FreeIndex(uint16 index)
{
	PersonRecord TempRecord;
	GetRecord(index,TempRecord);
	if(PersonManager::FreeIndex(index)){
		if(ManagerMode){
			SagoMan.DeleteSagoNode(TempRecord.GetBudae(),index);
			SagoMan.Write();
		}
		AdjustTitleAmount();
		return TRUE;
	}
	return FALSE;
}

BOOL InsaWindow::BackupRecord(uint16 index)
{
	PersonRecord TempRecord;
	GetRecord(index,TempRecord);
	if(PersonManager::BackupRecord(index)){
		if(ManagerMode){
			SagoMan.DeleteSagoNode(TempRecord.GetBudae(),index);
			SagoMan.Write();
		}
		AdjustTitleAmount();
		return TRUE;
	}
	return FALSE;
}

int16 InsaWindow::AddRecord(RecordBase& rec)
{
	int16 result=PersonManager::AddRecord(rec);
	if(result!=-1) AdjustTitleAmount();
	return result;
}

int16 InsaWindow::InsertRecord(uint16 index,RecordBase& rec)
{
	int16 result=PersonManager::InsertRecord(index,rec);
	if(result!=-1) AdjustTitleAmount();
	return result;
}

void InsaWindow::AdjustTitleAmount()
{
	MainTitle *title=(MainTitle *)GetControl(ID_USERCONTROL+1);
	if(title){
		if(ManagerMode){
			uint16 ganbucount=SagoMan.GetGanbuCount();
			uint16 sabyungcount=SagoMan.GetSabyungCount();
			title->ChangeWholeAmount(OfficerCount,RealCount-OfficerCount);
			title->ChangeSagoAmount(ganbucount,sabyungcount);
			title->ChangeCurrentAmount(OfficerCount-ganbucount,RealCount-OfficerCount-sabyungcount);
		}
		else{
			title->ChangeWholeAmount(OfficerCount,RealCount-OfficerCount);
			title->ChangeSagoAmount(0,0);
			title->ChangeCurrentAmount(OfficerCount,RealCount-OfficerCount);
		}
		title->ChangeManagerMode(ManagerMode);
	}
}


void InsaWindow::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ID_BUTTON+21;
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
