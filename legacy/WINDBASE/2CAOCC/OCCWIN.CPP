#include "okdialog.h"
#include "yesnodlg.h"

#include "tgrefent.h"
#include "tgman.h"
#include "plntgman.h"
#include "hptman.h"
#include "battman.h"
#include "xbattman.h"
#include "fireman.h"
#include "howizman.h"
#include "airman.h"

#include "menubar.h"
#include "battstat.h"
#include "logodlg.h"
#include "passdlg.h"

#include "tgdlg.h"
#include "plntgdlg.h"
#include "firelst.h"
#include "battlst.h"
#include "xbattlst.h"
#include "howmndlg.h"

#include "airmndlg.h"
#include "htimedlg.h"
#include "tglst.h"
#include "plntglst.h"
#include "hptmndlg.h"

#include "podaectl.h"
#include "dateent.h"
#include "timeent.h"

#include "sel1of2.h"
#include "tgaq1dlg.h"
#include "tgaq2dlg.h"
#include "hopedlg.h"
#include "selbatt.h"
#include "statview.h"

#include "cmdmsgbx.h"

#include "occmsg.h"
#include "occwin.h"

char TitleString[100]="¹A 2 Í¡¥wµa”e OCC Ïa¡‹aœ‘";

OCCWindow::OCCWindow():
	JKWindow(JKRect(0,0,getmaxx(),getmaxy()),0)
{
	WinForm=0x01;
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW;
	BorderSize=3;

	BattalionMan  = new BattalionManager;
	XBattalionMan  = new XBattalionManager;

	FireMan = new FireManager;
	PreFireMan = new FireManager("prefire");
	TempFireMan = new FireManager("tempfire");

	HowitzerMan = new HowitzerManager;

	TargetMan = new TargetManager;
	PlanTargetMan = new PlanTargetManager;
	HPTMan = new HPTManager;
	AirMan = new AirManager;

	if(!BattalionMan||!XBattalionMan||!FireMan||!PreFireMan||!TempFireMan||
			!HowitzerMan||!PlanTargetMan||!TargetMan||!HPTMan||!AirMan)
	CreationError=TRUE;
}

OCCWindow::~OCCWindow()
{
	if(BattalionMan) delete	BattalionMan;
	if(XBattalionMan) delete	XBattalionMan;

	if(FireMan) delete FireMan;
	if(PreFireMan) delete PreFireMan;
	if(TempFireMan) delete TempFireMan;

	if(HowitzerMan) delete HowitzerMan;

	if(TargetMan) delete TargetMan;
	if(PlanTargetMan) delete PlanTargetMan;
	if(HPTMan) delete HPTMan;
	if(AirMan) delete AirMan;
}

JKRect OCCWindow::CalcClientRect()
{
	JKRect rect=JKWindow::CalcClientRect();
	rect.top+=40;
	return rect;
}

void OCCWindow::InitWindow()
{
	JKWindow::InitWindow();
	SetTimer(30000);
}

void OCCWindow::SetupWindow()
{

	JKRect rect=ClientRect.SetLeftTopBy();
	rect.left=rect.right-100;
	AddControl(new MenuBar(rect));

	rect=ClientRect.SetLeftTopBy();
	rect.right-=100;
	JKWindow *win=new StatusView(rect);
	AddControl(win);
}

void OCCWindow::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
	SendCommand(CHECK_PASSWORD);
}

void OCCWindow::PaintWindow(JKDC& dc,JKRect& paintrect)
{
	JKRect rect=WinRect.SetLeftTopBy();
	dc.SolidBar(rect,LtBlue);
	GlobalVFontMan->ResetCTM();
	GlobalVFontMan->SetFontSize(40,36);
	dc.SetTextColor(Black);
	GlobalVFontMan->DrawString(dc,JKPoint(52,7),TitleString);
	dc.SetTextColor(White);
	GlobalVFontMan->DrawString(dc,JKPoint(50,5),TitleString);

	JKWindow::PaintWindow(dc,paintrect);
}

///////////////////////////////////////////////////////////



