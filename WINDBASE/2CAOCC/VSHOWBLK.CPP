#include "vshowblk.h"

VBlockShow::VBlockShow(JKRect& rect,WORD ctlid,JKPoint32& blksize,RealCoordManager& coordman):
	JKControl(rect,ctlid),BlockSize(blksize),RealCoordMan(coordman)
{
	BackColor=LtGray;
	WinAttr|=WA_NOINCLIENT|WA_NOHASFOCUS;
}

void VBlockShow::PaintClient(JKDC& dc,JKRect&)
{
	JKRect32 realrect;
	if(!RealCoordMan.GetClientRealRect(realrect)) return;

	JKRect rect=GetClientRect();
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(TextColor);

	JKPoint32 real=realrect.LeftTop();
	char buffer[11];
	real.y=(realrect.top+BlockSize.y-1)/BlockSize.y*BlockSize.y;
	for(;real.y<=realrect.bottom;real.y+=BlockSize.y){
		if(RealCoordMan.RealToStr(buffer,real)){
			int32 pos=real.y;
			if(RealCoordMan.RealToPixel_Y(pos)){
      	buffer[8]=0;
				dc.TextOutX(JKRect(rect.right-8*2,pos-8,rect.right-1,pos+8),
										buffer+6,ADJ_RIGHT|ADJ_YCENTER,FALSE);
			}
		}
	}
}

