#include "memdc.h"

JKMemoryDC::JKMemoryDC(uint16 xsize,uint16 ysize,BYTE initcolor):
	JKDC(),XSize(xsize),YSize(ysize),DataBuffer(0),ScreenBuffer(0)
{
	DataBuffer = new BYTE[4+XSize*YSize];
	if(DataBuffer){
		*((WORD *)DataBuffer)=XSize-1;
		*((WORD *)(DataBuffer+2))=YSize-1;
		ScreenBuffer=DataBuffer+4;
		InitDC(JKPoint(0,0),JKRect(0,0,GetMaxX(),GetMaxY()));
		ClearBuffer(initcolor);
	}
}

JKMemoryDC::JKMemoryDC(JKDC& dc,BYTE initcolor):
	JKDC(),DataBuffer(0),ScreenBuffer(0)
{
	XSize=dc.GetMaxX()+1;
	YSize=dc.GetMaxY()+1;
	DataBuffer = new BYTE[4+XSize*YSize];
	if(DataBuffer){
		*((WORD *)DataBuffer)=XSize-1;
		*((WORD *)(DataBuffer+2))=YSize-1;
		ScreenBuffer=DataBuffer+4;
		memset(ScreenBuffer,initcolor,XSize*YSize);
		AssignDC(dc);
	}
}

JKMemoryDC::~JKMemoryDC()
{
	if(DataBuffer) delete[] DataBuffer;
}


void JKMemoryDC::Set_PutMode(BYTE *dest,BYTE value,uint16 count,BYTE putmode)
{
	if(dest){
		switch(putmode){
			case COPY_PUT:
				while(count--)	*dest++ = value;
				break;
			case XOR_PUT:
				while(count--)	*dest++^= value;
				break;
			case OR_PUT:
				while(count--)	*dest++|= value;
				break;
			case AND_PUT:
				while(count--)	*dest++&= value;
				break;
			case NOT_PUT:
				while(count--)	*dest++ =~value;
				break;
			default:
				while(count--)  *dest++ = value;
				break;
		}
	}
}

void JKMemoryDC::Set_PutMode(BYTE *dest,BYTE *src,uint16 count,BYTE putmode)
{
	if(dest&&src){
		switch(putmode){
			case COPY_PUT:
				while(count--)	*dest++ = *src++;
				break;
			case XOR_PUT:
				while(count--)	*dest++^= *src++;
				break;
			case OR_PUT:
				while(count--)	*dest++|= *src++;
				break;
			case AND_PUT:
				while(count--)	*dest++&= *src++;
				break;
			case NOT_PUT:
				while(count--)	*dest++= ~*src++;
				break;
			default:
				while(count--)  *dest++ =*src++;
				break;
		}
	}
}

void JKMemoryDC::ClearBuffer(BYTE initcolor)
{
	memset(ScreenBuffer,initcolor,XSize*YSize);
}

void JKMemoryDC::_PutPixel(JKPoint p,int16 color)
{
	if(!ScreenBuffer) return;
	Set_PutMode(ScreenBuffer+p.x+p.y*XSize,color,1,WriteMode);
}


void JKMemoryDC::_Line(JKRect temp,int16 x1,int16 y1,int16 x2,int16 y2,int16 color)
{
	if(!ScreenBuffer) return;
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

	if(temp.IsContain(p)) _PutPixel(p,color);
	while(p.x!=p2.x||p.y!=p2.y){
		if(z<0){
			p.x++;	z+=dy;
		}
		else{
			p.y+=yfalg;	z-=dx;
		}
		if(temp.IsContain(p)) _PutPixel(p,color);
	}
}


void JKMemoryDC::_HLine(JKPoint p,WORD delta,int16 color)
{
	if(!ScreenBuffer) return;
	if(!delta) return;
	Set_PutMode(ScreenBuffer+p.x+p.y*XSize,color,delta,WriteMode);
}

void JKMemoryDC::_VLine(JKPoint p,WORD delta,int16 color)
{
	if(!ScreenBuffer) return;
	if(!delta) return;
	BYTE *dest=ScreenBuffer+p.x+p.y*XSize;
	while(delta--){
		Set_PutMode(dest,color,1,WriteMode);
		dest+=XSize;
	}
}

void JKMemoryDC::_SolidBar(JKRect rect,int16 color)
{
	if(!ScreenBuffer) return;
	int16 width =rect.Width()+1;
	int16 height=rect.Height()+1;
	if(width>0 && height>0) {
		BYTE *dest=ScreenBuffer+rect.left+rect.top*XSize;
		while(height--){
			Set_PutMode(dest,color,width,WriteMode);
			dest+=XSize;
		}
	}
}

//void JKMemoryDC::_Arc(JKRect temp,JKPoint p, int16 stangle, int16 endangle, int16 radius,int16 color)
void JKMemoryDC::_Arc(JKRect,JKPoint , int16 , int16 , int16 ,int16 )
{
/*
	setviewport(temp.left,temp.top,temp.right,temp.bottom,1);
	p-=temp.LeftTop();
	setwritemode(WriteMode);
	setcolor(color);
	arc(p.x,p.y,stangle,endangle,radius);
	p+=temp.LeftTop();
	setviewport(0,0,getmaxx(),getmaxy(),1);
*/
}

