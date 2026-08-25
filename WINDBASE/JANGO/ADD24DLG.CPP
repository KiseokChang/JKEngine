#include "add24dlg.h"
#include "eq24dman.h"
#include "ddaewin.h"
#include "jkstatic.h"
#include "jkbutton.h"

AddEquip24Dialog::AddEquip24Dialog(WORD ctrlid):
	JKDialog(JKRect(150,100,839,579),0,ctrlid)
{
	WinForm=0x03;
	Depth=2;
	SetText("2.4¹·  •A·¡ÈaÑÁ·© ÐsÃ¡‹¡ Ïa¡‹aœ‘");
	BackColor=LtGray;
	//WinAttr|=WA_FIRSTSHOW|WA_TITLEMOVEABLE;
	WinAttr|=WA_TITLEMOVEABLE;
}

AddEquip24Dialog::~AddEquip24Dialog()
{
}

void AddEquip24Dialog::SetupWindow()
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
		stc->SetText(BudaeName2[i]);
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
	rect.left+=130; rect.right=rect.left+100;
	rect.top+=150; rect.bottom=rect.top+20;
	for(i=0;i<4;i++) {
		stc=new JKStatic(rect,ID_STATIC+13+i);
		stc->WinAttr|=WA_NOHASFOCUS;
		stc->BackColor=White;
		stc->TextColor=Black;
		stc->AdjustFlag=ADJ_YCENTER;
		AddControl(stc);
		rect.left=rect.right+20; rect.right=rect.left+100;
	}

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=130; rect.right=rect.left+100;
	rect.top+=190; rect.bottom=rect.top+20;
	for(i=0;i<4;i++) {
		stc=new JKStatic(rect,ID_STATIC+17+i);
		stc->WinAttr|=WA_NOHASFOCUS;
		stc->BackColor=White;
		stc->TextColor=Black;
		stc->AdjustFlag=ADJ_YCENTER;
		AddControl(stc);
		rect.left=rect.right+20; rect.right=rect.left+100;
	}

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=130; rect.right=rect.left+100;
	rect.top+=230; rect.bottom=rect.top+20;
	for(i=0;i<4;i++) {
		stc=new JKStatic(rect,ID_STATIC+21+i);
		stc->WinAttr|=WA_NOHASFOCUS;
		stc->BackColor=White;
		stc->TextColor=Black;
		stc->AdjustFlag=ADJ_YCENTER;
		AddControl(stc);
		rect.left=rect.right+20; rect.right=rect.left+100;
	}

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=130; rect.right=rect.left+100;
	rect.top+=270; rect.bottom=rect.top+20;
	for(i=0;i<4;i++) {
		stc=new JKStatic(rect,ID_STATIC+25+i);
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
	stc=new JKStatic(rect,ID_STATIC+29);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("º ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect.top+=40;rect.bottom=rect.top+27;
	stc=new JKStatic(rect,ID_STATIC+30);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("º ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect.top+=40;rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+31);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("­¡¡¡ ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect.top+=40;rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+32);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("­¡¡¡ ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect.top+=40;rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+33);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("¤es ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect.top+=40;rect.bottom=rect.top+20;
	stc=new JKStatic(rect,ID_STATIC+34);
	stc->WinAttr|=WA_NOHASFOCUS;
	stc->BackColor=LtGray;
	stc->TextColor=LtMagenta;
	stc->SetText("¤es ÑÁ·©");
	stc->AdjustFlag=ADJ_RIGHT|ADJ_YCENTER;
	AddControl(stc);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=200; rect.right=rect.left+100;
	rect.top+=350; rect.bottom=rect.top+30;
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

int16 AddEquip24Dialog::SearchFile(char *filename)
{
	struct ffblk ffblk;
	if(filename) {
		if(!findfirst(filename,&ffblk,FA_ARCH)) return TRUE;
		else return FALSE;
	}
	return TRUE;
}

void AddEquip24Dialog::ChangeStatic(WORD ctrlid,char *filename)
{
	JKControl *stc=GetControl(ctrlid);
	if(stc) {
		stc->SetText(filename);
		stc->RedrawWindow();
	}
}

void AddEquip24Dialog::SetStaticName()
{
	for(int16 i=0;i<4;i++) {
		if(SearchFile(NameFileName[i])) {
			NameExistFile[i]=TRUE;
			ChangeStatic(ID_STATIC+5+i,NameFileName[i]);
		}
		else {
			NameExistFile[i]=FALSE;
			ChangeStatic(ID_STATIC+5+i,"ÑÁ·© ´ô·q");
		}
		if(SearchFile(KindFileName[i])) {
			KindExistFile[i]=TRUE;
			ChangeStatic(ID_STATIC+9+i,KindFileName[i]);
		}
		else {
			KindExistFile[i]=FALSE;
			ChangeStatic(ID_STATIC+9+i,"ÑÁ·© ´ô·q");
		}

		if(SearchFile(SNameFileName[i])) {
			SNameExistFile[i]=TRUE;
			ChangeStatic(ID_STATIC+13+i,SNameFileName[i]);
		}
		else {
			SNameExistFile[i]=FALSE;
			ChangeStatic(ID_STATIC+13+i,"ÑÁ·© ´ô·q");
		}
		if(SearchFile(SKindFileName[i])) {
			SKindExistFile[i]=TRUE;
			ChangeStatic(ID_STATIC+17+i,SKindFileName[i]);
		}
		else {
			SKindExistFile[i]=FALSE;
			ChangeStatic(ID_STATIC+17+i,"ÑÁ·© ´ô·q");
		}
		if(SearchFile(RNameFileName[i])) {
			RNameExistFile[i]=TRUE;
			ChangeStatic(ID_STATIC+21+i,RNameFileName[i]);
		}
		else {
			RNameExistFile[i]=FALSE;
			ChangeStatic(ID_STATIC+21+i,"ÑÁ·© ´ô·q");
		}
		if(SearchFile(RKindFileName[i])) {
			RKindExistFile[i]=TRUE;
			ChangeStatic(ID_STATIC+25+i,RKindFileName[i]);
		}
		else {
			RKindExistFile[i]=FALSE;
			ChangeStatic(ID_STATIC+25+i,"ÑÁ·© ´ô·q");
		}
	}
}

void AddEquip24Dialog::EvButtonClick(WORD ctrlid)
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

void AddEquip24Dialog::EvCharDown(uint16 key,WORD /*ctrlid*/)
{
	if(key==ESC) CloseWindow();
}

ProcessResult AddEquip24Dialog::RespondMessage(JKMSG& msg)
{
	switch(msg.main) {
	 case DAEDAESELECTED:
		 SetStaticName();
		 MakePodaeName();
		break;
	 }
	return JKDialog::RespondMessage(msg);
}

void AddEquip24Dialog::MakePodaeName()
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

void AddEquip24Dialog::AddFile(char *ndest,char *kdest,char *nscr,char *kscr,char *s)
{
	RPTR index=1,tempindex=1;
	EQP24 Equip24;

	EqpDest.Eqp24Kind.ChangeFileName(kdest);
	EqpDest.Eqp24Name.ChangeFileName(ndest);

	EqpScr.Eqp24Kind.ChangeFileName(kscr);
	EqpScr.Eqp24Name.ChangeFileName(nscr);

	while(index!=-1&&index!=0) {
		tempindex=EqpScr.Eqp24Name.Search(s,index,0x0002);
		if(tempindex!=-1){
			if(EqpScr.Eqp24Name.GetRecord(tempindex)){
				index=EqpScr.Eqp24Name.NameRecord.FirstIndex;
				while(index!=-1){
					if(EqpScr.Eqp24Kind.GetRecord(index)){
						Equip24.Name24=EqpScr.Eqp24Name.NameRecord;
						Equip24.Kind24=EqpScr.Eqp24Kind.KindRecord;
						EqpDest.PutRecord(Equip24);
						index=EqpScr.Eqp24Kind.KindRecord.NextIndex;
					}
				}
			}
		}
		index=tempindex+1;
	}
}

BOOL AddEquip24Dialog::MakeBackupFile(char *file,char *backfile)
{
	if(SearchFile(backfile)) {
		if(remove(backfile)!=0) return FALSE;
	}
	if(SearchFile(file)) {
		rename(file,backfile);
		remove(file);
	}
	return TRUE;
}


void AddEquip24Dialog::MakeAllBackupFile()
{
	for(int16 i=0;i<6;i++){
		MakeBackupFile(FileName[i],BackFileName[i]);
	}
}

void AddEquip24Dialog::FileUnity()
{
	MakeAllBackupFile();

	for(int16 i=0;i<4;i++) {
		if(NameExistFile[i]&&KindExistFile[i]) AddFile(FileName[0],FileName[1],NameFileName[i],KindFileName[i],PodaeName[i]);
		if(SNameExistFile[i]&&SKindExistFile[i]) AddFile(FileName[2],FileName[3],SNameFileName[i],SKindFileName[i],PodaeName[i]);
		if(RNameExistFile[i]&&RKindExistFile[i]) AddFile(FileName[4],FileName[5],RNameFileName[i],RKindFileName[i],PodaeName[i]);
	}
}


void AddEquip24Dialog::MakeFileName()
{
	if(DaeDaeName) {
// Org File name
		strcpy(FileName[0],DaeDaeName);
		strcat(FileName[0],"name.dat");
		FileName[0][12]=NULL;
		strcpy(FileName[1],DaeDaeName);
		strcat(FileName[1],"kind.dat");
		FileName[0][12]=NULL;

		strcpy(FileName[2],DaeDaeName);
		strcat(FileName[2],"snme.dat");
		FileName[0][12]=NULL;
		strcpy(FileName[3],DaeDaeName);
		strcat(FileName[3],"sknd.dat");
		FileName[0][12]=NULL;

		strcpy(FileName[4],DaeDaeName);
		strcat(FileName[4],"rnme.dat");
		FileName[0][12]=NULL;
		strcpy(FileName[5],DaeDaeName);
		strcat(FileName[5],"rknd.dat");
		FileName[0][12]=NULL;

//Back file name
		strcpy(BackFileName[0],DaeDaeName);
		strcat(BackFileName[0],"name.bak");
		BackFileName[0][12]=NULL;
		strcpy(BackFileName[1],DaeDaeName);
		strcat(BackFileName[1],"kind.bak");
		BackFileName[0][12]=NULL;

		strcpy(BackFileName[2],DaeDaeName);
		strcat(BackFileName[2],"snme.bak");
		BackFileName[0][12]=NULL;
		strcpy(BackFileName[3],DaeDaeName);
		strcat(BackFileName[3],"sknd.bak");
		BackFileName[0][12]=NULL;

		strcpy(BackFileName[4],DaeDaeName);
		strcat(BackFileName[4],"rnme.bak");
		BackFileName[0][12]=NULL;
		strcpy(BackFileName[5],DaeDaeName);
		strcat(BackFileName[5],"rknd.bak");
		BackFileName[0][12]=NULL;

//Name file name& Kind file name
		strcpy(NameFileName[0],DaeDaeName);
		strcat(NameFileName[0],"name.hq");
		NameFileName[0][12]=NULL;
		strcpy(KindFileName[0],DaeDaeName);
		strcat(KindFileName[0],"kind.hq");
		KindFileName[0][12]=NULL;

		strcpy(NameFileName[1],DaeDaeName);
		strcat(NameFileName[1],"name.a");
		NameFileName[0][12]=NULL;
		strcpy(KindFileName[1],DaeDaeName);
		strcat(KindFileName[1],"kind.a");
		KindFileName[0][12]=NULL;

		strcpy(NameFileName[2],DaeDaeName);
		strcat(NameFileName[2],"name.b");
		NameFileName[0][12]=NULL;
		strcpy(KindFileName[2],DaeDaeName);
		strcat(KindFileName[2],"kind.b");
		KindFileName[0][12]=NULL;

		strcpy(NameFileName[3],DaeDaeName);
		strcat(NameFileName[3],"name.c");
		NameFileName[0][12]=NULL;
		strcpy(KindFileName[3],DaeDaeName);
		strcat(KindFileName[3],"kind.c");
		KindFileName[0][12]=NULL;

//Somo Name file name& Somo Kind file name
		strcpy(SNameFileName[0],DaeDaeName);
		strcat(SNameFileName[0],"snme.hq");
		NameFileName[0][12]=NULL;
		strcpy(SKindFileName[0],DaeDaeName);
		strcat(SKindFileName[0],"sknd.hq");
		KindFileName[0][12]=NULL;

		strcpy(SNameFileName[1],DaeDaeName);
		strcat(SNameFileName[1],"snme.a");
		SNameFileName[0][12]=NULL;
		strcpy(SKindFileName[1],DaeDaeName);
		strcat(SKindFileName[1],"sknd.a");
		SKindFileName[0][12]=NULL;

		strcpy(SNameFileName[2],DaeDaeName);
		strcat(SNameFileName[2],"snme.b");
		SNameFileName[0][12]=NULL;
		strcpy(SKindFileName[2],DaeDaeName);
		strcat(SKindFileName[2],"sknd.b");
		SKindFileName[0][12]=NULL;

		strcpy(SNameFileName[3],DaeDaeName);
		strcat(SNameFileName[3],"snme.c");
		SNameFileName[0][12]=NULL;
		strcpy(SKindFileName[3],DaeDaeName);
		strcat(SKindFileName[3],"sknd.c");
		SKindFileName[0][12]=NULL;

//Return Name file name& Return Kind file name
		strcpy(RNameFileName[0],DaeDaeName);
		strcat(RNameFileName[0],"rnme.hq");
		SNameFileName[0][12]=NULL;
		strcpy(RKindFileName[0],DaeDaeName);
		strcat(RKindFileName[0],"rknd.hq");
		SKindFileName[0][12]=NULL;

		strcpy(RNameFileName[1],DaeDaeName);
		strcat(RNameFileName[1],"rnme.a");
		RNameFileName[0][12]=NULL;
		strcpy(RKindFileName[1],DaeDaeName);
		strcat(RKindFileName[1],"rknd.a");
		RKindFileName[0][12]=NULL;

		strcpy(RNameFileName[2],DaeDaeName);
		strcat(RNameFileName[2],"rnme.b");
		RNameFileName[0][12]=NULL;
		strcpy(RKindFileName[2],DaeDaeName);
		strcat(RKindFileName[2],"rknd.b");
		RKindFileName[0][12]=NULL;

		strcpy(RNameFileName[3],DaeDaeName);
		strcat(RNameFileName[3],"rnme.c");
		RNameFileName[0][12]=NULL;
		strcpy(RKindFileName[3],DaeDaeName);
		strcat(RKindFileName[3],"rknd.c");
		RKindFileName[0][12]=NULL;
	}
}

