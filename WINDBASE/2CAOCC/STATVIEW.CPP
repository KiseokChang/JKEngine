#include "resbttn.h"
#include "jkedit.h"
#include "combobox.h"
#include "strchkbx.h"
#include "reschkbx.h"
#include "toolbox.h"
#include "hshowblk.h"
#include "vshowblk.h"
#include "showprfx.h"
#include "cordview.h"
#include "statbar.h"
#include "showcur.h"
#include "popupstc.h"

#include "battman.h"
#include "xbattman.h"
#include "tgman.h"
#include "fireman.h"
#include "airman.h"
#include "howizman.h"

#include "battdlg.h"
#include "xbattdlg.h"
#include "tgdlg.h"

#include "getwidth.h"
#include "selcolor.h"
#include "selattr.h"

#include "cmdmsgbx.h"

#include "occmsg.h"
#include "basefunc.h"
#include "statview.h"


#define OCCMessageCount 36

static WORD OCCMessageControlID[OCCMessageCount]={
	ID_USERCONTROL+4, ID_LISTBOX+1,   ID_CHECKBOX+0x100+1, ID_CHECKBOX+0x100+2, ID_CHECKBOX+0x100+3,
	ID_BUTTON+1,      ID_BUTTON+2,    ID_BUTTON+3,         ID_EDIT+1,					  ID_CHECKBOX+0x200+1,
	ID_CHECKBOX+ 1,   ID_CHECKBOX+ 2, ID_CHECKBOX+ 3,      ID_CHECKBOX+ 4,      ID_CHECKBOX+ 5,
	ID_CHECKBOX+ 6,   ID_CHECKBOX+ 7, ID_CHECKBOX+ 8,      ID_CHECKBOX+ 9,      ID_CHECKBOX+10,
	ID_CHECKBOX+11,   ID_CHECKBOX+12, ID_CHECKBOX+13,      ID_CHECKBOX+14,      ID_CHECKBOX+15,
	ID_CHECKBOX+16,   ID_CHECKBOX+17, ID_CHECKBOX+18,      ID_CHECKBOX+19,      ID_CHECKBOX+20,
	ID_CHECKBOX+21,   ID_CHECKBOX+22, ID_CHECKBOX+23,      ID_CHECKBOX+24,      ID_USERCONTROL+3,
	ID_STATIC+1
};
static char *OCCMessageString[OCCMessageCount]={
	"Ñe ¹ÁÎa",               " a¶¯a ‹¡“w",       "¬wÑ×•¡µA •¡¡/ˆw ¥¡‹¡","¬wÑ×•¡µA ‰¡­¢•¡¡/Àé•¡ ¥¡‹¡","¬wÑ×•¡µA •¡‰w‰/ ¬e ¥¡‹¡",
	"¸åŠ‚»¡•¡¥¡‹¡","Ò…e»¡µb¥¡‹¡",      "‹aŸ±‹¡“w ¸b¬÷",        "ÑÁ·© ·¡Ÿq",                    "‰·‰b/¤w´á ¸åÑÅ",
	"¸â Í¡¥w¦” Îa¯¡",      "¸â ¥¡¥w¦” Îa¯¡",  "¸â Ëb® ¦” Îa¯¡",    "¸â ‹¡ˆs¦” Îa¯¡",             "¸â Ð‚¯±Îa¸â Îa¯¡",
	"¸â ¸åÊ»¡‰w¬å",         "¸â ‹¡•·‰ÒB",       "´aŠ… Í¡¥w¦” Îa¯¡",   "´aŠ… ‹¡Èa¦” Îa¯¡",           "´aŠ… ¸åÊ»¡‰w¬å",
	"´aŠ… ‹¡•·‰ÒB",         "ÑÁb»¡¶¥Ñs¹¡¤w¤ó",  "¬a‰b“wb•¡",           "¹·Ðs¸w´¢‰",                   "¸å¸w¸÷¥¡¦…¬â 1”e‰",
	"¸å¸w¸÷¥¡¦…¬â 2”e‰",    "¸å¸w¸÷¥¡¦…¬â 3”e‰","‰¡¬a‰ ¦…¬â",          "»¡žA»¡µb ¦…¬â",                "ICM»¡µb ¦…¬â",
	"¬qˆb¸ñ",                "‰·µbÉ·¹A",          "¸â µ¬wÍ¡»¥»¡/‰ÅÃb­¡", "¸âµ¬w ¸ó‹e¡ ",               "¬wÑ×•¡ ¶áÃ¡ ¸÷¥¡",
	"Copyright   K.H.S  &  J.K.S     1996.3"
};

/*
static BYTE OCCMessagePosFlag[OCCMessageCount]={
	ADJ_YNEXT,ADJ_YNEXT,ADJ_YNEXT,ADJ_YNEXT,ADJ_YNEXT,
	ADJ_YNEXT,ADJ_YNEXT,ADJ_YNEXT,ADJ_YNEXT,ADJ_YNEXT,
	ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,
	ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,
	ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,
	ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,
	ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_XPREV,ADJ_YPREV,
	0
};
*/
static JKPoint OCCMessageBoxSize[OCCMessageCount]={
	JKPoint(200,30),JKPoint(200,30),JKPoint(200,50),JKPoint(200,50),JKPoint(200,50),
	JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),
	JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),
	JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),
	JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),
	JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),
	JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),JKPoint(200,30),
	JKPoint(400,30)
};

#define WKIND_NOWORK		0
#define WKIND_MOVING		1
#define WKIND_ZOOMING		2


#define CheckCount	24
BOOL SaveCheck[CheckCount]={
	TRUE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,
	TRUE,FALSE,FALSE,FALSE,
	FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE
};

StatusView::StatusView(JKRect& rect,JKControl *parent):
	JKWindow(rect,parent),StatMemDC(0),BaseMemDC(0),
	BlockSize(500,500),MoveInterval(1000,1000),
	IsSVEditing(FALSE),PaintStatMeta("noname.smf"),
	WorkKind(WKIND_NOWORK),Working(FALSE),
	MessageIndex(-1)
{
	WinForm=0x01;
	WinAttr|=WA_FIRSTSHOW;//|WA_TITLEMOVEABLE|WA_BORDERRESIZABLE;
	Depth=1;
	BorderSize=2;
	SetText("¦” ¶áÃ¡ ¬wÑ×");
	BackColor=255;
	MtPolyLine.Width=4;
}

StatusView::~StatusView()
{
	if(BaseMemDC)	delete BaseMemDC;
}

