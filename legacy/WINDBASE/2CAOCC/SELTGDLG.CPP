#include "jkbutton.h"
#include "scrllist.h"
#include "tgman.h"
#include "seltgdlg.h"

SelectTargetDialog::SelectTargetDialog(JKPoint p,int32& index,JKControl *parent):
	JKDialog(JKRect(0,0,600,320).OffsetBy(p),parent),RefIndex(index)
{
	WinAttr|=WA_FIRSTSHOW|WA_NORESTRICT|WA_TITLEMOVEABLE|WA_CHANGEPLACE;
	SetText("Îa¸â ¬åÈ‚");
	BackColor=FindNearestColor(55,55,55);
	TargetMan->AddedRecord->InitData();
}

SelectTargetDialog::~SelectTargetDialog()
{
}

void SelectTargetDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitList();
}

void SelectTargetDialog::SetupWindow()
{
	JKButton *bttn;
	bttn=new JKButton(JKRect(10,10,575,30),ID_STATIC+1);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	bttn->AdjustFlag=ADJ_YCENTER;
	bttn->SetText("Îa ¸â   ¹Á      Îa   Îa   ¸â    ¬÷   »©     Îa‰¡  ¸åÊb §¡        ‰¡");
	AddControl(bttn);
	ScrollListBox *scrllist=new ScrollListBox(JKRect(10,31,575,210),ID_LISTBOX+1);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);
	bttn=new JKButton(JKRect(10,211,575,230),ID_STATIC+2);
	bttn->SetEnable(FALSE);
	bttn->BackColor=Gray;
	bttn->TextColor=White;
	AddControl(bttn);

	bttn=new JKButton(JKRect( 10,250,100,280),ID_BUTTON+1);
	bttn->SetText("ÑÂ  ·¥");
	AddControl(bttn);
	bttn=new JKButton(JKRect(110,250,200,280),ID_BUTTON+2);
	bttn->SetText("Âá  ­¡");
	AddControl(bttn);
}

void SelectTargetDialog::PaintClient(JKDC& dc,JKRect&)
{
	dc.IsOverWrite=FALSE;
}

void SelectTargetDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void SelectTargetDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
		{
			JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
			if(listbox){
				HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
				if(hubo && (int16)hubo->Number>=0) RefIndex=(int32)hubo->Number;
			}
			SendMessage(ID_OK);
		}
			break;
		case ID_BUTTON+2:
			SendMessage(ID_CANCEL);
			break;
		default:
			break;
	}
}

char *SelectTargetDialog::CreateListString(TargetRecord& rec)
{
	uint16 lengtharray[6],indexarray[6]={0,1,2,3,5,4};
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

void SelectTargetDialog::InitList()
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();
		REFENTRY refentry;
		char *buffer;
		int16	index=TargetMan->TableMan.TableHead;
		while(index!=-1&&TargetMan->TableMan.GetRefEntry(index,refentry)){
			if(refentry.HasData) {
				TargetMan->RecordMan.GetRecord(index,*TargetMan->AddedRecord);
				buffer=CreateListString(*TargetMan->AddedRecord);
				if(buffer) {
					listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue));
					delete[] buffer;
				}
				index=refentry.next;
			}
			else index=-1;
		}
		listbox->Add(listbox->HuboManager::NodeCount,new HuboBase("************* < µa‹¡ˆa {·³“¡”a. > *************",-1,Gray));
		if(listbox->HuboManager::NodeCount) {
			index=RefIndex;
			index=listbox->GetIndexByNumber(index);
			if(index>=0) listbox->SetCurrentIndex(index);
			else listbox->SetCurrentIndex(0);
		}
		listbox->RedrawClient();
		InitListCount();
	}
}

void SelectTargetDialog::InitListCount()
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

