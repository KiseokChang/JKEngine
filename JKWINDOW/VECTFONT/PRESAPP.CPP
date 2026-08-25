#include "preswin.h"
#include "presapp.h"

PresentApplication::PresentApplication():
	JKApplication()
{
	BackColor=LtBlue;
}

PresentApplication::~PresentApplication()
{}

void PresentApplication::SetupWindow()
{
	PresentWindow *Presentwin = new PresentWindow();
	if(Presentwin){
		Presentwin->WinAttr|=WA_CHANGEPLACE;
		AddControl(Presentwin);
	}
}

void PresentApplication::PaintClient(JKDC& dc,JKRect& rect)
{}


void PresentApplication::EvRightClick(JKPoint&)
{
	PresentWindow *PresentWin = new PresentWindow();
	AddControl(PresentWin);
}


///////////////////////////////////////////////////////

EventHandler *CreateApplication()
{
	return new PresentApplication();
}

///////////////////////////////////////////////////////

