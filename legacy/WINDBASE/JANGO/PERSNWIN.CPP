#include "fltbttn.h"
#include "resbttn.h"
#include "boolent.h"
#include "typectrl.h"
#include "selctrl.h"
#include "chkctrl.h"
#include "strchkbx.h"
#include "obsview.h"
#include "passdlg.h"
#include "persnwin.h"

PersonWindow::PersonWindow(PersonManager& recman,PersonRecord& recbase,int16 currec,WORD *& srcharray,uint16& srchcount,BOOL enmodify,BOOL viewkmpi,JKControl *parent):
	RecordViewBase(JKRect(190,80,830,560),recbase,0,parent),RecordMan(recman),CurrentIndex(currec),SearchArray(srcharray),SearchCount(srchcount),EnModify(enmodify),ViewKMPI(viewkmpi)
{
	WinForm=0x07;
	WinAttr|=WA_TITLEMOVEABLE;
	if(!RecordMan.ManagerMode) EnModify=FALSE;
	SearchCount=0;
	SetText("ˆ  ·¥    ¯¥  ¬w");
}

PersonWindow::~PersonWindow()
{
}

void PersonWindow::InitWindow()
{
	RecordViewBase::InitWindow();
	if(CurrentIndex>=0)
		RecordMan.GetRecord(CurrentIndex,RefRecord);
	else
		RefRecord.InitData();
	ReLoad();
}

