#include <stdio.h>
#include <stdlib.h>
#include "basefunc.h"
#include "jkscrndc.h"

JKScreenDC::JKScreenDC():
	JKDC()
{
	InitDC(JKPoint(0,0),JKRect(0,0,GetMaxX(),GetMaxY()));
}

JKScreenDC::~JKScreenDC()
{}



void JKScreenDC::_PutPixel(JKPoint p,int16 color)
{
	setwritemode(WriteMode);
	putpixel(p.x,p.y,color);
}


void JKScreenDC::_Line(JKRect temp,int16 x1,int16 y1,int16 x2,int16 y2,int16 color)
{
//	setviewport(temp.left,temp.top,temp.right,temp.bottom,1);
	setwritemode(WriteMode);
//	setlinestyle(USERBIT_LINE,CurrentPen.Pen,CurrentPen.Thick);
//	line(x1,y1,x2,y2,color);
//	setviewport(0,0,GetMaxX(),GetMaxY(),1);
	JKPoint p1(x1,y1),p2(x2,y2);
	if(x2<x1) {
		p1.Set(x2,y2);
		p2.Set(x1,y1);
	}
	int16 dx=abs(p2.x-p1.x);
	int16 dy=abs(p2.y-p1.y);
	int16 z = -dx/2;
	int16 yfalg= p2.y>p1.y? 1:-1;

	JKPoint p(p1);

	if(temp.IsContain(p)) putpixel(p.x,p.y,color);
	while(p.x!=p2.x||p.y!=p2.y){
		if(z<0){
			p.x++;	z+=dy;
		}
		else{
			p.y+=yfalg;	z-=dx;
		}
		if(temp.IsContain(p)) putpixel(p.x,p.y,color);
	}
}


void JKScreenDC::_HLine(JKPoint p,WORD delta,int16 color)
{
	if(!delta) return;

	setwritemode(WriteMode);
// setlinestyle(USERBIT_LINE,CurrentPen.Pen,CurrentPen.Thick);
	hline(p.x,p.y,delta,color);
}

void JKScreenDC::_VLine(JKPoint p,WORD delta,int16 color)
{
	if(!delta) return;

	setwritemode(WriteMode);
//	setlinestyle(USERBIT_LINE,CurrentPen.Pen,CurrentPen.Thick);
	vline(p.x,p.y,delta,color);
}

void JKScreenDC::_SolidBar(JKRect rect,int16 color)
{
	setwritemode(WriteMode);
	bar(rect.left,rect.top,rect.right,rect.bottom,color);
}

void JKScreenDC::_WriteImageLine(JKPoint& p,BYTE *buff,int16 n,BYTE putmode)
{
	writeimageline(p.x,p.y,buff,n,putmode);
}

void JKScreenDC::_ScanImageLine(JKPoint& p,BYTE *buff,int16 n)
{
	scanimageline(p.x,p.y,buff,n);
}

//-----------------------------------------------------------//

BOOL JKScreenDC::_PutEngImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount,BYTE lshift,BYTE rshift)
{
	return putengimage(p.x,p.y,buff,color,ycount,lshift,rshift);
}

BOOL JKScreenDC::_PutHanImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount,BYTE lshift,BYTE rshift)
{
	return puthanimage(p.x,p.y,buff,color,ycount,lshift,rshift);
}


/////////////////////////////////
JKScreenDC *GlobalDC=0;//////////
/////////////////////////////////