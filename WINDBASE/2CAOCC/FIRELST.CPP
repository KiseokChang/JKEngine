#include "jkbutton.h"
#include "checkbox.h"
#include "scrllist.h"
#include "yesnodlg.h"
#include "typectrl.h"
#include "selctrl.h"
#include "firedlg.h"
#include "firelst.h"

FireListDialog::FireListDialog(JKPoint& p,JKControl *parent):
	JKDialog(JKRect(0,0,600,610).OffsetBy(p),parent),
	MinDate(8,""),MaxDate(8,""),MinTime(9,""),MaxTime(9,"")
{
	WinAttr|=WA_FIRSTSHOW|WA_NORESTRICT|WA_TITLEMOVEABLE|WA_CHANGEPLACE;
	SetText("¬a‰b ÑeÑ× ·©œqÎa");
	BackColor=FindNearestColor(55,55,55);
	FireMan->AddedRecord->InitData();
	CompRecord = (FireRecord *)FireMan->AddedRecord->Duplicate();
	InputRecord = (FireRecord *)FireMan->AddedRecord->Duplicate();
	if(!CompRecord||!InputRecord) EventHandler::CreationError=JKWindow::CreationError=TRUE;
}

FireListDialog::~FireListDialog()
{
	if(CompRecord) delete CompRecord;
	if(InputRecord) delete InputRecord;
}
void FireListDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitList();
}

void FireListDialog::SetupWindow()
{
	JKListBox *listbox;
	listbox=new JKListBox(JKRect(10,30,150,130),ID_LISTBOX+1);
	AddControl(listbox);
	listbox->Add(0,new HuboBase("¬a‰b¦”¥i",0));
	listbox->Add(1,new HuboBase("Îa¸â¤åÑ¡¥i",1));
	listbox->Add(2,new HuboBase("Îa¸â¬÷»©¥i",3));
	listbox->Add(3,new HuboBase("¬a‰b·©¯¡¥i",8));
	listbox->Add(4,new HuboBase("**************",-1));
	listbox->CurrentIndex=0;
	listbox=new JKListBox(JKRect(210,30,350,130),ID_LISTBOX+2);
	AddControl(listbox);
	listbox->Add(0,new HuboBase("**************",-1));
	listbox->CurrentIndex=0;

	JKButton *bttn;
	bttn=new JKButton(JKRect(160, 30,200, 60),ID_BUTTON+1);
	bttn->SetText(">>>");
	AddControl(bttn);
	bttn=new JKButton(JKRect(160,100,200,130),ID_BUTTON+2);
	bttn->SetText("<<<");
	AddControl(bttn);

	AddControl(new TypeEntryControl(  JKRect(470, 31,575, 50),ID_USERCONTROL+1,*CompRecord->GetEntry((WORD)0)));
	AddControl(new TypeEntryControl(  JKRect(470, 51,575, 70),ID_USERCONTROL+2,*CompRecord->GetEntry(1)));
	AddControl(new SelectEntryControl(JKRect(380, 91,575,110),ID_USERCONTROL+3,*CompRecord->GetEntry(3)));

	AddControl(new TypeEntryControl(  JKRect(325,141,430,160),ID_USERCONTROL+4,MinDate));
	AddControl(new TypeEntryControl(  JKRect(325,161,430,180),ID_USERCONTROL+5,MinTime));
	AddControl(new TypeEntryControl(  JKRect(470,141,575,160),ID_USERCONTROL+6,MaxDate));
	AddControl(new TypeEntryControl(  JKRect(470,161,575,180),ID_USERCONTROL+7,MaxTime));

	bttn=new JKButton(JKRect(10,200,575-18,220),ID_STATIC+1);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	bttn->AdjustFlag=ADJ_YCENTER;
	AddControl(bttn);
	//////////////////////////////////////////////////////////////////
	JKCheckBox *checkbox=new JKCheckBox(JKRect(575-17,200,575,220),ID_CHECKBOX+1);
	checkbox->Status=FALSE;
	AddControl(checkbox);
	///////////////////////////////////////////////////////////////////
	ScrollListBox *scrllist=new ScrollListBox(JKRect(10,221,575,500),ID_LISTBOX+3);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);
	bttn=new JKButton(JKRect(10,501,575,520),ID_STATIC+2);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	AddControl(bttn);

	bttn=new JKButton(JKRect( 10,540,100,570),ID_BUTTON+3);
	bttn->SetText("Â ˆa");
	AddControl(bttn);
	bttn=new JKButton(JKRect(110,540,200,570),ID_BUTTON+4);
	bttn->SetText("® ¸÷");
	AddControl(bttn);
	bttn=new JKButton(JKRect(210,540,300,570),ID_BUTTON+5);
	bttn->SetText("¬b ¹A");
	AddControl(bttn);
	bttn=new JKButton(JKRect(310,540,400,570),ID_BUTTON+6);
	bttn->SetText("Â‰ b");
	AddControl(bttn);
	bttn=new JKButton(JKRect(410,540,500,570),ID_BUTTON+7);
	bttn->SetText("”h ‹¡");
	AddControl(bttn);
}

void FireListDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
	dc.TextOutX(JKRect(10,10,150,30),"q·e Ðw¡¢ :",ADJ_YCENTER);
	dc.TextOutX(JKRect(210,10,350,30),"¬åÈ‚–E Ðw¡¢ :",ADJ_YCENTER);


	dc.TextOutX(JKRect(365,30,465, 50),"¦  ”  ¡w",ADJ_YCENTER);
	dc.TextOutX(JKRect(365,51,465, 70),"Îa¸â  ¤åÑ¡",ADJ_YCENTER);
	dc.TextOutX(JKRect(365,71,575, 90),"Îa¸â  ¬÷»©",ADJ_YCENTER);

	dc.TextOutX(JKRect(220,141,320,160),"¬a‰b  ·©¸a",ADJ_YCENTER);
	dc.TextOutX(JKRect(220,161,320,180),"¬a‰b  ¯¡ˆe",ADJ_YCENTER);
	dc.TextOutX(JKRect(430,141,470,160),"~",ADJ_XYCENTER);
	dc.TextOutX(JKRect(430,161,470,180),"~",ADJ_XYCENTER);

	JKRect rect=ClientRect.SetLeftTopBy();
	dc.HLine(JKPoint(0,190),rect.Width()+1,LtGray);
	dc.HLine(JKPoint(0,191),rect.Width()+1,Black);
	dc.HLine(JKPoint(0,192),rect.Width()+1,White);
}

ProcessResult FireListDialog::RespondMessage(JKMSG& msg)
{
	uint16 ctrlid;
	switch(msg.main){
		case ENTRYCTRL_ENDMODIFY:
			InitList();
			break;
	}
	return JKDialog::RespondMessage(msg);
}

void FireListDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void FireListDialog::EvCheckToggle(WORD /*ctrlid*/,BOOL /*status*/)
{
	InitList();
}

void FireListDialog::EvButtonClick(WORD ctrlid)
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
			PrintList(*FireMan);
			break;
		case ID_BUTTON+7:
			SendMessage(ID_OK);
			break;
		default:
			break;
	}
}

void FireListDialog::EvAddTarget()
{
	InputRecord->InitData();
	JKDialog *dlg=new FireDialog(*InputRecord);
	if(dlg){
		WORD result=dlg->Run();
		delete dlg;
		if(result==ID_OK){
			int16 allocindex=FireMan->AddRecord(*InputRecord);
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
}

void FireListDialog::EvModifyTarget()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox){
		if(listbox->CurrentIndex>=0)
			EvLBSelectEntry(ID_LISTBOX+3,listbox->CurrentIndex);
	}
}

void FireListDialog::EvDeleteTarget()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox){
		if(listbox->CurrentIndex>=0){
			HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
			if(hubo && (int16)hubo->Number>=0){
				if(YesNoDialog(JKRect(300,300,700,500),"»¡¶¯¡‰V¯s“¡Œa?").Run()==ID_YES){
					FireMan->DeleteRecord(hubo->Number);
					listbox->Destroy(listbox->CurrentIndex);
					listbox->RedrawClient();
					InitListCount();
				}
			}
		}
	}
}

char *FireListDialog::CreateListString(FireRecord& rec)
{
	char *buffer=0;
	if(GetListKind()){
		uint16 lengtharray[6],indexarray[6]={0,1,2,5,6,7};
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
	else{
		uint16 lengtharray[6],indexarray[6]={0,1,2,3,8,9};
		uint16 len=0;
		uint16 i;
		for(i=0;i<6;i++) {
			len+=1;
			len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
		}
		len-=1;
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
				strcat(buffer+len," ");
				len+=1;
			}
		}
	}
	return buffer;
}

int16 FireListDialog::CompareDateTime(FireRecord& rec,DateEntry *dateent,TimeEntry *timeent)
{
	int16 result=rec.Compare(dateent);
	if(!result) result=rec.Compare(timeent);
	return result;
}

BOOL FireListDialog::IsTimeScope(FireRecord& rec)
{
	int16 comp1 = CompareDateTime(rec,&MinDate,&MinTime);
	int16 comp2 = CompareDateTime(rec,&MaxDate,&MaxTime);
	if(comp1>=0&&comp1!=2&&comp2<=0) return TRUE;
	return FALSE;
}