void PersonWindow::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKButton *bttn;

	rect.left+=10;
	rect.right=rect.left+100;
	rect.top=rect.bottom-40;
	rect.bottom=rect.top+30;
	if(RecordMan.ManagerMode&&CurrentIndex>=0){
		bttn=new JKFloatingButton(rect,ID_BUTTON+1);
		bttn->SetText("¡e ”q");
		AddControl(bttn);
	}
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	if(CurrentIndex>=0){
		bttn=new JKFloatingButton(rect,ID_BUTTON+2);
		bttn->SetText("Â‰ b");
		AddControl(bttn);
	}

	rect.left=rect.right+10;
	rect.right=rect.left+100;
	if(EnModify){
		bttn=new JKFloatingButton(rect,ID_BUTTON+3);
		bttn->SetText("ÑÂ ·¥");
		AddControl(bttn);
	}
	rect.left=rect.right+10;
	rect.right=rect.left+100;
	bttn=new JKFloatingButton(rect,ID_BUTTON+4);
	bttn->SetText("Âá ­¡");
	AddControl(bttn);


	AddControl(new TypeEntryControl(JKRect(100,10,220,30).ExpandBy(-1),ID_USERCONTROL+1,*RefRecord.GetEntry((uint16)0)));
	AddControl(new SelectEntryControl(JKRect(100,35,220,55).ExpandBy(-1),ID_USERCONTROL+2,*RefRecord.GetEntry(1)));
	AddControl(new SelectEntryControl(JKRect(100,60,220,80).ExpandBy(-1),ID_USERCONTROL+3,*RefRecord.GetEntry(2)));
	AddControl(new TypeEntryControl(JKRect(100,85,220,105).ExpandBy(-1),ID_USERCONTROL+4,*RefRecord.GetEntry(3)));
	AddControl(new TypeEntryControl(JKRect(100,110,220,130).ExpandBy(-1),ID_USERCONTROL+5,*RefRecord.GetEntry(4)));
	AddControl(new SelectEntryControl(JKRect(100,135,220,155).ExpandBy(-1),ID_USERCONTROL+6,*RefRecord.GetEntry(5)));
	AddControl(new TypeEntryControl(JKRect(100,160,220,180).ExpandBy(-1),ID_USERCONTROL+7,*RefRecord.GetEntry(6)));
	AddControl(new TypeEntryControl(JKRect(100,185,220,205).ExpandBy(-1),ID_USERCONTROL+8,*RefRecord.GetEntry(7)));

	CheckEntryControl *chkctrl = new CheckEntryControl(JKRect(100,210,120,230).ExpandBy(-1),ID_USERCONTROL+9,*RefRecord.GetEntry(8));
	AddControl(chkctrl);
	JKControl *ctrl=chkctrl->GetControl(ID_CHECKBOX+1);
	if(ctrl){
		ctrl->CloseWindow();
		StringCheckBox *strchk=new StringCheckBox(ctrl->WinRect,ID_CHECKBOX+1,"·q","´·");
		strchk->IsAutoControl=FALSE;
		strchk->BackColor=chkctrl->BackColor;
		strchk->TextColor=chkctrl->TextColor;
		strchk->Status=chkctrl->ChangeValue;
		chkctrl->AddControl(strchk);
	}

	AddControl(new TypeEntryControl(JKRect(120,210,220,230).ExpandBy(-1),ID_USERCONTROL+10,*RefRecord.GetEntry(9)));
	AddControl(new TypeEntryControl(JKRect(100,235,220,255).ExpandBy(-1),ID_USERCONTROL+11,*RefRecord.GetEntry(10)));
	AddControl(new TypeEntryControl(JKRect(100,260,220,280).ExpandBy(-1),ID_USERCONTROL+12,*RefRecord.GetEntry(11)));
	AddControl(new SelectEntryControl(JKRect(100,285,209,305).ExpandBy(-1),ID_USERCONTROL+13,*RefRecord.GetEntry(12)));
	AddControl(new TypeEntryControl(JKRect(210,285,420,305).ExpandBy(-1),ID_USERCONTROL+14,*RefRecord.GetEntry(13)));
	AddControl(new SelectEntryControl(JKRect(100,310,209,330).ExpandBy(-1),ID_USERCONTROL+15,*RefRecord.GetEntry(14)));
	AddControl(new TypeEntryControl(JKRect(210,310,420,330).ExpandBy(-1),ID_USERCONTROL+16,*RefRecord.GetEntry(15)));

	AddControl(new SelectEntryControl(JKRect(300,10,359,30).ExpandBy(-1),ID_USERCONTROL+17,*RefRecord.GetEntry(17)));
	AddControl(new SelectEntryControl(JKRect(360,10,420,30).ExpandBy(-1),ID_USERCONTROL+18,*RefRecord.GetEntry(18)));
	AddControl(new CheckEntryControl(JKRect(300,35,320,55).ExpandBy(-1),ID_USERCONTROL+19,*RefRecord.GetEntry(19)));
	AddControl(new TypeEntryControl(JKRect(300,60,420,80).ExpandBy(-1),ID_USERCONTROL+20,*RefRecord.GetEntry(20)));
	AddControl(new TypeEntryControl(JKRect(300,85,420,105).ExpandBy(-1),ID_USERCONTROL+21,*RefRecord.GetEntry(16)));
	AddControl(new SelectEntryControl(JKRect(300,110,420,130).ExpandBy(-1),ID_USERCONTROL+22,*RefRecord.GetEntry(22)));
	AddControl(new SelectEntryControl(JKRect(300,135,420,155).ExpandBy(-1),ID_USERCONTROL+23,*RefRecord.GetEntry(23)));
	AddControl(new TypeEntryControl(JKRect(300,160,420,180).ExpandBy(-1),ID_USERCONTROL+24,*RefRecord.GetEntry(24)));
	AddControl(new SelectEntryControl(JKRect(300,185,420,205).ExpandBy(-1),ID_USERCONTROL+25,*RefRecord.GetEntry(27)));
	AddControl(new SelectEntryControl(JKRect(300,210,420,230).ExpandBy(-1),ID_USERCONTROL+26,*RefRecord.GetEntry(28)));
	AddControl(new TypeEntryControl(JKRect(300,235,420,255).ExpandBy(-1),ID_USERCONTROL+27,*RefRecord.GetEntry(29)));
	AddControl(new TypeEntryControl(JKRect(300,260,420,280).ExpandBy(-1),ID_USERCONTROL+28,*RefRecord.GetEntry(30)));

	AddControl(new SelectEntryControl(JKRect(500,10,620,30).ExpandBy(-1),ID_USERCONTROL+29,*RefRecord.GetEntry(34)));
	AddControl(new TypeEntryControl(JKRect(500,35,620,55).ExpandBy(-1),ID_USERCONTROL+30,*RefRecord.GetEntry(32)));
	AddControl(new TypeEntryControl(JKRect(500,60,620,80).ExpandBy(-1),ID_USERCONTROL+31,*RefRecord.GetEntry(39)));
	AddControl(new TypeEntryControl(JKRect(500,85,620,105).ExpandBy(-1),ID_USERCONTROL+32,*RefRecord.GetEntry(40)));

	AddControl(new TypeEntryControl(JKRect(500,110,559,130).ExpandBy(-1),ID_USERCONTROL+33,*RefRecord.GetEntry(41)));
	AddControl(new TypeEntryControl(JKRect(560,110,620,130).ExpandBy(-1),ID_USERCONTROL+34,*RefRecord.GetEntry(42)));

	AddControl(new SelectEntryControl(JKRect(500,135,620,155).ExpandBy(-1),ID_USERCONTROL+35,*RefRecord.GetEntry(43)));
	AddControl(new TypeEntryControl(JKRect(500,160,620,180).ExpandBy(-1),ID_USERCONTROL+36,*RefRecord.GetEntry(46)));
	AddControl(new TypeEntryControl(JKRect(500,185,620,205).ExpandBy(-1),ID_USERCONTROL+37,*RefRecord.GetEntry(47)));
	AddControl(new SelectEntryControl(JKRect(500,210,620,230).ExpandBy(-1),ID_USERCONTROL+38,*RefRecord.GetEntry(50)));

	AddControl(new TypeEntryControl(JKRect(500,235,620,255).ExpandBy(-1),ID_USERCONTROL+39,*RefRecord.GetEntry(60)));
	AddControl(new TypeEntryControl(JKRect(500,260,620,280).ExpandBy(-1),ID_USERCONTROL+40,*RefRecord.GetEntry(61)));
	AddControl(new TypeEntryControl(JKRect(500,285,620,305).ExpandBy(-1),ID_USERCONTROL+41,*RefRecord.GetEntry(62)));
	AddControl(new TypeEntryControl(JKRect(500,310,620,330).ExpandBy(-1),ID_USERCONTROL+42,*RefRecord.GetEntry(63)));

	if(ViewKMPI)
		AddControl(new SelectEntryControl(JKRect(500,335,620,355).ExpandBy(-1),ID_USERCONTROL+43,*RefRecord.GetEntry(54)));
}

