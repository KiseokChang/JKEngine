#include "jkapp.h"
#include "resbttn.h"
#include "fltbttn.h"
#include "jkedit.h"
#include "scrllist.h"
#include "inwonctl.h"
#include "insamsg.h"
#include "sagowin.h"

SagoWindow::SagoWindow(JKRect rect,WORD ctrlid,SagoManager& recman,WORD *& srcharray,uint16& srchcount):
	JKWindow(rect,0,ctrlid),CondEntryID(24),RecordMan(recman),SearchArray(srcharray),SearchCount(srchcount),
	CurrentMan(0),SelectLine(-1),SelectKind(-1)
{
	SagoTextLen=8;
	CurrentMan.InwonTextLen=SagoTextLen;
	WinForm=0x01;
	BackColor=LtGray;
}

SagoWindow::~SagoWindow()
{}

void SagoWindow::SetupWindow()
{
	JKButton *bttn;
	bttn=new JKButton(JKRect(20,10,340,27),0);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	bttn->SetText("Ñe ¸   ·¥ ¶¥");
	AddControl(bttn);

	bttn=new JKButton(JKRect(410,10,600,27),0);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	bttn->SetText("¬a ‰¡   ·¥ ¶¥");
	AddControl(bttn);

	ScrollListBox * scrllist;
	scrllist = new ScrollListBox(JKRect(20,30,340,140),ID_LISTBOX+1);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);
	scrllist = new ScrollListBox(JKRect(410,30,600,140),ID_LISTBOX+2);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);

	InwonControl *inwonctrl= new InwonControl(JKPoint(20,160),ID_USERCONTROL+0x0100,SagoTextLen,&CurrentMan);
	inwonctrl->SetText(WinText);
	AddControl(inwonctrl);

	JKRect rect;

	rect.Set(500,280,600,310);
	bttn=new JKFloatingButton(rect,ID_BUTTON+1);
	bttn->SetText("¥¡    ‹¡");
	AddControl(bttn);

	rect.Set(500,330,600,360);
	bttn=new JKFloatingButton(rect,ID_BUTTON+2);
	bttn->SetText("a ˆa ‹¡");
	AddControl(bttn);

	rect.Set(500,240,600,260);
	JKEdit *edit=new JKEdit(rect,ID_EDIT+1,0,SagoTextLen,FALSE);
	AddControl(edit);


	rect.Set(364,60,380,75);
	bttn=new JKResButton(rect,ID_BUTTON+3,InternImageID[ArrowRight]);
	AddControl(bttn);
	rect.Set(364,90,380,105);
	bttn=new JKResButton(rect,ID_BUTTON+4,InternImageID[ArrowLeft]);
	AddControl(bttn);

}

void SagoWindow::PaintClient(JKDC& dc,JKRect&)
{
	dc.Rectangle3D(JKRect(10,5,610,145),-1);
	dc.Rectangle3D(JKRect(10,155,610,365),-1);

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.HLine(JKPoint(0,150),rect.Width()+1,Gray);
	dc.HLine(JKPoint(0,151),rect.Width()+1,White);
}

void SagoWindow::EvShowWindow()
{
	MouseEvents->SetCursorID(5);
	JKWindow::EvShowWindow();
	InitTable();
	InitList();
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) listbox->SetFocus();
}

void SagoWindow::EvButtonClick(WORD ctrlid)
{
	InwonControl *inwonctrl;
	SagoManager& sagoman=RecordMan;
	switch(ctrlid){
		case ID_BUTTON+1:
			break;
		case ID_BUTTON+2:
			CloseWindow();
			break;
		case ID_BUTTON+3:
		case ID_BUTTON+4:
			{
			JKListBox *list1=(JKListBox *)GetControl(ID_LISTBOX+1);
			JKListBox *list2=(JKListBox *)GetControl(ID_LISTBOX+2);
			JKEdit *edit=(JKEdit *)GetControl(ID_EDIT+1);
			if(list1&&list2&&edit){
				JKListBox *listbox = (ctrlid==ID_BUTTON+3)? list1:list2;
				if(listbox->CurrentIndex<0||!(*listbox)[listbox->CurrentIndex]) break;
				int16 index = (*listbox)[listbox->CurrentIndex]->Number;
				if(index<0) break;
				RecordMan.PersonMan.GetRecord(index,*RecordMan.PersonMan.AddedRecord);
				uint16 budae=RecordMan.PersonMan.AddedRecord->GetBudae();
				if(ctrlid==ID_BUTTON+3){
					if(SelectLine<0){
						if(!edit->GetTextSize()) break;
						sagoman.AddSagoNode(edit->GetText(),budae,index);
					}
					else{
						InwonBase *base=CurrentMan.GetNode(SelectLine);
						if(!base) break;
						sagoman.AddSagoNode(base->Text,budae,index);
					}
				}
				else{
					sagoman.DeleteSagoNode(budae,index);
				}
				MouseEvents->SetCursorID(5);
				sagoman.Write();
				InitTable(SelectLine,SelectKind);
				InitList(SelectLine,SelectKind);
				SendControlMessage(REDRAW_TITLE);
			}
      }
			break;
		default:
			break;
	}
}

