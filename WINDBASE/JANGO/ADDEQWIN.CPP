#include "addeqwin.h"
#include "jkstatic.h"
#include "jkbutton.h"
#include "eqfilemn.h"
#include "ddaewin.h"

AddEquipWindow::AddEquipWindow(WORD ctrlid):
	JKDialog(JKRect(150,100,800,350),0,ctrlid)
{
	WinForm=0x03;
	Depth=2;
	SetText("Š…® •A·¡ÈaÑÁ·© ÐsÃ¡‹¡ Ïa¡‹aœ‘");
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW|WA_TITLEMOVEABLE;
}

AddEquipWindow::~AddEquipWindow()
{
}

void AddEquipWindow::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=130; rect.right=rect.left+100;
	rect.top+=40; rect.bottom=rect.top+20;
	JKStatic *stc;
	for(int16 i=0;i<4;i++) {
		stc=new JKStatic(rect,ID_STATIC+1+i);
		stc->WinAttr|=WA_NOHASFOCUS;
		stc->BackColor=LtGray;
		stc->TextColor=Black;
		stc->AdjustFlag=ADJ_XYCENTER;
		stc->SetText(EqpBudaeName[i]);
		AddControl(stc);
		rect.left=rect.right+20; rect.right=rect.left+100;
	}

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=130; rect.right=rect.left+100;
	rect.top+=70; rect.bottom=rect.top+20;
	for(i=0;i<4;i++) {
		stc=new JKStatic(rect,ID_STATIC+5+i);
		stc->WinAttr|=WA_NOHASFOCUS;
		stc->BackColor=White;
		stc->TextColor=Black;
		stc->AdjustFlag=ADJ_YCENTER;
		AddControl(stc);
		rect.left=rect.right+20; rect.right=rect.left+100;
	}

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=130; rect.right=rect.left+100;
	rect.top+=110; rect.bottom=rect.top+20;
	for(i=0;i<4;i++) {
		stc=new JKStatic(rect,ID_STATIC+9+i);
		stc->WinAttr|=WA_NOHASFOCUS;
		stc->BackColor=White;
		stc->TextColor=Black;
		stc->AdjustFlag=ADJ_YCENTER;
		AddControl(stc);
		rect.left=rect.right+20; rect.right=rect.left+100;
	}
	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=10; rect.right=rect.left+100;
	rect.top+=70; rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+13);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("¥¡·A ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect.top+=40;rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+14);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("­¡¡¡ ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=200; rect.right=rect.left+100;
	rect.top+=150; rect.bottom=rect.top+30;
	JKButton *btn=new JKButton(rect,ID_BUTTON+1);
	btn->SetText("¦” ¬é¸÷");
	AddControl(btn);

	rect.left=rect.right+50; rect.right=rect.left+100;
	btn=new JKButton(rect,ID_BUTTON+2);
	btn->SetText("ÐsÃ¡‹¡");
	AddControl(btn);

	rect.left=rect.right+50; rect.right=rect.left+100;
	btn=new JKButton(rect,ID_BUTTON+3);
	btn->SetText("¹· ža");
	AddControl(btn);
}

int16 AddEquipWindow::SearchFile(char *filename)
{
	struct ffblk ffblk;
	if(filename) {
		if(!findfirst(filename,&ffblk,FA_ARCH)) return TRUE;
		else return FALSE;
	}
	return TRUE;
}

void AddEquipWindow::ChangeStatic(WORD ctrlid,char *filename)
{
	JKControl *stc=GetControl(ctrlid);
	if(stc) {
		stc->SetText(filename);
		stc->RedrawWindow();
	}
}

void AddEquipWindow::SetStaticName()
{
	for(int16 i=0;i<8;i++) {
		if(SearchFile(FileName[i])) {
			ExistFile[i]=TRUE;
			ChangeStatic(ID_STATIC+5+i,FileName[i]);
		}
		else {
			ExistFile[i]=FALSE;
			ChangeStatic(ID_STATIC+5+i,"ÑÁ·© ´ô·q");
		}
	}
	SearchFile(MainEquipFile[0])?ExistFile[8]=TRUE:ExistFile[8]=FALSE;
	SearchFile(MainSomoFile[0])?ExistFile[9]=TRUE:ExistFile[9]=FALSE;
}

void AddEquipWindow::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid) {
		case ID_BUTTON+1:
      {
			DaedaeDialog *dlg=new DaedaeDialog(DaeDaeName,0);
			if(dlg){
				if(dlg->Run()==ID_OK) {
					MakeFileName();
					SendMessage(DAEDAESELECTED);
				}
				delete dlg;
			}
			}
			break;
		case ID_BUTTON+2:
			FileUnity();
			CloseWindow();
			break;
		case ID_BUTTON+3:
			CloseWindow();
			break;
	}
}

