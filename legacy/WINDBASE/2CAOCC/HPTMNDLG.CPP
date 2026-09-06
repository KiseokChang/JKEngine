#include "fltbttn.h"
#include "scrllist.h"
#include "boolent.h"
#include "tgrefent.h"
#include "tgrec.h"
#include "hptdlg.h"
#include "hptman.h"
#include "hptmndlg.h"

HPTManDialog::HPTManDialog(JKPoint p,HPTRecord& inputrec,JKControl *parent):
	JKDialog(JKRect(0,0,700,480).OffsetBy(p),parent),InputRecord(inputrec)
{
	WinAttr|=WA_FIRSTSHOW|WA_TITLEMOVEABLE;
	WinForm=0x03;
	BackColor=LtGray;
	SetText("Ð‚¯± Îa¸â  ‰ÅŸ¡");
}

HPTManDialog::~HPTManDialog()
{}

void HPTManDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitList();
}

void HPTManDialog::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy();

	rect.Expand(-20);
	AddControl(new HPTDialog(rect.LeftTop(),InputRecord,-1));

	JKRect temp=rect;
	temp.left=300;
	temp.bottom=temp.top+20;
	JKStatic *stc = new JKStatic(temp,0);
	stc->SetText(GetListHeader());
	stc->BackColor=Gray;
	stc->TextColor=White;
	stc->AdjustFlag=ADJ_YCENTER;
	AddControl(stc);

	temp.top=temp.bottom+5;
	temp.bottom=temp.top+340;
	ScrollListBox *scrllist=new ScrollListBox(temp,ID_LISTBOX+1);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);

	JKButton *bttn;
	temp.top=temp.bottom+20;
	temp.bottom=temp.top+30;

	temp.left=rect.left;
	temp.right=temp.left+100;
	bttn=new JKFloatingButton(temp,ID_BUTTON+1);
	bttn->SetText("Â  ˆa");
	AddControl(bttn);

	temp.left=temp.right+10;
	temp.right=temp.left+100;
	bttn=new JKFloatingButton(temp,ID_BUTTON+2);
	bttn->SetText("®  ¸÷");
	AddControl(bttn);

	temp.left=temp.right+10;
	temp.right=temp.left+100;
	bttn=new JKFloatingButton(temp,ID_BUTTON+3);
	bttn->SetText("¬b  ¹A");
	AddControl(bttn);

	temp.left=temp.right+10;
	temp.right=temp.left+100;
	bttn=new JKFloatingButton(temp,ID_BUTTON+4);
	bttn->SetText("¬a  ‰b");
	AddControl(bttn);

	temp.left=temp.right+10;
	temp.right=temp.left+100;
	bttn=new JKFloatingButton(temp,ID_BUTTON+5);
	bttn->SetText("Â‰  b");
	AddControl(bttn);

	temp.left=temp.right+10;
	temp.right=temp.left+100;
	bttn=new JKFloatingButton(temp,ID_BUTTON+6);
	bttn->SetText("aˆa‹¡");
	AddControl(bttn);
}

void HPTManDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	SetFocus();
}

void HPTManDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			HPTMan->AddRecord(InputRecord);
			InitList();
			break;
		case ID_BUTTON+2:
			ModifyRecord();
			break;
		case ID_BUTTON+3:
			DeleteRecord();
			break;
		case ID_BUTTON+4:
			{
			JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
			if(listbox&&listbox->CurrentIndex>=0){
				HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
				if(hubo && (int16)hubo->Number!=-1){
					ReturnValue=hubo->Number;
					CloseWindow();
				}
			}
			}
			break;
		case ID_BUTTON+5:
			PrintList(*HPTMan);
			break;
		case ID_BUTTON+6:
			SendMessage(ID_OK);
			break;
		default:
			break;
	}
}

void HPTManDialog::EvLBTouchEntry(WORD ctrlid,uint16 index)
{
	JKListBox *listbox=(JKListBox *)GetControl(ctrlid);
	if(listbox){
		HPTDialog *hptdlg=(HPTDialog *)GetControl(ID_SUBWINDOW+1);
		if(hptdlg){
			HuboBase *hubo=(*listbox)[index];
			if(hubo && (int16)hubo->Number!=-1){
				HPTMan->RecordMan.GetRecord(hubo->Number,InputRecord);
			}
			else{
				InputRecord.InitData();
			}
			hptdlg->ReLoad();
		}
	}
}

void HPTManDialog::ModifyRecord()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
		if(hubo && (int16)hubo->Number!=-1){
			if(HPTMan->SetRecord(hubo->Number,InputRecord))
				InitList();
		}
	}
}

void HPTManDialog::DeleteRecord()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
		if(hubo && (int16)hubo->Number!=-1){
			if(HPTMan->DeleteRecord(hubo->Number))
				InitList();
		}
	}
}

char *HPTManDialog::CreateListString(HPTRecord& rec)
{
	char *buffer=new char[43+1];
	memset(buffer,' ',43);
	buffer[43]=0;

	TargetRecord tgrecord;
	TargetRefEntry& tgrefent=(TargetRefEntry&)*rec.GetEntry((uint16)0);
	if(tgrefent.GetTargetRecord(tgrecord)){
		tgrecord.GetData(2,buffer);
		buffer[20]=' ';
		tgrecord.GetData(1,buffer+20+1);
		buffer[20+1+10]=' ';
		tgrecord.GetData(5,buffer+20+1+10+1);
		buffer[20+1+10+1+3]='%';
		buffer[20+1+10+1+4]=' ';
	}
	BooleanEntry& hptent=(BooleanEntry&)*rec.GetEntry(2);
  if(hptent.DataBoolean) strcpy(buffer+20+1+10+1+4+1,"‰¡ˆaÃ¡");
  else strcpy(buffer+20+1+10+1+4+1,"Ð‚  ¯±");
	return buffer;
}


void HPTManDialog::InitList()
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();

		REFENTRY refentry;
		char *buffer;
		int16	index=HPTMan->TableMan.TableHead;
		while(index!=-1&&HPTMan->TableMan.GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				HPTMan->RecordMan.GetRecord(index,*HPTMan->AddedRecord);
				buffer=CreateListString(*HPTMan->AddedRecord);
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

char *HPTManDialog::GetListTitle()
{
  return "<< Ð‚    ¯±     Îa     ¸â       ¡¢     ¢ >>";
}

char *HPTManDialog::GetListHeader()
{
	return " Îa  ¸â  ¬÷  »©      ¹Á     Îa ¸åÊb ‰¡ˆaÃ¡";
}

BOOL HPTManDialog::PrintList(HPTManager& dataman)
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