void OCCWindow::EvCommand(WORD cmdid,WORD data,DWORD ,WORD )
{
	JKDialog *dlg;
	JKWindow *win;
	WORD result;
	switch(cmdid){
		case SHOW_LOGO:
			dlg=new LogoDialog(JKRect(220,100,800,640));
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;
		case CHECK_PASSWORD:
			dlg=new PasswordDialog(JKRect(350,100,700,300),"855fa2ba","´q Ñ¡   ·³ b",this);
			if(dlg){
				if(dlg->Run()!=ID_OK)	MsgQue->SendMessage(0,APPEXIT);
				else SendCommand(SHOW_LOGO);
				delete dlg;
			}
			break;
		case FIRE_PROCESS:
			EvFireProcess(data);
			break;
		case FIRE_PLANTARGET:
			EvFirePlanTarget();
			break;
		case FIRE_BEGIN:
			EvFireBegin();
			break;
		case FIRE_FINISH:
			EvFireFinish(data);
			break;

		case OCC_COMMAND+1:
			dlg=new HTimeDialog(JKPoint(350,140));
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;

		case OCC_COMMAND+2:
			if(PrepareFire(0))	EvFireProcess(0);
			break;
		case OCC_COMMAND+3:
			dlg=new SelectOneOfTwoDialog(JKPoint(340,200),"¶¥Ñw Îa¸â","•¡¡ Îa¸â");
			if(dlg){
				dlg->SetText("Îa¸âÑwÈ ¬åÈ‚");
				WORD result=dlg->Run(); delete dlg;
				if(result==1)	{
					if(PrepareFire(1)) EvFireProcess(1);
				}
				else if(result==2) {
					if(PrepareFire(2)) EvFireProcess(2);
				}
			}
			break;
		case OCC_COMMAND+4:
			dlg=new AirManDialog(JKPoint(120,80));
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;

		case OCC_COMMAND+5:
			dlg=new FireListDialog(JKPoint(200,100));
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;
		case OCC_COMMAND+6:
			dlg=new BattalionListDialog(JKRect(200,100,800,660));
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;
		case OCC_COMMAND+7:
			dlg=new XBattalionListDialog(JKRect(200,100,800,660));
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;
		case OCC_COMMAND+8:
			int32 temp;
			dlg=new TargetListDialog(JKPoint(180,50),temp);
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;
		case OCC_COMMAND+9:
			dlg=new SelectOneOfTwoDialog(JKPoint(340,200),"Ð‚¯± Îa¸â","‰ÒB Îa¸â");
			if(dlg){
				dlg->SetText("Îa¸â ÀáŸ¡");
				WORD result=dlg->Run(); delete dlg;
				if(result==1)	{
					HPTRecord hptrec;
					dlg=new HPTManDialog(JKPoint(100,70),hptrec);
					if(dlg){
						result=dlg->Run();	delete dlg;
						if(result!=ID_OK)	{
							TargetRecord tgrec;
							if(!hptrec.GetTargetRecord(tgrec)) return;

							FireMan->AddedRecord->InitData();
							FireMan->AddedRecord->SetData(4,"ÌaŠA");
							char buff[30];
							tgrec.GetData(0,buff);
							FireMan->AddedRecord->SetData(1,buff);
							tgrec.GetData(1,buff);
							FireMan->AddedRecord->SetData(2,buff);
							tgrec.GetData(2,buff);
							FireMan->AddedRecord->SetData(3,buff);

							if(EvFireProcess(0)){
								TimeEntry *timeent=(TimeEntry *)HPTMan->AddedRecord->GetEntry(1);
								if(timeent) timeent->SetCurrentTime();
								HPTMan->SetRecord(result,hptrec);
							}
						}
					}
				}
				else if(result==2) {
					dlg=new PlanTargetListWindow(JKPoint(200,50));
					if(dlg){
						dlg->Run();	delete dlg;
					}
				}
			}
			break;
		case OCC_COMMAND+10:
			dlg=new HowitzerManDialog(JKPoint(180,80));
			if(dlg){
				dlg->Run();	delete dlg;
			}
			break;
	}
}

