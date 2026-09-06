#include "showbrd.h"

ShowBoard::ShowBoard(JKRect& rect,WORD ctlid,BYTE*& buf,JKControl *parent):
	JKControl(rect,ctlid,parent),ImageBuff(buf)
{}

void ShowBoard::PaintClient(JKDC& dc,JKRect&)
{
	if(ImageBuff) dc.PutImage(JKPoint(0,0),ImageBuff,COPY_PUT);
}

