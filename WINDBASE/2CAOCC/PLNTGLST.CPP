#include "jkbutton.h"
#include "scrllist.h"
#include "yesnodlg.h"
#include "typectrl.h"
#include "selctrl.h"
#include "tgrefent.h"
#include "tgrec.h"
#include "plntgdlg.h"
#include "plntgman.h"
#include "plntglst.h"

PlanTargetListWindow::PlanTargetListWindow(JKPoint p,JKControl *parent):
	JKDialog(JKRect(0,0,600,440).OffsetBy(p),parent,ID_SUBWINDOW+1)
{
	WinAttr|=WA_FIRSTSHOW|WA_NORESTRICT|WA_TITLEMOVEABLE|WA_CHANGEPLACE;
	SetText("‰ÒB ¬a‰b Îa¸â");
	BackColor=FindNearestColor(55,55,55);
	PlanTargetMan->AddedRecord->InitData();
	InputRecord = (PlanTargetRecord *)PlanTargetMan->AddedRecord->Duplicate();
	if(!InputRecord) EventHandler::CreationError=JKWindow::CreationError=TRUE;
}

PlanTargetListWindow::~PlanTargetListWindow()
{
	if(InputRecord) delete InputRecord;
}

void PlanTargetListWindow::InitWindow()
{
	JKDialog::InitWindow();
	InitList();
}

void PlanTargetListWindow::SetupWindow()
{
	JKListBox *listbox;
	JKButton *bttn;
	bttn=new JKButton(JKRect(10,10,575,30),ID_STATIC+1);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	bttn->AdjustFlag=ADJ_YCENTER;
	bttn->SetText(GetListHeader());
	AddControl(bttn);
	ScrollListBox *scrllist=new ScrollListBox(JKRect(10,31,575,310),ID_LISTBOX+1);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);
	bttn=new JKButton(JKRect(10,311,575,330),ID_STATIC+2);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	AddControl(bttn);

	bttn=new JKButton(JKRect( 10,350,100,380),ID_BUTTON+1);
	bttn->SetText("Â ˆa");
	AddControl(bttn);
	bttn=new JKButton(JKRect(110,350,200,380),ID_BUTTON+2);
	bttn->SetText("® ¸÷");
	AddControl(bttn);
	bttn=new JKButton(JKRect(210,350,300,380),ID_BUTTON+3);
	bttn->SetText("¬b ¹A");
	AddControl(bttn);
	bttn=new JKButton(JKRect(310,350,400,380),ID_BUTTON+4);
	bttn->SetText("Â‰ b");
	AddControl(bttn);
	bttn=new JKButton(JKRect(410,350,500,380),ID_BUTTON+5);
	bttn->SetText("”h  ‹¡");
	AddControl(bttn);
}

void PlanTargetListWindow::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
}

ProcessResult PlanTargetListWindow::RespondMessage(JKMSG& msg)
{
	uint16 ctrlid;
	switch(msg.main){
		case ENTRYCTRL_ENDMODIFY:
			InitList();
			break;
	}
	return JKDialog::RespondMessage(msg);
}

void PlanTargetListWindow::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void PlanTargetListWindow::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			EvAddTarget();
			break;
		case ID_BUTTON+2:
			EvModifyTarget();
			break;
		case ID_BUTTON+3:
			EvDeleteTarget();
			break;
		case ID_BUTTON+4:
			PrintList(*PlanTargetMan);
			break;
		case ID_BUTTON+5:
			SendMessage(ID_OK);
			break;
		default:
			break;
	}
}

void PlanTargetListWindow::EvAddTarget()
{
	InputRecord->InitData();
	JKDialog *dlg=new PlanTargetDialog(*InputRecord);
	if(dlg){
    WORD result=dlg->Run(); delete dlg;
		if(result==ID_OK){
			int16 allocindex=PlanTargetMan->AddRecord(*InputRecord);
			if(allocindex>=0){
				JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
				if(listbox){
					char *buffer=CreateListString(*InputRecord);
					if(buffer) {
						listbox->Add(listbox->HuboManager::NodeCount-1,new HuboBase(buffer,allocindex,LtBlue));
						listbox->SetCurrentIndex(listbox->HuboManager::NodeCount-2);
						listbox->RedrawClient();
						delete[] buffer;
						InitListCount();
					}
				}
			}
		}
	}
}

