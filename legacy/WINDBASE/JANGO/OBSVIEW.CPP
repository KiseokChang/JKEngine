#include "okdialog.h"
#include "fltbttn.h"
#include "jkedit.h"
#include "typectrl.h"
#include "insamsg.h"
#include "obsview.h"

ObserveView::ObserveView(JKRect rect,WORD ctrlid,PersonManager& recman,PersonRecord& rec,int16 currec,JKControl *parent):
	JKDialog(rect,parent,ctrlid),RecordMan(recman),RefRecord(rec),CurrentIndex(currec),ObserveIndex(-1),
	Date(1)
{
	WinForm=0x03;
	WinAttr|=WA_TITLEMOVEABLE;
	SetText("¡e  ”q		¯¥  ¬w");

	if(CurrentIndex>=0) {
		RecordMan.GetRecord(CurrentIndex,RefRecord);
		ObserveIndex=RecordMan.ObserveMan.FindTail(RefRecord.ObserveIndex);
	}
	else EventHandler::CreationError=TRUE;
}

void ObserveView::SetupWindow()
{
	JKRect rect;
	JKEdit *editor;

	rect= ClientRect.SetLeftTopBy(JKPoint(0,0)).ExpandBy(-5);
	rect.left=rect.right-90;
	rect.bottom=25;
	TypeEntryControl *stc= new TypeEntryControl(rect,ID_USERCONTROL+1,Date);
	stc->TextColor=Black;
	stc->BackColor=BackColor;
	AddControl(stc);

	rect= ClientRect.SetLeftTopBy(JKPoint(0,0)).ExpandBy(-5);
	rect.top   =26;
	rect.bottom=195;
	editor=new JKEdit(rect,ID_USERCONTROL+2,0,2000,TRUE);
	editor->TextColor=White;
	editor->BackColor=LtBlue;
	AddControl(editor);

	rect.top   =220;
	rect.bottom=310;
	editor=new JKEdit(rect,ID_USERCONTROL+3,0,1000,TRUE);
	editor->TextColor=White;
	editor->BackColor=LtBlue;
	AddControl(editor);

	JKButton *bttn;
	rect=JKRect(  5,330,60,355);
	bttn=new JKFloatingButton(rect,ID_BUTTON+1);
	bttn->SetText("—w¢");
	AddControl(bttn);
	rect.left=rect.right+5;
	rect.right=rect.left+55;
	bttn=new JKFloatingButton(rect,ID_BUTTON+2);
	bttn->SetText("¥e‰w");
	AddControl(bttn);
	rect.left=rect.right+5;
	rect.right=rect.left+55;
	bttn=new JKFloatingButton(rect,ID_BUTTON+3);
	bttn->SetText("¬b¹A");
	AddControl(bttn);
	rect.left=rect.right+5;
	rect.right=rect.left+55;
	bttn=new JKFloatingButton(rect,ID_BUTTON+4);
	bttn->SetText("<<<");
	AddControl(bttn);
	rect.left=rect.right+5;
	rect.right=rect.left+55;
	bttn=new JKFloatingButton(rect,ID_BUTTON+5);
	bttn->SetText(">>>");
	rect.left=rect.right+5;
	rect.right=rect.left+55;
	AddControl(bttn);
	rect.left=rect.right+5;
	rect.right=rect.left+55;
	bttn=new JKFloatingButton(rect,ID_BUTTON+6);
	bttn->SetText("Â‰b");
	AddControl(bttn);
	rect.left=rect.right+5;
	rect.right=rect.left+75;
	bttn=new JKFloatingButton(rect,ID_BUTTON+7);
	bttn->SetText("¸åÁAÂ‰b");
	AddControl(bttn);
	rect.left=rect.right+5;
	rect.right=rect.left+55;
	bttn=new JKFloatingButton(rect,ID_BUTTON+8);
	bttn->SetText("{‹¡");
	AddControl(bttn);

	ReLoad();
}

void ObserveView::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect;

	dc.SetTextColor(Black);
	dc.SetBackColor(BackColor);
	rect=JKRect(5,5,95,25);
	dc.TextOutX(rect,"¡e”q¶w :",ADJ_YCENTER);
	rect=JKRect(5,200,260,220);
	dc.TextOutX(rect,"¹¡Ã¡£¹ ¸b¬÷¸a :",ADJ_YCENTER);
}

void ObserveView::EvCtrlLeftClick(WORD ctrlid)
{
	if(CurrentIndex>=0){
		JKControl *ctrl=GetControl(ctrlid);
		if(ctrl){
			ctrl->SendMessage(EDIT_BEGIN);
		}
	}
}

