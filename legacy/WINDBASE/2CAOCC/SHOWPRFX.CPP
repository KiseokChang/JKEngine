#include "showprfx.h"

PrefixShow::PrefixShow(JKRect& rect,WORD ctlid,RealCoordManager& coordman):
	JKButton(rect,ctlid),RealCoordMan(coordman)
{
	BackColor=Yellow;
	TextColor=Black;
	WinAttr|=WA_NOINCLIENT|WA_NOHASFOCUS;
}

void PrefixShow::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=GetClientRect();
	dc.Box3D(rect,-1,BackColor);
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(TextColor);
	char buffer[11];
	if(RealCoordMan.RealToStr(buffer,RealCoordMan.RealOrgPos)){
		buffer[2]=0;
		dc.TextOutX(rect,buffer,ADJ_XYCENTER,FALSE);
	}
}

