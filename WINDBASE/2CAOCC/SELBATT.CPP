#include "yesnodlg.h"
#include "jkbutton.h"
#include "scrllist.h"
#include "typectrl.h"
#include "selctrl.h"
#include "calccord.h"
#include "prfcctrl.h"
#include "roaddlg.h"
#include "circldlg.h"
#include "podaectl.h"
#include "gmettbl.h"
#include "tgwin.h"
#include "occmsg.h"
#include "fireman.h"
#include "battman.h"
#include "howizman.h"
#include "selbatt.h"


BOOL SelectFireBattalionDialog::GetFireCapability(BattalionRecord& battrec,JKPoint32 coord,FIRECAPABILITY& firecapable)
{
	CoordCalculator coordcalc;
	double distance, direction;

	firecapable.distance=firecapable.direction=0;
	if(battrec.GetStatus()!=1) return FALSE;
	EntryBase *ent=battrec.GetEntry(4)->Duplicate();
	if(!ent) return FALSE;
	ent->EntryID=0;
	int16 index=HowitzerMan->Search(ent);
	delete ent;
	if(index>=0){
		HowitzerMan->RecordMan.GetRecord(index,*HowitzerMan->AddedRecord);
		int32 nomal=0,rap=0;
		HowitzerMan->AddedRecord->GetDistance(nomal,rap);
		JKPoint32 battpos;
		battrec.GetCoord(battpos);

		distance=coordcalc.CalcDistance(battpos,coord);
		if(distance<(double)nomal*0.1) firecapable.distance=1;
		else if(distance<(double)rap*0.1) firecapable.distance=2;
		else return FALSE;

		int32 AZ;
		battrec.GetAZ(AZ);
		coordcalc.AdjustMil(AZ);
		int32 leftlimit=0l,rightlimit=0l;
		HowitzerMan->AddedRecord->GetLimit(leftlimit,rightlimit);
		coordcalc.AdjustMil(leftlimit);
		coordcalc.AdjustMil(rightlimit);

		direction=coordcalc.CalcDirection(battpos,coord);
		int16 mildirection=coordcalc.RadianToMil(direction);
		int16 leftdiff=coordcalc.AdjustMil(AZ-mildirection);
		int16 rightdiff=coordcalc.AdjustMil(mildirection-AZ);
		if(leftdiff<leftlimit){
			if(leftdiff<100) firecapable.direction=1;
			else if(leftdiff<200) firecapable.direction=2;
			else firecapable.direction=3;
			return TRUE;
		}
		else if(rightdiff<rightlimit){
			if(rightdiff<100) firecapable.direction=1;
			else if(rightdiff<200) firecapable.direction=2;
			else firecapable.direction=3;
			return TRUE;
		}
	}
	return FALSE;
}

SelectFireBattalionDialog::SelectFireBattalionDialog(JKPoint& p,FireRecord& firerec,uint16 tgtype,JKControl *parent):
	JKDialog(JKRect(0,0,600,400).OffsetBy(p),parent),FireRec(firerec),TargetType(tgtype)
{
	BackColor=FindNearestColor(55,55,55);

	WinForm=0x03;
	SetText("¸ñ Îa¸â ¬a‰b");

	PreFireMan->DeleteAll();
	PreFireMan->AddedRecord->Assign(&FireRec);
	WinAttr|=WA_NOINCLIENT|WA_TITLEMOVEABLE;
}

SelectFireBattalionDialog::~SelectFireBattalionDialog()
{
}

void SelectFireBattalionDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitList();
	SendCommand(DECIDE_TARGETTYPE);
}