void ObserveView::EvButtonClick(WORD ctrlid)
{
	int16 index;
	switch(ctrlid){
		case ID_BUTTON+1:
			if(ObserveIndex>=0){
				ObserveIndex=-1;
				ReLoad();
			}
			else{
				ObserveIndex=RecordMan.ObserveMan.FindTail(RefRecord.ObserveIndex);
				ReLoad();
			}
			break;
		case ID_BUTTON+2:
			SaveChange();
			break;
		case ID_BUTTON+3:
			if(ObserveIndex>=0){
				OBSREFENTRY refentry;
				if(RecordMan.ObserveMan.GetRefEntry(ObserveIndex,refentry)){
					index = ObserveIndex;
					int16 prev = refentry.prev;
					int16 next = refentry.next;
					RecordMan.ObserveMan.FreeIndex(index);
					if(next>=0){
						ObserveIndex=next;
					}
					else if(prev>=0){
						ObserveIndex=prev;
					}
					else {
						ObserveIndex=-1;
					}
					if(RefRecord.ObserveIndex==index){
						RefRecord.ObserveIndex=ObserveIndex;
						RecordMan.SetRecord(CurrentIndex,RefRecord);
					}
					ReLoad();
				}
			}
			break;
		case ID_BUTTON+4:
			if(ObserveIndex>=0){
				index=RecordMan.ObserveMan.GetPrevIndex(ObserveIndex);
				if(index>=0){
					ObserveIndex=index;
					ReLoad();
				}
			}
			break;
		case ID_BUTTON+5:
			if(ObserveIndex>=0){
				index=RecordMan.ObserveMan.GetNextIndex(ObserveIndex);
				if(index>=0){
					ObserveIndex=index;
					ReLoad();
				}
			}
			break;
		case ID_BUTTON+6:
			if(ObserveIndex>=0){
				if(RecordMan.ObserveMan.PrintObserveData(ObserveIndex))
					PrintMan->PrintCharacter(FF);
			}
			else{
				if(PrintObserveData())
					PrintMan->PrintCharacter(FF);
			}
			break;
		case ID_BUTTON+7:
			if(ObserveIndex>=0&&RecordMan.ObserveMan.PrintAll(ObserveIndex))
				PrintMan->PrintCharacter(FF);
			break;
		case ID_BUTTON+8:
			CloseWindow();
			break;
		default:
			break;
	}
}

BOOL ObserveView::PrintObserveData()
{
	JKControl *ctrl1=GetControl(ID_USERCONTROL+1);
	if(ctrl1) ctrl1=ctrl1->GetControl(ID_EDIT+1);
	JKControl *ctrl2=GetControl(ID_USERCONTROL+2);
	JKControl *ctrl3=GetControl(ID_USERCONTROL+3);
	if(ctrl1&&ctrl2&&ctrl3){
		if(!PrintMan->PrintString("< ¡e”q ·©¸a >\r\n")) return FALSE;
		if(!PrintMan->PrintString(ctrl1->GetText())) return FALSE;
		if(!PrintMan->PrintString("\r\n\r\n")) return FALSE;
		if(!PrintMan->PrintString("< ¡e”q ¶w >\r\n")) return FALSE;
		if(!PrintMan->PrintString(ctrl2->GetText())) return FALSE;
		if(!PrintMan->PrintString("\r\n\r\n")) return FALSE;
		if(!PrintMan->PrintString("< ¹¡Ã¡£¹ ¸b¬÷¸a >\r\n")) return FALSE;
		if(!PrintMan->PrintString(ctrl3->GetText())) return FALSE;
		if(!PrintMan->PrintString("\r\n\r\n")) return FALSE;
		return TRUE;
	}
	return FALSE;
}

void ObserveView::EvDeadKeyDown(uint16 /*key*/,WORD /*modifier*/)
{
//	EvDeadKeyDown(key,modifier);
}

void ObserveView::EvKeyDown(uint16 key,WORD )
{
	switch(key){
		case INSKEY:
			SendMessage(BUTTON_CLICK,ID_BUTTON+1);
			break;
		case HOMEKEY:
			SendMessage(BUTTON_CLICK,ID_BUTTON+2);
			break;
		case DELKEY:
			SendMessage(BUTTON_CLICK,ID_BUTTON+3);
			break;
		case LEFTARROW:
			SendMessage(BUTTON_CLICK,ID_BUTTON+4);
			break;
		case RIGHTARROW:
			SendMessage(BUTTON_CLICK,ID_BUTTON+5);
			break;
		case ALT_O:
			SendMessage(BUTTON_CLICK,ID_BUTTON+6);
			break;
		case ALT_X:
			CloseWindow();
			break;
		default:
			break;
	}
}

