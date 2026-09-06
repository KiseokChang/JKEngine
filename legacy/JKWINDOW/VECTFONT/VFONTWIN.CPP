#include "vfontwin.h"

VectorFontWindow::VectorFontWindow():
	JKWindow(JKRect(50,50,250,250)),FontSize(32,32)
{
	WinForm=0x07;
	BackColor=White;
	WinAttr|=WA_FIRSTSHOW | WA_TITLEMOVEABLE | WA_BORDERRESIZABLE;
	SetText("Vector Font Window");

	GlobalVFontMan->ResetCTM();
	GlobalVFontMan->SetFont(HANGUL, FN_MYUNGJO);
}

VectorFontWindow::~VectorFontWindow()
{}

void VectorFontWindow::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
}

void VectorFontWindow::PaintClient(JKDC& dc,JKRect&/* paintrect*/)
{
	dc.SetTextColor(Black);
	GlobalVFontMan->SetFontSize(FontSize.x,FontSize.y);
  GlobalVFontMan->DrawString(dc,JKPoint(10,10),"I92Run¸w‹¡¬â");
}

void VectorFontWindow::EvKeyDown(WORD key,WORD)
{
	switch(key){
		case ALT_X:
			CloseWindow();
			break;
		case LEFTARROW:
			if(FontSize.x>4) {
				FontSize.x-=4;
				RedrawClient();
			}
			break;
		case RIGHTARROW:
			if(FontSize.x<1000) {
				FontSize.x+=4;
				RedrawClient();
			}
			break;
		case UPARROW:
			if(FontSize.y<1000) {
				FontSize.y+=4;
				RedrawClient();
			}
			break;
		case DOWNARROW:
			if(FontSize.y>4) {
				FontSize.y-=4;
				RedrawClient();
			}
			break;
		default:
			break;
	}
}


