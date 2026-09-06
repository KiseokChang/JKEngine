#include "fltbttn.h"
#include "scrllist.h"
#include "yesnodlg.h"
#include "cmdmsgbx.h"
#include "passdlg.h"
#include "persnwin.h"
#include "insamsg.h"
#include "listwin.h"

ListWindow::ListWindow(JKRect rect,WORD ctrlid,PersonManager& recman,WORD *& srcharray,uint16& srchcount):
	JKWindow(rect,0,ctrlid),RecordMan(recman),SearchArray(srcharray),SearchCount(srchcount)
{
	WinForm=0x01;
	BackColor=LtGray;
}

ListWindow::~ListWindow()
{}

void ListWindow::SetupWindow()
{
	JKRect rect;
	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

	rect.top+=10;
	rect.bottom=rect.top+20;
	rect.left+=20;
	rect.right-=200;
	JKButton *stc=new JKButton(rect,0);
	stc->SetEnable(FALSE);
	stc->SetText(" ­¡   ­¢     ¬÷    ¡w    ‰‹s    Š…   ¤å     ºËb‹¡   ·³ ” ·©     ¥¡ »¢ ·©     ¬—    ·© ");
	stc->AdjustFlag=ADJ_YCENTER;
	stc->BackColor=Gray;
	stc->TextColor=White;
	AddControl(stc);

	rect.top=rect.bottom+3;
	rect.bottom=rect.top+550;
	ScrollListBox *listbox=new ScrollListBox(rect,ID_LISTBOX+1);
	listbox->DragScrollFlag=TRUE;
	AddControl(listbox);

	rect.top=rect.bottom+5;
	rect.bottom=rect.top+20;
	JKButton *bttn=new JKButton(rect,ID_STATIC+1);
	bttn->SetEnable(FALSE);
	bttn->Depth=-1;
	bttn->TextColor=LtMagenta;
	bttn->SetText("Á·¶¥ :");
	bttn->AdjustFlag=ADJ_YCENTER;
	AddControl(bttn);

	uint16 YInterval=20,Height=30;
	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=rect.right-180;
	rect.right-=20;
	rect.top+=10;

	rect.bottom=rect.top+Height;
	if(RecordMan.ManagerMode){
		bttn=new JKFloatingButton(rect,ID_BUTTON+1);
		bttn->SetText("¯¥‹A·³b");
		AddControl(bttn);
	}

	rect.top=rect.bottom+YInterval;
	rect.bottom=rect.top+Height;
	bttn=new JKFloatingButton(rect,ID_BUTTON+2);
	bttn->SetText("­A¦ˆñ¬‚");
	AddControl(bttn);

	rect.top=rect.bottom+YInterval;
	rect.bottom=rect.top+Height;
	bttn=new JKFloatingButton(rect,ID_BUTTON+3);
	if(RecordMan.ManagerMode)
		bttn->SetText("®    ¸÷");
	else
		bttn->SetText("¥¡    ‹¡");
	AddControl(bttn);

	rect.top=rect.bottom+YInterval;
	rect.bottom=rect.top+Height;
	bttn=new JKFloatingButton(rect,ID_BUTTON+4);
	bttn->SetText("¬b    ¹A");
	AddControl(bttn);

	rect.top=rect.bottom+YInterval;
	rect.bottom=rect.top+Height;
	bttn=new JKFloatingButton(rect,ID_BUTTON+5);
	bttn->SetText("Â‰    b");
	AddControl(bttn);

	rect.top=rect.bottom+YInterval;
	rect.bottom=rect.top+Height;
	bttn=new JKFloatingButton(rect,ID_BUTTON+6);
	bttn->SetText("¸÷    i");
	AddControl(bttn);

	rect.top=rect.bottom+YInterval;
	rect.bottom=rect.top+Height;
	bttn=new JKFloatingButton(rect,ID_BUTTON+7);
	bttn->SetText("a ˆa ‹¡");
	AddControl(bttn);

}

