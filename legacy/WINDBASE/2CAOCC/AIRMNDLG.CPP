
#include "fltbttn.h"
#include "scrllist.h"
#include "airdlg.h"
#include "airman.h"
#include "airmndlg.h"

AirManDialog::AirManDialog(JKPoint p,JKControl *parent):
	JKDialog(JKRect(0,0,600,480).OffsetBy(p),parent)
{
	WinAttr|=WA_FIRSTSHOW|WA_TITLEMOVEABLE;
	WinForm=0x03;
	BackColor=LtGray;
	SetText("‰·µb É·¹A ‰ÅŸ¡");
	InputRecord = (AirRecord *)AirMan->AddedRecord->Duplicate();
	if(!InputRecord) JKWindow::CreationError=EventHandler::CreationError=TRUE;
}

AirManDialog::~AirManDialog()
{
	if(InputRecord) delete InputRecord;
}

void AirManDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitList();
}

void AirManDialog::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy();

	rect.Expand(-20);
	rect.top=10;
	rect.bottom=140;
	AddControl(new AirDialog(rect,*InputRecord));

	rect.top=160;
	rect.bottom=rect.top+20;
	JKStatic *stc = new JKStatic(rect,0);
	stc->SetText(GetListHeader());
	stc->BackColor=Gray;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_YCENTER;
	AddControl(stc);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+200;
	ScrollListBox *scrllist=new ScrollListBox(rect,ID_LISTBOX+1);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);

	JKButton *bttn;
	rect.top=rect.bottom+20;
	rect.bottom=rect.top+30;

	rect.right=rect.left+100;
	bttn=new JKFloatingButton(rect,ID_BUTTON+1);
	bttn->SetText("Âˆa");
	AddControl(bttn);

	rect.left=rect.right+10;
	rect.right=rect.left+100;
	bttn=new JKFloatingButton(rect,ID_BUTTON+2);
	bttn->SetText("®¸÷");
	AddControl(bttn);

	rect.left=rect.right+10;
	rect.right=rect.left+100;
	bttn=new JKFloatingButton(rect,ID_BUTTON+3);
	bttn->SetText("¬b¹A");
	AddControl(bttn);

	rect.left=rect.right+10;
	rect.right=rect.left+100;
	bttn=new JKFloatingButton(rect,ID_BUTTON+4);
	bttn->SetText("Â‰b");
	AddControl(bttn);

	rect.left=rect.right+10;
	rect.right=rect.left+100;
	bttn=new JKFloatingButton(rect,ID_BUTTON+5);
	bttn->SetText("aˆa‹¡");
	AddControl(bttn);
}

void AirManDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	SetFocus();
}

void AirManDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			AirMan->AddRecord(*InputRecord);
			InitList();
			break;
		case ID_BUTTON+2:
			ModifyRecord();
			break;
		case ID_BUTTON+3:
			DeleteRecord();
			break;
		case ID_BUTTON+4:
			PrintList(*AirMan);
			break;
		case ID_BUTTON+5:
			SendMessage(ID_OK);
			break;
		default:
			break;
	}
}

void AirManDialog::EvLBTouchEntry(WORD ctrlid,uint16 index)
{
	JKListBox *listbox=(JKListBox *)GetControl(ctrlid);
	if(listbox){
		AirDialog *airdlg=(AirDialog *)GetControl(ID_SUBWINDOW+1);
		if(airdlg){
			HuboBase *hubo=(*listbox)[index];
			if(hubo && (int16)hubo->Number!=-1){
				AirMan->RecordMan.GetRecord(hubo->Number,*InputRecord);
			}
			else{
				InputRecord->InitData();
			}
			airdlg->ReLoad();
		}
	}
}

void AirManDialog::ModifyRecord()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
		if(hubo && (int16)hubo->Number!=-1){
			if(AirMan->SetRecord(hubo->Number,*InputRecord))
				InitList();
		}
	}
}

void AirManDialog::DeleteRecord()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
		if(hubo && (int16)hubo->Number!=-1){
			if(AirMan->DeleteRecord(hubo->Number))
				InitList();
		}
	}
}

char *AirManDialog::CreateListString(AirRecord& rec)
{
	uint16 lengtharray[6],indexarray[6]={0,1,2,3,4,5};
	uint16 len=0;
	uint16 i;
	for(i=0;i<6;i++) {
		len+=3;
		len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
	}
	len-=3;
	char *buffer=new char[len+1];
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
	return buffer;
}


void AirManDialog::InitList()
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();

		REFENTRY refentry;
		char *buffer;
		int16	index=AirMan->TableMan.TableHead;
		while(index!=-1&&AirMan->TableMan.GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				AirMan->RecordMan.GetRecord(index,*AirMan->AddedRecord);
				buffer=CreateListString((AirRecord&)*AirMan->AddedRecord);
				if(buffer) {
					listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue));
					delete[] buffer;
				}
				index=refentry.next;
			}
			else index=-1;
		}

		listbox->Add(listbox->HuboManager::NodeCount,new HuboBase("************* < µa‹¡ˆa {·³“¡”a. > *************",-1,Gray));
		if(listbox->HuboManager::NodeCount) listbox->SetCurrentIndex(0);
		listbox->RedrawClient();

		JKStatic *stc = (JKStatic *)GetControl(ID_STATIC+1);
		if(stc) {
			char buf[11];
			sprintf(buf,"Á·¶¥ : %3d",listbox->HuboManager::NodeCount-1);
			stc->SetText(buf);
			stc->RedrawClient();
		}

	}
}

char *AirManDialog::GetListTitle()
{
  return "<<  ‰·     µb     É·     ¹A       ¡¢     ¢>>";
}

char *AirManDialog::GetListHeader()
{
	return "¹Á    Îa 1   ¹Á    Îa 2   ¹Á    Îa 3    ¹Á    Îa 4  ÂA¸á‰¡•¡ ÂA‰¡‰¡•¡";
}

BOOL AirManDialog::PrintList(AirManager& dataman)
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
