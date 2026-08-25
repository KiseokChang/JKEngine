#include "sintbl.h"
#include "preswin.h"

PresentWindow::PresentWindow():
	JKWindow(JKRect(50,50,250,250)),
	TxtCenter(0,0),TxtColor(Yellow),TxtCount(0)
{
	WinForm=0x07;
	BackColor=White;
	WinAttr|=WA_FIRSTSHOW | WA_TITLEMOVEABLE | WA_BORDERRESIZABLE;
	SetText("Vector Font Window");

	GlobalVFontMan->ResetCTM();
	GlobalVFontMan->SetFont(HANGUL, FN_MYUNGJO);
	SetTimer(500);
}

PresentWindow::~PresentWindow()
{}

void PresentWindow::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
	JKRect rect=GetClientRect();
	TxtCenter.Set(rect.Center());
}

void PresentWindow::PaintClient(JKDC& dc,JKRect&/* paintrect*/)
{
	GlobalVFontMan->SetFontSize(4+TxtCount/12,4+TxtCount/12);
	dc.SetTextColor(TxtColor);
	TxtColor=(TxtColor+1)%256;
	if (TxtColor==dc.BackColor) TxtColor++;
	int16 r=4+TxtCount/4;
	GlobalVFontMan->ResetCTM();
	GlobalVFontMan->RotateFont(TxtCount);
	JKPoint p(TxtCenter.x+(int16)((int32)r*CosTbl[TxtCount%360]/1024),
								TxtCenter.y+(int16)((int32)r*SinTbl[TxtCount%360]/1024));
	GlobalVFontMan->DrawString(dc,p,"I92Run¸w‹¡¬â");
}

void PresentWindow::EvTimerHappen(DWORD /*happencount*/)
{
	TxtCount+=10;
	if(TxtCount>=720) {
		TxtCount=0;
		RedrawClient();
	}
	else RedrawClient(FALSE);
}

void PresentWindow::EvResizeWindow(JKRect )
{
	JKRect rect=GetClientRect();
	TxtCenter.Set(rect.Center());
	RedrawClient();
}

