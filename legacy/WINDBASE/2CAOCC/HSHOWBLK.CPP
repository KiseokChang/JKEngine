#include "hshowblk.h"


HBlockShow::HBlockShow(JKRect& rect,WORD ctlid,JKPoint32& blksize,RealCoordManager& coordman):
	JKControl(rect,ctlid),BlockSize(blksize),RealCoordMan(coordman)
{
	BackColor=LtGray;
	WinAttr|=WA_NOINCLIENT|WA_NOHASFOCUS;
}

void HBlockShow::PaintClient(JKDC& dc,JKRect&)
{
	JKRect32 realrect;
	if(!RealCoordMan.GetClientRealRect(realrect)) return;

	JKRect rect=GetClientRect();
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(TextColor);

	JKPoint32 real=realrect.LeftTop();
	char buffer[11];
	real.x=(realrect.left+BlockSize.x-1)/BlockSize.x*BlockSize.x;
	for(;real.x<=realrect.right;real.x+=BlockSize.x){
		if(RealCoordMan.RealToStr(buffer,real)){
			int32 pos=real.x;
            if(RealCoordMan.RealToPixel_X(pos)){
				buffer[4]=0;
				dc.TextOutX(JKRect(pos-8,rect.top+1,pos+8,rect.top+16),
										buffer+2,ADJ_TOP|ADJ_XCENTER,FALSE);
			}
		}
	}
}
