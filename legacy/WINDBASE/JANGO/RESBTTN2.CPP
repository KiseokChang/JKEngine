#include "resbttn2.h"

JKResButton2::JKResButton2(JKRect& rect,WORD ctlid,WORD resid,JKControl *parent):
	JKResButton(rect,ctlid,resid,parent)
{
	Depth=0;
	Depth2=2;
}

void JKResButton2::PaintClient(JKDC& dc,JKRect& rect)
{
	JKResButton::PaintClient(dc,rect);
	BYTE color1,color2;
	if(!Status){
		if(!HasFocus()) {
			color1=White; color2=Gray;
		}
		else {
			color1=LtRed;	color2=LtRed;
		}
	}
	else {
		color1=Gray;	color2=White;
	}
	JKRect clntrect=ClientRect.SetLeftTopBy();
	dc.Rectangle3D(clntrect,Depth2,color1,color2);
}