JKRect StatusView::CalcClientRect()
{
	JKRect rect=JKWindow::CalcClientRect();
	MinimumSize+=JKPoint(31,51);
	rect.top+=30;
	rect.bottom-=30+30;
	rect.left+=40;
	rect.right-=30;
	return rect;
}

void StatusView::InitWindow()
{
	JKWindow::InitWindow();
	JKPoint32 clntsize(ClientRect.Width()+1,ClientRect.Height()+1);
	BaseMemDC=new JKMemoryDC(clntsize.x,clntsize.y,BackColor);
  StatMemDC=new JKMemoryDC(clntsize.x,clntsize.y,BackColor);
	RealCoordMan.ClientSize=clntsize;
	JKPoint32 real;
	if(RealCoordMan.StrToReal("CS90003000",real))
		SetClientRealRect(JKRect32(real.x-6000,real.y-5000,real.x+6000,real.y+5000));
	EvChangeBase();
}

void StatusView::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy();
	JKControl *ctrl;
	JKRect temp;
	temp=rect;
	temp.top-=27;	temp.bottom=temp.top+23;

	temp.left-=40; temp.right=temp.left+100;
	ctrl=new CoordView(temp.ExpandBy(-1),ID_USERCONTROL+4,RealCoordMan);
	AddControl(ctrl);


	temp.left=temp.right+10; temp.right=temp.left+100;
	HuboManager *huboman = new HuboManager();
	if(huboman){
		huboman->Add(-1,new HuboBase("·©¤e‹¡“w",0));
		huboman->Add(-1,new HuboBase("¶áÃ¡·¡•·",1));
		huboman->Add(-1,new HuboBase("ÑÂ”Â‚­¡",2));
		ctrl=new Combobox(JKRect(temp.left,temp.top,temp.right,temp.bottom+100),ID_LISTBOX+1,4,huboman,FALSE);
		ctrl->SetText("·©¤e‹¡“w");
		ctrl->WinAttr|=WA_NOINCLIENT;
		AddControl(ctrl);
	}

	JKCheckBox *chk;
	char *chktext[3]={
		"•¡¡/ˆw","‰¡­¢•¡¡","‰w‰/¬e"
	};
	temp.right+=30;
	for(uint16 i=0;i<3;i++){
		temp.left=temp.right+1+5; temp.right=temp.left+90;
		ctrl=new JKCheckBox(temp,ID_CHECKBOX+0x100+1+i);
		ctrl->SetText(chktext[i]);
		ctrl->WinAttr|=WA_NOINCLIENT;
		ctrl->BackColor=Gray;
		ctrl->TextColor=White;
		AddControl(ctrl);
	}

	temp.right+=30;
	for(i=0;i<3;i++){
		temp.left=temp.right+1+5; temp.right=temp.left+23;
		ctrl=new JKResButton(temp,ID_BUTTON+1+i,0x400+i);
		ctrl->WinAttr|=WA_NOINCLIENT;
		AddControl(ctrl);
	}

	temp.left=temp.right+1+5; temp.right=temp.left+150;
	ctrl=new JKEdit(temp,ID_EDIT+1,PaintStatMeta.MetaFileName,20,FALSE,FALSE);
	ctrl->SetText("noname.smf");
	ctrl->WinAttr&=~BYTE(WA_FIRSTSHOW);
	ctrl->WinAttr|=WA_NOINCLIENT;
	AddControl(ctrl);

	temp.right=rect.right;
	temp.right-=20;
	temp.left=temp.right-50;
	chk =new StringCheckBox(temp.ExpandBy(2),ID_CHECKBOX+0x200+1,"‰·‰b","¤w´á");
	chk->BackColor=LtRed;
	chk->TextColor=Yellow;
	chk->Status=FALSE;
	chk->WinAttr|=WA_NOINCLIENT;
	AddControl(chk);

	temp=rect;
	temp.right+=27;	temp.left=temp.right-23;
	temp.bottom=temp.top;
	for(i=0;i<CheckCount;i++){
		if(i==7||i==11) temp.bottom+=10;
		temp.top=temp.bottom+1; temp.bottom=temp.top+23;
		chk=new ResCheckBox(temp,ID_CHECKBOX+1+i,0x4000+i*2+1,0x4000+i*2);
		chk->Status=SaveCheck[i];
		chk->WinAttr|=WA_NOINCLIENT;
		AddControl(chk);
	}


	temp=rect;
	temp.bottom+=30;	temp.top=temp.bottom-29;
	ctrl=new HBlockShow(temp,ID_USERCONTROL+1,BlockSize,RealCoordMan);
	AddControl(ctrl);
	temp=rect;
	temp.left-=40; temp.right=temp.left+39;
	ctrl=new VBlockShow(temp,ID_USERCONTROL+2,BlockSize,RealCoordMan);
	AddControl(ctrl);
	temp=rect;
	temp.left-=40; temp.right=temp.left+39;
	temp.bottom+=30;	temp.top=temp.bottom-29;
	ctrl=new PrefixShow(temp,ID_USERCONTROL+3,RealCoordMan);
	AddControl(ctrl);

	temp=rect;
	temp.left-=40; temp.right+=30;
	temp.bottom+=30+30;	temp.top=temp.bottom-29;
	AddControl(new StatusBar(temp,ID_STATIC+1));

	rect=WinRect.SetLeftTopBy(-ClientRect.LeftTop());
	rect.left=rect.right+1;
	rect.right=rect.left+100;

	rect.bottom=rect.top+200;
	AddControl(new ToolBox(rect,ID_SUBWINDOW+1));

/*
	JKStatic *stc= new PopupStatic(JKRect(),ID_USERCONTROL+6);
	stc->WinAttr|=WA_NORESTRICT;
	stc->AdjustFlag=ADJ_XYCENTER;
	stc->BackColor=Yellow;
	AddControl(stc);
*/
}

void StatusView::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
}

