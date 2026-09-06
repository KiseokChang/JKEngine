#include "cordview.h"

CoordView::CoordView(JKRect& rect,WORD ctlid,RealCoordManager& coordman):
	JKEdit(rect,ctlid,0,10,FALSE,FALSE),RealCoordMan(coordman)
{
	BackColor=Black;
  TextColor=Yellow;
	WinAttr|=WA_NOINCLIENT;
	SetText("          ");
}

void CoordView::SetCoord(JKPoint32 coord)
{
	char buffer[11];
	if(!RealCoordMan.RealToStr(buffer,coord))
		strcpy(buffer,"          ");
	if(strcmp(GetText(),buffer)) {
		SetText(buffer);
		RedrawClient();
	}
}