void PersonWindow::PaintClient(JKDC& dc,JKRect&)
{
	dc.SetBackColor(BackColor);
	dc.SetTextColor(Black);
	dc.TextOutX(JKRect(0,10,99,30),"¬÷¡w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,35,99,55),"‰‹s",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,60,99,80),"­¡­¢",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,85,99,105),"Š…¤å",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,110,99,130),"ºËb‹¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,135,99,155),"»¢À‚",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,160,99,180),"·³”·©",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,185,99,205),"¥¡»¢·©",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,210,99,230),"¬—‘e¶©·©",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,235,99,255),"º£¥—w¢¤åÑ¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,260,99,280),"¸åÑÁ¤åÑ¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,285,99,305),"¥¥¸â",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(0,310,99,330),"º­¡",ADJ_RIGHT|ADJ_YCENTER);

	dc.TextOutX(JKRect(230,10,299,30),"¦¡¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(325,35,399,55),"¥iˆáµa¦",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,60,299,80),"Ñw¹A",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,85,299,105),"¸a‰b»w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,110,299,130),"ºˆá",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,135,299,155),"‰iÑ¥‰Å‰",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,160,299,180),"´·¥",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,185,299,205),"»³­¡·A",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,210,299,230),"»³ÑwÈ",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,235,299,255),"ˆåÍw",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(230,260,299,280),"¤w",ADJ_RIGHT|ADJ_YCENTER);

	dc.TextOutX(JKRect(430,10,499,30),"ÂA¹·Ðbb",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,35,499,55),"Ðb‰Á¡w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,60,499,80),"¯¥¸w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,85,499,105),"ÁAº—",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,110,499,130),"¯¡b",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,135,499,155),"Ñi´‚Ñw",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,160,499,180),"Âá£¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,185,499,205),"Ëb‹¡",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,210,499,230),"¹·Ša",ADJ_RIGHT|ADJ_YCENTER);

	dc.TextOutX(JKRect(430,235,499,255),"¸åÊ¥¢¬w",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,260,499,280),"¸åÊ¥¢Ða·",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,285,499,305),"¸åÊÑÁ",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(430,310,499,330),"Á·¤å",ADJ_RIGHT|ADJ_YCENTER);
	if(ViewKMPI)
		dc.TextOutX(JKRect(430,335,499,355),"»¡Òá‰Å¯±",ADJ_RIGHT|ADJ_YCENTER);
}