void StatusView::PaintWindow(JKDC& dc,JKRect& paintrect)
{
	JKWindow::PaintWindow(dc,paintrect);

	JKRect rect=ClientRect;
	rect.top-=30;	 rect.bottom=rect.top+29;
	rect.left-=40; rect.right+=30;
	dc.SolidBar(rect,Gray);

	rect=ClientRect;
	rect.left=rect.right+1; rect.right=rect.left+29;
	rect.bottom+=30;
	dc.SolidBar(rect,Gray);

	rect.left+=3;	rect.right-=3;

	JKRect temp=rect;
	temp.bottom=temp.top;

	temp.top=temp.bottom+1; temp.bottom=temp.top+24*7-1;
	dc.SolidBar(temp.ExpandBy(2),LtMagenta);
	temp.bottom+=10;
	temp.top=temp.bottom+1; temp.bottom=temp.top+24*4-1;
	dc.SolidBar(temp.ExpandBy(2),LtCyan);
	temp.bottom+=10;
	temp.top=temp.bottom+1; temp.bottom=temp.top+24*13-1;
	dc.SolidBar(temp.ExpandBy(2),LtGreen);
}

/***************************************************************/
void StatusView::PaintClient(JKDC& dc,JKRect& paintrect)
{
	if(StatMemDC) dc.PutImage(JKPoint(0,0),StatMemDC->DataBuffer,COPY_PUT,paintrect);
	if(IsSVEditing)
		PaintStatMeta.Draw(dc,RealCoordMan);
}
/****************************************************************/

/////////////////////////////////////////////////////////////////////////

void StatusView::EvCtrlOnlyMove(WORD ctrlid)
{
	ShowExplain(ctrlid);
}

void StatusView::EvCtrlLeftClick(WORD)
{
	HideExplain();
}

ProcessResult StatusView::RespondMessage(JKMSG& msg)
{
	if(!msg.winid){
		if(MessageIndex!=-1){
			if(msg.main==ONLYMOVE||msg.main==ELONLYMOVE||msg.main==CONTROL_LEFTCLICK||msg.main==CONTROL_RIGHTCLICK)
				HideExplain();
			else if(msg.main==NCONLYMOVE){
				JKPoint p=JKPoint(LOWORD(msg.sub),HIWORD(msg.sub));
				if(!IsContainPoint(p,TRUE)) HideExplain();
			}
		}
	}
	return JKWindow::RespondMessage(msg);
}
///////////////////////////////////////////////////////////////////////////
void StatusView::EvKeyDown(WORD key,WORD)
{
	switch(key){
		case LEFTARROW:
		case RIGHTARROW:
		case UPARROW:
		case DOWNARROW:
			{
			JKControl *ctrl;
			JKRect32 realrect;
			if(!RealCoordMan.GetClientRealRect(realrect)) return;

			if(key==LEFTARROW){
				if(realrect.left>BlockSize.x) RealCoordMan.RealOrgPos.x-=BlockSize.x;
				else return;
			}
			else if(key==RIGHTARROW){
				if(realrect.right<100000-BlockSize.x)	RealCoordMan.RealOrgPos.x+=BlockSize.x;
				else return;
			}
			else if(key==DOWNARROW){
				if(realrect.top>BlockSize.y) RealCoordMan.RealOrgPos.y-=BlockSize.y;
				else return;
			}
			else if(key==UPARROW){
				if(realrect.bottom<100000-BlockSize.y) RealCoordMan.RealOrgPos.y+=BlockSize.y;
				else return;
			}
      SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGEBASE);
			}
			break;
		default:
			break;
	}
}

void StatusView::DrawBlock(JKDC& dc,JKPoint32 blocksize)
{
	JKRect32 realrect;
	if(!RealCoordMan.GetClientRealRect(realrect)) return;

	int16 wmode=dc.WriteMode;
	dc.SetWriteMode(XOR_PUT);
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(TextColor);

	JKRect rect=GetClientRect();
	int16 width=rect.Width()+1,height=rect.Height()+1;

	int32 i,t;
	JKPoint p;
	i=(realrect.top+blocksize.y-1)/blocksize.y*blocksize.y;
	for(;i<=realrect.bottom;i+=blocksize.y){
		t=i;
		if(RealCoordMan.RealToPixel_Y(t)){
			p.Set(rect.left,t);
			dc.HLine(p,width,255);
		}
	}

	i=(realrect.left+blocksize.x-1)/blocksize.x*blocksize.x;
	for(;i<=realrect.right;i+=blocksize.x){
		t=i;
		if(RealCoordMan.RealToPixel_X(t)){
			p.Set(t,rect.top);
			dc.VLine(p,height,255);
		}
	}
	dc.SetWriteMode(wmode);
}

void StatusView::DrawBase(JKDC& dc,JKRect32 paintrect)
{
	uint16 indexarray[3]={0x100,0x101,0x102};
	BOOL chaeckvalue;
	char *fname[3]={"yu.smf","newline.smf","belt.smf"};

	for(uint16 i=0;i<3;i++){
		if(GetCheck(indexarray[i],chaeckvalue)&&chaeckvalue){
			PaintStatMeta.Draw(dc,RealCoordMan,fname[i],paintrect);
		}
	}
}
void StatusView::DrawStatMeta(JKDC& dc,JKRect32 paintrect)
{
	char *fnamearray[2][14]={
		{
			"eline.smf",	"aline.smf",	"scp.smf",		"eonlyln.smf","hiang.smf",
			"fascam.smf",	"icm.smf",		"obstcl.smf",	"ctrl.smf",		"ipb1.smf",
			"ipb2.smf",  	"ipb3.smf", 	"emove.smf",  "amove.smf"
		},
		{
			"eline_.smf",	"aline_.smf",	"scp_.smf",	  "eonlyln_.smf","hiang_.smf",
			"fascam_.smf","icm_.smf",	  "obstcl_.smf","ctrl_.smf",	 "ipb1_.smf",
			"ipb2_.smf",  "ipb3_.smf",  "emove_.smf", "amove_.smf"
		}
	};

	uint16 indexarray[14]={5,9,20,23,17, 18,19,13,11,14, 15,16,6,10};

	BOOL isattack;
	GetCheck(0x200,isattack);
	BOOL chaeckvalue;
	for(uint16 i=0;i<14;i++){
		uint16 index=indexarray[i];
		char *str=fnamearray[isattack? 1:0][i];
		if(GetCheck(index,chaeckvalue)&&chaeckvalue){
			PaintStatMeta.Draw(dc,RealCoordMan,str,paintrect);
		}
	}
}

int16 StatusView::GetBattalionResKind(WORD resid)
{
	if(resid>=0x4000+0x200 && resid<0x4000+0x200+61){
		resid-=0x4000+0x200;
		if(resid<15) return 0;
		return 1;
	}
	return 1;
}