void PlanTargetListWindow::EvModifyTarget()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		if(listbox->CurrentIndex>=0)
			EvLBSelectEntry(ID_LISTBOX+1,listbox->CurrentIndex);
	}
}

void PlanTargetListWindow::EvDeleteTarget()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		if(listbox->CurrentIndex>=0){
			HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
			if(hubo && (int16)hubo->Number>=0){
				if(YesNoDialog(JKRect(300,300,700,500),"»¡¶¯¡‰V¯s“¡Œa?").Run()==ID_YES){
					PlanTargetMan->DeleteRecord(hubo->Number);
					listbox->Destroy(listbox->CurrentIndex);
					listbox->RedrawClient();
					InitListCount();
				}
			}
		}
	}
}

char *PlanTargetListWindow::CreateListString(PlanTargetRecord& rec)
{
	TargetRecord tgrecord;
	TargetRefEntry& tgrefent=(TargetRefEntry&)*rec.GetEntry((uint16)0);
	if(!tgrefent.GetTargetRecord(tgrecord)) return 0;

	uint16 lengtharray[5],indexarray[5]={0,1,2,3,4};
	uint16 len=0;
	uint16 i;
	for(i=0;i<5;i++) {
		len+=1;
		len+=(lengtharray[i]=tgrecord.GetEntry(indexarray[i])->BufferSize);
	}
	len-=1;
	char *buffer=new char[len+1];
	buffer[len]='\0';
	memset(buffer,' ',len);
	len=0;
	for(i=0;i<5;i++) {
		tgrecord.GetData(indexarray[i],buffer+len);
		len+=lengtharray[i];
		if(i!=5-1){
			buffer[strlen(buffer)]=' ';
			buffer[len]=0;
			strcat(buffer+len," ");
			len+=1;
		}
	}
	return buffer;
}


BOOL PlanTargetListWindow::IsSearchRecord(PlanTargetRecord&)
{
	return TRUE;
}

void PlanTargetListWindow::InitList()
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();
		REFENTRY refentry;
		char *buffer;
		int16	index=PlanTargetMan->TableMan.TableHead;
		while(index!=-1&&PlanTargetMan->TableMan.GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				PlanTargetMan->RecordMan.GetRecord(index,*PlanTargetMan->AddedRecord);
				if(IsSearchRecord(*PlanTargetMan->AddedRecord)){
					buffer=CreateListString(*PlanTargetMan->AddedRecord);
					if(buffer) {
						listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue));
						delete[] buffer;
					}
				}
				index=refentry.next;
			}
			else index=-1;
		}
		listbox->Add(listbox->HuboManager::NodeCount,new HuboBase("************* < µa‹¡ˆa {·³“¡”a. > *************",-1,Gray));
		if(listbox->HuboManager::NodeCount) listbox->SetCurrentIndex(0);
		listbox->RedrawClient();
		InitListCount();
	}
}

void PlanTargetListWindow::InitListCount()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		JKControl *ctrl=GetControl(ID_STATIC+2);
		if(ctrl){
			uint16 nodecount=listbox->HuboManager::NodeCount;
			if(nodecount) nodecount--;
			char buf[20];
			sprintf(buf,"Á·Îa¸â® : %5d",nodecount);
			ctrl->SetText(buf);
			ctrl->RedrawClient();
		}
	}
}

void PlanTargetListWindow::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid!=ID_LISTBOX+1) return;

	JKListBox *listbox=(JKListBox *)GetControl(ctrlid);
	if(listbox){
		HuboBase *hubo=(*listbox)[index];
		if(hubo && (int16)hubo->Number>=0){
			PlanTargetMan->RecordMan.GetRecord(hubo->Number,*InputRecord);
			MouseEvents->SetCursorID(5);
			JKDialog *dlg=new PlanTargetDialog(*InputRecord);
			if(dlg){
				WORD result=dlg->Run();
				delete dlg;
				if(result==ID_OK){
					PlanTargetMan->SetRecord(hubo->Number,*InputRecord);
					char *buffer=CreateListString(*InputRecord);
					if(buffer) {
						listbox->ChangeHuboBase(index,new HuboBase(buffer,hubo->Number,LtBlue));
						listbox->RedrawClient();
						delete[] buffer;
					}
				}
			}
		}
	}
}


