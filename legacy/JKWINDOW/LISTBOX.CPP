#include "listbox.h"

JKListBox::JKListBox(JKRect& rect,WORD ctlid,JKControl *parent):
	JKControl(rect,ctlid,parent),HuboManager(),
	FirstIndex(0),CurrentIndex(-1),IsSelecting(FALSE),IsLBSelectEntry(FALSE)
{
	WinRect=rect;
	rect.Expand(-1);
	ShowEntryCount = (rect.Height()+1)/16;
}

JKListBox::~JKListBox()
{}

JKRect JKListBox::CalcClientRect()
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-1);
	return rect;
}

BOOL JKListBox::ChangePosition(JKPoint pos)
{
	if(!IsShow){
		WinRect.SetLeftTop(pos);
		return TRUE;
	}
	return FALSE;
}

void JKListBox::EvSetFocus(DWORD)
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

void JKListBox::EvKillFocus(DWORD)
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

void JKListBox::EvCharDown(WORD key,WORD modifier)
{
	if(CurrentIndex!=-1) SendControlMessage(LB_CHARDOWN,CurrentIndex,MAKEDWORD(key,modifier));
}

void JKListBox::EvKeyDown(WORD key,WORD modifier)
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
			break;
	}
}

void JKListBox::EvLeftClick(JKPoint& p)
{
	int16 selhubo= FindSelectEntry(p);
	if(selhubo>=0 && selhubo<HuboManager::NodeCount){
		if(selhubo==CurrentIndex) IsLBSelectEntry=TRUE;
		else IsLBSelectEntry=FALSE;
		SetCurrentIndex(selhubo);
		IsSelecting=TRUE;
	}
	BeginCapture();
}

void JKListBox::EvLeftDragging(JKPoint& p)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(rect.IsContain(p)){
		if(IsSelecting){
			int16 selhubo= FindSelectEntry(p);
			if(selhubo>=0 && selhubo<HuboManager::NodeCount)
				if(selhubo!=CurrentIndex)	{
					SetCurrentIndex(selhubo);
					IsLBSelectEntry=FALSE;
					return;
				}
		}
	}
}

void JKListBox::EvLeftPressing(JKPoint& p)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(p.y<0) 	SetCurrentIndex(CurrentIndex-1);
	else if(p.y>rect.Height())	SetCurrentIndex(CurrentIndex+1);
}

void JKListBox::EvLeftRelease(JKPoint& p)
{
	if(IsSelecting){
		JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
		if(rect.IsContain(p)){
			int16 selhubo= FindSelectEntry(p);
			if(selhubo>=0 && selhubo<HuboManager::NodeCount){
				if(selhubo!=CurrentIndex) SetCurrentIndex(selhubo);
				if(IsLBSelectEntry) SendControlMessage(LB_SELECTENTRY,selhubo);
			}
		}
		IsLBSelectEntry=FALSE;
		IsSelecting=FALSE;
	}
	EndCapture();
}

BOOL JKListBox::Destroy(int16 index)
{
	if(HuboManager::Destroy(index)){
		if(index<CurrentIndex) CurrentIndex--;
		else if(CurrentIndex>=HuboManager::NodeCount) CurrentIndex=HuboManager::NodeCount-1;
		SetFirstIndex(FirstIndex);
		return TRUE;
	}
	return FALSE;
}

BOOL JKListBox::DestroyAll()
{
	if(HuboManager::DestroyAll()){
		FirstIndex=0;		CurrentIndex=-1;
		return TRUE;
	}
	return FALSE;
}

uint16 JKListBox::SetFirstIndex(uint16 index)
{
	uint16 first=FirstIndex;
	if(ShowEntryCount>HuboManager::NodeCount) FirstIndex=0;
	else if(index<=HuboManager::NodeCount-ShowEntryCount) FirstIndex=index;
	else FirstIndex=HuboManager::NodeCount-ShowEntryCount;
	if(first!=FirstIndex){
		RedrawClient();
		SendControlMessage(LB_SETFIRSTINDEX,FirstIndex);
	}
	return FirstIndex;
}

void JKListBox::DrawEntry(JKDC& dc,int16 sel)
{
	if(sel<FirstIndex) return;
	if(sel>=FirstIndex+ShowEntryCount) return;

	HuboBase *hubo = GetHuboBase(sel);
	if(!hubo) return;

	BOOL overwrite=dc.IsOverWrite;
	int16 textcolor =dc.TextColor;
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(hubo->AddData);

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=(sel-FirstIndex)*16;
	rect.bottom=rect.top+15;
	if(sel==CurrentIndex){
		if(HasFocus()){
			dc.SolidBar(rect,hubo->AddData);
			dc.SetTextColor(BackColor);
			dc.TextOut(rect.LeftTop(),hubo->TextBuff);
		}
		else{
			dc.SolidBar(rect,BackColor);
			dc.TextOut(rect.LeftTop(),hubo->TextBuff);
			dc.Rectangle(rect,hubo->AddData);
		}
	}
	else{
		dc.SolidBar(rect,BackColor);
		dc.TextOut(rect.LeftTop(),hubo->TextBuff);
	}
	dc.IsOverWrite=overwrite;
	dc.SetTextColor(textcolor);

}

void JKListBox::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
	dc.Rectangle(rect,Black);
}

void JKListBox::PaintClient(JKDC& dc,JKRect&)
{
	int16 i;
	for(i=0;i<HuboManager::NodeCount;i++){
		DrawEntry(dc,i);
	}
}

int16 JKListBox::FindSelectEntry(JKPoint p)
{
	int16 sel=p.y/16;
	if(sel>=0&&sel<ShowEntryCount){
		sel+=FirstIndex;
		return sel;
	}
	return -1;
}

BOOL JKListBox::SetCurrentIndex(int16 sel)
{
	if(sel!=CurrentIndex && sel>=0 && sel<HuboManager::NodeCount) {
		int16 temp = CurrentIndex;
		CurrentIndex=sel;
		if(sel<FirstIndex || sel>=FirstIndex+ShowEntryCount)
			SetFirstIndex(sel);

		JKDC *dc=CreateClientDC();
		if(dc){
			MouseEvents->HideCursor();
			if(temp!=-1) 					DrawEntry(*dc,temp);
			if(CurrentIndex!=-1) 	DrawEntry(*dc,CurrentIndex);
			MouseEvents->ShowCursor();
		}
		SendControlMessage(LB_TOUCHENTRY,CurrentIndex);
		return TRUE;
	}
	return FALSE;
}