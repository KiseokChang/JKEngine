#include "jkbutton.h"
#include "checkbox.h"
#include "scrllist.h"
#include "yesnodlg.h"
#include "typectrl.h"
#include "selctrl.h"
#include "battdlg.h"
#include "battman.h"
#include "battlst.h"

BattalionListDialog::BattalionListDialog(JKRect rect,JKControl *parent):
	JKDialog(rect,parent),SearchCount(0)
{
	WinAttr|=WA_FIRSTSHOW|WA_NORESTRICT|WA_TITLEMOVEABLE|WA_CHANGEPLACE;
  SetText("´aŠ… Í¡¥w ¦” ¸÷¥¡ ·©œqÎa");
	SearchCount=0;
	BackColor=FindNearestColor(55,55,55);
	BattalionMan->AddedRecord->InitData();
	CompRecord = (BattalionRecord *)BattalionMan->AddedRecord->Duplicate();
	InputRecord = (BattalionRecord *)BattalionMan->AddedRecord->Duplicate();
	if(!CompRecord||!InputRecord) EventHandler::CreationError=JKWindow::CreationError=TRUE;

}

BattalionListDialog::~BattalionListDialog()
{
	if(CompRecord) delete CompRecord;
	if(InputRecord) delete InputRecord;
}

void BattalionListDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitList();
}

void BattalionListDialog::SetupWindow()
{
	JKListBox *listbox;
	listbox=new JKListBox(JKRect(10,30,150,130),ID_LISTBOX+1);
	AddControl(listbox);
	listbox->Add(0,new HuboBase("¬w‹s¦”¥i",0));
	listbox->Add(1,new HuboBase("Š ‰w   ¥i",4));
	listbox->Add(2,new HuboBase("",-1));
	listbox->CurrentIndex=0;
	listbox=new JKListBox(JKRect(210,30,350,130),ID_LISTBOX+2);
	AddControl(listbox);
	listbox->Add(0,new HuboBase("",-1));
	listbox->CurrentIndex=0;

	JKButton *bttn;
	bttn=new JKButton(JKRect(160, 30,200, 60),ID_BUTTON+1);
	bttn->SetText(">>>");
	AddControl(bttn);
	bttn=new JKButton(JKRect(160,100,200,130),ID_BUTTON+2);
	bttn->SetText("<<<");
	AddControl(bttn);

	AddControl(new SelectEntryControl(JKRect(365, 50,575, 70),ID_USERCONTROL+1,*CompRecord->GetEntry((WORD)0)));
	AddControl(new SelectEntryControl(JKRect(365,110,575,130),ID_USERCONTROL+2,*CompRecord->GetEntry(4)));

	bttn=new JKButton(JKRect(10,150,575-18,170),ID_STATIC+1);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	bttn->AdjustFlag=ADJ_YCENTER;
	AddControl(bttn);

	//////////////////////////////////////////////////////////////////
	JKCheckBox *checkbox=new JKCheckBox(JKRect(575-17,150,575,170),ID_CHECKBOX+1);
	checkbox->Status=FALSE;
	AddControl(checkbox);
	///////////////////////////////////////////////////////////////////
	ScrollListBox *scrllist=new ScrollListBox(JKRect(10,171,575,450),ID_LISTBOX+3);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);
	bttn=new JKButton(JKRect(10,451,575,470),ID_STATIC+2);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	AddControl(bttn);

	bttn=new JKButton(JKRect( 10,490,100,520),ID_BUTTON+3);
	bttn->SetText("Â ˆa");
	AddControl(bttn);
	bttn=new JKButton(JKRect(110,490,200,520),ID_BUTTON+4);
	bttn->SetText("® ¸÷");
	AddControl(bttn);
	bttn=new JKButton(JKRect(210,490,300,520),ID_BUTTON+5);
	bttn->SetText("¬b ¹A");
	AddControl(bttn);
	bttn=new JKButton(JKRect(310,490,400,520),ID_BUTTON+6);
	bttn->SetText("Â‰ b");
	AddControl(bttn);
	bttn=new JKButton(JKRect(410,490,500,520),ID_BUTTON+7);
	bttn->SetText("”h ‹¡");
	AddControl(bttn);

}

void BattalionListDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(10,10,150,30),"q·e Ðw¡¢ :",ADJ_YCENTER);
	dc.TextOutX(JKRect(210,10,350,30),"¬åÈ‚–E Ðw¡¢ :",ADJ_YCENTER);

	dc.TextOutX(JKRect(365,30,575, 50),"¬w‹s¦”¡w",ADJ_YCENTER);
	dc.TextOutX(JKRect(365,90,575,110),"Š      ‰w",ADJ_YCENTER);

	JKRect rect=ClientRect.SetLeftTopBy();
	dc.HLine(JKPoint(0,140),rect.Width()+1,LtGray);
	dc.HLine(JKPoint(0,141),rect.Width()+1,Black);
	dc.HLine(JKPoint(0,142),rect.Width()+1,White);
}

