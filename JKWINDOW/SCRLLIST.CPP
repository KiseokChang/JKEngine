#include "scrolbar.h"
#include "scrllist.h"

ScrollListBox::ScrollListBox(JKRect rect,WORD ctrlid,JKControl *parent):
	JKListBox(rect,ctrlid,parent),DragScrollFlag(FALSE)
{}

void ScrollListBox::InitWindow()
{
	JKListBox::InitWindow();
	SetDragScroll(DragScrollFlag);
}

void ScrollListBox::SetupWindow()
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=rect.right-18+1;
	JKScrollBar *scroll = new JKScrollBar(rect,ID_SCROLLBAR+1,0,0);
	scroll->WinAttr|=WA_NOINCLIENT;
	AddControl(scroll);
}

JKRect ScrollListBox::CalcClientRect()
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.right-=18;
	return rect;
}

void ScrollListBox::EvSBScrollUp(WORD /*ctrlid*/,WORD /*curpos*/)
{
	SetFirstIndex(FirstIndex>0? FirstIndex-1:0);
	RedrawClient();
}
void ScrollListBox::EvSBScrollDown(WORD /*ctrlid*/,WORD /*curpos*/)
{
	SetFirstIndex(FirstIndex+1);
	RedrawClient();
}
void ScrollListBox::EvSBScrollJump(WORD /*ctrlid*/,WORD curpos)
{
	SetFirstIndex(curpos);
}

BOOL ScrollListBox::DestroyAll()
{
	if(JKListBox::DestroyAll()){
		JKScrollBar *scrlbar = (JKScrollBar *)GetControl(ID_SCROLLBAR+1);
		if(scrlbar){
			scrlbar->ChangeMaxCount(0);
			scrlbar->CurrentPosition=FirstIndex;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL ScrollListBox::Add(uint16 index,HuboBase *hubo)
{
	if(JKListBox::Add(index,hubo)) {
		JKScrollBar *scrlbar = (JKScrollBar *)GetControl(ID_SCROLLBAR+1);
		if(scrlbar){
			uint16 maxcount = HuboManager::NodeCount>ShowEntryCount? HuboManager::NodeCount-ShowEntryCount:0;
			scrlbar->ChangeMaxCount(maxcount);
			scrlbar->CurrentPosition=FirstIndex;
			return TRUE;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL ScrollListBox::Destroy(int16 index)
{
	if(JKListBox::Destroy(index)){
		JKScrollBar *scrlbar = (JKScrollBar *)GetControl(ID_SCROLLBAR+1);
		if(scrlbar){
			uint16 maxcount = HuboManager::NodeCount>ShowEntryCount? HuboManager::NodeCount-ShowEntryCount:0;
			scrlbar->ChangeMaxCount(maxcount);
			scrlbar->CurrentPosition=FirstIndex;
			return TRUE;
		}
	}
	return FALSE;
}

void ScrollListBox::SetDragScroll(BOOL flag)
{
		JKScrollBar *scrlbar = (JKScrollBar *)GetControl(ID_SCROLLBAR+1);
		if(scrlbar) scrlbar->DragScrollFlag=flag;
}

uint16 ScrollListBox::SetFirstIndex(uint16 index)
{
	uint16 firstindex=FirstIndex;
	if(JKListBox::SetFirstIndex(index)!=firstindex){
		JKScrollBar *scrlbar = (JKScrollBar *)GetControl(ID_SCROLLBAR+1);
		if(scrlbar){
			scrlbar->CurrentPosition=FirstIndex;
			scrlbar->RedrawWindow();
		}
	}
	return FirstIndex;
}