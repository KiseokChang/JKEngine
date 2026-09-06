#include "vfontwin.h"
#include "vfontapp.h"

VectorFontApplication::VectorFontApplication():
	JKApplication()
{
	BackColor=LtBlue;
}

VectorFontApplication::~VectorFontApplication()
{}

void VectorFontApplication::SetupWindow()
{
	VectorFontWindow *VectorFontwin = new VectorFontWindow();
	if(VectorFontwin){
		VectorFontwin->WinAttr|=WA_CHANGEPLACE;
		AddControl(VectorFontwin);
	}
}

void VectorFontApplication::PaintClient(JKDC& dc,JKRect& rect)
{}


void VectorFontApplication::EvRightClick(JKPoint&)
{
	VectorFontWindow *VectorFontWin = new VectorFontWindow();
	AddControl(VectorFontWin);
}


///////////////////////////////////////////////////////

EventHandler *CreateApplication()
{
	return new VectorFontApplication();
}

///////////////////////////////////////////////////////