void AddEquipWindow::EvCharDown(uint16 key,WORD /*ctrlid*/)
{
	if(key==ESC) CloseWindow();
}

ProcessResult AddEquipWindow::RespondMessage(JKMSG& msg)
{
	switch(msg.main) {
	 case DAEDAESELECTED:
     SetStaticName();
     MakePodaeName();
   	break;
   }
	return JKDialog::RespondMessage(msg);
}

void AddEquipWindow::MakePodaeName()
{
	strcpy(PodaeName[0],DaeDaeName);
	strcat(PodaeName[0],"-HQ");
	PodaeName[0][7]=NULL;
	strcpy(PodaeName[1],DaeDaeName);
	strcat(PodaeName[1],"-A");
	PodaeName[1][7]=NULL;
	strcpy(PodaeName[2],DaeDaeName);
	strcat(PodaeName[2],"-B");
	PodaeName[2][7]=NULL;
	strcpy(PodaeName[3],DaeDaeName);
	strcat(PodaeName[3],"-C");
	PodaeName[3][7]=NULL;
}

void AddEquipWindow::AddFile(char *dest,char *scr,char *s)
{
	int16 i=0;
	BOMB bm;
	EqiupFileManage *equipdest=new EqiupFileManage(dest);
	EqiupFileManage *equipscr=new EqiupFileManage(scr);

	strcpy(equipdest->FileName,dest);
	strcpy(equipscr->FileName,scr);

	if(equipdest&&equipscr) {
		while(equipscr->Read(bm,i++)) {
			if(!strcmp(bm.Name,s)) equipdest->Append(bm);
		}
		delete equipdest;
		delete equipscr;
	}
}

BOOL AddEquipWindow::MakeBackupFile(char *file,char *backfile)
{
	if(SearchFile(backfile)) {
		if(remove(backfile)!=0) return FALSE;
	}
	if(SearchFile(file)) rename(file,backfile);
	return TRUE;
}

void AddEquipWindow::FileUnity()
{
	MakeBackupFile(MainEquipFile[0],MainEquipFile[1]);
	MakeBackupFile(MainSomoFile[0],MainSomoFile[1]);

	for(int16 i=0;i<4;i++) {
		if(ExistFile[i]) AddFile(MainEquipFile[0],FileName[i],PodaeName[i]);
		if(ExistFile[i+4]) AddFile(MainSomoFile[0],MainSomoFile[i],PodaeName[i]);
		//if(!ExistFile[i]) AddFile(MainEquipFile[0],MainEquipFile[1],PodaeName[i]);
		//if(!ExistFile[i+4]) AddFile(MainSomoFile[0],MainSomoFile[1],PodaeName[i]);
	}
}


void AddEquipWindow::MakeFileName()
{
	if(*DaeDaeName) {
		strcpy(FileName[0],DaeDaeName);
		strcat(FileName[0],"equip.hq");
		FileName[0][12]=NULL;
		strcpy(SomoFileName[0],DaeDaeName);
		strcat(SomoFileName[0],"somo.hq");
		SomoFileName[0][12]=NULL;

		strcpy(FileName[1],DaeDaeName);
		strcat(FileName[1],"equip.a");
		FileName[1][12]=NULL;
		strcpy(SomoFileName[1],DaeDaeName);
		strcat(SomoFileName[1],"somo.a");
		SomoFileName[1][12]=NULL;

		strcpy(FileName[2],DaeDaeName);
		strcat(FileName[2],"equip.b");
		FileName[2][12]=NULL;
		strcpy(SomoFileName[2],DaeDaeName);
		strcat(SomoFileName[2],"somo.b");
		SomoFileName[2][12]=NULL;

		strcpy(FileName[3],DaeDaeName);
		strcat(FileName[3],"equip.c");
		FileName[3][12]=NULL;
		strcpy(SomoFileName[3],DaeDaeName);
		strcat(SomoFileName[3],"somo.c");
		SomoFileName[3][12]=NULL;

		strcpy(MainEquipFile[0],DaeDaeName);
		strcat(MainEquipFile[0],"equip.dat");
		MainEquipFile[0][12]=NULL;
		strcpy(MainEquipFile[1],DaeDaeName);
		strcat(MainEquipFile[1],"equip.bak");
		MainEquipFile[1][12]=NULL;

		strcpy(MainSomoFile[0],DaeDaeName);
		strcat(MainSomoFile[0],"somo.dat");
		MainSomoFile[0][12]=NULL;
		strcpy(MainSomoFile[1],DaeDaeName);
		strcat(MainSomoFile[1],"somo.bak");
		MainSomoFile[1][12]=NULL;
	}
}