void ListWindow::EvShowWindow()
{
	JKWindow::EvShowWindow();
	JKControl *ctrl=GetControl(ID_LISTBOX+1);
	if(ctrl) ctrl->SetFocus();
}

void ListWindow::EvButtonClick(WORD ctrlid)
{
	JKListBox *listbox;
	HuboBase *hubo;
	switch(ctrlid){
		case ID_BUTTON+1:
			if(RecordMan.ManagerMode) {
				MouseEvents->SetCursorID(5);
				PersonWindow *prsnwin= new PersonWindow(RecordMan,(PersonRecord& )*RecordMan.AddedRecord,-1,SearchArray,SearchCount,TRUE,FALSE,this);
				if(prsnwin){
					if(prsnwin->Run()==ID_OK){
						int16 index = RecordMan.AddRecord(*RecordMan.AddedRecord);
						if(index>=0){
							SendControlMessage(REDRAW_TITLE);
							SendMessage(FINISH_ADD,index);
						}
					}
					else SendMessage(CANCEL_ADD);
					delete prsnwin;
				}
			}
			break;
		case ID_BUTTON+2:
			{
			MouseEvents->SetCursorID(5);
			PersonWindow *prsnwin= new PersonWindow(RecordMan,(PersonRecord& )*RecordMan.AddedRecord,-1,SearchArray,SearchCount,TRUE,FALSE,this);
			if(prsnwin){
				if(prsnwin->Run()==ID_OK){
					SendMessage(FINISH_SEARCH);
				}
				else SendMessage(CANCEL_SEARCH);
				delete prsnwin;
			}
			}
			break;
		case ID_BUTTON+3:
			listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
			if(listbox&&listbox->CurrentIndex!=-1)
				EvLBSelectEntry(ID_LISTBOX+1,listbox->CurrentIndex);
			break;
		case ID_BUTTON+4:
			listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
			if(listbox&&listbox->CurrentIndex!=-1){
				hubo=(*listbox)[listbox->CurrentIndex];
				if(hubo && (int16)hubo->Number!=-1){
					if(RecordMan.ManagerMode){
						YesNoDialog msgdlg(JKRect(140,150,500,330),"< ¬b¹A Ò >\n¸åµb¸a ÑÁ·©µA ý·a¯¡‰V¯s“¡Œa?");
						msgdlg.SetText("¬b¹A   ¬åÈ‚    Àw");
						WORD result=msgdlg.Run();
						if(result==ID_YES) SendControlMessage(REQUIRE_BACKUP,hubo->Number);
						else if(result==ID_NO) SendControlMessage(REQUIRE_DELETE,hubo->Number);
					}
					else{
						YesNoDialog msgdlg(JKRect(140,150,500,330),"¬b¹AÐe Ò                                \n"
																											 "·¡ ·¥¶¥µA ”Ðe ¸aža“e ¥¢ŠÐi ® ´ô¯s“¡”a.\n\n\n"
																																	 "ÑÂ¯©Ðs“¡Œa?");
						msgdlg.SetText("¬b¹A   ÑÂ·¥    Àw");
						WORD result=msgdlg.Run();
						if(result==ID_YES) SendControlMessage(REQUIRE_DELETE,hubo->Number);
					}
				}
			}
			break;
		case ID_BUTTON+5:
			PrintList();
			break;
		case ID_BUTTON+6:
      {
			JKControl *ctrl=new ComandMessageBox(JKRect(350,300,650,400),0,0x0100);
			ctrl->SetText("¸q¯¡ ‹¡”aa º¯³¯¡µ¡!\n¸÷i º— .....");
			ctrl->BackColor=LtMagenta;
			ctrl->TextColor=Black;
			AddControl(ctrl);
			}
			break;
		case ID_BUTTON+7:
			CloseWindow();
			break;
		default:
			break;
	}
}

