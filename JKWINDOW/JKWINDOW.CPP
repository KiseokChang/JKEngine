#include "jkwindow.h"

#define CORNERSIZE 30

JKWindow::JKWindow(JKRect rect,JKControl *parent,WORD ctrlid):
	JKControl(rect,ctrlid,parent),
	WinForm(0x07),BorderSize(2),Depth(1),
	BorderColor(LtGray),TitleColor(LtGray),TitleBkColor(LtBlue),NFTitleColor(Gray),NFTitleBkColor(LtGray)
{
	WinAttr&=~(BYTE)WA_FIRSTSHOW;
}

JKWindow::~JKWindow()
{}

JKRect JKWindow::CalcClientRect()
{
	int16 depth=abs(Depth);
	int16 delta=depth+BorderSize;
	JKRect rect = WinRect.SetLeftTopBy(JKPoint(0,0));
	if(WinForm & 0x01) rect.Expand(-delta);
	if(WinForm & 0x06) rect.top += 16+2;
	MinimumSize=JKPoint(delta*2+18+100,delta*2+18);
	return rect;
}

BOOL JKWindow::GetRegion(WORD element,JKRect& rect)
{
	int16 depth=abs(Depth);
	int16 delta=depth+BorderSize;
	rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	switch(element){
		case BORDER_TOP:
			if(!(0x01&WinForm)) return FALSE;
			rect.bottom=rect.top+delta-1;
			return TRUE;
		case BORDER_BOTTOM:
			if(!(0x01&WinForm)) return FALSE;
			rect.top=rect.bottom-delta+1;
			return TRUE;
		case BORDER_LEFT:
			if(!(0x01&WinForm)) return FALSE;
			rect.right=rect.left+delta+1;
			return TRUE;
		case BORDER_RIGHT:
			if(!(0x01&WinForm)) return FALSE;
			rect.left=rect.right-delta+1;
			return TRUE;
		case TITLEBOX:
			if(!(0x06&WinForm)) return FALSE;
			if(0x01&WinForm) rect.Expand(-delta);
			rect.bottom = rect.top+15+2;
			if(0x04&WinForm)	rect.left += 16+2;
			return TRUE;
		case SYSTEMBOX:
			if(!(0x04&WinForm)) return FALSE;
			if(0x01&WinForm) rect.Expand(-delta);
			rect.bottom = rect.top+15+2;
			rect.right = rect.left+15+2;
			return TRUE;
		default:
			break;
	}
	return FALSE;
}

int16 JKWindow::FindWindowElement(JKPoint& p)
{
	JKRect rect;
	JKRect winrect=WinRect.SetLeftTopBy();
	int16 ret =0;
	p=ScreenToClient(p);
	p+=ClientRect.LeftTop();
	if(WinRect.SetLeftTopBy(JKPoint(0,0)).IsContain(p)){
		if(ClientRect.IsContain(p)) {	p-=ClientRect.LeftTop();	ret=-1;	}
		else if(GetRegion(BORDER_TOP,rect)&&rect.IsContain(p)) {
			ret=BORDER_TOP;
			if(p.x>WinRect.Width()-CORNERSIZE) ret=BORDER_CORNER2;
			else if(p.x<CORNERSIZE)            ret=BORDER_CORNER1;
		}
		else if(GetRegion(BORDER_BOTTOM,rect)&&rect.IsContain(p)) {
			ret=BORDER_BOTTOM;
			if(p.x>WinRect.Width()-CORNERSIZE) ret=BORDER_CORNER4;
			else if(p.x<CORNERSIZE)            ret=BORDER_CORNER3;
		}
		else if(GetRegion(BORDER_LEFT,rect)&&rect.IsContain(p)) {
			ret=BORDER_LEFT;
			if(p.y>WinRect.Height()-CORNERSIZE) ret=BORDER_CORNER3;
			else if(p.y<CORNERSIZE)            	ret=BORDER_CORNER1;
		}
		else if(GetRegion(BORDER_RIGHT,rect)&&rect.IsContain(p)) {
			ret = BORDER_RIGHT;
			if(p.y>WinRect.Height()-CORNERSIZE) ret=BORDER_CORNER4;
			else if(p.y<CORNERSIZE)            	ret=BORDER_CORNER2;
		}
		else if(GetRegion(TITLEBOX,rect)&&rect.IsContain(p)) ret = TITLEBOX;
		else if(GetRegion(SYSTEMBOX,rect)&&rect.IsContain(p)) ret = SYSTEMBOX;
		else { p=ClientToScreen(p); ret=0;}
	}
	else{
		p-=ClientRect.LeftTop();
		p=ClientToScreen(p);
		ret=0;
	}
	return ret;
}

void JKWindow::DrawSystemBox(JKDC& dc,JKRect rect)
{
	dc.SolidBar(rect,IsContainFocus()? TitleBkColor:NFTitleBkColor);
	rect.Expand(-2);
	dc.Box3D(rect,-1,IsContainFocus()? White:LtGray);
	rect.Expand(-1);
	rect.top+=4;
	rect.bottom-=4;
	rect.left += Parent? 3:2;
	rect.right-= Parent? 3:2;
	if(IsContainFocus()) dc.SolidBar(rect.OffsetBy(JKPoint(1,1)),Gray);
	dc.SolidBar(rect,IsContainFocus()? White:Gray);
	dc.Rectangle(rect,Black);
}

void JKWindow::DrawTitleBar(JKDC& dc,JKRect rect)
{
	dc.SolidBar(rect,IsContainFocus()? TitleBkColor:NFTitleBkColor);
	rect.Expand(-1);
	if(WinText){
		BOOL overwrite=dc.IsOverWrite;
		dc.IsOverWrite=FALSE;
		dc.SetTextColor(IsContainFocus()? TitleColor:NFTitleColor);
		dc.TextOutX(rect,WinText,ADJ_YCENTER,TRUE);
		dc.IsOverWrite=overwrite;
	}
}

void JKWindow::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	int16 depth=abs(Depth);
	if(WinForm & 0x01) {
		dc.Rectangle3D(rect,Depth);
		rect.Expand(-depth);
		dc.Rectangle3D(rect,BorderSize,BorderColor,BorderColor);
		rect.Expand(-(int16)BorderSize);
	}
	if(GetRegion(TITLEBOX,rect)) DrawTitleBar(dc,rect);
	if(GetRegion(SYSTEMBOX,rect)) DrawSystemBox(dc,rect);
}

void JKWindow::EvElLeftClick(WORD element)
{
	if(element==SYSTEMBOX) {
		CloseWindow();
	}
}

ProcessResult JKWindow::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case SETFOCUS:
		case KILLFOCUS:
			if(msg.sub!=WinID && msg.detail!=WinID && TestSubWindow(msg.sub,TRUE)) break;
			JKDC *dc=CreateWindowDC();
			if(dc){
				JKRect rect;
				MouseEvents->HideCursor();
				if(GetRegion(TITLEBOX,rect)) DrawTitleBar(*dc,rect);
				if(GetRegion(SYSTEMBOX,rect)) DrawSystemBox(*dc,rect);
				MouseEvents->ShowCursor();
			}
			break;
		defaule:
			break;
	}
	return JKControl::RespondMessage(msg);
}