void SelectFireBattalionDialog::SetupWindow()
{
	JKButton *bttn;
	JKRect rect=ClientRect.SetLeftTopBy();
	rect.left=rect.right-100;

	rect.top=5;
	rect.bottom=rect.top+30;
	bttn=new JKButton(rect,ID_BUTTON+2);
	bttn->SetText("Âá­¡");
	AddControl(bttn);

	rect.top=rect.bottom+10;
	rect.bottom=rect.top+50;
	bttn=new JKButton(rect,ID_BUTTON+1);
	bttn->SetText("ÑÂ·¥");
	AddControl(bttn);

	ScrollListBox *scrllist;
	rect.Set(290,120,290,230);
	for(uint16 i=0;i<2;i++){
		rect.left=rect.right+10;
		rect.right=rect.left+140;
		scrllist=new ScrollListBox(rect,ID_LISTBOX+1+i);
		scrllist->DragScrollFlag=TRUE;
		AddControl(scrllist);
	}

	rect.Set(0,260,0,360);
	for(i=2;i<5;i++){
		rect.left=rect.right+30;
		rect.right=rect.left+150;
		scrllist=new ScrollListBox(rect,ID_LISTBOX+1+i);
		scrllist->DragScrollFlag=TRUE;
		AddControl(scrllist);
	}
}

char *SelectFireBattalionDialog::CreateListString(BattalionRecord& rec)
{
	uint16 lengtharray[2],indexarray[2]={1,4};
	uint16 len=0;
	uint16 i;
	for(i=0;i<2;i++) {
		len+=1;
		len+=(lengtharray[i]=rec.GetEntry(indexarray[i])->BufferSize);
	}
	len-=1;
	char *buffer=new char[len+1];
	buffer[len]='\0';
	memset(buffer,' ',len);
	len=0;
	for(i=0;i<2;i++) {
		rec.GetData(indexarray[i],buffer+len);
		len+=lengtharray[i];
		if(i!=2-1){
			buffer[strlen(buffer)]=' ';
			buffer[len]=0;
			strcat(buffer+len," ");
			len+=1;
		}
	}
	return buffer;
}

void SelectFireBattalionDialog::InitList()
{
	ScrollListBox *scrllist[5];
	for(uint16 i=0;i<5;i++){
		scrllist[i]=(ScrollListBox *)GetControl(ID_LISTBOX+1+i);
		scrllist[i]->HuboManager::ClearNode();
		scrllist[i]->Add(scrllist[i]->HuboManager::NodeCount,new HuboBase("<µa‹¡ˆa {·³“¡”a.>",-1,Gray));
	}
	REFENTRY refentry;
	int16	index=BattalionMan->TableMan.TableHead;
	while(index!=-1&&BattalionMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			AddList(index);
			index=refentry.next;
		}
		else index=-1;
	}
	for(i=0;i<5;i++){
		if(scrllist[i]->HuboManager::NodeCount) scrllist[i]->SetCurrentIndex(0);
		scrllist[i]->RedrawWindow();
	}
}

void SelectFireBattalionDialog::AddList(int16 index,BOOL redraw)
{
	BattalionMan->RecordMan.GetRecord(index,*BattalionMan->AddedRecord);
	ScrollListBox *scrllist[5];
	for(uint16 i=0;i<5;i++)
		scrllist[i]=(ScrollListBox *)GetControl(ID_LISTBOX+1+i);
	uint16 status = BattalionMan->AddedRecord->GetStatus();
	JKPoint32 coord;
	FireRec.GetCoord(coord);
	FIRECAPABILITY firecapable;
	GetFireCapability(*BattalionMan->AddedRecord,coord,firecapable);
	int16 kind=0;
	if(status==1){
		if(firecapable.direction)	kind=firecapable.distance? (firecapable.distance==1? 0:1):2;
		else											kind=2;
	}
	else if(status==2) kind=3;
	else if(status==3) kind=4;
	else kind=2;

	char *buffer=CreateListString(*BattalionMan->AddedRecord);
	if(buffer) {
		scrllist[kind]->Add(scrllist[kind]->HuboManager::NodeCount-1,new HuboBase(buffer,index,LtBlue));
		if(redraw) scrllist[kind]->RedrawWindow();
		delete[] buffer;
	}
}