char *ListWindow::CreateListString(PersonRecord& rec)
{
	uint16 lengtharray[7],indexarray[7]={2,0,1,3,4,6,7};
	uint16 len=0;
	uint16 i;
	for(i=0;i<7;i++) {
		len+=3;
		len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
	}
	len += 10;
	char *buffer=new char[len+1];
	buffer[len]='\0';
	memset(buffer,' ',len);
	len=0;
	for(i=0;i<7;i++) {
		rec.GetData(indexarray[i],buffer+len);
		len+=lengtharray[i];

		buffer[strlen(buffer)]=' ';
		buffer[len]=0;
		strcat(buffer+len,"   ");
		len+=3;
	}

	struct date TempDate;
	getdate(&TempDate);
	rec.GetBirthDay(TempDate,TRUE,TempDate.da_year);
	sprintf(buffer+len,"%4d.%2d.%2d",TempDate.da_year,TempDate.da_mon,TempDate.da_day);
	return buffer;
}

void ListWindow::InitList()
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();
		//=====================================
		REFENTRY refentry;
		char *buffer;
		int16	index=RecordMan.TableHead;
		while(index!=-1&&RecordMan.GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				RecordMan.GetRecord(index,*RecordMan.AddedRecord);
				buffer=CreateListString(*RecordMan.AddedRecord);
				if(buffer) {
					listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue));
					delete[] buffer;
				}
				index=refentry.next;
			}
			else index=-1;
		}
		//====================================
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

void ListWindow::SortView(uint16 index)
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox&&listbox->HuboManager::NodeCount>1) {
		HuboManager *huboman=new HuboManager();
		if(huboman){
			HuboBase *hubo;
			uint16& count=listbox->HuboManager::NodeCount;
			uint16 *IndexArray=new WORD[count];
			uint16 ArrayCount=0;
			while(count){
				hubo=listbox->HuboManager::GetNodeFirst(TRUE);
				if(hubo){
					if((int16)hubo->Number>=0)
						IndexArray[ArrayCount++]=hubo->Number;
					huboman->PutNodeLast(hubo);
				}
			}

			if(ArrayCount){
				RecordMan.Sort(IndexArray,ArrayCount,index);

				IndexArray[ArrayCount++]=-1;
				int16 tempindex;
				for(uint16 i=0;i<ArrayCount;i++){
					tempindex=huboman->GetIndexByNumber(IndexArray[i]);
					if(tempindex>=0){
						hubo=huboman->GetNode(tempindex,TRUE);
						if(hubo) listbox->HuboManager::PutNodeLast(hubo);
					}

				}
			}

			delete[] IndexArray;
			delete huboman;
			listbox->RedrawWindow();
		}
	}
}

void ListWindow::SortFile(uint16 index)
{
	if(RecordMan.FileSort(index)) InitList();
}

void ListWindow::SortView(WORD *compoder,uint16 ordercount)
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox&&listbox->HuboManager::NodeCount>1) {
		HuboManager *huboman=new HuboManager();
		if(huboman){
			HuboBase *hubo;
			uint16 *IndexArray=new WORD[listbox->HuboManager::NodeCount];
			uint16 ArrayCount=0;
			while(listbox->HuboManager::NodeCount){
				hubo=listbox->HuboManager::GetNodeFirst(TRUE);
				if(hubo){
					if((int16)hubo->Number>=0)
						IndexArray[ArrayCount++]=hubo->Number;
					huboman->PutNodeLast(hubo);
				}
			}

			if(ArrayCount){
				RecordMan.Sort(IndexArray,ArrayCount,compoder,ordercount);

				IndexArray[ArrayCount++]=-1;
				int16 tempindex;
				for(uint16 i=0;i<ArrayCount;i++){
					tempindex=huboman->GetIndexByNumber(IndexArray[i]);
					if(tempindex>=0){
						hubo=huboman->GetNode(tempindex,TRUE);
						if(hubo) listbox->HuboManager::PutNodeLast(hubo);
					}

				}
			}

			delete[] IndexArray;
			delete huboman;
		}
	}
}

void ListWindow::SortFile(WORD *compoder,uint16 ordercount)
{
	if(RecordMan.FileSort(compoder,ordercount)) InitList();
}


