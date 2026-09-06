#include "testwin.h"
#include "testapp.h"

TestApplication::TestApplication():
	JKApplication()
{
	BackColor=LtBlue;
}

TestApplication::~TestApplication()
{}

void TestApplication::SetupWindow()
{
	TestWindow *Testwin = new TestWindow();
	if(Testwin){
		Testwin->WinAttr|=WA_CHANGEPLACE;
		AddControl(Testwin);
	}
}

void TestApplication::PaintClient(JKDC& dc,JKRect& rect)
{
	dc.PutIMG("kk.img",JKPoint(0,0),rect);
}


void TestApplication::EvRightClick(JKPoint&)
{
	TestWindow *Testwin = new TestWindow();
	AddControl(Testwin);
}