void FireListDialog::InitList()
{
	JKControl *ctrl=GetControl(ID_STATIC+1);
	if(ctrl) {
		ctrl->SetText(GetListHeader());
		ctrl->RedrawClient();
	}

	uint16 searcharray[4];
	uint16 searchcount=0;
	BOOL iscomptime=FALSE;

	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+2);
	if(listbox){
		HuboBase *hubo=listbox->HuboManager::HeadNode;
		while(hubo){
			int16 number=hubo->Number;
			if(number>=0) {
				if(number==8) iscomptime=TRUE;
				else searcharray[searchcount++]=number;
			}
			hubo=hubo->next;
		}
	}
/////////////////////////////////////////////////////////////////////////
	listbox = (JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox) {
		listbox->DestroyAll();
		FireRecord& firerec= *FireMan->AddedRecord;
		uint32 TotalBalsu=0;
		REFENTRY refentry;
		char *buffer;
		int16	index=FireMan->TableMan.TableHead;
		while(index!=-1&&FireMan->TableMan.GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				FireMan->RecordMan.GetRecord(index,firerec);
				if(!searchcount || !CompRecord->Compare(&firerec,searcharray,searchcount)){
					if(!iscomptime||IsTimeScope(firerec)){
						buffer=CreateListString(firerec);
						if(buffer) {
							int32 balsu;
							if(firerec.GetBalsu(balsu)) TotalBalsu+=balsu;

							listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue));
							delete[] buffer;
						}
					}
				}
				index=refentry.next;
			}
			else index=-1;
		}
		listbox->Add(listbox->HuboManager::NodeCount,new HuboBase("************* < µa‹¡ˆa {·³“¡”a. > *************",-1,Gray));
		if(listbox->HuboManager::NodeCount) listbox->SetCurrentIndex(0);
		listbox->RedrawClient();
		InitListCount(TRUE,TotalBalsu);
	}
}

void FireListDialog::InitListCount(BOOL showbalsu,uint32 balsu)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+3);
	if(listbox){
		JKControl *ctrl=GetControl(ID_STATIC+2);
		if(ctrl){
			uint16 nodecount=listbox->HuboManager::NodeCount;
			if(nodecount) nodecount--;
			char buf[100];
			if(showbalsu)	sprintf(buf," Á· ¬a‰b® : %5d    Á· ¤i® : %d ¤i",nodecount,balsu);
			else sprintf(buf," Á· ¬a‰b® : %5d",nodecount);
			ctrl->SetText(buf);
			ctrl->RedrawClient();
		}
	}
}

void FireListDialog::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid!=ID_LISTBOX+3) return;

	JKListBox *listbox=(JKListBox *)GetControl(ctrlid);
	if(listbox){
		HuboBase *hubo=(*listbox)[index];
		if(hubo && (int16)hubo->Number>=0){
			FireMan->RecordMan.GetRecord(hubo->Number,*InputRecord);
			MouseEvents->SetCursorID(5);
			JKDialog *dlg=new FireDialog(*InputRecord);
			if(dlg){
				WORD result=dlg->Run();
				delete dlg;
				if(result==ID_OK){
					FireMan->SetRecord(hubo->Number,*InputRecord);
					char *buffer=CreateListString(*InputRecord);
					if(buffer) {
						listbox->ChangeHuboBase(index,new HuboBase(buffer,hubo->Number,LtBlue));
            delete[] buffer;
						listbox->RedrawClient();
						InitListCount();
					}
				}
			}
		}
	}
}

void FireListDialog::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			ctrl=GetControl(ctrlid);
			if(ctrl) {
				ctrl= ctrl->next? ctrl->next : HeadNode;
				if(ctrl) ctrl->SetFocus();
			}
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			ctrl=GetControl(ctrlid);
			if(ctrl) {
				ctrl= ctrl->prev? ctrl->prev : TailNode;
				if(ctrl) ctrl->SetFocus();
			}
		}
	}
}

BOOL FireListDialog::GetListKind()
{
	JKCheckBox *checkbox=(JKCheckBox *)GetControl(ID_CHECKBOX+1);
	if(checkbox) return checkbox->Status;
	return FALSE;
}

char *FireListDialog::GetListTitle()
{
  return "<< ¬a    ‰b     Ñe    Ñ×        ¡¢        ¢ >>";
}

char *FireListDialog::GetListHeader()
{
	if(GetListKind())
    return "¦ ”   Îa ¸â   ¹Á      Îa   Èe      ¹·   ¤i®   ¯¥      ‰Å";
	else
		return "¦ ” Îa ¸â ¹Á    Îa   Îa  ¸â   ¬÷  »©     ¬a‰b ·©¸a  ¬a‰b ¯¡ˆe";
}

BOOL FireListDialog::PrintList(FireManager& dataman)
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

		uint32 TotalBalsu=0;
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
						int32 balsu;
						if(dataman.AddedRecord->GetBalsu(balsu)) TotalBalsu+=balsu;
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
    sprintf(buf,"Á· %4d       Á· ¤i® : %d ¤i",listbox->HuboManager::NodeCount-1,TotalBalsu);
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