ProcessResult BattalionListDialog::RespondMessage(JKMSG& msg)
{
	uint16 ctrlid;
	switch(msg.main){
		case ENTRYCTRL_ENDMODIFY:
			InitList();
			break;
	}
	return JKDialog::RespondMessage(msg);
}

void BattalionListDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void BattalionListDialog::EvCheckToggle(WORD /*ctrlid*/,BOOL /*status*/)
{
	InitList();
}

void BattalionListDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
		case ID_BUTTON+2:
			{
			JKListBox *listbox1=(JKListBox *)GetControl(ctrlid==ID_BUTTON+1? ID_LISTBOX+1:ID_LISTBOX+2);
			JKListBox *listbox2=(JKListBox *)GetControl(ctrlid==ID_BUTTON+1? ID_LISTBOX+2:ID_LISTBOX+1);
			if(!listbox1 || !listbox2) return;
			int16 index1=listbox1->CurrentIndex;
			int16 index2=listbox2->CurrentIndex;
			if(index1<0) return;
			HuboBase *hubo=(*listbox1)[index1];
			if(!hubo || (int16)hubo->Number<0) return;
			listbox2->Add(index2,hubo->Duplicate());
			listbox1->Destroy(index1);
			listbox1->RedrawClient();
			listbox2->RedrawClient();
			InitList();
			}
			break;
		case ID_BUTTON+3:
			EvAddTarget();
			break;
		case ID_BUTTON+4:
			EvModifyTarget();
			break;
		case ID_BUTTON+5:
			EvDeleteTarget();
			break;
		case ID_BUTTON+6:
			PrintList(*BattalionMan);
			break;
		case ID_BUTTON+7:
			SendMessage(ID_OK);
			break;
		default:
			break;
	}
}