void SelectFireBattalionDialog::PaintClient(JKDC& dc,JKRect&)
{
/////////////////////////////////////////////////////////////////////////////////////
	int16 index=FireRec.GetEntry(3)->GetHuboIndex();
	if(index<0||index>=16) index=0;

	int16 hope=FireRec.GetEntry(4)->GetHuboIndex();
	if(hope<0||hope>2) hope=0;

	JKPoint p(10,30);
	uint16 cellwidth=40;

	dc.IsOverWrite=FALSE;
	dc.SetTextColor(LtMagenta);
	dc.TextOutX(JKRect(0,-25,cellwidth*11,-5).OffsetBy(p),"<  Òa   ‰Á   Îa  >",ADJ_YCENTER);

	dc.SetTextColor(LtBlue);
	char *howz[5]={"105","155","8\"","”ae¸w","H.J"};
	char *fuze[3]={"PD","VT","ICM"};
	int16 munsu[11]={6*3,6*3,6*3,  6*3,6*3,6*3,  4*3,4*3,4*3,  1,  1};
	char buff[10];
	for(uint16 j=0;j<5;j++){
		if(j<3)	dc.TextOutX(JKRect(j*cellwidth*3,0,(j+1)*cellwidth*3-1,19).OffsetBy(p),howz[j],ADJ_XYCENTER);
		else dc.TextOutX(JKRect((j-3)*cellwidth+3*cellwidth*3,0,(j-2)*cellwidth+3*cellwidth*3,19).OffsetBy(p),howz[j],ADJ_XYCENTER);
	}
	for(j=0;j<9;j++){
		dc.TextOutX(JKRect(j*cellwidth,20,(j+1)*cellwidth-1,39).OffsetBy(p),fuze[j%3],ADJ_XYCENTER);
	}
	dc.SetTextColor(Black);
	for(j=0;j<11;j++){
		sprintf(buff,"%4d",GMET_Table[index][hope][j]/munsu[j]);
		dc.TextOutX(JKRect(j*cellwidth,40,(j+1)*cellwidth-1,59).OffsetBy(p),buff,ADJ_XYCENTER);
	}
	for(j=1;j<11;j++){
		if(j<10 && j%3){
			dc.VLine(JKPoint(j*cellwidth,20)+p,2*20,Gray);
			dc.VLine(JKPoint(j*cellwidth+1,20)+p,2*20,White);
		}
		else{
			dc.VLine(JKPoint(j*cellwidth,0)+p,3*20,Gray);
			dc.VLine(JKPoint(j*cellwidth+1,0)+p,3*20,White);
		}
	}
	dc.HLine(JKPoint(0,20*1)+p,cellwidth*9,Gray);
	dc.HLine(JKPoint(0,20*1+1)+p,cellwidth*9,White);
	dc.HLine(JKPoint(0,20*2)+p,cellwidth*11,Gray);
	dc.HLine(JKPoint(0,20*2+1)+p,cellwidth*11,White);
	dc.Rectangle3D(JKRect(0,0,cellwidth*11,20*3).OffsetBy(p),-1);
	dc.Rectangle3D(JKRect(0,0,cellwidth*11,20*3).ExpandBy(5).OffsetBy(p),1);
///////////////////////////////////////////////////////////////////////////////
	dc.SetTextColor(LtBlue);
	dc.TextOutX(JKRect(50,120,95,140),"Îa¸â¤åÑ¡ :",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,145,95,165),"¹Á    Îa :",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,170,95,190),"Îa¸â¬÷»© :",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(50,195,95,215),"¶a wÒa‰Á :",ADJ_RIGHT|ADJ_YCENTER);

	dc.SetTextColor(Black);
	dc.TextOutX(JKRect(100,120,310,140),FireRec.GetEntry(1)->DataBuffer,ADJ_YCENTER);
	dc.TextOutX(JKRect(100,145,310,165),FireRec.GetEntry(2)->DataBuffer,ADJ_YCENTER);
	dc.TextOutX(JKRect(100,170,310,190),FireRec.GetEntry(3)->DataBuffer,ADJ_YCENTER);
	dc.TextOutX(JKRect(100,195,310,215),FireRec.GetEntry(4)->DataBuffer,ADJ_YCENTER);
