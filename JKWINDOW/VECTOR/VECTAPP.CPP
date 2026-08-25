#include "vectwin.h"
#include "vectapp.h"

VectorApplication::VectorApplication():
	JKApplication()
{
	BackColor=LtBlue;
}

VectorApplication::~VectorApplication()
{}

void VectorApplication::SetupWindow()
{
	VectorWindow *Vectorwin = new VectorWindow();
	if(Vectorwin){
		Vectorwin->WinAttr|=WA_CHANGEPLACE;
		AddControl(Vectorwin);
	}
}

void VectorApplication::PaintClient(JKDC& dc,JKRect& rect)
{
//	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
//	dc.PutPCX("multipg.pcx",JKPoint(0,0),rect);
	dc.PutIMG("multipg.img",JKPoint(0,0),rect);
}


void VectorApplication::EvRightClick(JKPoint&)
{
	VectorWindow *Vectorwin = new VectorWindow();
//	Vectorwin->WinAttr|=WA_CHANGEPLACE;
	AddControl(Vectorwin);
}