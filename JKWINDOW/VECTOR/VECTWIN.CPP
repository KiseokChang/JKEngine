#include "jkbutton.h"
#include "vectwin.h"

VectorWindow::VectorWindow():
	JKWindow(JKRect(0,0,getmaxx(),getmaxy()),0),IsKBezMode(FALSE),BezWorkID(0),BezWorkData(0L)

{
	WinForm=0x07;
	BackColor=LtGray;
	WinAttr|=WA_FIRSTSHOW | WA_TITLEMOVEABLE | WA_BORDERRESIZABLE;
	Depth=1;
	BorderSize=4;
	SetText("¯¡Ðñ¶w Window");
}

VectorWindow::~VectorWindow()
{
}

void VectorWindow::InitWindow()
{
	JKWindow::InitWindow();
	SetFocus();
	uint16 width = ClientRect.Width();
	uint16 height= ClientRect.Height();
	BezEngin.NormalizeControlPoint(JKRect(width*3/8,height*3/8,width*5/8,height*5/8),CtrlPoint);
}

void VectorWindow::SetupWindow()
{
	AddControl(new JKButton(JKRect(10,20,50,50),ID_BUTTON+1));
	AddControl(new JKButton(JKRect(10,60,50,90),ID_BUTTON+2));
	AddControl(new JKButton(JKRect(10,100,50,130),ID_BUTTON+3));
}

#define CTRLPOINTXSIZE	8
#define CTRLPOINTYSIZE	8

void VectorWindow::PaintClient(JKDC& dc,JKRect&)
{
	JKPoint Temp[4];
	double u,v;
	for(v=0;v<=1.;v+=0.03){
		BezEngin.GetCurvePoint(IsKBezMode,TRUE,v,CtrlPoint,Temp);
		dc.Bezier(Temp,LtBlue,IsKBezMode);
	}
	for(u=0;u<=1.;u+=0.03){
		BezEngin.GetCurvePoint(IsKBezMode,FALSE,u,CtrlPoint,Temp);
		dc.Bezier(Temp,LtBlue,IsKBezMode);
	}
	int8 pname[3]="00";
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(LtRed);
	for(uint16 i=0;i<4;i++){
		pname[0]='0'+i;
		for(uint16 j=0;j<4;j++){
			pname[1]='0'+j;
			JKPoint& p=CtrlPoint[i][j];
			JKRect rect(p.x-CTRLPOINTXSIZE,p.y-CTRLPOINTYSIZE,p.x+CTRLPOINTXSIZE,p.y+CTRLPOINTYSIZE);
			dc.SolidBar(rect,Yellow);
			dc.TextOutX(rect,pname,ADJ_XYCENTER);
		}
	}
}

int16 VectorWindow::CheckControlPoint(JKPoint& point)
{
	for(uint16 i=0;i<4;i++){
		for(uint16 j=0;j<4;j++){
			JKPoint& p=CtrlPoint[i][j];
			JKRect rect(p.x-CTRLPOINTXSIZE,p.y-CTRLPOINTYSIZE,p.x+CTRLPOINTXSIZE,p.y+CTRLPOINTYSIZE);
			if(rect.IsContain(point)) return i*4+j;
		}
	}
	return -1;
}

void VectorWindow::EvLeftClick(JKPoint& p)
{
	int16 index=CheckControlPoint(p+ClientOrgPos);
	if(index>=0&&index<16){
		BezWorkID=1;
		BezWorkData=index;
		BeginCapture();
		MouseEvents->SetCursorID(CURSOR_CROSS);
	}
}

void VectorWindow::EvLeftRelease(JKPoint& p)
{
	if(BezWorkID==1){
		EndCapture();
		int16 index = BezWorkData;
		if(index>=0&&index<16){
			CtrlPoint[index/4][index%4]=p+ClientOrgPos;
			BezWorkID=0;
			RedrawClient();
			MouseEvents->SetCursorID(CursorID);
		}
	}
}

void VectorWindow::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			{
			uint16 width = ClientRect.Width();
			uint16 height= ClientRect.Height();
			BezEngin.NormalizeControlPoint(JKRect(width*3/8,height*3/8,width*5/8,height*5/8),CtrlPoint);
			RedrawClient();
			}
			break;
		case ID_BUTTON+2:
			IsKBezMode=!IsKBezMode;
			RedrawClient();
			break;
		case ID_BUTTON+3:
			IsKBezMode=!IsKBezMode;
			JKPoint Temp[4][4];
			BezEngin.ChangeBezMode(IsKBezMode,CtrlPoint,Temp);
			memcpy(CtrlPoint,Temp,4*4*sizeof(JKPoint));
			RedrawClient();
			break;
	}
}

void VectorWindow::EvKeyDown(WORD key,WORD)
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