void ListWindow::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	JKListBox *listbox=(JKListBox *)GetControl(ctrlid);
	if(listbox){
		HuboBase *hubo=(*listbox)[index];
		if(hubo && (int16)hubo->Number!=-1){
			BOOL enmodify=FALSE;
			if(RecordMan.ManagerMode)
				enmodify=TRUE;

			WORD result=PasswordDialog(JKRect(180,150,459,329),6,"»¡Òá‰Å¯± Ðw¡¢·i ¥¡a¡e\n"
																													 "             ´qÑ¡Ÿi ·³bÐa¯¡µ¡.").Run();
			MouseEvents->SetCursorID(5);
			PersonWindow *prsnwin= new PersonWindow(RecordMan,(PersonRecord& )*RecordMan.AddedRecord,hubo->Number,SearchArray,SearchCount,enmodify,result==ID_OK? TRUE:FALSE,this);
			if(prsnwin){
				if(prsnwin->Run()==ID_OK){
					if(RecordMan.SetRecord(hubo->Number,*RecordMan.AddedRecord)){
						SendControlMessage(REDRAW_TITLE);
						SendMessage(FINISH_MODIFY,hubo->Number);
					}
				}
				else SendMessage(CANCEL_MODIFY,hubo->Number);
				delete prsnwin;
			}
		}
	}
}

void ListWindow::EvLBCharDown(WORD ctrlid,uint16 index,uint16 key,WORD /*modifier*/)
{
	if(key==ENTER) EvLBSelectEntry(ctrlid,index);
}

void ListWindow::EvLBKeyDown(WORD ctrlid,uint16 index,uint16 key,WORD /*modifier*/)
{
	JKListBox *listbox=(JKListBox *)GetControl(ctrlid);
	HuboBase *hubo;
	if(listbox){
		if(key==DELKEY){
			hubo=(*listbox)[index];
			if(hubo && (int16)hubo->Number!=-1)
				SendControlMessage(REQUIRE_DELETE,hubo->Number);
		}
	}
}

void ListWindow::EvCommand(WORD cmdid,WORD,DWORD,WORD)
{
	if(cmdid==0x100) {
		MouseEvents->HideCursor();
		SortFile(1);
		MouseEvents->ShowCursor();
	}
}

ProcessResult ListWindow::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case FINISH_ADD:
		case CANCEL_ADD:
		case FINISH_MODIFY:
		case CANCEL_MODIFY:
		case FINISH_DELETE:
		case CANCEL_SEARCH:
		case FINISH_SEARCH:
      {
			JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
			if(listbox) listbox->SetFocus();
			switch(msg.main){
				case FINISH_ADD:
					EvFinishAdd(LOWORD(msg.sub));
					break;
				case CANCEL_ADD:
				case CANCEL_SEARCH:
					break;
				case CANCEL_MODIFY:
				case FINISH_MODIFY:
					EvFinishModify(LOWORD(msg.sub));
					break;
				case FINISH_DELETE:
					EvFinishDelete(LOWORD(msg.sub));
					break;
				case FINISH_BACKUP:
					EvFinishBackup(LOWORD(msg.sub));
					break;
				case FINISH_SEARCH:
					EvFinishSearch(LOWORD(msg.sub));
					break;
			}
			}
			break;
		default:
			break;
	}
	return JKWindow::RespondMessage(msg);
}

void ListWindow::EvFinishModify(uint16 index)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		RecordMan.GetRecord(index,*RecordMan.AddedRecord);
		char *buffer=CreateListString(*RecordMan.AddedRecord);
		if(buffer) {
			int16 order=listbox->GetIndexByNumber(index);
			HuboBase *hubo=(*listbox)[order];
			if(hubo){
				hubo->SetText(buffer);
				hubo->Number=index;
				listbox->SetCurrentIndex(order);
				listbox->RedrawClient();
			}
			delete[] buffer;
		}
	}
}