ProcessResult SagoWindow::RespondMessage(JKMSG& msg)
{

	if(msg.main==INWON_SELECTCELL)
		EvSelectCell(LOWORD(msg.detail),HIWORD(msg.detail));

	return JKWindow::RespondMessage(msg);
}

void SagoWindow::EvSelectCell(int16 line,int16 kind)
{
	MouseEvents->SetCursorID(5);
	SelectLine=line;
	SelectKind=kind;
	InitList(line,kind);
}

void SagoWindow::CreatePresentCondition()
{
	CurrentMan.ClearNode();
	SagoManager& sagoman=RecordMan;
	SagoBase *base=sagoman.HeadNode;
	uint16 TempCount[4];
	while(base) {
		for(uint16 i=0;i<4;i++)
			TempCount[i]=base->SagoCount[i][0]+base->SagoCount[i][1];
		CurrentMan.PutNodeLast(new InwonBase(SagoTextLen,base->Text,0,TempCount));
		base=base->next;
	}
}

void SagoWindow::InitTable(int16 line,int16 kind)
{
	CreatePresentCondition();

	InwonControl *inwonctrl=(InwonControl *)GetControl(ID_USERCONTROL+0x0100);
	if(inwonctrl) {
		inwonctrl->ShowFirstIndex=0;
		inwonctrl->SelectLine=line;
		inwonctrl->SelectKind=kind;
		inwonctrl->RedrawClient();
	}
}

void SagoWindow::InitList(int16 line,int16 kind)
{
	JKListBox *listbox1 = (JKListBox *)GetControl(ID_LISTBOX+1);
	JKListBox *listbox2 = (JKListBox *)GetControl(ID_LISTBOX+2);
	if(listbox1&&listbox2) {
		listbox1->DestroyAll();
		listbox2->DestroyAll();
		//=====================================
		SagoManager& sagoman=RecordMan;
		PersonManager& personman=RecordMan.PersonMan;

		REFENTRY refentry;
		char *buffer;
		int16	index=personman.TableHead;
		while(index!=-1){
			if(personman.GetRefEntry(index,refentry)&&refentry.HasData) {
				if(!sagoman.GetSagoNode(index)) {
					personman.GetRecord(index,*personman.AddedRecord);
					if(kind<=0||personman.AddedRecord->GetBudae()==kind-1){
						buffer=CreateListString(*personman.AddedRecord);
						if(buffer) {
							listbox1->Add(-1,new HuboBase(buffer,index,LtBlue));
							delete[] buffer;
						}
					}
				}
				else if(line>=0) {
					SagoBase *base=sagoman.GetNode(line);
					if(base&&base->GetSagoNode(index)){
						personman.GetRecord(index,*personman.AddedRecord);
						if(kind<=0||personman.AddedRecord->GetBudae()==kind-1){
							buffer=CreateListString(*personman.AddedRecord,2);
							if(buffer) {
								listbox2->Add(-1,new HuboBase(buffer,index,LtBlue));
								delete[] buffer;
							}
						}
					}
				}
				index=refentry.next;
			}
			else index=-1;
		}
		//====================================
		listbox1->Add(-1,new HuboBase("********* < µa‹¡ˆa {·³“¡”a. > *********",-1,Gray));
		if(listbox1->HuboManager::NodeCount) listbox1->SetCurrentIndex(0);
		listbox1->RedrawClient();

		listbox2->Add(-1,new HuboBase(" < µa‹¡ˆa {·³“¡”a. > ",-1,Gray));
		if(listbox2->HuboManager::NodeCount) listbox2->SetCurrentIndex(0);
		listbox2->RedrawClient();
	}
}

char *SagoWindow::CreateListString(RecordBase& rec,uint16 count)
{
	uint16 lengtharray[4],indexarray[4]={2,0,1,3};
	uint16 len=0;
	uint16 i;
	for(i=0;i<count;i++) {
		len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
		len+=1;
	}
	char *buffer=new char[len+1];
	if(buffer){
		memset(buffer,' ',len);
		buffer[len]='\0';
		len=0;
		for(i=0;i<count;i++) {
			rec.GetData(indexarray[i],buffer+len);
			len+=lengtharray[i];
			if(i!=count-1){
				buffer[strlen(buffer)]=' ';
				buffer[len]=0;
				strcat(buffer+len," ");
				len+=1;
			}
		}
	}
	return buffer;
}