BOOL OCCWindow::PrepareFire(uint16 type)
{
	JKDialog *dlg;
	WORD result;

	FireMan->AddedRecord->InitData();

	if(!type){
		dlg=new SelectOneOfTwoDialog(JKPoint(340,200),"Îa¸â ¤åÑ¡","Îa¸â ¹ÁÎa");
		if(!dlg) return FALSE;
		dlg->SetText("Îa¸âÒB—b¤w¤ó ¬åÈ‚");
		result=dlg->Run();
		delete dlg;
	}
	else result=2;

	if(result==1){
		dlg=new TargetAcquisition1Dialog(*FireMan->AddedRecord);
		if(!dlg) return FALSE;
		result=dlg->Run(); delete dlg;
		if(result!=ID_OK) return FALSE;
	}
	else if(result==2){
		dlg=new TargetAcquisition2Dialog(*FireMan->AddedRecord,type? FALSE : TRUE);
		if(!dlg) return FALSE;
		result=dlg->Run(); delete dlg;
		if(result!=ID_OK) return FALSE;
	}
	else return FALSE;

	dlg=new YomangAcquisitionDialog(*FireMan->AddedRecord);
	if(!dlg) return FALSE;
	result=dlg->Run(); delete dlg;
	if(result!=ID_OK) return FALSE;
	return TRUE;
}

BOOL OCCWindow::EvFireProcess(uint16 type)
{
	JKRect rect=ClientRect.SetLeftTopBy();

	JKDialog *dlg=new SelectFireBattalionDialog(JKPoint(50,30),*FireMan->AddedRecord,type,this);
	if(!dlg) return FALSE;
	WORD result=dlg->Run();
	delete dlg;
	if(result!=ID_OK) return FALSE;
	SendCommand(FIRE_BEGIN);
	return TRUE;
}

void OCCWindow::EvFireBegin()
{
	BattalionRecord& battrec=*BattalionMan->AddedRecord;
	char budaename[6];
	REFENTRY refentry;
	int16	index=PreFireMan->TableMan.TableHead;
	while(index!=-1&&PreFireMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			PreFireMan->RecordMan.GetRecord(index,*PreFireMan->AddedRecord);
			PreFireMan->AddedRecord->GetData(0,budaename);
			int32 balsu;
			PreFireMan->AddedRecord->GetBalsu(balsu);
			if(balsu>0){
				EntryBase budaeent(1,"¦”¡wÃ·",5);
				budaeent.SetData(budaename);
				int16 index=BattalionMan->Search(&budaeent);
				if(index>=0) {
					BattalionMan->RecordMan.GetRecord(index,battrec);
					uint16 status=battrec.GetStatus();
					if(status==BATTSTAT_READY) {
						int32 munsu;
						battrec.GetMunsu(munsu);
						struct time firetime;
						gettime(&firetime);
						JKPoint32 tgcoord,battcoord;
						PreFireMan->AddedRecord->GetCoord(tgcoord);
						battrec.GetCoord(battcoord);
						if(AirMan->AddedRecord->IsFirable(tgcoord,battcoord,firetime)) {
							AddControl(new PodaeControl(index,balsu*munsu,balsu*10000));
							DateEntry *dateent=(DateEntry *)PreFireMan->AddedRecord->GetEntry(8);
							TimeEntry *timeent=(TimeEntry *)PreFireMan->AddedRecord->GetEntry(9);
							if(dateent&&timeent) {
								dateent->SetCurrentDay();
								timeent->SetCurrentTime();

								PrintFireCommand(*PreFireMan->AddedRecord);

								int32 realbalsu=balsu*munsu;
								PreFireMan->AddedRecord->SetBalsu(realbalsu);
								FireMan->AddRecord(*PreFireMan->AddedRecord);
							}
						}
					}
				}
			}
			index=refentry.next;
		}
		else index=-1;
	}
}

void OCCWindow::EvFireFinish(uint16 index)
{
	BattalionMan->RecordMan.GetRecord(index,*BattalionMan->AddedRecord);
	BattalionMan->AddedRecord->SetStatus(BATTSTAT_READY);
	BattalionMan->SetRecord(index,*BattalionMan->AddedRecord);
}

void OCCWindow::EvTimerHappen(DWORD)
{
	SendCommandBox("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‰ÒB¬a‰b º— ......",FIRE_PLANTARGET);
}

