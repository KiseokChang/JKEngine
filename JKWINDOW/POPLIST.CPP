#include "poplist.h"

PopupListBox::PopupListBox(JKRect& rect,WORD ctlid,HuboManager *huboman,JKControl *parent):
	JKControl(rect,ctlid,parent),HuboMan(huboman),
	Ownership(FALSE),FirstIndex(0),CurrentIndex(-1),IsSelecting(FALSE)
{
	WinAttr&=~BYTE(WA_FIRSTSHOW);
	CursorID=0;

	rect=WinRect.ExpandBy(-1);
	MaxTextWidth = (rect.Width()+1)/8;
	ShowEntryCount = (rect.Height()+1)/16;
	if(!HuboMan){
		HuboMan  = new HuboManager();
		Ownership=TRUE;
	}
	if(!HuboMan) CreationError=TRUE;
}

PopupListBox::~PopupListBox()
{
	if(Ownership&&HuboMan) delete HuboMan;
}

JKRect PopupListBox::CalcClientRect()
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-1);
	return rect;
}

BOOL PopupListBox::ChangePosition(JKPoint pos)
{
	if(!IsShow){
		WinRect.SetLeftTop(pos);
		return TRUE;
	}
	return FALSE;
}

void PopupListBox::EvSetFocus(DWORD)
{
	if(CurrentIndex!=-1) 	{
		JKDC *dc=CreateClientDC();
		if(dc){
			MouseEvents->HideCursor();
			DrawEntry(*dc,CurrentIndex);
			MouseEvents->ShowCursor();
		}
	}
}

void PopupListBox::EvKillFocus(DWORD)
{
	HideWindow();
}

void PopupListBox::EvCharDown(WORD key,WORD modifier)
{
	if(CurrentIndex!=-1) SendControlMessage(LB_CHARDOWN,CurrentIndex,MAKEDWORD(key,modifier));
	HideWindow();
}

void PopupListBox::EvKeyDown(WORD key,WORD modifier)
{
	switch(key){
		case UPARROW:
			SetCurrentIndex(CurrentIndex-1);
			break;
		case DOWNARROW:
			SetCurrentIndex(CurrentIndex+1);
			break;
		default:
			if(CurrentIndex!=-1) SendControlMessage(LB_KEYDOWN,CurrentIndex,MAKEDWORD(key,modifier));
			HideWindow();
			break;
	}
}

void PopupListBox::EvShowWindow()
{
	JKControl::EvShowWindow();
	BeginCapture();
	SetFocus();
}

void PopupListBox::EvLeftClick(JKPoint& p)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(rect.IsContain(p)){
		int16 selhubo= FindSelectEntry(p);
		if(selhubo>=0 && selhubo<HuboMan->NodeCount){
			SetCurrentIndex(selhubo);
		}
	}
	else HideWindow();
}

void PopupListBox::EvLeftDragging(JKPoint& p)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(rect.IsContain(p)){
		int16 selhubo= FindSelectEntry(p);
		if(selhubo>=0 && selhubo<HuboMan->NodeCount) {
			if(selhubo!=CurrentIndex){
				SetCurrentIndex(selhubo);
			}
		}
	}
}

void PopupListBox::EvLeftPressing(JKPoint& p)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(p.y<0) 	SetCurrentIndex(CurrentIndex-1);
	else if(p.y>rect.Height())	SetCurrentIndex(CurrentIndex+1);
}

void PopupListBox::EvLeftRelease(JKPoint& p)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(rect.IsContain(p)){
		int16 selhubo= FindSelectEntry(p);
		if(selhubo>=0 && selhubo<HuboMan->NodeCount){
			SetCurrentIndex(selhubo);
			SendControlMessage(LB_SELECTENTRY,selhubo);
		}
	}
	HideWindow();
}

BOOL PopupListBox::Add(uint16 index,HuboBase *hubo)
{
	return HuboMan->Add(index,hubo);
}

BOOL PopupListBox::Destroy(int16 index)
{
	if(HuboMan->Destroy(index)){
		if(index<CurrentIndex) CurrentIndex--;
		else if(CurrentIndex>=HuboMan->NodeCount) CurrentIndex=HuboMan->NodeCount-1;
		SetFirstIndex(FirstIndex);
		return TRUE;
	}
	return FALSE;
}

BOOL PopupListBox::DestroyAll()
{
	if(HuboMan->DestroyAll()){
		FirstIndex=0;		CurrentIndex=-1;
		return TRUE;
	}
	return FALSE;
}

uint16 PopupListBox::SetFirstIndex(uint16 index)
{
	uint16 first=FirstIndex;
	if(ShowEntryCount>HuboMan->NodeCount) FirstIndex=0;
	else if(index<=HuboMan->NodeCount-ShowEntryCount) FirstIndex=index;
	else FirstIndex=HuboMan->NodeCount-ShowEntryCount;
	if(first!=FirstIndex){
		RedrawClient();
		SendControlMessage(LB_SETFIRSTINDEX,FirstIndex);
	}
	return FirstIndex;
}

void PopupListBox::DrawEntry(JKDC& dc,int16 sel)
{
	if(sel<FirstIndex) return;
	if(sel>=FirstIndex+ShowEntryCount) return;

	HuboBase *hubo = HuboMan->GetHuboBase(sel);
	if(!hubo) return;

	BOOL overwrite=dc.IsOverWrite;
	int16 textcolor=dc.TextColor;

	dc.IsOverWrite=FALSE;

	dc.SetTextColor(hubo->AddData);
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=(sel-FirstIndex)*16;
	rect.bottom=rect.top+15;
	if(sel==CurrentIndex){
		if(HasFocus()){
			dc.SolidBar(rect,hubo->AddData);
			dc.SetTextColor(BackColor);
			dc.TextOut(rect.LeftTop(),MaxTextWidth,hubo->TextBuff);
		}
		else{
			dc.SolidBar(rect,BackColor);
			dc.TextOut(rect.LeftTop(),MaxTextWidth,hubo->TextBuff);
			dc.Rectangle(rect,hubo->AddData);
		}
	}
	else{
		dc.SolidBar(rect,BackColor);
		dc.TextOut(rect.LeftTop(),MaxTextWidth,hubo->TextBuff);
	}
	dc.IsOverWrite=overwrite;
	dc.SetTextColor(textcolor);
}

void PopupListBox::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
	dc.Rectangle(rect,Black);
}

void PopupListBox::PaintClient(JKDC& dc,JKRect&)
{
	int16 i;
	for(i=0;i<HuboMan->NodeCount;i++){
		DrawEntry(dc,i);
	}
}

int16 PopupListBox::FindSelectEntry(JKPoint p)
{
	int16 sel=p.y/16;
	if(sel>=0&&sel<ShowEntryCount){
		sel+=FirstIndex;
		return sel;
	}
	return -1;
}

BOOL PopupListBox::SetCurrentIndex(int16 sel)
{
	if(sel!=CurrentIndex && sel>=0 && sel<(int16)HuboMan->NodeCount) {
		int16 temp = CurrentIndex;
		CurrentIndex=sel;
		if(sel<FirstIndex || sel>=FirstIndex+ShowEntryCount)
			SetFirstIndex(sel);
		else {
			JKDC *dc=CreateClientDC();
			if(dc){
					MouseEvents->HideCursor();
					if(temp!=-1) 					DrawEntry(*dc,temp);
					if(CurrentIndex!=-1) 	DrawEntry(*dc,CurrentIndex);
					MouseEvents->ShowCursor();
			}
		}
		return TRUE;
	}
	return FALSE;
}