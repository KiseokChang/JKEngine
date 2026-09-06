#include <math.h>
#include "filedlg.h"
#include "jkbutton.h"
#include "scrllist.h"
#include "poplist.h"
#include "jkedit.h"
#include "checkbox.h"
#include "combobox.h"
#include "joclock.h"
#include "memdc.h"
#include "testwin.h"

TestWindow::TestWindow():
	JKWindow(JKRect(50,50,250,250))
{
	WinForm=0x07;
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW | WA_TITLEMOVEABLE | WA_BORDERRESIZABLE;
	SetText("Multi ¯¡Ðñ¶w Window");
	ClientColor[0]=Yellow;
	ClientColor[1]=LtBlue;
}

TestWindow::~TestWindow()
{
}

void TestWindow::InitWindow()
{
	JKWindow::InitWindow();
	SetFocus();
}

void TestWindow::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.Set(10,10,100,100);
	AddControl(new JKButton(rect,ID_BUTTON+1));
	rect.Set(10,110,100,130);
	AddControl(new JOClock(rect,0));
	rect.Set(10,140,400,230);
	JKControl *ctrl=new JKEdit(rect,ID_EDIT+2,0,10,FALSE,FALSE);
  if(ctrl) ctrl->SetText("          ");
	AddControl(ctrl);
	rect.Set(10,240,100,270);
	AddControl(new JKCheckBox(rect,ID_CHECKBOX+1));

	rect.Set(10,240,100,270);
	JKButton *bttn=new JKButton(rect,ID_STATIC+1);
	bttn->SetEnable(FALSE);
	AddControl(bttn);

	rect.Set(100,240,200,270);
	JKWindow *win = new JKWindow(rect);
	win->WinAttr |= WA_FIRSTSHOW|WA_TITLEMOVEABLE|WA_BORDERRESIZABLE|WA_NORESTRICT;
	win->SetText("Normal");
	AddControl(win);
}

void TestWindow::EvShowWindow()
{
	JKWindow::EvShowWindow();
	SetFocus();
}

void TestWindow::PaintClient(JKDC& dc,JKRect& paintrect)
{
	dc.SetTextColor(Black);
	GlobalVFontMan->SetFontSize(100,100);
	GlobalVFontMan->DrawString(dc,JKPoint(10,200),"I92Run¸w‹¡¬â");

	dc.Pieslice(JKPoint(500,300),M_PI/3,M_PI*5/6,JKPoint(300,100),LtBlue);
	dc.Pieslice(JKPoint(500-4,300-2),M_PI*5/6,M_PI/3,JKPoint(300,100),LtRed);
/*
	JKRect rect(0,0,1023,767);
	if(MemDC)	{
		dc.SolidBar(rect.ExpandBy(-20),LtGray);
		dc.Line(rect.LeftTop(),rect.RightBottom(),LtRed);
		dc.PutIMG("bomb.img",JKPoint(100,100),paintrect);
		dc.Circle(JKPoint(510,380),200,LtBlue);
		dc.Arc(JKPoint(510,380),0.,M_PI/2.,200,Yellow);
		dc.IsOverWrite=FALSE;
		dc.TextOutX(rect,"Ðe‹i•¡\n¿¢Ó¡“eŠ…\nGoodTimes\nBadTimes\nSkip me",ADJ_XYCENTER);
	}
*/
}

void TestWindow::EvKeyDown(WORD key,WORD)
{
	switch(key){
		case ALT_X:
			CloseWindow();
			break;
		case LEFTARROW:
			ScrollClient(JKPoint(-100,0));
			break;
		case RIGHTARROW:
			ScrollClient(JKPoint( 100,0));
			break;
		case UPARROW:
			ScrollClient(JKPoint(0,-100));
			break;
		case DOWNARROW:
			ScrollClient(JKPoint(0, 100));
			break;
		default:
			break;
	}
}

void TestWindow::EvDeadKeyDown(WORD key,WORD)
{
	if(key==ALT_X) CloseWindow();
}

void TestWindow::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		FileDialog *filedlg=new FileDialog(this);
		if(filedlg){
			filedlg->WinAttr|=WA_TITLEMOVEABLE | WA_BORDERRESIZABLE;
			filedlg->Run();
			delete filedlg;
		}
	}
}

void TestWindow::EvRightClick(JKPoint&)
{
	if(WorkID==WW_WINDOWMOVE){
		JKDC *dc = CreateScreenDC();
		if(dc){
			MouseEvents->HideCursor();
			dc->SetWriteMode(XOR_PUT);
			dc->Rectangle3D(ResizeRect,3,0xff,0xff);
			MouseEvents->ShowCursor();
		}
		EndCapture();
		JKRect rect;
		if(Parent) rect = Parent->ClientRect.SetLeftTopBy(JKPoint(0,0));
		else rect.Set(0,0,getmaxx(),getmaxy());
		ChangeWindowSize(rect);
		WorkID=0;
	}
}

void TestWindow::EvMoveWindow(JKRect )
{
	SetText("¶áÃ¡ˆa µ«‰v¯s“¡”a");
	JKDC *dc=CreateWindowDC();
	if(dc){
		JKRect rect;
		if(GetRegion(TITLEBOX,rect)) {
			MouseEvents->HideCursor();
			DrawTitleBar(*dc,rect);
			MouseEvents->ShowCursor();
		}
	}
}

void TestWindow::EvResizeWindow(JKRect )
{
	SetText("Ça‹¡ˆa ¤aŽá´ö¯s“¡”a");
	JKDC *dc=CreateWindowDC();
	if(dc){
		JKRect rect;
		if(GetRegion(TITLEBOX,rect)) {
			MouseEvents->HideCursor();
			DrawTitleBar(*dc,rect);
			MouseEvents->ShowCursor();
		}
	}
}

void TestWindow::EvPlaceForward()
{
	SetText(" … ¶á¡ µ©œaµÖ¯s“¡”a");
	JKDC *dc=CreateWindowDC();
	if(dc){
		JKRect rect;
		if(GetRegion(TITLEBOX,rect)) {
			MouseEvents->HideCursor();
			DrawTitleBar(*dc,rect);
			MouseEvents->ShowCursor();
		}
	}
}

void TestWindow::EvPlaceBackward()
{
	SetText(" … ´aœ¡ aˆv¯s“¡”a");
	JKDC *dc=CreateWindowDC();
	if(dc){
		JKRect rect;
		if(GetRegion(TITLEBOX,rect)) {
			MouseEvents->HideCursor();
			DrawTitleBar(*dc,rect);
			MouseEvents->ShowCursor();
		}
	}
}

void TestWindow::EvTimerHappen(DWORD /*happencount*/)
{
	CloseWindow();

/*	if(happencount%2){
	 BYTE temp=ClientColor[0];
	 ClientColor[0]=ClientColor[1];
	 ClientColor[1]=temp;
	 RedrawClient();
	}
*/
}