//void JKMemoryDC::_Circle(JKRect temp,JKPoint p,int16 radius,int16 color)
void JKMemoryDC::_Circle(JKRect ,JKPoint ,int16 ,int16 )
{
/*
	setviewport(temp.left,temp.top,temp.right,temp.bottom,1);
	p-=temp.LeftTop();

	setwritemode(WriteMode);
	setcolor(TextColor);
	setlinestyle(USERBIT_LINE,CurrentPen.Pen,CurrentPen.Thick);
	setfillpattern(CurrentBrush.Brush,color);
	circle(p.x,p.y,radius);
	p+=temp.LeftTop();
	setviewport(0,0,getmaxx(),getmaxy(),1);
*/
}

//void JKMemoryDC::_Pieslice(JKRect temp,JKPoint p, int16 stangle, int16 endangle, int16 radius,int16 color)
void JKMemoryDC::_Pieslice(JKRect,JKPoint , int16 , int16 , int16 ,int16 )
{
/*
	setviewport(temp.left,temp.top,temp.right,temp.bottom,1);
	p-=temp.LeftTop();
	setwritemode(WriteMode);
	setcolor(TextColor);
	setlinestyle(USERBIT_LINE,CurrentPen.Pen,CurrentPen.Thick);
	setfillpattern(CurrentBrush.Brush,color);
	pieslice(p.x,p.y,stangle,endangle,radius);
	p+=temp.LeftTop();
	setviewport(0,0,getmaxx(),getmaxy(),1);
*/
}
//void JKMemoryDC::_DrawPolygon(JKRect temp,JKPoint *polypoint16,WORD numpoints,int16 color)
void JKMemoryDC::_DrawPolygon(JKRect ,JKPoint *,WORD ,int16 )
{
/*
	setviewport(temp.left,temp.top,temp.right,temp.bottom,1);
	for(WORD i=0;i<numpoints;i++) polypoint[i]-=temp.LeftTop();
	setwritemode(WriteMode);
	setlinestyle(USERBIT_LINE,CurrentPen.Pen,CurrentPen.Thick);
	setcolor(color);
	drawpoly(numpoints,(int16 *)polypoint);
	for(i=0;i<numpoints;i++) polypoint[i]+=temp.LeftTop();
	setviewport(0,0,getmaxx(),getmaxy(),1);
*/
}

//void JKMemoryDC::_FillPolygon(JKRect temp,JKPoint *polypoint,WORD numpoints,int16 color)
void JKMemoryDC::_FillPolygon(JKRect ,JKPoint *,WORD ,int16 )
{
/*
	setviewport(temp.left,temp.top,temp.right,temp.bottom,1);
	for(WORD i=0;i<numpoints;i++) polypoint[i]-=temp.LeftTop();
	setwritemode(WriteMode);
	setcolor(TextColor);
	setlinestyle(USERBIT_LINE,CurrentPen.Pen,CurrentPen.Thick);
	setfillpattern(CurrentBrush.Brush,color);
	fillpoly(numpoints,(int16 *)polypoint);
	for(i=0;i<numpoints;i++) polypoint[i]+=temp.LeftTop();
	setviewport(0,0,getmaxx(),getmaxy(),1);
*/
}

void JKMemoryDC::_WriteImageLine(JKPoint& p,BYTE *buff,int16 n,BYTE putmode)
{
	if(!ScreenBuffer) return;
	if(n>0) Set_PutMode(ScreenBuffer+p.x+p.y*XSize,buff,n,putmode);
}

void JKMemoryDC::_ScanImageLine(JKPoint& p,BYTE *buff,int16 n)
{
	if(!ScreenBuffer) return;
	if(n>0) memcpy(buff,ScreenBuffer+p.x+p.y*XSize,n);
}

//-----------------------------------------------------------//

BOOL JKMemoryDC::_PutEngImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount,BYTE lshift,BYTE rshift)
{
	if(!buff) return FALSE;
	int16 endx=8-rshift;
	for(uint16 i=0;i<ycount;i++){
		BYTE temp=*(buff+i);
		temp<<=lshift;
		JKPoint temppos(p.x+lshift,p.y+i);
		for(uint16 j=lshift;j<endx;j++){
			if(temp&0x80) _PutPixel(temppos,color);
			temp<<=1;	temppos.x++;
		}
	}
	return TRUE;
}

BOOL JKMemoryDC::_PutHanImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount,BYTE lshift,BYTE rshift)
{
	if(!buff) return FALSE;
	int16 endx=16-rshift;
	for(uint16 i=0;i<ycount;i++){
		WORD temp=MAKEWORD(buff[i*2+1],buff[i*2]);
		temp<<=lshift;
		JKPoint temppos(p.x+lshift,p.y+i);
		for(uint16 j=lshift;j<endx;j++){
			if(temp&0x8000) _PutPixel(temppos,color);
			temp<<=1; temppos.x++;
		}
	}
	return TRUE;
}

