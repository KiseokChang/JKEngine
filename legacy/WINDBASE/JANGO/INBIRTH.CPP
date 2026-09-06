#include "joglobal.h"
#include "jkapp.h"
#include "jkstatic.h"
#include "jkedit.h"
#include "insamsg.h"
#include "inwonctl.h"
#include "dateent.h"
#include "inbirth.h"

BirthCondWin::BirthCondWin(JKRect rect,WORD ctrlid,PersonManager& recman,WORD *& srcharray,uint16& srchcount):
	CommonCondWin(rect,ctrlid,9,recman,srcharray,srchcount)
{
	InwonTextLen=10;
	struct date curdate;
	getdate(&curdate);
	ViewYear=curdate.da_year;
}

void BirthCondWin::SetupWindow()
{
	CommonCondWin::SetupWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+3);
	if(ctrl) {
		JKRect rect = ctrl->WinRect;

		rect.top=rect.bottom+40;
		rect.bottom=rect.top+20;

		rect.right-=65;
		JKStatic *stc=new JKStatic(rect,0);
		stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
		stc->BackColor=BackColor;
		stc->SetText("µe•¡");
		AddControl(stc);

		rect.left=rect.right+5;
		rect.right+=65;
		ctrl=new JKEdit(rect,ID_EDIT+1,0,4,FALSE,FALSE);
		char buffer[5];
		sprintf(buffer,"%4d",ViewYear);
		ctrl->SetText(buffer);
		AddControl(ctrl);
	}
}

void BirthCondWin::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		InwonControl *inwonctrl = (InwonControl *)GetControl(ID_USERCONTROL+0x100);
		if(inwonctrl){
			SearchCount=0;
			if(inwonctrl->SelectKind!=-1){
				char buffer[10];
				strcpy(buffer,BudaeName);
				strcat(buffer,inwonctrl->SelectKind==4? "-C":inwonctrl->SelectKind==3? "-B":inwonctrl->SelectKind==2?"-A":"-HQ");
				RecordMan.WorkRecord->SetData(2,buffer);
				RecordMan.AddedRecord->SetData(2,buffer);
				SearchArray[SearchCount++]=2;
			}
			if(inwonctrl->SelectLine!=-1){
				struct date tempdate1,tempdate2;
				tempdate1.da_year=ViewYear;
				tempdate2.da_year=ViewYear;
				if(inwonctrl->SelectLine<12){
					tempdate1.da_mon=inwonctrl->SelectLine+1;
					tempdate1.da_day=1;

					tempdate2.da_mon=inwonctrl->SelectLine+2;
					if(tempdate2.da_mon>12) {
						tempdate2.da_year++;
						tempdate2.da_mon=1;
					}
					tempdate2.da_day=0;
				}
				else{
					tempdate1.da_mon=0;
					tempdate1.da_day=0;
					tempdate2.da_mon=0;
					tempdate2.da_day=0;
				}

				DateEntry dateent(9);
				dateent.DateToStr(dateent.Data,tempdate1);
				RecordMan.WorkRecord->SetData(8,"FALSE");
				RecordMan.WorkRecord->SetData(9,dateent.Data);
				dateent.DateToStr(dateent.Data,tempdate2);
				RecordMan.AddedRecord->SetData(8,"FALSE");
				RecordMan.AddedRecord->SetData(9,dateent.Data);
				SearchArray[SearchCount++]=CondEntryID;
			}
			SendControlMessage(FINISH_SEARCH,1);
			CloseWindow();
		}
		return;
	}
	CommonCondWin::EvButtonClick(ctrlid);
}

void BirthCondWin::EvEditFinish(WORD )
{
	JKControl *ctrl= GetControl(ID_EDIT+1);
	if(ctrl){
		int16 tempyear=atoi(ctrl->GetText());
		if(tempyear<=0) tempyear=1;
		char buffer[5];
		sprintf(buffer,"%4d",tempyear);
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
		if(ViewYear!=tempyear){
			ViewYear=tempyear;
			InitList();
			RedrawWindow();
		}
	}
}

void BirthCondWin::InitPresentCondition()
{
	CommonMan.ClearNode();

	char buffer[13];
	for(uint16 i=0;i<12;i++){
		sprintf(buffer,"%-4d¶©%-2d¶©",ViewYear,i+1);
		CommonMan.PutNodeLast(new InwonBase(InwonTextLen,buffer));
	}
	sprintf(buffer,"¬—·©£¡‹¡·³");
	CommonMan.PutNodeLast(new InwonBase(InwonTextLen,buffer));
}

void BirthCondWin::CreatePresentCondition()
{
	InitPresentCondition();

	REFENTRY refentry;
	int16	index=RecordMan.TableHead;
	while(index!=-1&&RecordMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			RecordMan.GetRecord(index,*RecordMan.AddedRecord);
			uint16 budae=RecordMan.AddedRecord->GetBudae();
			struct date tempdate;
			RecordMan.AddedRecord->GetBirthDay(tempdate,TRUE,ViewYear);
			InwonBase *base=CommonMan.GetNode((tempdate.da_mon>=1&&tempdate.da_mon<=12)? tempdate.da_mon-1:12);
			if(base) base->InwonCount[budae]++;

			index=refentry.next;
		}
		else index=-1;
	}
}