void ListWindow::EvFinishAdd(uint16 index)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		RecordMan.GetRecord(index,*RecordMan.AddedRecord);
		char *buffer=CreateListString(*RecordMan.AddedRecord);
		if(buffer) {
			listbox->Add(listbox->HuboManager::NodeCount-1,new HuboBase(buffer,index,LtBlue));
			delete[] buffer;
		}
		listbox->SetCurrentIndex(listbox->HuboManager::NodeCount-2);
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

void ListWindow::EvFinishDelete(uint16 /*index*/)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		listbox->Destroy(listbox->CurrentIndex);
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

void ListWindow::EvFinishBackup(uint16 /*index*/)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		listbox->Destroy(listbox->CurrentIndex);
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

void ListWindow::EvFinishSearch(BOOL isscope)
{
	JKControl *ctrl;
	ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->CloseWindow();
	ctrl=GetControl(ID_BUTTON+2);
	if(ctrl) ctrl->CloseWindow();
	ctrl=GetControl(ID_BUTTON+6);
	if(ctrl) ctrl->CloseWindow();

	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();
		//=====================================
		REFENTRY refentry;
		char *buffer;
		int16 index=-1;
		if(!isscope){
			while((index=RecordMan.Search(RecordMan.AddedRecord,SearchArray,SearchCount,index))>=0){
				RecordMan.GetRefEntry(index,refentry);
				if(refentry.HasData) {
					RecordMan.GetRecord(index,*RecordMan.WorkRecord);
					buffer=CreateListString((PersonRecord&)*RecordMan.WorkRecord);
					if(buffer) {
						listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue));
						delete[] buffer;
					}
				}
			}
		}
		else{
			RecordBase *CompRecord = RecordMan.WorkRecord->CreateEqualRecord();
			if(CompRecord){
				while((index=RecordMan.SearchScope(CompRecord,RecordMan.AddedRecord,SearchArray,SearchCount,index))>=0){
					RecordMan.GetRefEntry(index,refentry);
					if(refentry.HasData) {
						RecordMan.GetRecord(index,*RecordMan.WorkRecord);
						buffer=CreateListString((PersonRecord&)*RecordMan.WorkRecord);
						if(buffer) {
							listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue));
							delete[] buffer;
						}
					}
				}
				delete CompRecord;
			}
		}
		listbox->Add(listbox->HuboManager::NodeCount,new HuboBase("************* < µa‹¡ˆa {·³“¡”a. > *************",-1,Gray));
		if(listbox->HuboManager::NodeCount) listbox->SetCurrentIndex(0);
		listbox->RedrawClient();

		JKStatic *stc = (JKStatic *)GetControl(ID_STATIC+1);
		if(stc) {
			char buf[20];
			sprintf(buf,"Á·¶¥ : %4d",listbox->HuboManager::NodeCount-1);
			stc->SetText(buf);
			stc->RedrawClient();
		}
	}
}


BOOL ListWindow::PrintList()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		uint16 lineperpage=47;
		char linefeed[3]="\r\n";

		REFENTRY refentry;
		char *buffer;
		uint16 count=0;
		HuboBase *base=listbox->HuboManager::HeadNode;
		while(base){
			int16	index=base->Number;
			if(index>=0&&RecordMan.GetRefEntry(index,refentry)){
				if(refentry.HasData) {
					RecordMan.GetRecord(index,*RecordMan.AddedRecord);
					buffer=CreateListString(*RecordMan.AddedRecord);
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
							if(!PrintMan->PrintString("  <<    ·¥      ¶¥           ¡¢      ¢    >>"))	return FALSE;
							if(!PrintMan->PrintString(linefeed))	return FALSE;
							if(!PrintMan->PrintString(linefeed))	return FALSE;
							if(!PrintMan->PrintString("==============================================================="))
								return FALSE;
							if(!PrintMan->PrintString(linefeed)) return FALSE;
							if(!PrintMan->PrintString(" ­¡   ­¢     ¬÷    ¡w    ‰‹s    Š…   ¤å     ºËb‹¡    ·³ ” ·©"))
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
		sprintf(buf,"Á· %4d ¡w·³“¡”a.",listbox->HuboManager::NodeCount-1);
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