void StatusView::DrawBattalion(JKDC& dc)
{
	BOOL checkvalue[3];
	GetCheck(7,checkvalue[0]); GetCheck(8,checkvalue[1]);
	GetCheck(12,checkvalue[2]);

	uint16 status;
	BattalionRecord rec;
	JKPoint32 coord;
	REFENTRY refentry;
	int16	index=BattalionMan->TableMan.TableHead;
	while(index!=-1&&BattalionMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			BattalionMan->RecordMan.GetRecord(index,rec);
			if(rec.GetCoord(coord)){
				JKPoint32 pixcoord(coord);
				if(RealCoordMan.RealToPixel(pixcoord,FALSE)){
					WORD resid=rec.GetResID();
					if(checkvalue[GetBattalionResKind(resid)]){
						ResourceBase *res=ResMan->GetResource(resid);
						if(res){
							BYTE *image = res->AllocRealData();
							if(image)	{
								dc.PutImage(JKPoint(pixcoord.x-12,pixcoord.y-12),image+24*24+4,AND_PUT);
								dc.PutImage(JKPoint(pixcoord.x-12,pixcoord.y-12),image,OR_PUT);
								res->FreeRealData(image);
							}
						}

						status=rec.GetStatus();
						JKRect temprect(pixcoord.x-15,pixcoord.y-15,pixcoord.x+14,pixcoord.y+14);
						BYTE color = status==1? LtBlue:(status==2? LtRed:LtGreen);
						dc.Rectangle3D(temprect,3,color,color);
						temprect.Set(pixcoord.x-25,pixcoord.y+10,pixcoord.x+24,pixcoord.y+26);
            dc.SolidBar(temprect,color);
						char budaename[6];
						rec.GetData(1,budaename);
						::DeleteWhiteSpace(budaename);
						dc.IsOverWrite=FALSE;
						dc.SetTextColor(Black);
						dc.TextOutX(temprect.OffsetBy(JKPoint(2,2)),budaename,ADJ_XCENTER|ADJ_BOTTOM,FALSE);
						dc.SetTextColor(Yellow);
						dc.TextOutX(temprect,budaename,ADJ_XCENTER|ADJ_BOTTOM,FALSE);
						if(checkvalue[2]){
//////////////////////////////////////////////
							if(status==1) {
								EntryBase *ent=rec.GetEntry(4)->Duplicate();
								if(ent) {
									ent->EntryID=0;
									int16 howindex=HowitzerMan->Search(ent);
									delete ent;
									if(howindex>=0){
										HowitzerMan->RecordMan.GetRecord(howindex,*HowitzerMan->AddedRecord);
										int32 normal=0,rap=0;
										HowitzerMan->AddedRecord->GetDistance(normal,rap);
										JKPoint32 battpos;
										rec.GetCoord(battpos);
										int32 AZ;
										rec.GetAZ(AZ);
										int32 leftlimit=0l,rightlimit=0l;
										HowitzerMan->AddedRecord->GetLimit(leftlimit,rightlimit);
		//////////////////////////////////////////////
										if(rap)    SVPieslice(dc,RealCoordMan,coord,AZ-leftlimit,AZ+rightlimit,rap/10,LtGray,4);
										if(normal) SVPieslice(dc,RealCoordMan,coord,AZ-leftlimit,AZ+rightlimit,normal/10,LtRed,4);
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

void StatusView::DrawXBattalion(JKDC& dc)
{
	BOOL checkvalue[2];
	GetCheck(7,checkvalue[0]); GetCheck(8,checkvalue[1]);

	JKPoint32 coord;
	REFENTRY refentry;
	int16	index=XBattalionMan->TableMan.TableHead;
	while(index!=-1&&XBattalionMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			XBattalionMan->RecordMan.GetRecord(index,*XBattalionMan->AddedRecord);
			if(XBattalionMan->AddedRecord->GetCoord(coord)){
				if(RealCoordMan.RealToPixel(coord,FALSE)){
					WORD resid=XBattalionMan->AddedRecord->GetResID();
					if(checkvalue[GetBattalionResKind(resid)]){
						ResourceBase *res=ResMan->GetResource(resid);
						if(res){
							BYTE *image = res->AllocRealData();
							if(image)	{
								dc.PutImage(JKPoint(coord.x-12,coord.y-12),image+24*24+4,AND_PUT);
								dc.PutImage(JKPoint(coord.x-12,coord.y-12),image,OR_PUT);
								res->FreeRealData(image);
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

int16 StatusView::GetTargetResKind(WORD resid)
{
	if(resid>=0x4000+0x300&&resid<0x4000+0x300+80){
		resid-=0x4000+0x300;
		if((resid<10)||(resid>=20&&resid<24)||(resid>=46&&resid<49)) return 0;
		if((resid>=10&&resid<20)||(resid>=30&&resid<34)||(resid>=56&&resid<59)) return 5;
		if((resid>=24&&resid<30)||(resid>=34&&resid<40)||(resid>=61&&resid<70)||(resid>=71&&resid<79)) return 1;
		if(resid==79) return 4;
		if((resid>=42&&resid<46)||(resid>=52&&resid<56)) return 3;
		return 2;
	}
	return 1;
}

void StatusView::DrawTarget(JKDC& dc)
{
	BOOL checkvalue[6];
	GetCheck(0,checkvalue[0]); GetCheck(1,checkvalue[1]);
	GetCheck(2,checkvalue[2]); GetCheck(3,checkvalue[3]);
	GetCheck(4,checkvalue[4]);
	GetCheck(22,checkvalue[5]);

	JKPoint32 coord;
	REFENTRY refentry;
	int16	index=TargetMan->TableMan.TableHead;
	while(index!=-1&&TargetMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			TargetMan->RecordMan.GetRecord(index,*TargetMan->AddedRecord);
			if(TargetMan->AddedRecord->GetCoord(coord)){
				if(RealCoordMan.RealToPixel(coord,FALSE)){
					WORD resid=TargetMan->AddedRecord->GetResID();
					if(checkvalue[GetTargetResKind(resid)]){
						ResourceBase *res=ResMan->GetResource(resid);
						if(res){
							BYTE *image = res->AllocRealData();
							if(image)	{
								dc.PutImage(JKPoint(coord.x-12,coord.y-12),image+24*24+4,AND_PUT);
								dc.PutImage(JKPoint(coord.x-12,coord.y-12),image,OR_PUT);
								res->FreeRealData(image);
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

void StatusView::DrawAirControl(JKDC& dc,JKRect32 paintrect)
{
	BOOL check;
	if(!GetCheck(21,check) || !check) return;
	JKPoint32 coord;
	REFENTRY refentry;
	int16	index=AirMan->TableMan.TableHead;
	while(index!=-1&&AirMan->TableMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			AirMan->RecordMan.GetRecord(index,*AirMan->AddedRecord);
			JKPoint32 realp[4];
			if(AirMan->AddedRecord->GetCoords(realp)){
				if(CoordCalc.IsCross(realp[0],realp[1],realp[2],realp[3])){
					JKPoint32 temppos(realp[0]);
					realp[0]=realp[1];
					realp[1]=temppos;
				}
				for(uint16 i=0;i<4;i++)
					SVLine(dc,RealCoordMan,realp[i],realp[(i+1)%4],LtBlue,0,paintrect);
			}
			index=refentry.next;
		}
		else index=-1;
	}
}

void StatusView::EvOnlyMove(JKPoint& p)
{
	JKPoint32 temp(p.x,p.y);
	if(RealCoordMan.PixelToReal(temp)){
		CoordView *coordview=(CoordView *)GetControl(ID_USERCONTROL+4);
		if(coordview)	coordview->SetCoord(temp);
	}
}

void StatusView::EvNcOnlyMove(JKPoint&)
{
	CoordView *coordview=(CoordView *)GetControl(ID_USERCONTROL+4);
	if(coordview)	coordview->SetCoord(JKPoint32(-1,-1));
}

void StatusView::EvElOnlyMove(WORD)
{
	CoordView *coordview=(CoordView *)GetControl(ID_USERCONTROL+4);
	if(coordview)	coordview->SetCoord(JKPoint32(-1,-1));
}

/////////////////////////////////////////////////////////////////////////////////

BOOL StatusView::GetRealSquareSize(JKPoint32& size)
{
	RealCoordMan.PixelToReal_SX(size.x);
	RealCoordMan.PixelToReal_SY(size.y);
	JKPoint32 VirtualSize(size.x*RealCoordMan.ClientSize.y,size.y*RealCoordMan.ClientSize.x);
	JKPoint32 TempSize(abs(VirtualSize.x),abs(VirtualSize.y));
	if(TempSize.x>TempSize.y) size.y=(size.y>0? TempSize.x:-TempSize.x)/RealCoordMan.ClientSize.x;
	else size.x=(size.x>0? TempSize.y:-TempSize.y)/RealCoordMan.ClientSize.y;
	RealCoordMan.RealToPixel_SX(size.x);
	RealCoordMan.RealToPixel_SY(size.y);
	return TRUE;
}

void StatusView::EvLeftClick(JKPoint& p)
{
	if(WorkKind==WKIND_ZOOMING){
		Working=TRUE;
		WorkingStart=WorkingEnd=p;

		JKPoint32 size(WorkingEnd.x-WorkingStart.x,WorkingEnd.y-WorkingStart.y);
		GetRealSquareSize(size);
		JKPoint endp(WorkingStart.x+size.x,WorkingStart.y+size.y);
		JKDC *dc = CreateClientDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Rectangle3D(JKRect(WorkingStart,endp),3,0xff,0xff);
			MouseEvents->ShowCursor();
		}
	}
	else if(WorkKind==WKIND_MOVING){
		Working=TRUE;
		WorkingStart=WorkingEnd=p;

		JKDC *dc = CreateClientDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Line(WorkingStart,WorkingEnd,0xff);
			MouseEvents->ShowCursor();
		}
	}
	else if(WorkKind==WKIND_NOWORK){
		if(IsSVEditing){
			Working=TRUE;
			MtPolyLine.PolyPoint.ClearNode();
			JKPoint32 realpos(p);
			RealCoordMan.PixelToReal(realpos);
			WorkingPoint=realpos;
			MtPolyLine.PolyPoint.PutPoint(realpos);
		}
		else{

			JKPoint32 realp(p);
			if(!RealCoordMan.PixelToReal(realp)) return;
			int32 delta=10;
			if(!RealCoordMan.PixelToReal_SX(delta)) return;
	//////////////////////////////////////////////////////////
			BOOL checkvalue[6];
			GetCheck(0,checkvalue[0]); GetCheck(1,checkvalue[1]);
			GetCheck(2,checkvalue[2]); GetCheck(3,checkvalue[3]);
			GetCheck(4,checkvalue[4]); GetCheck(22,checkvalue[5]);

			int16 index = TargetMan->SearchNearCoord(realp,delta);
			if(index>=0) {
				TargetRecord tgrec;
				TargetMan->RecordMan.GetRecord(index,tgrec);
				WORD resid=tgrec.GetResID();
				if(checkvalue[GetTargetResKind(resid)]){
					FireMan->AddedRecord->InitData();
					FireMan->AddedRecord->SetData(4,"ÌaŠA");
					char buff[30];
					tgrec.GetData(0,buff);
					FireMan->AddedRecord->SetData(1,buff);
					tgrec.GetData(1,buff);
					FireMan->AddedRecord->SetData(2,buff);
					tgrec.GetData(2,buff);
					FireMan->AddedRecord->SetData(3,buff);

					SendControlCommand(FIRE_PROCESS,0);
				}
			}
		}
	}
}

void StatusView::EvLeftDragging(JKPoint& p)
{
	EvOnlyMove(p);

	if(Working){
		if(WorkKind==WKIND_ZOOMING){
			JKPoint32 size1(WorkingEnd.x-WorkingStart.x,WorkingEnd.y-WorkingStart.y);
			GetRealSquareSize(size1);
			JKPoint endp1(WorkingStart.x+size1.x,WorkingStart.y+size1.y);
			JKPoint32 size2(p.x-WorkingStart.x,p.y-WorkingStart.y);
			GetRealSquareSize(size2);
			JKPoint endp2(WorkingStart.x+size2.x,WorkingStart.y+size2.y);
			JKDC *dc = CreateClientDC();
			if(dc){
				dc->SetWriteMode(XOR_PUT);
				MouseEvents->HideCursor();
				dc->Rectangle3D(JKRect(WorkingStart,endp1),3,0xff,0xff);
				dc->Rectangle3D(JKRect(WorkingStart,endp2),3,0xff,0xff);
				MouseEvents->ShowCursor();
			}
			WorkingEnd=p;
		}
		else if(WorkKind==WKIND_MOVING){
			JKDC *dc = CreateClientDC();
			if(dc){
				dc->SetWriteMode(XOR_PUT);
				MouseEvents->HideCursor();
				dc->Line(WorkingStart,WorkingEnd,0xff);
				dc->Line(WorkingStart,p,0xff);
				MouseEvents->ShowCursor();
			}
			WorkingEnd=p;
		}
		else if(WorkKind==WKIND_NOWORK){
			if(IsSVEditing){
				JKPoint32 realpos(p);
				RealCoordMan.PixelToReal(realpos);
				JKDC *dc=CreateClientDC();
				if(dc) {
					MouseEvents->HideCursor();
					SVLine(*dc,RealCoordMan,WorkingPoint,realpos,MtPolyLine.Color,MtPolyLine.Width);
					MouseEvents->ShowCursor();
				}
				WorkingPoint=realpos;
				MtPolyLine.PolyPoint.PutPoint(realpos);
			}
		}
	}
}

void StatusView::EvLeftRelease(JKPoint&)
{
	if(Working){
		if(WorkKind==WKIND_ZOOMING){
			JKPoint32 size(WorkingEnd.x-WorkingStart.x,WorkingEnd.y-WorkingStart.y);
			GetRealSquareSize(size);
			JKPoint endp(WorkingStart.x+size.x,WorkingStart.y+size.y);
			JKDC *dc = CreateClientDC();
			if(dc){
				dc->SetWriteMode(XOR_PUT);
				MouseEvents->HideCursor();
				dc->Rectangle3D(JKRect(WorkingStart,endp),3,0xff,0xff);
				MouseEvents->ShowCursor();
			}
			Working=FALSE;
			JKRect32 rect(min(WorkingStart.x,endp.x),min(WorkingStart.y,endp.y),
										max(WorkingStart.x,endp.x),max(WorkingStart.y,endp.y));
			if(RealCoordMan.PixelToReal(rect)){
				if(rect.Width()&&rect.Height()) SetClientRealRect(rect);
			}
		}
		else if(WorkKind==WKIND_MOVING){
			JKDC *dc = CreateClientDC();
			if(dc){
				dc->SetWriteMode(XOR_PUT);
				MouseEvents->HideCursor();
				dc->Line(WorkingStart,WorkingEnd,0xff);
				MouseEvents->ShowCursor();
			}
			JKRect clientrect=ClientRect.SetLeftTopBy();
			clientrect.Offset(WorkingStart-WorkingEnd);
			JKRect32 rect(clientrect);
			if(RealCoordMan.PixelToReal(rect,FALSE)) {
				if(AdjustRealRect(rect)) SetClientRealRect(rect);
			}
			Working=FALSE;
		}
		else if(WorkKind==WKIND_NOWORK){
			if(IsSVEditing){
				Working=FALSE;
				if(MtPolyLine.PolyPoint.NodeCount>1) {
					PaintStatMeta.PutNodeLast(MtPolyLine.Duplicate());
				}
			}
		}
	}
}

void StatusView::EvNcLeftRelease(JKPoint&)
{
	if(Working){
		if(WorkKind==WKIND_ZOOMING){
			JKPoint32 size(WorkingEnd.x-WorkingStart.x,WorkingEnd.y-WorkingStart.y);
			GetRealSquareSize(size);
			JKPoint endp(WorkingStart.x+size.x,WorkingStart.y+size.y);
			JKDC *dc = CreateClientDC();
			if(dc){
				dc->SetWriteMode(XOR_PUT);
				MouseEvents->HideCursor();
				dc->Rectangle3D(JKRect(WorkingStart,endp),3,0xff,0xff);
				MouseEvents->ShowCursor();
			}
			Working=FALSE;
		}
		else if(WorkKind==WKIND_MOVING){
			JKDC *dc = CreateClientDC();
			if(dc){
				dc->SetWriteMode(XOR_PUT);
				MouseEvents->HideCursor();
				dc->Line(WorkingStart,WorkingEnd,0xff);
				MouseEvents->ShowCursor();
			}
			Working=FALSE;
		}
		else if(WorkKind==WKIND_NOWORK){
			if(IsSVEditing){
				Working=FALSE;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

void StatusView::EvRightClick(JKPoint& p)
{
	if(Working) return;
	if(WorkKind==WKIND_ZOOMING){
		JKPoint32 center(p);
		if(RealCoordMan.PixelToReal(center))
			SetZoomOut(center);
	}
	else if(WorkKind==WKIND_NOWORK){
		JKPoint32 realp(p);
		if(!RealCoordMan.PixelToReal(realp)) return;
		if(IsSVEditing) {
			if(SelectAttributeDialog(JKPoint(100,100),MtText,this).Run()==ID_OK){
				MtText.Position=realp;
				if(MtText.TextBuffer&&MtText.TextBuffer[0]){
					JKDC *dc = CreateClientDC();
					if(dc) {
						MouseEvents->HideCursor();
						MtText.Draw(*dc,RealCoordMan);
						MouseEvents->ShowCursor();
					}
					PaintStatMeta.PutNodeLast(MtText.Duplicate());
				}
			}
		}
		else {
			int32 delta=10;
			if(!RealCoordMan.PixelToReal_SX(delta)) return;
			int16 index;
	//////////////////////////////////////////////////////////
			BOOL checkvalue[2+6];
			GetCheck(7,checkvalue[0]); GetCheck(8,checkvalue[1]);

			GetCheck(0,checkvalue[2+0]); GetCheck(1,checkvalue[2+1]);
			GetCheck(2,checkvalue[2+2]); GetCheck(3,checkvalue[2+3]);
			GetCheck(4,checkvalue[2+4]); GetCheck(22,checkvalue[2+5]);

			index = BattalionMan->SearchNearCoord(realp,delta);
			if(index>=0) {
				BattalionRecord rec;
				BattalionMan->RecordMan.GetRecord(index,rec);
				WORD resid=rec.GetResID();
				if(checkvalue[GetBattalionResKind(resid)]){
					if(BattalionDialog(JKPoint(200,220),rec).Run()==ID_OK){
						BattalionMan->SetRecord(index,rec);
						SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGESTAT);
					}
					return;
				}
			}
	//////////////////////////////////////////////////////////////////
			index = XBattalionMan->SearchNearCoord(realp,delta);
			if(index>=0) {
				XBattalionRecord rec;
				XBattalionMan->RecordMan.GetRecord(index,rec);
				WORD resid=rec.GetResID();
				if(checkvalue[GetBattalionResKind(resid)]){
					if(XBattalionDialog(JKPoint(200,220),rec).Run()==ID_OK){
						XBattalionMan->SetRecord(index,rec);
						SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGESTAT);
					}
					return;
				}
			}
	//////////////////////////////////////////////////////////////////
			index = TargetMan->SearchNearCoord(realp,delta);
			if(index>=0) {
				TargetRecord rec;
				TargetMan->RecordMan.GetRecord(index,rec);
				WORD resid=rec.GetResID();
				if(checkvalue[2+GetTargetResKind(resid)]){
					if(TargetDialog(JKPoint(200,220),rec).Run()==ID_OK){
						TargetMan->SetRecord(index,rec);
						SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGESTAT);
					}
					return;
				}
			}
	//////////////////////////////////////////////////////////////////
		}
	}
}
void StatusView::EvRightDragging(JKPoint& p)
{
	EvOnlyMove(p);
}
void StatusView::EvRightRelease(JKPoint&){}
void StatusView::EvNcRightRelease(JKPoint&){}

///////////////////////////////////////////////////////////////////////////

void StatusView::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_USERCONTROL+3:
			ShowCurInfom(JKRect(200,100,550,450),RealCoordMan,BlockSize,this).Run();
		break;
		case ID_BUTTON+1:
			SetClientRealRect(JKRect32(0,0,5*10000-1,10*10000-1));
		break;
		case ID_BUTTON+2:
		{
			JKPoint32 real;
			if(RealCoordMan.StrToReal("CS90003000",real))
				SetClientRealRect(JKRect32(real.x-5000,real.y-5000,real.x+5000,real.y+5000));
		}
		break;

//		case ID_BUTTON+3:
//			AdjustZoomRatio();
//		break;

		case ID_BUTTON+3:
			if(IsSVEditing) {
				PaintStatMeta.Save();
				PaintStatMeta.ClearNode();
				JKControl *ctrl=GetControl(ID_SUBWINDOW+1);
				if(ctrl) ctrl->HideWindow();

				ctrl=GetControl(ID_BUTTON+4);
				if(ctrl) ctrl->HideWindow();
				ctrl=GetControl(ID_BUTTON+5);
				if(ctrl) ctrl->HideWindow();
				ctrl=GetControl(ID_EDIT+1);
				if(ctrl) ctrl->HideWindow();

				IsSVEditing=FALSE;
				for(uint16 i=0;i<CheckCount;i++) SetCheck(i,SaveCheck[i]);
				SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGESTAT);
			}
			else{
				PaintStatMeta.Load();
				JKControl *ctrl=GetControl(ID_SUBWINDOW+1);
				if(ctrl) ctrl->ShowWindow();

				ctrl=GetControl(ID_BUTTON+4);
				if(ctrl) ctrl->ShowWindow();
				ctrl=GetControl(ID_BUTTON+5);
				if(ctrl) ctrl->ShowWindow();
				ctrl=GetControl(ID_EDIT+1);
				if(ctrl) ctrl->ShowWindow();

				IsSVEditing=TRUE;
				for(uint16 i=0;i<CheckCount;i++) {
					GetCheck(i,SaveCheck[i]);
					SetCheck(i,FALSE);
				}
        SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGESTAT);
			}
		break;
	}
}


void StatusView::EvEditFinish(WORD ctrlid)
{
	if(ctrlid==ID_USERCONTROL+4){
		CoordView *coordview=(CoordView *)GetControl(ID_USERCONTROL+4);
		if(coordview)	{
			char buffer[11];
			coordview->GetText(buffer);
			JKPoint32 p;
			if(RealCoordMan.StrToReal(buffer,p)) {
				RealCoordMan.RealOrgPos=p;
				SetFocus();
				SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGEBASE);
			}
			else coordview->SetCoord(JKPoint32(-1,-1));
		}
	}
}

void StatusView::EvCommand(WORD cmdid,WORD data,DWORD detail,WORD option)
{
	JKWindow::EvCommand(cmdid,data,detail,option);
	switch(cmdid){
		case CM_CHANGEBASE:
			EvChangeBase();
			break;
		case CM_CHANGESTAT:
			EvChangeStat();
			break;
		case OCC_COMMAND+1:
			SelectColorDialog(JKPoint(100,100),MtPolyLine.Color,this).Run();
			break;
		case OCC_COMMAND+2:
			GetWidthDialog(JKPoint(100,100),MtPolyLine.Width,this).Run();
			break;
		case OCC_COMMAND+3:
			SelectAttributeDialog(JKPoint(100,100),MtText,this).Run();
			break;
		case OCC_COMMAND+4:
			break;
		case OCC_COMMAND+5:
			break;
		case OCC_COMMAND+6:
			if(IsSVEditing) {
				PaintStatMeta.Load();
				RedrawClient();
			}
			break;
		case OCC_COMMAND+7:
			if(IsSVEditing) PaintStatMeta.Save();
			break;
	}
}

void StatusView::MakeBaseImage()
{
	if(BaseMemDC) {
		BaseMemDC->ClearBuffer(BackColor);
		DrawBase(*BaseMemDC);
	}
}

void StatusView::MakeStatImage()
{
	if(StatMemDC) {
		if(BaseMemDC) StatMemDC->PutImage(JKPoint(0,0),BaseMemDC->DataBuffer,COPY_PUT);
		DrawStatMeta(*StatMemDC);
		DrawBlock(*StatMemDC,BlockSize);
		DrawAirControl(*StatMemDC);
		DrawTarget(*StatMemDC);
		DrawXBattalion(*StatMemDC);
		DrawBattalion(*StatMemDC);
	}
}

void StatusView::EvChangeBase()
{
	BlockSize.x=int32(100./RealCoordMan.XZoom);
	if(BlockSize.x<500) BlockSize.x=100;
	else if(BlockSize.x<1000) BlockSize.x=500;
	else if(BlockSize.x<5000) BlockSize.x=1000;
	else if(BlockSize.x<10000) BlockSize.x=5000;
	else if(BlockSize.x<50000) BlockSize.x=10000;
	else BlockSize.x=50000;
	BlockSize.y=int32(100./RealCoordMan.YZoom);
	if(BlockSize.y<500) BlockSize.y=100;
	else if(BlockSize.y<1000) BlockSize.y=500;
	else if(BlockSize.y<5000) BlockSize.y=1000;
	else if(BlockSize.y<10000) BlockSize.y=5000;
	else if(BlockSize.y<50000) BlockSize.y=10000;
	else BlockSize.y=50000;

	if(BlockSize.x<BlockSize.y) BlockSize.x=BlockSize.y;
	else 												BlockSize.y=BlockSize.x;

	MakeBaseImage();

	JKControl* ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) ctrl->RedrawClient();
	ctrl=GetControl(ID_USERCONTROL+2);
	if(ctrl) ctrl->RedrawClient();
	ctrl=GetControl(ID_USERCONTROL+3);
	if(ctrl) ctrl->RedrawClient();

	EvChangeStat();
}

void StatusView::EvChangeStat()
{
  MakeStatImage();
	RedrawClient();
}

BOOL StatusView::AdjustRealRect(JKRect32& rect)
{
	int32 width=rect.Width()+1;
	if(width>=5*10000) {
		rect.left=0; rect.right=5*10000-1;
	}
	else{
		int32 offset=0;
		if(rect.left<0) offset=-rect.left;
		else if(rect.right>=5*10000) offset=5*10000-rect.right+1;
		rect.left+=offset; rect.right+=offset;
	}
	int32 height=rect.Height()+1;
	if(height>=10*10000){
		rect.top=0; rect.bottom=10*10000-1;
	}
	else {
		int32 offset=0;
		if(rect.top<0) offset=-rect.top;
		else if(rect.bottom>=10*10000) offset=10*10000-rect.bottom+1;
		rect.top+=offset; rect.bottom+=offset;
	}
	return TRUE;
}

void StatusView::SetClientRealRect(JKRect32 rect)
{
	if(RealCoordMan.SetClientRealRect(rect))
		SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGEBASE);
}

void StatusView::SetZoomOut(JKPoint32& p)
{
	JKRect32 realrect;
	RealCoordMan.GetClientRealRect(realrect);
	JKPoint32 realsize(realrect.Width()+1,realrect.Height()+1);
	JKRect32 temp(-realsize.x*3/4,-realsize.y*3/4,realsize.x*3/4,realsize.y*3/4);
	temp.Offset(p);
	if(AdjustRealRect(temp)){
		if(RealCoordMan.SetClientRealRect(temp))
			SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGEBASE);
	}
}

BOOL StatusView::GetCheck(uint16 i,BOOL& check)
{
	JKCheckBox *chkbox=(JKCheckBox *)GetControl(ID_CHECKBOX+1+i);
	if(chkbox){
		check=chkbox->Status;
		return TRUE;
	}
	return FALSE;
}

BOOL StatusView::SetCheck(uint16 i,BOOL check)
{
	JKCheckBox *chkbox=(JKCheckBox *)GetControl(ID_CHECKBOX+1+i);
	if(chkbox){
		if(chkbox->Status!=check) {
			chkbox->Status=check;
			chkbox->RedrawClient();
		}
		return TRUE;
	}
	return FALSE;
}

void StatusView::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid==ID_LISTBOX+1){
		Combobox *combobox=(Combobox *)GetControl(ID_LISTBOX+1);
		if(combobox){
			HuboManager *huboman=combobox->HuboMan;
			if(huboman){
				HuboBase *hubo=huboman->GetHuboBase(index);
				if(hubo) WorkKind=hubo->Number;
			}
		}
	}
}

void StatusView::AdjustZoomRatio()
{
	if(RealCoordMan.XZoom!=RealCoordMan.YZoom){
		if(RealCoordMan.XZoom<RealCoordMan.YZoom) RealCoordMan.XZoom=RealCoordMan.YZoom;
		else	RealCoordMan.YZoom=RealCoordMan.XZoom;
		SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGEBASE);
	}
}

void StatusView::EvCheckToggle(WORD ctrlid,BOOL /*status*/)
{
	if(ctrlid>=ID_CHECKBOX+0x100+1&&ctrlid<=ID_CHECKBOX+0x100+3) SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGEBASE);
	else SendCommand("¸q¯¡ e ‹¡”aŸ¡­A¶a.\n‹aŸ¡“e º— ......",CM_CHANGESTAT);
}


void StatusView::ShowExplain(WORD ctrlid)
{
/*
	JKControl *popup;
	int32 index = ::SearchItem(OCCMessageControlID,(uint32)OCCMessageCount,ctrlid);
	if(index>=0){
		if(index!=MessageIndex){
			JKControl *ctrl=GetControl(ctrlid);
			if(!ctrl) return;
			JKPoint pos=ctrl->WinRect.Adjust(OCCMessagePosFlag[index],OCCMessageBoxSize[index]);
			popup = GetControl(ID_USERCONTROL+6);
			if(popup) {
				popup->SetText(OCCMessageString[index]);
				popup->ChangeWindowSize(JKRect(pos,pos+OCCMessageBoxSize[index]));
				if(popup->IsShow) popup->RedrawWindow();
				else popup->ShowWindow();
				MessageIndex=index;
			}
		}
	}
	else{
		if(MessageIndex!=-1){
			popup = GetControl(ID_USERCONTROL+6);
			if(popup) popup->HideWindow();
			MessageIndex=-1;
		}
	}
*/
	JKControl *ctrl;
	int32 index = ::SearchItem(OCCMessageControlID,(uint32)OCCMessageCount,ctrlid);
	if(index>=0){
		if(index!=MessageIndex){
			ctrl = GetControl(ID_STATIC+1);
			if(ctrl) {
				ctrl->SetText(OCCMessageString[index]);
				ctrl->RedrawClient();
			}
			MessageIndex=index;
		}
	}
	else{
		if(MessageIndex!=-1){
			ctrl = GetControl(ID_STATIC+1);
			if(ctrl) {
				ctrl->SetText("");
				ctrl->RedrawClient();
			}
			MessageIndex=-1;
		}
	}
}

void StatusView::HideExplain()
{
/*
	if(MessageIndex!=-1){
		JKControl *ctrl = GetControl(ID_USERCONTROL+6);
		if(ctrl) ctrl->HideWindow();
		MessageIndex=-1;
	}
*/
	if(MessageIndex!=-1){
		JKControl *ctrl = GetControl(ID_STATIC+1);
		if(ctrl) {
			ctrl->SetText("");
			ctrl->RedrawClient();
		}
		MessageIndex=-1;
	}
}


void StatusView::SendCommand(char *message,WORD command)
{
	JKControl *ctrl=new ComandMessageBox(JKRect(300,200,500,300),0,command);
	ctrl->SetText(message);
	ctrl->BackColor=LtMagenta;
	ctrl->TextColor=Black;
	AddControl(ctrl);
}
