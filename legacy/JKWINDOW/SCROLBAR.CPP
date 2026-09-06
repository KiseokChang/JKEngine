#include "jkapp.h"
#include "resbttn.h"
#include "scrolbar.h"

JKScrollBar::JKScrollBar(JKRect& rect,WORD ctlid,WORD maxcount,WORD curpos,JKControl *parent):
	JKControl(rect,ctlid,parent),MaxCount(maxcount),CurrentPosition(curpos),DragScrollFlag(FALSE),IsScrolling(FALSE)
{
	BackColor=LtGray;
	WinAttr|=0x01;
	WinRect.right=WinRect.left+17;
}

JKRect JKScrollBar::CalcClientRect()
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-1);
	rect.bottom-=32;
	return rect;
}

void JKScrollBar::SetupWindow()
{
	JKRect rect = ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKButton *bttn;
	bttn = new JKResButton(JKRect(0,rect.bottom+1,15,rect.bottom+16),ID_BUTTON+1,InternImageID[ArrowUp]);
	bttn->WinAttr|=WA_NOINCLIENT;
	AddControl(bttn);
	bttn = new JKResButton(JKRect(0,rect.bottom+1+16,15,rect.bottom+32),ID_BUTTON+2,InternImageID[ArrowDown]);
	bttn->WinAttr|=WA_NOINCLIENT;
	AddControl(bttn);
}

void JKScrollBar::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.Rectangle(rect,Black);
}

void JKScrollBar::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect =ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,BackColor);
	rect = CalcThumbRect();
	if(MaxCount) dc.Rectangle3D(rect,IsScrolling? -1:1);
}

int16 JKScrollBar::CalcThumbHeight()
{
	JKRect rect =ClientRect.SetLeftTopBy(JKPoint(0,0));
	return rect.Height()/2;
}

int16 JKScrollBar::CalcThumbVPosition()
{
	int16 vpos=0;
	if(MaxCount){
		JKRect rect =ClientRect.SetLeftTopBy(JKPoint(0,0));
		int16 height = rect.Height()-CalcThumbHeight();
		vpos = (int32)CurrentPosition*height/MaxCount;
	}
	else vpos=0;
	return vpos;
}

JKPoint JKScrollBar::CalcThumbPosition()
{
	int16 vpos=CalcThumbVPosition();
	JKPoint p(0,vpos);
	return p;
}

JKRect JKScrollBar::CalcThumbRect()
{
	JKRect rect = ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(MaxCount&&rect.Height()>=CalcThumbHeight()) {
		JKPoint tmp=CalcThumbPosition();
		rect = JKRect(tmp,tmp+JKPoint(15,CalcThumbHeight()));
	}
	return rect;
}

JKRect JKScrollBar::CalcUpRect()
{
	JKPoint tmp=JKPoint(1,ClientRect.SetLeftTopBy(JKPoint(0,0)).bottom-16-1);
	return JKRect(tmp,tmp+JKPoint(15,15));

}

JKRect JKScrollBar::CalcDownRect()
{
	JKPoint tmp=JKPoint(1,ClientRect.SetLeftTopBy(JKPoint(0,0)).bottom-32-1);
	return JKRect(tmp,tmp+JKPoint(15,15));
}

void JKScrollBar::ChangeMaxCount(WORD maxcount)
{
	if(MaxCount!=maxcount){
		MaxCount=maxcount;
		if(CurrentPosition>MaxCount) CurrentPosition=MaxCount;
	}
}

void JKScrollBar::EvLeftClick(JKPoint& p)
{
	if(!IsScrolling){
		IsScrolling=TRUE;
		int16 height=ClientRect.Height();
		WORD currentpos;
		if(p.y<height/4) currentpos=0;
		else if(p.y>height*3/4) currentpos=MaxCount;
		else currentpos= WORD(int32(p.y-height/4)*(MaxCount+1)/(height/2+1));
		if(CurrentPosition!=currentpos){
			CurrentPosition=currentpos;
			RedrawClient();
			SendControlMessage(SB_SCROLLJUMP,CurrentPosition);
		}
	}
}

void JKScrollBar::EvLeftPressing(JKPoint& p)
{
	if(IsScrolling && DragScrollFlag){
		int16 height=ClientRect.Height();
		WORD currentpos;
		if(p.y<height/4) currentpos=0;
		else if(p.y>height*3/4) currentpos=MaxCount;
		else currentpos= WORD(int32(p.y-height/4)*(MaxCount+1)/(height/2+1));
		if(CurrentPosition!=currentpos){
			CurrentPosition=currentpos;
			RedrawClient();
			SendControlMessage(SB_SCROLLJUMP,CurrentPosition);
		}
	}
}

void JKScrollBar::EvLeftDragging(JKPoint& p)
{
	EvLeftPressing(p);
}

void JKScrollBar::EvLeftRelease(JKPoint&)
{
	if(IsScrolling) {
		IsScrolling=FALSE;
		RedrawClient();
	}
}

void JKScrollBar::EvNcLeftRelease(JKPoint&)
{
	if(IsScrolling) {
		IsScrolling=FALSE;
		RedrawClient();
	}
}

void JKScrollBar::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1 && CurrentPosition>0) {
		CurrentPosition--;
		RedrawClient();
		SendControlMessage(SB_SCROLLUP,CurrentPosition);
	}
	else if(ctrlid==ID_BUTTON+2	&& CurrentPosition<MaxCount) {
		CurrentPosition++;
		RedrawClient();
		SendControlMessage(SB_SCROLLDOWN,CurrentPosition);
	}
}

void JKScrollBar::EvButtonPressing(WORD ctrlid)
{
	EvButtonClick(ctrlid);
}