void OCCWindow::EvFirePlanTarget()
{
	char buff[30],budaename[6];
	WORD budaeidarray[5]={2,7,12,17,22};
	PlanTargetRecord planrec;
	TargetRecord tgrec;
	BattalionRecord battrec;
	JKPoint32 coord;

	PreFireMan->AddedRecord->SetData(4,"ÌaŠA");

	REFENTRY refentry;
	int16	index=PlanTargetMan->TableMan.TableHead;
	while(index!=-1&&PlanTargetMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			PlanTargetMan->RecordMan.GetRecord(index,planrec);
			int16 targettype=planrec.GetType();
			if(targettype>=0&&targettype<6 && PlanTargetMan->EnableSet[targettype]){
				if(planrec.GetTargetRecord(tgrec)){
					tgrec.GetData(0,buff);
					PreFireMan->AddedRecord->SetData(1,buff);
					tgrec.GetData(1,buff);
					PreFireMan->AddedRecord->SetData(2,buff);
					tgrec.GetData(2,buff);
					PreFireMan->AddedRecord->SetData(3,buff);

					struct time currtime;
					gettime(&currtime);
					int32 currHtime=TimerMan->CalcValidCount(PlanTargetMan->HTimeSet[targettype],currtime,1l);

					for(uint16 i=0;i<5;i++){
						int32 startH,endH;
						planrec.GetStartH(i,startH);
						planrec.GetEndH(i,endH);
						startH*=60000;
						endH*=60000;
						if(currHtime>=startH&&currHtime<=endH){
							planrec.GetData(budaeidarray[i],budaename);
							PreFireMan->AddedRecord->SetData(0,budaename);
							planrec.GetData(budaeidarray[i]+1,buff);
							PreFireMan->AddedRecord->SetData(5,buff);
							int32 balsu;
							planrec.GetBalsu(i,balsu);
							if(balsu>0){
                PreFireMan->AddedRecord->SetBalsu(balsu);

								uint32 interval;
								planrec.GetIntervalH(i,interval);
								if((int32)interval<=0) interval=0;
								else interval*=60000;

								EntryBase budaeent(1,"¦”¡wÃ·",5);
								budaeent.SetData(budaename);
								int16 index=BattalionMan->Search(&budaeent);
								if(index>=0) {
									BattalionMan->RecordMan.GetRecord(index,battrec);
									uint16 status=battrec.GetStatus();
									if(status==BATTSTAT_READY) {
										AddControl(new PodaeControl(index,balsu,interval));
										DateEntry *dateent=(DateEntry *)PreFireMan->AddedRecord->GetEntry(8);
										TimeEntry *timeent=(TimeEntry *)PreFireMan->AddedRecord->GetEntry(9);
										if(dateent&&timeent) {
											dateent->SetCurrentDay();
											timeent->SetCurrentTime();
											FireMan->AddRecord(*PreFireMan->AddedRecord);
										}
									}
								}
							}
						}
					}
				}
			}
			index=refentry.next;
		}
		else index=-1;
	}
}

BOOL OCCWindow::PrintFireCommand(FireRecord& rec)
{
	WORD EntryID[10]={0,1,2,3,4,5,6,7,8,9};
	char *EntryName[10]={
		"\r\n    ¦”¡wÃ· : ",
		"\r\n    Îa¸â¤åÑ¡ : ",
		"\r\n    Îa¸â¹ÁÎa : ",
		"\r\n    Îa¸â¬÷»© : ",
		"\r\n    ¶a wÒa‰Á : ",
		"\r\n        Èe¹· : ",
		"\r\n        ¤i® : ",
		"\r\n        ¯¥‰Å : ",
		"\r\n¬a‰b¡ww·©¸a : ",
		"\r\n¬a‰b¡ww¯¡ˆe : "
	};

	char linefeed[3]="\r\n";

	if(!PrintMan->PrintString("  <<    ¬a      ‰b        ¡w      w      ¬á   >>"))	return FALSE;
	if(!PrintMan->PrintString(linefeed))	return FALSE;
	if(!PrintMan->PrintString(linefeed))	return FALSE;
	uint16 maxbuflen = rec.GetMaxBufferSize();
	BYTE * entbuffer = new BYTE[maxbuflen+1];
	EntryBase *ent;
	uint16 count= 10;
	for(uint16 i=0;i<count;i++){
		ent = rec.GetEntry(EntryID[i]);
		if(ent){
			if(!PrintMan->PrintString(EntryName[i]))	return FALSE;
			ent->GetData(entbuffer);
			if(!PrintMan->PrintString(entbuffer)) return FALSE;
		}
	}
	delete[] entbuffer;
	if(!PrintMan->PrintCharacter(FF)) return FALSE;
	return TRUE;
}

void OCCWindow::SendCommandBox(char *message,WORD command)
{
	JKControl *ctrl=new ComandMessageBox(JKRect(400,300,600,400),0,command);
	ctrl->SetText(message);
	ctrl->BackColor=LtBlue;
	ctrl->TextColor=White;
	AddControl(ctrl);
}