void ObserveView::ReLoad()
{
	TypeEntryControl *typectrl=(TypeEntryControl *)GetControl(ID_USERCONTROL+1);
	JKEdit *editor1=(JKEdit *)GetControl(ID_USERCONTROL+2);
	JKEdit *editor2=(JKEdit *)GetControl(ID_USERCONTROL+3);
	JKButton *bttn1=(JKButton *)GetControl(ID_BUTTON+1);
	JKButton *bttn2=(JKButton *)GetControl(ID_BUTTON+2);
	if(typectrl&&editor1&&editor2&&bttn1&&bttn2){
		int16 index=ObserveIndex;
		if(index<0){
			Date.SetCurrentDay();
			typectrl->ReLoad();
			editor1->SetText("");
			editor2->SetText("");
			bttn1->SetText("Âá­¡");
			bttn2->SetText("Âˆa");
		}
		else{
			OBSREFENTRY refentry;
			RecordMan.ObserveMan.GetRefEntry(index,refentry);
			uint16 totalsize=refentry.size;
			if(totalsize){
				BYTE *TempBuffer = new BYTE[totalsize+3];
				BYTE *dateptr=&TempBuffer[0];
				BYTE *obsptr=0;
				BYTE *treatptr=0;
				RecordMan.ObserveMan.GetObserveData(index,dateptr,obsptr,treatptr);
				Date.SetData(dateptr);
				typectrl->ReLoad();
				editor1->SetText(obsptr);
				editor2->SetText(treatptr);
				delete[] TempBuffer;
			}
			bttn1->SetText("—w¢");
			bttn2->SetText("¥e‰w");
		}
		RedrawWindow();
	}
}

BOOL ObserveView::SaveData()
{
	TypeEntryControl *typectrl=(TypeEntryControl *)GetControl(ID_USERCONTROL+1);
	JKEdit *editor1=(JKEdit *)GetControl(ID_USERCONTROL+2);
	JKEdit *editor2=(JKEdit *)GetControl(ID_USERCONTROL+3);
	if(typectrl&&editor1&&editor2){
		uint16 datesize=10;
		uint16 obssize=editor1->GetTextSize();
		uint16 treatsize=editor2->GetTextSize();
		if(!(obssize+treatsize)) return FALSE;
		uint16 totalsize=datesize+obssize+treatsize;
		int16 index=ObserveIndex;
		BOOL ReallocSucess=TRUE;
		if(index<0){
			index=ObserveIndex=RecordMan.ObserveMan.AllocIndex(totalsize+sizeof(uint16)*2);
			if(index>=0){
				if(RefRecord.ObserveIndex>=0){
					OBSREFENTRY refentry;
					RecordMan.ObserveMan.GetRefEntry(index,refentry);
					int16 previndex=RecordMan.ObserveMan.FindTail(RefRecord.ObserveIndex);
					refentry.prev= previndex;
					refentry.next= -1;
					RecordMan.ObserveMan.PutRefEntry(index,refentry);

					RecordMan.ObserveMan.GetRefEntry(previndex,refentry);
					refentry.next= index;
					RecordMan.ObserveMan.PutRefEntry(previndex,refentry);
				}
				else {
					RefRecord.ObserveIndex=index;
				}
			}
		}
		else {
			ReallocSucess=RecordMan.ObserveMan.ReallocIndex(index,totalsize+sizeof(uint16)*2);
		}

		if(index>=0 && ReallocSucess){
			char *TempBuffer = new char[totalsize+3];
			char *dateptr=&TempBuffer[0];
			char *obsptr=&TempBuffer[datesize+1];
			char *treatptr=&TempBuffer[datesize+obssize+2];

			Date.GetData(dateptr);
			editor1->GetText(obsptr,obssize);
			editor2->GetText(treatptr,treatsize);

			RecordMan.ObserveMan.PutObserveData(index,dateptr,obsptr,treatptr);
			delete[] TempBuffer;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL ObserveView::SaveChange()
{
	if(CurrentIndex>=0){
		int16 index1=RefRecord.ObserveIndex;
		int16 index2=ObserveIndex;
		if(SaveData()){
			if(index1<0)
				RecordMan.SetRecord(CurrentIndex,RefRecord);
			if(index2<0)
				ReLoad();
			return TRUE;
		}
	}
	return FALSE;
}