///////////////////////////////////////////////////////////////////////////////////

	JKRect rect;

	BYTE TempColor[5]={LtRed,Yellow,LtGray,LtBlue,LtGreen};
	char *TempText[5]={
		"¬a‰bˆa“w-¥¡É·Èe",
		"¬a‰bˆa“w-RAP Èe",
		"¬a  ‰b   ¦‰  “w",
		"¬a     ‰b    º—",
		"·¡     •·    º—"
	};
	rect.Set(290,100,290,120);
	for(j=0;j<5;j++){
		rect.left=rect.right+10;
		rect.right=rect.left+140;
		dc.SetTextColor(Black);
		dc.TextOutX(rect.OffsetBy(JKPoint(1,1)),TempText[j],ADJ_XYCENTER);
		dc.SetTextColor(TempColor[j]);
		dc.TextOutX(rect,TempText[j],ADJ_XYCENTER);
	}
	rect.Set(0,240,0,260);
	for(j=2;j<5;j++){
		rect.left=rect.right+30;
		rect.right=rect.left+150;
		dc.SetTextColor(Black);
		dc.TextOutX(rect.OffsetBy(JKPoint(1,1)),TempText[j],ADJ_XYCENTER);
		dc.SetTextColor(TempColor[j]);
		dc.TextOutX(rect,TempText[j],ADJ_XYCENTER);
	}
}

void SelectFireBattalionDialog::EvShowWindow()
{
	JKDialog::EvShowWindow();
	JKControl *ctrl=GetControl(ID_BUTTON+1);
	if(ctrl) ctrl->SetFocus();
}

void SelectFireBattalionDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1) {
		if(PreFireMan->RealCount) SendMessage(ID_OK);
	}
	else if(ctrlid==ID_BUTTON+2) SendMessage(ID_CANCEL);
}

void SelectFireBattalionDialog::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid==ID_LISTBOX+1||ctrlid==ID_LISTBOX+2){
		ScrollListBox *scrllist=(ScrollListBox *)GetControl(ctrlid);
		if(scrllist){
			HuboBase *hubo=scrllist->GetHuboBase(index);
			if(!hubo) return;
			if((int16)hubo->Number>=0){
				TargetWindow *tgwin = (TargetWindow *)GetControl(ID_SUBWINDOW+1+FireIndex);
				if(tgwin){
					if(tgwin->AddBattalion(hubo->Number)){
						scrllist->Destroy(index);
						scrllist->RedrawWindow();
					}
				}
			}
		}
	}
}

void SelectFireBattalionDialog::DecideTempFireMan()
{
	TempFireMan->DeleteAll();
	switch(TargetType){
		case 0:
			TempFireMan->AddRecord(FireRec);
			break;
		case 1:
			if(CircleDialog(FireRec).Run()!=ID_OK)
				SendMessage(ID_CANCEL);
			break;
		case 2:
			if(RoadDialog(FireRec).Run()!=ID_OK)
				SendMessage(ID_CANCEL);
			break;
		default:
			break;
	}

	JKRect rect=WinRect.SetLeftTopBy(-ClientRect.LeftTop());
	JKWindow *win;
	uint16 i=0;
	char buff[15];
	REFENTRY refentry;
	int16	index=TempFireMan->TableMan.TableHead;
	while(index!=-1&&TempFireMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			TempFireMan->RecordMan.GetRecord(index,*TempFireMan->AddedRecord);
			win=new TargetWindow(JKPoint(rect.right+1,rect.top+i*20),ID_SUBWINDOW+1+index,index);
			TempFireMan->AddedRecord->GetData(2,buff);
			win->SetText(buff);
			AddControl(win);
			i++;
			FireIndex=index;

			index=refentry.next;
		}
		else index=-1;
	}
}

void SelectFireBattalionDialog::EvCommand(WORD cmdid,WORD data,DWORD,WORD)
{
	switch(cmdid){
		case RETRIEVE_BATTALION:
			AddList(data,TRUE);
			break;
		case TARGET_SELECTED:
			EvTargetSlected(data);
			break;
		case DECIDE_TARGETTYPE:
			DecideTempFireMan();
			break;
		default:
			break;
	}
}

void SelectFireBattalionDialog::EvTargetSlected(WORD index)
{
	TempFireMan->RecordMan.GetRecord(index,*TempFireMan->AddedRecord);
	FireIndex=index;
}