void PersonWindow::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			{
			JKControl *ctrl=GetControl(ID_BUTTON+1);
			if(ctrl&&CurrentIndex>=0){
				if(PasswordDialog(JKRect(180,150,459,329),9,"´qÑ¡Ÿi ·³bÐa¯³¯¡µ¡").Run()==ID_OK){
					JKRect rect(10,10,600,400);
					JKDialog *dlg=new ObserveView(rect,0,RecordMan,(PersonRecord& )*RecordMan.WorkRecord,CurrentIndex,this);
					if(dlg){
						ctrl->HideWindow();
						dlg->IsModal=FALSE;
						dlg->Run();	delete dlg;
						ctrl->ShowWindow();
					}
				}
			}
			}
			break;
		case ID_BUTTON+2:
			if(CurrentIndex>=0) PrintRecord();
			break;
		case ID_BUTTON+3:
			SendMessage(ID_OK);
			break;
		case ID_BUTTON+4:
			SendMessage(ID_CANCEL);
			break;
		default:
			break;
	}
}

void PersonWindow::EvShowWindow()
{
	RecordViewBase::EvShowWindow();
	JKControl* ctrl=GetControl(ID_BUTTON+3);
	if(ctrl) ctrl->SetFocus();
}

void PersonWindow::EvDeadKeyDown(uint16 key,WORD /*modifier*/)
{
	WORD ctrlid;
	JKControl* ctrl;
	WORD LastCtrlID = ViewKMPI? ID_USERCONTROL+43:ID_USERCONTROL+42;
	if(key==PGDNKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>=ID_USERCONTROL+1&&ctrlid<LastCtrlID) ctrlid++;
			else if(ctrlid==LastCtrlID) {
				ctrl=GetControl(ID_BUTTON+1);
				if(ctrl&&ctrl->IsShow&&!(ctrl->WinAttr&WA_NOHASFOCUS))
					ctrlid=ID_BUTTON+1;
				else
					ctrlid=ID_BUTTON+2;
			}
			else if(ctrlid==ID_BUTTON+1) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) {
				ctrl=GetControl(ID_BUTTON+3);
				if(ctrl&&ctrl->IsShow&&!(ctrl->WinAttr&WA_NOHASFOCUS))
					ctrlid=ID_BUTTON+3;
				else
					ctrlid=ID_BUTTON+4;
			}
			else if(ctrlid==ID_BUTTON+3) ctrlid=ID_BUTTON+4;
			else ctrlid=ID_USERCONTROL+1;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
	else if(key==PGUPKEY){
		if(FindSubWindow(MsgFilter->FocusWinID,ctrlid,TRUE)&&ctrlid){
			if(ctrlid>ID_USERCONTROL+1&&ctrlid<=LastCtrlID) ctrlid--;
			else if(ctrlid==ID_USERCONTROL+1) ctrlid=ID_BUTTON+4;
			else if(ctrlid==ID_BUTTON+4) {
				ctrl=GetControl(ID_BUTTON+3);
				if(ctrl&&ctrl->IsShow&&!(ctrl->WinAttr&WA_NOHASFOCUS))
					ctrlid=ID_BUTTON+3;
				else
					ctrlid=ID_BUTTON+2;
			}
			else if(ctrlid==ID_BUTTON+3) ctrlid=ID_BUTTON+2;
			else if(ctrlid==ID_BUTTON+2) {
				ctrl=GetControl(ID_BUTTON+1);
				if(ctrl&&ctrl->IsShow&&!(ctrl->WinAttr&WA_NOHASFOCUS))
					ctrlid=ID_BUTTON+1;
				else
					ctrlid=LastCtrlID;
			}
			else if(ctrlid==ID_BUTTON+1) ctrlid=LastCtrlID;
			else ctrlid=ID_BUTTON+4;

			ctrl=GetControl(ctrlid);
			if(ctrl) ctrl->SetFocus();
		}
	}
}

void PersonWindow::AddSearchEntry(WORD index)
{
	for(uint16 i=0;i<SearchCount;i++)
		if(SearchArray[i]==index) return;
	if(SearchCount<RefRecord.NodeCount) SearchArray[SearchCount++]=index;
}