char *PlanTargetListWindow::GetListTitle()
{
	return "<< ‰   ÒB    ¬a   ‰b    Îa   ¸â       ¡¢   ¢ >>";
}

char *PlanTargetListWindow::GetListHeader()
{
	return "Îa ¸â ¹Á     Îa  Îa   ¸â    ¬÷   »©  Îa ‰¡ §¡        ‰¡";
}

BOOL PlanTargetListWindow::PrintList(PlanTargetManager& dataman)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		uint16 lineperpage=47;
		char linefeed[3]="\r\n";

		if(listbox->HuboManager::NodeCount<=1){
			if(!PrintMan->PrintString(GetListTitle()))	return FALSE;
			if(!PrintMan->PrintString(linefeed))	return FALSE;
			if(!PrintMan->PrintString(linefeed))	return FALSE;
			if(!PrintMan->PrintString("==============================================================="))
				return FALSE;
			if(!PrintMan->PrintString(linefeed)) return FALSE;
			if(!PrintMan->PrintString(GetListHeader()))
				return FALSE;
			if(!PrintMan->PrintString(linefeed)) return FALSE;
			if(!PrintMan->PrintString("---------------------------------------------------------------"))
				return FALSE;
			if(!PrintMan->PrintString(linefeed)) return FALSE;
		}
		REFENTRY refentry;
		char *buffer;
		uint16 count=0;
		HuboBase *base=listbox->HuboManager::HeadNode;
		while(base){
			int16	index=base->Number;
			if(index>=0&&dataman.TableMan.GetRefEntry(index,refentry)){
				if(refentry.HasData) {
					dataman.RecordMan.GetRecord(index,*dataman.AddedRecord);
					buffer=CreateListString(*dataman.AddedRecord);
					if(buffer) {

						if(!(count%lineperpage)){
							if(count&&base->next){
								if(!PrintMan->PrintString("---------------------------------------------------------------"))
									return FALSE;
								if(!PrintMan->PrintString(linefeed)) return FALSE;
								if(!PrintMan->PrintString("  * ”a·q ¸wµA  . . . . . ."))
									return FALSE;
								if(!PrintMan->PrintString(linefeed)) return FALSE;
								if(!PrintMan->PrintString("==============================================================="))
									return FALSE;
								if(!PrintMan->PrintString(linefeed)) return FALSE;
									if(!PrintMan->PrintCharacter(FF)) return FALSE;
							}
							if(!PrintMan->PrintString(GetListTitle()))	return FALSE;
							if(!PrintMan->PrintString(linefeed))	return FALSE;
							if(!PrintMan->PrintString(linefeed))	return FALSE;
							if(!PrintMan->PrintString("==============================================================="))
								return FALSE;
							if(!PrintMan->PrintString(linefeed)) return FALSE;
							if(!PrintMan->PrintString(GetListHeader()))
								return FALSE;
							if(!PrintMan->PrintString(linefeed)) return FALSE;
							if(!PrintMan->PrintString("---------------------------------------------------------------"))
								return FALSE;
							if(!PrintMan->PrintString(linefeed)) return FALSE;
						}

						if(!PrintMan->PrintString(buffer)) return FALSE;
						if(!PrintMan->PrintString(linefeed)) return FALSE;
						delete[] buffer;
						count++;
					}
				}
			}
			base=base->next;
		}
		if(!PrintMan->PrintString("---------------------------------------------------------------"))
			return FALSE;
		if(!PrintMan->PrintString(linefeed)) return FALSE;
		char buf[20];
		sprintf(buf,"Á· %4d",listbox->HuboManager::NodeCount-1);
		if(!PrintMan->PrintString(buf)) return FALSE;
		if(!PrintMan->PrintString(linefeed)) return FALSE;

		if(!PrintMan->PrintString("==============================================================="))
			return FALSE;
		if(!PrintMan->PrintString(linefeed)) return FALSE;
		if(!PrintMan->PrintCharacter(FF)) return FALSE;
		return TRUE;
	}
	return FALSE;
}

