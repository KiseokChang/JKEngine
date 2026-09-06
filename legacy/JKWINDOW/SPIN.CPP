#include "jkapp.h"
#include "spin.h"

Spin::Spin(JKRect& rect,WORD ctlid,JKControl *parent):
	JKControl(rect,ctlid,parent),IsActive(FALSE)
{
	WinRect.right=WinRect.left+17;
	WinRect.bottom=WinRect.top+17;
}

JKRect Spin::CalcClientRect()
{
	JKRect rect =WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.Expand(-1);
	return rect;
}

void Spin::PaintWindow(JKDC& dc,JKRect&)
{
	dc.Rectangle(JKRect(0,0,17,17),Black);
}

void Spin::PaintClient(JKDC& dc,JKRect&)
{
	JKPoint p;
	ResourceBase *res;
	BYTE *data;

	res=ResMan->GetResource(InternImageID[SpinUp]);
	if(res){
		data = res->AllocRealData();
		if(data) {
			p = JKPoint(0,0);
			dc.PutImage(p,data);
			res->FreeRealData(data);
		}
	}
	res=ResMan->GetResource(InternImageID[SpinDown]);
	if(res){
		data = res->AllocRealData();
		if(data) {
			p = JKPoint(0,8);
			dc.PutImage(p,data);
			res->FreeRealData(data);
		}
	}
}

JKRect Spin::CalcUpRect()
{
	return JKRect(0,0,15,7);
}

JKRect Spin::CalcDownRect()
{
	return JKRect(0,8,15,15);
}

void Spin::EvLeftClick(JKPoint& p)
{
	IsActive=TRUE;
	if(CalcUpRect().IsContain(p))
		SendControlMessage(SPIN_UP);
	else if(CalcDownRect().IsContain(p))
		SendControlMessage(SPIN_DOWN);
}

void Spin::EvLeftPressing(JKPoint& p)
{
	if(IsActive){
		if(CalcUpRect().IsContain(p))
			SendControlMessage(SPIN_UP);
		else if(CalcDownRect().IsContain(p))
			SendControlMessage(SPIN_DOWN);
	}
}

void Spin::EvLeftRelease(JKPoint& )
{
	if(IsActive) IsActive=FALSE;
}
void Spin::EvNcLeftRelease(JKPoint& )
{
	if(IsActive) IsActive=FALSE;
}