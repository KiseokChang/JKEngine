#include "jkbutton.h"
#include "listbox.h"
#include "yesnodlg.h"
#include "entbase.h"
#include "inpbalsu.h"
#include "occmsg.h"
#include "fireman.h"
#include "battman.h"
#include "tgwin.h"

TargetWindow::TargetWindow(JKPoint& p,WORD ctrlid,uint16 index,JKControl *parent):
	JKWindow(JKRect(0,0,310,120).OffsetBy(p),parent,ctrlid),Index(index)
{
	WinForm=0x03;
	BackColor=LtGray;
	WinAttr|=WA_NORESTRICT|WA_FIRSTSHOW|WA_TITLEMOVEABLE|WA_CHANGEPLACE;
}

TargetWindow::~TargetWindow()
{}

void TargetWindow::SetupWindow()
{
	JKListBox *listbox=new JKListBox(JKRect(10,10,290,60),ID_LISTBOX+1);
	AddControl(listbox);
	listbox->SetCurrentIndex(0);

	JKButton *bttn;
	JKRect rect(10,65,100,90);
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("¬b¹A");
	AddControl(bttn);

}

ProcessResult TargetWindow::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case SETFOCUS:
			if(msg.sub==WinID || TestSubWindow(msg.sub,TRUE))
				SendControlCommand(TARGET_SELECTED,Index);
			break;
		defaule:
			break;
	}
	return JKWindow::RespondMessage(msg);
}

void TargetWindow::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) {
		JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
		if(listbox&&listbox->CurrentIndex>=0){
			HuboBase *hubo=(*listbox)[listbox->CurrentIndex];
			if(hubo && (int16)hubo->Number>=0){
				YesNoDialog dlg(JKRect(400,300,600,400),"»¡¶¯¡‰V¯s“¡Œa?");
				dlg.WinForm=0x01;
				if(dlg.Run()==ID_YES){
					SendControlCommand(RETRIEVE_BATTALION,hubo->IsEnable);
					PreFireMan->DeleteRecord(hubo->Number);
					listbox->Destroy(listbox->CurrentIndex);
					listbox->RedrawClient();
				}
			}
		}
	}
}

char *TargetWindow::CreateListString(FireRecord& rec)
{
	uint16 lengtharray[4],indexarray[4]={0,5,6,7};
	uint16 len=0;
	uint16 i;
	for(i=0;i<4;i++) {
		len+=1;
		len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
	}
	len-=1;
	char *buffer=new char[len+1];
	buffer[len]='\0';
	memset(buffer,' ',len);
	len=0;
	for(i=0;i<4;i++) {
		rec.GetData(indexarray[i],buffer+len);
		len+=lengtharray[i];
		if(i!=4-1){
			buffer[strlen(buffer)]=' ';
			buffer[len]=0;
			strcat(buffer+len," ");
			len+=1;
		}
	}
	return buffer;
}

BOOL TargetWindow::AddBattalion(uint16 podaeindex)
{
	JKListBox *listbox=(JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		BattalionMan->RecordMan.GetRecord(podaeindex,*BattalionMan->AddedRecord);
		EntryBase *ent=BattalionMan->AddedRecord->GetEntry(1)->Duplicate();
		if(!ent) return FALSE;
		ent->EntryID=0;
		PreFireMan->AddedRecord->GetEntry((uint16)0)->Assign(ent);
		delete ent;
		if(InputBalusuDialog(*PreFireMan->AddedRecord).Run()==ID_OK){
			int16 index=PreFireMan->AddRecord(*PreFireMan->AddedRecord);
			if(index>=0){
				char *buffer=CreateListString(*PreFireMan->AddedRecord);
				if(buffer) {
					listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(buffer,index,LtBlue,podaeindex));
					delete[] buffer;
				}
				listbox->RedrawClient();
			}
			return TRUE;
		}
	}
	return FALSE;
}