void PersonWindow::EvEntryCtrlEndModify(WORD ctrlid)
{
	RecordViewBase::EvEntryCtrlEndModify(ctrlid);
	EntryControl *entctrl=(EntryControl *)GetControl(ctrlid);
	if(entctrl)	AddSearchEntry(entctrl->RefEntry.EntryID);

}

BOOL PersonWindow::PrintRecord()
{
	WORD EntryID[42]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,16,22,23,24,27,28,29,30,34,39,40,41,42,43,46,47,50,60,61,62,63,54};
	char *EntryName[42]={
		"\r\n        ¬÷¡w : ",
		"\r\n        ‰‹s : ",
		"\r\n        ­¡­¢ : ",
		"\r\n        Š…¤å : ",
		"\r\n      ºËb‹¡ : ",
		"\r\n        »¢À‚ : ",
		"\r\n      ·³”·© : ",
		"\r\n      ¥¡»¢·© : ",
		"\r\n    ¬—‘e¶©·© : ",
		" ",
		"\r\nº£¥—w¢¤åÑ¡ : ",
		"\r\n    ¸åÑÁ¤åÑ¡ : ",
		"\r\n        ¥¥¸â : ",
		" ",
		"\r\n        º­¡ : ",
		" ",
		"\r\n        ¦¡¡ : ",
		"  ",
		"    ",
		"\r\n        Ñw¹A : ",
		"\r\n      ¸a‰b»w : ",
		"\r\n        ºˆá : ",
		"\r\n    ‰iÑ¥‰Å‰ : ",
		"\r\n        ´·¥ : ",
		"\r\n      »³­¡·A : ",
		"\r\n      »³ÑwÈ : ",
		"\r\n        ˆåÍw : ",
		"\r\n          ¤w : ",
		"\r\n    ÂA¹·Ðbb : ",
		"\r\n        ¯¥¸w : ",
		"\r\n        ÁAº— : ",
		"\r\n        ¯¡b : ",
		"",
		"\r\n      Ñi´‚Ñw : ",
		"\r\n        Âá£¡ : ",
		"\r\n        Ëb‹¡ : ",
		"\r\n        ¹·Ša : ",
		"\r\n  ¸åÊ¥¢¬w· : ",
		"\r\n  ¸åÊ¥¢Ða· : ",
		"\r\n      ¸åÊÑÁ : ",
		"\r\n        Á·¤å : ",
		"\r\n    »¡Òá‰Å¯± : "
	};


	char linefeed[3]="\r\n";

	REFENTRY refentry;
	if(RecordMan.GetRefEntry(CurrentIndex,refentry)&&refentry.HasData){
		if(!PrintMan->PrintString("  <<    ˆ      ·¥           ¯¥      ¬w    >>"))	return FALSE;
		if(!PrintMan->PrintString(linefeed))	return FALSE;
		if(!PrintMan->PrintString(linefeed))	return FALSE;
		PersonRecord TempRecord;
		RecordMan.GetRecord(CurrentIndex,TempRecord);
		uint16 maxbuflen = TempRecord.GetMaxBufferSize();
		BYTE * entbuffer = new BYTE[maxbuflen+1];
		EntryBase *ent;
		uint16 count=ViewKMPI? 42:41;
		for(uint16 i=0;i<count;i++){
			ent = TempRecord.GetEntry(EntryID[i]);
			if(ent){
				if(!PrintMan->PrintString(EntryName[i]))	return FALSE;
				if(EntryID[i]==8){
					BooleanEntry *boolent=(BooleanEntry *)ent;
					strcpy(entbuffer,boolent->DataBoolean? "(·q)":"(´·)");
				}
				else if(EntryID[i]==19){
					BooleanEntry *boolent=(BooleanEntry *)ent;
					strcpy(entbuffer,boolent->DataBoolean? "¥iˆáº—":"");
				}
				else ent->GetData(entbuffer);

				if(!PrintMan->PrintString(entbuffer)) return FALSE;
			}
		}
		delete[] entbuffer;
		if(!PrintMan->PrintCharacter(FF)) return FALSE;
		return TRUE;
	}
	return FALSE;
}