BOOL BattalionListDialog::PreSaveTarget(BattalionRecord& rec)
{
	int16 index=BattalionMan->Search(&rec,1);
	if(index>=0 && BattalionMan->RecordMan.GetRecord(index,*BattalionMan->WorkRecord)){
		char buff[80],tmpbuf[6];
		BattalionMan->WorkRecord->GetData(1,tmpbuf);
		sprintf(buff," %5s ¦”“e ·¡£¡ —w¢–A´á ·¶¯s“¡”a.\n\n‹¡¹¥ ¸÷¥¡Ÿi ˆ—¯¥Ðs“¡Œa?",tmpbuf);
		if(YesNoDialog(JKRect(300,300,700,500),buff).Run()==ID_YES){
			BattalionMan->SetRecord(index,rec);

			JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
			if(listbox){
				char *buffer=CreateListString(rec);
				if(buffer) {
					int16 listindex=listbox->GetIndexByNumber(index);
					if(listindex>=0){
						listbox->ChangeHuboBase(listindex,new HuboBase(buffer,index,LtBlue));
						listbox->SetCurrentIndex(listindex);
						listbox->RedrawClient();
					}
					delete[] buffer;
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

void BattalionListDialog::EvAddTarget()
{
	InputRecord->InitData();
	JKDialog *dlg=new BattalionDialog(JKPoint(200,220),*InputRecord);
	if(dlg){
		if(dlg->Run()==ID_OK){
			if(!PreSaveTarget(*InputRecord)) {
				int16 allocindex=BattalionMan->AddRecord(*InputRecord);
				if(allocindex>=0){
					JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
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
		delete dlg;
	}
}

void BattalionListDialog::EvModifyTarget()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox){
		if(listbox->CurrentIndex>=0)
			EvLBSelectEntry(ID_LISTBOX+3,listbox->CurrentIndex);
	}
}

void BattalionListDialog::EvDeleteTarget()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox){
		if(listbox->CurrentIndex>=0){
			HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
			if(hubo && (int16)hubo->Number>=0){
				if(YesNoDialog(JKRect(300,300,700,500),"»¡¶¯¡‰V¯s“¡Œa?").Run()==ID_YES){
					BattalionMan->DeleteRecord(hubo->Number);
					listbox->Destroy(listbox->CurrentIndex);
					listbox->RedrawClient();
					InitListCount();
				}
			}
		}
	}
}

char *BattalionListDialog::CreateListString(BattalionRecord& rec)
{
	char *buffer=0;
	if(GetListKind()){
		uint16 lengtharray[7],indexarray[7]={1,4,5,6,7,8,9};
		uint16 len=0;
		uint16 i;
		for(i=0;i<7;i++) {
      len+=4;
			len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
		}
    len-=4;
		buffer=new char[len+1];
		buffer[len]='\0';
		memset(buffer,' ',len);
		len=0;
		for(i=0;i<7;i++) {
			rec.GetData(indexarray[i],buffer+len);
			len+=lengtharray[i];
			if(i!=7-1){
				buffer[strlen(buffer)]=' ';
				buffer[len]=0;
        strcat(buffer+len,"    ");
        len+=4;
			}
		}
	}
	else{
		uint16 lengtharray[6],indexarray[6]={0,1,2,3,4,10};
		uint16 len=0;
		uint16 i;
		for(i=0;i<6;i++) {
			len+=3;
			len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
		}
		len-=3;
		buffer=new char[len+1];
		buffer[len]='\0';
		memset(buffer,' ',len);
		len=0;
		for(i=0;i<6;i++) {
			rec.GetData(indexarray[i],buffer+len);
			len+=lengtharray[i];
			if(i!=6-1){
				buffer[strlen(buffer)]=' ';
				buffer[len]=0;
				strcat(buffer+len,"   ");
				len+=3;
			}
		}
	}
	return buffer;
}


BOOL BattalionListDialog::IsSearchRecord(BattalionRecord& rec)
{
	if(!SearchCount || !CompRecord->Compare(&rec,SearchArray,SearchCount))
		return TRUE;
	return FALSE;
}

void BattalionListDialog::InitList()
{
	JKControl *ctrl=GetControl(ID_STATIC+1);
	if(ctrl) {
		ctrl->SetText(GetListHeader());
		ctrl->RedrawClient();
	}

	SearchCount=0;
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+2);
	if(listbox){
		HuboBase *hubo=listbox->HuboManager::HeadNode;
		while(hubo){
			int16 number=hubo->Number;
			if(number>=0) SearchArray[SearchCount++]=number;
			hubo=hubo->next;
		}
	}
////////////////////////////////////////////
	listbox = (JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox) {
		listbox->DestroyAll();
		REFENTRY refentry;
		char *buffer;
		int16	index=BattalionMan->TableMan.TableHead;
		while(index!=-1&&BattalionMan->TableMan.GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				BattalionMan->RecordMan.GetRecord(index,*BattalionMan->AddedRecord);
				if(IsSearchRecord(*BattalionMan->AddedRecord)){
					buffer=CreateListString(*BattalionMan->AddedRecord);
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

void BattalionListDialog::InitListCount()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox){
		JKControl *ctrl=GetControl(ID_STATIC+2);
		if(ctrl){
			uint16 nodecount=listbox->HuboManager::NodeCount;
			if(nodecount) nodecount--;
			char buf[20];
			sprintf(buf,"Á· ¦”® : %5d",nodecount);
			ctrl->SetText(buf);
			ctrl->RedrawClient();
		}
	}
}

void BattalionListDialog::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid!=ID_LISTBOX+3) return;

	JKListBox *listbox=(JKListBox *)GetControl(ctrlid);
	if(listbox){
		HuboBase *hubo=(*listbox)[index];
		if(hubo && (int16)hubo->Number>=0){
			BattalionMan->RecordMan.GetRecord(hubo->Number,*InputRecord);
			MouseEvents->SetCursorID(5);
			JKDialog *dlg=new BattalionDialog(JKPoint(200,220),*InputRecord);
			if(dlg){
				WORD result=dlg->Run();
				delete dlg;
				if(result==ID_OK){
					BattalionMan->SetRecord(hubo->Number,*InputRecord);
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

BOOL BattalionListDialog::GetListKind()
{
	JKCheckBox *checkbox=(JKCheckBox *)GetControl(ID_CHECKBOX+1);
	if(checkbox) return checkbox->Status;
	return FALSE;
}

char *BattalionListDialog::GetListTitle()
{
	return "<<  ´a     Š…       Í¡      ¥w         ¡¢        ¢>>";
}

char *BattalionListDialog::GetListHeader()
{
	if(GetListKind())
    return "¦”¡w   Š     ‰w   Í¡¢…®  ¥wb(Ñe/·¥ˆa)   Èe´¢(Ñe/·¥ˆa)";
	else
		return "¬w‹s¦”  ¦”¡w  C  A   ¹Á      Îa   Š     ‰w   ¦”¬wÑ×";
}

BOOL BattalionListDialog::PrintList(BattalionManager& dataman)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
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
