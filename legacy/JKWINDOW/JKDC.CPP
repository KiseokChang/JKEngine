#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "jkdc.h"

int8 BackFont[32]={
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};
JKDC::JKDC():
	GDISet()
{
	memset(ColorConvTable256,0,256);
}

JKDC::~JKDC()
{
}


BOOL JKDC::InitDC(JKPoint p,JKRect rect)
{
	if(InitClipRegion(p,rect)){
		InitAttribute();
		return TRUE;
	}
	return FALSE;
}

BOOL JKDC::InitClipRegion(JKPoint p,JKRect rect)
{
	ClipRegion.ClearNode();
	ClipRegion.PutRect(rect);
	TempOrgPos=p;
	CurrentPos.Set(0,0);
	return TRUE;
}

BOOL JKDC::InitDC(JKPoint p,RectQue& region)
{
	if(InitClipRegion(p,region)){
		InitAttribute();
		return TRUE;
	}
	return FALSE;
}

BOOL JKDC::InitClipRegion(JKPoint p,RectQue& region)
{
	TempOrgPos=p;
	ClipRegion=region;
	return ClipRegion.NodeCount? TRUE:FALSE;
}

void JKDC::InitAttribute()
{
	SetWriteMode(COPY_PUT);
	SetTextColor(Black);
	SetBackColor(White);

	SetPen(GetStdPen(SOLID_PEN));
	SetBrush(GetStdBrush(SOLID_BRUSH));
	IsOverWrite=TRUE;
}

void JKDC::SetWriteMode(BYTE wmode)
{
	WriteMode=wmode;
}

void JKDC::SetTextColor(int16 color)
{
	TextColor=color;
}

void JKDC::SetBackColor(int16 color)
{
	BackColor=color;
}

void JKDC::SetPen(JKPen pen)
{
	CurrentPen=pen;
}

void JKDC::SetBrush(JKBrush brush)
{
	CurrentBrush=brush;
}

void JKDC::PutPixel(JKPoint p,int16 color)
{
	p+=TempOrgPos;

	if(!JKRect(0,0,GetMaxX(),GetMaxY()).IsContain(p)) return;
	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		if(container->Rect.IsContain(p)){
			_PutPixel(p,color);
			break;
		}
		container=container->next;
	}
}

void JKDC::Circle(JKPoint p,int16 radius,int16 color)
{
	JKPoint r;
	int16 mx=0,nx=radius;
	int16 t=3-2*radius;
	do{
		for(BYTE i=0;i<8;i++){
			if(i&0x01){	r.x=nx; r.y=mx;	}
			else{	r.x=mx; r.y=nx;	}
			if(i&0x02) r.x=-r.x;
			if(i&0x04) r.y=-r.y;

			PutPixel(p+r,color);
		}
		if(t<0) {
			t+=4*mx+6;
		}
		else {
			t+=4*(mx-nx)+10;
			nx--;
		}
		mx++;
	}while(mx<=nx);
}

void JKDC::Arc(JKPoint p, double startangle, double endangle, int16 radius,int16 color)
{
	int32 startx=10000*sin(startangle) ,starty=-10000*cos(startangle);
	int32 endx  =10000*sin(endangle),endy  =-10000*cos(endangle);
	double diffangle=fmod(endangle-startangle,M_PI*2);
	if(diffangle<0) diffangle+=M_PI*2;
	BOOL flag= diffangle>M_PI? 1:0;

	JKPoint r;
	int16 mx=0,nx=radius;
	int16 t=3-2*radius;
	do{
		for(BYTE i=0;i<8;i++){
			if(i&0x01){	r.x=nx; r.y=mx;	}
			else{	r.x=mx; r.y=nx;	}
			if(i&0x02) r.x=-r.x;
			if(i&0x04) r.y=-r.y;

			if(startx==endx&&starty==endy) PutPixel(p+r,color);
			else if(!flag){	if(starty*r.x-startx*r.y<=0 && endy*r.x-endx*r.y>=0) PutPixel(p+r,color);}
			else if(starty*r.x-startx*r.y<=0 || endy*r.x-endx*r.y>=0)	PutPixel(p+r,color);
		}
		if(t<0) {
			t+=4*mx+6;
		}
		else {
			t+=4*(mx-nx)+10;
			nx--;
		}
		mx++;
	}while(mx<=nx);
}

void JKDC::Pieslice(JKPoint p, double startangle, double endangle, int16 radius,int16 color)
{
	JKPoint StartPos(p.x+radius*sin(startangle),p.y-radius*cos(startangle));
	JKPoint EndPos(p.x+radius*sin(endangle),p.y-radius*cos(endangle));
	Line(p,StartPos,color);	Line(p,EndPos,color);
	Arc(p,startangle,endangle,radius,color);
}

void JKDC::Circle(JKPoint p,JKPoint radius,int16 color)
{
	int32 Squared[2]={(int32)radius.x*radius.x,(int32)radius.y*radius.y};
	int32 Adjust[2];
	int16 Working[2];

	JKPoint r;
	int16 cp[2]={p.x,p.y};
	int16 rad[2]={radius.x,radius.y};
	for(int16 i=0;i<2;i++) {
		r.x = i? 0:radius.y;
		r.y = i? radius.x:0;
		PutPixel(p+r,color);
		r.x = i? 0:-radius.y;
		r.y = i? -radius.x:0;
		PutPixel(p+r,color);

		Working[i]=0; Working[!i]=rad[!i];
		Adjust[i]=0; Adjust[!i]=Squared[i]*2*rad[!i];

		int32 Threshold=Squared[i]/4-Squared[i]*rad[!i];
		while(1){
			Threshold+=Adjust[i]+Squared[!i];
			if(Threshold>=0){
				Adjust[!i]-=Squared[i]*2;
				Threshold-=Adjust[!i];
				Working[!i]--;
			}
			Adjust[i]+=Squared[!i]*2;
			Working[i]++;
			if(Adjust[i]>=Adjust[!i]) break;

			for(BYTE j=0;j<4;j++){
				r.x = (j&0x01)? Working[0]:-Working[0];
				r.y = (j&0x02)? Working[1]:-Working[1];
				PutPixel(p+r,color);
			}
		}
	}
}

void JKDC::Arc(JKPoint p, double startangle, double endangle, JKPoint radius,int16 color)
{
	int32 startx=radius.x*sin(startangle) ,starty=-radius.y*cos(startangle);
	int32 endx  =radius.x*sin(endangle),endy  =-radius.y*cos(endangle);
	double diffangle=fmod(endangle-startangle,M_PI*2);
	if(diffangle<0) diffangle+=M_PI*2;
	BOOL flag= diffangle>M_PI? 1:0;

	int32 Squared[2]={(int32)radius.x*radius.x,(int32)radius.y*radius.y};
	int32 Adjust[2];
	int16 Working[2];

	JKPoint r;
	int16 cp[2]={p.x,p.y};
	int16 rad[2]={radius.x,radius.y};
	for(int16 i=0;i<2;i++) {
		r.x = i? 0:radius.y;
		r.y = i? radius.x:0;
		if(startx==endx&&starty==endy) PutPixel(p+r,color);
		else if(!flag){	if(starty*r.x-startx*r.y<=0 && endy*r.x-endx*r.y>=0) PutPixel(p+r,color);}
		else if(starty*r.x-startx*r.y<=0 || endy*r.x-endx*r.y>=0)	PutPixel(p+r,color);
		r.x = i? 0:-radius.y;
		r.y = i? -radius.x:0;
		if(startx==endx&&starty==endy) PutPixel(p+r,color);
		else if(!flag){	if(starty*r.x-startx*r.y<=0 && endy*r.x-endx*r.y>=0) PutPixel(p+r,color);}
		else if(starty*r.x-startx*r.y<=0 || endy*r.x-endx*r.y>=0)	PutPixel(p+r,color);

		Working[i]=0; Working[!i]=rad[!i];
		Adjust[i]=0; Adjust[!i]=Squared[i]*2*rad[!i];

		int32 Threshold=Squared[i]/4-Squared[i]*rad[!i];
		while(1){
			Threshold+=Adjust[i]+Squared[!i];
			if(Threshold>=0){
				Adjust[!i]-=Squared[i]*2;
				Threshold-=Adjust[!i];
				Working[!i]--;
			}
			Adjust[i]+=Squared[!i]*2;
			Working[i]++;
			if(Adjust[i]>=Adjust[!i]) break;
			for(BYTE j=0;j<4;j++){
				r.x = (j&0x01)? Working[0]:-Working[0];
				r.y = (j&0x02)? Working[1]:-Working[1];
				if(startx==endx&&starty==endy) PutPixel(p+r,color);
				else if(!flag){	if(starty*r.x-startx*r.y<=0 && endy*r.x-endx*r.y>=0) PutPixel(p+r,color);}
				else if(starty*r.x-startx*r.y<=0 || endy*r.x-endx*r.y>=0)	PutPixel(p+r,color);
			}
		}
	}
}
void JKDC::Pieslice(JKPoint p, double startangle, double endangle, JKPoint radius,int16 color)
{
	JKPoint StartPos(p.x+radius.x*sin(startangle),p.y-radius.y*cos(startangle));
	JKPoint EndPos(p.x+radius.x*sin(endangle),p.y-radius.y*cos(endangle));
	Line(p,StartPos,color);	Line(p,EndPos,color);
	Arc(p,startangle,endangle,radius,color);
}

void JKDC::DrawPolygon(JKPoint *polypoint,WORD numpoints,int16 color)
{
	if(polypoint&&numpoints>1){
		MoveTo(polypoint[0]);
		for(WORD i=1;i<numpoints;i++)
			LineTo(polypoint[i],color);
		LineTo(polypoint[0],color);
	}
}

void JKDC::FillPolygon(JKPoint */*polypoint*/,WORD /*numpoints*/,int16 /*color*/)
{
}

void JKDC::Line(int16 x1,int16 y1,int16 x2,int16 y2,int16 color)
{
	JKPoint p1(x1,y1),p2(x2,y2);
	p1+=TempOrgPos;		p2+=TempOrgPos;
	JKRect rect(min(p1.x,p2.x),min(p1.y,p2.y),max(p1.x,p2.x),max(p1.y,p2.y));

	JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
	rect=MaxRect.CalcOverlap(rect);
	if(!rect.IsValid()) return;
	JKRect temprect;
	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		temprect=container->Rect.CalcOverlap(rect);
		if(temprect.IsValid())
			_Line(temprect,p1.x,p1.y,p2.x,p2.y,color);
		container=container->next;
	}

	CurrentPos.x = x2;
	CurrentPos.y = y2;
}

void JKDC::Line(JKPoint p1,JKPoint p2,int16 color)
{
	Line(p1.x,p1.y,p2.x,p2.y,color);
}

void JKDC::LineTo(int16 x,int16 y,int16 color)
{
	Line(CurrentPos.x,CurrentPos.y,x,y,color);
}
void JKDC::LineTo(JKPoint p,int16 color)
{
	LineTo(p.x,p.y,color);
}

void JKDC::MoveTo(int16 x,int16 y)
{
	CurrentPos.x = x;
	CurrentPos.y = y;
	_MoveTo(x,y);
}

void JKDC::MoveTo(JKPoint p)
{
	MoveTo(p.x,p.y);
}

void JKDC::HLine(JKPoint p,int16 delta,int16 color)
{
	if(!delta) return;

	BOOL sign=TRUE;
	JKPoint realpos=p+TempOrgPos;
	WORD realdelta=delta;
	if(delta<0) {
		realdelta=abs(delta);
		realpos.x-=realdelta-1;
		sign=FALSE;
	}


	JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		JKPoint temppos=realpos;
		WORD tempdelta=realdelta;
		JKRect temp=MaxRect.CalcOverlap(container->Rect);
		if(temp.IsValid() && temp.CalcInnerHLine(temppos,tempdelta))
			_HLine(temppos,tempdelta,color);
		container=container->next;
	}

	CurrentPos.x = p.x+delta-(sign? 1:-1);
	CurrentPos.y = p.y;
}

void JKDC::VLine(JKPoint p,int16 delta,int16 color)
{
	if(!delta) return;

	BOOL sign=TRUE;
	JKPoint realpos=p+TempOrgPos;
	WORD realdelta=delta;
	if(delta<0) {
		realdelta=abs(delta);
		realpos.y-=realdelta-1;
		sign=FALSE;
	}

	JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		JKPoint temppos=realpos;
		WORD tempdelta=realdelta;
		JKRect temp=MaxRect.CalcOverlap(container->Rect);
		if(temp.IsValid() && container->Rect.CalcInnerVLine(temppos,tempdelta))
			_VLine(temppos,tempdelta,color);
		container=container->next;
	}

	CurrentPos.x = p.x;
	CurrentPos.y = p.y+delta-(sign? 1:-1);
}

void JKDC::HLineTo(int16 delta,int16 color)
{
	HLine(CurrentPos,delta,color);
}

void JKDC::VLineTo(int16 delta,int16 color)
{
	VLine(CurrentPos, delta,color);
}

void JKDC::Bezier(JKPoint ps[4],int16 color,BOOL iskbez,double delta)
{
	JKPoint TempPos;
	double table[4];

	MoveTo(ps[0]);
	for(double u=0;u<1.0;u+=delta){
		if(iskbez){
			table[0]=1./2.*(1.-3.*u)*(2.-3.*u)*(1.-u);
			table[1]=9./2.*(2.-3.*u)*(1.-u)*u;
			table[2]=-9./2.*(1.-u)*u*(1.-3.*u);
			table[3]=1./2.*u*(1.-3*u)*(2.-3.*u);
		}
		else{
			table[0]=(1-u)*(1-u)*(1-u);
			table[1]=3*u*(1-u)*(1-u);
			table[2]=3*u*u*(1-u);
			table[3]=u*u*u;
		}
		TempPos.x=int16(table[0]*ps[0].x+table[1]*ps[1].x+table[2]*ps[2].x+table[3]*ps[3].x+0.5);
		TempPos.y=int16(table[0]*ps[0].y+table[1]*ps[1].y+table[2]*ps[2].y+table[3]*ps[3].y+0.5);
		LineTo(TempPos,color);
	}
	LineTo(ps[3],color);
}

void JKDC::BezierTo(JKPoint ps[3],int16 color,BOOL mode,double delta)
{
	JKPoint temp[4];
	temp[0]=CurrentPos;
	temp[1]=ps[0]; temp[2]=ps[1]; temp[3]=ps[2];

	Bezier(temp,color,mode,delta);
}



void JKDC::SolidBar(JKRect rect,int16 color)
{
	rect.Offset(TempOrgPos);

	JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
	rect=MaxRect.CalcOverlap(rect);
	if(!rect.IsValid()) return;

	JKRect temprect;
	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		temprect=container->Rect.CalcOverlap(rect);
		if(temprect.IsValid())
			_SolidBar(temprect,color);
		container=container->next;
	}
}

void JKDC::Rectangle(JKRect rect,int16 color)
{
	MoveTo(rect.LeftTop());
	HLineTo(rect.Width()+1,color);	VLineTo(rect.Height()+1,color);
	HLineTo(-(rect.Width()+1),color); VLineTo(-(rect.Height()+1),color);
}

void JKDC::Rectangle3D(JKRect& rect,int16 depth,int16 color1,int16 color2)
{
	BOOL status=TRUE;
	if(depth<0) {
		status=FALSE;
		depth = -depth;
	}
	int16 clr;
	for(int16 i=0;i<depth;i++){
		MoveTo(rect.left+i,rect.bottom-i);
		clr = status? color1 : color2;
		VLineTo(-(rect.Height()+1-i*2),clr);	HLineTo(rect.Width()+1-i*2,clr);
		clr = status? color2 : color1;
		VLineTo(rect.Height()+1-i*2,clr);     HLineTo(-(rect.Width()+1-i*2),clr);
	}
}

void JKDC::Box3D(JKRect& rect,int16 depth,int16 color,int16 color1,int16 color2)
{
	SolidBar(rect,color);
	Rectangle3D(rect,depth,color1,color2);
}

BOOL JKDC::GetImage(JKRect rect, BYTE *buffer)
{
	if(!buffer) return FALSE;
	rect.Offset(TempOrgPos);

	*((WORD *)buffer)=rect.Width();
	*((WORD *)(buffer+2))=rect.Height();

	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		JKRect temprect=container->Rect.CalcOverlap(rect);
		if(temprect.IsValid())
			_GetImage(rect.LeftTop(),buffer,temprect);
		container=container->next;
	}
	return TRUE;
}


BOOL JKDC::PutImage(JKPoint pos, BYTE *buffer,BYTE op,JKRect cliprect)
{
	if(!buffer) return FALSE;
	pos+=TempOrgPos;
	BOOL isclip= cliprect.IsValid()? TRUE:FALSE;
	if(isclip) cliprect.Offset(TempOrgPos);

	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		if(isclip){
			JKRect temprect=container->Rect.CalcOverlap(cliprect);
			if(temprect.IsValid())
				_PutImage(pos,buffer,temprect,op);
		}
		else _PutImage(pos,buffer,container->Rect,op);
		container=container->next;
	}
	return TRUE;
}

BOOL JKDC::SkipPCXLine(FILE *fp,WORD bytes,WORD lines)
{
	if(!fp) return FALSE;

	WORD n;
	BYTE c;
	while(lines--){
		n=0;
		while(n<bytes){
			BYTE c=(fgetc(fp) & 0xff);
			if((c&0xc0)==0xc0) {
				n+=c&0x3f;
				fseek(fp,1L,SEEK_CUR);
			}
			else n++;
		}
	}
	return TRUE;
}

WORD JKDC::ReadPCXLine(FILE *fp,int8 *buf,int16 bytes,BOOL palchange)
{
	if(!fp || !buf) return 0;
	WORD n=0;
	BYTE c,i;
	while(n<bytes){
		c=(fgetc(fp)&0xff);
		if((c&0xc0)==0xc0) {
			i=c&0x3f;
			c=fgetc(fp);
			if(!palchange) c=ColorConvTable256[c];
			while(i--) buf[n++]=c;
		}
		else{
			if(!palchange) buf[n++]=ColorConvTable256[c];
			else buf[n++]=c;
		}
	}
	return n;
}

BOOL JKDC::PutPCX(int8 *fname,JKPoint p,JKRect rect,BYTE putmode,BOOL palchange)
{
	p+=TempOrgPos;
	rect.Offset(TempOrgPos);

	if(!fname) return FALSE;
	FILE *fp=fopen(fname,"rb");
	if (!fp) return FALSE;

	fseek(fp,-769L,SEEK_END);
	BOOL is256 = (fgetc(fp)==12)? TRUE:FALSE;

	PCXHDR  header;
	fseek(fp,0,SEEK_SET);
	fread(&header,128,1,fp);

	WORD mode=NO_PCX;

	if(header.Manufacturer!=0x0a) mode= NO_PCX;
	else if(header.HRes==720 && header.VRes==348) mode=PCX_HERCULES;
	else if(header.NPlanes==4) mode=PCX_VGA16;
	else if(is256) mode=PCX_VGA256;

	if(mode!=PCX_VGA256) {
		fclose(fp);
		return FALSE;
	}

	WORD width=header.x2-header.x1+1;
	WORD height=header.y2-header.y1+1;

////////////////////////////////////////////////////////////////

	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);
	JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
	rect=MaxRect.CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return TRUE;
	}

	JKRect ImageRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=ImageRect.CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}

	rect=JKRect(0,0,GetMaxX(),GetMaxY()).CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}


	BYTE tempRGB[256][3];
	fseek(fp,-768L,SEEK_END);
	fread(tempRGB,256*3,1,fp);

	for (WORD i=0;i<256;i++) {
		if(!palchange)
			ColorConvTable256[i]= FindNearestColor(tempRGB[i][0]>>2,tempRGB[i][1]>>2,tempRGB[i][2]>>2);
		else
			SetPalette(i,tempRGB[i][0]>>2,tempRGB[i][1]>>2,tempRGB[i][2]>>2);
	}

	WORD bytes=header.BytesperLine;
	fseek(fp,128,SEEK_SET);
	SkipPCXLine(fp,bytes,rect.top-p.y);

	RectContainer *container;
	JKRect temprect;

	BYTE *LineBuffer = new int8[bytes];
	for(i=rect.top;i<=rect.bottom;i++) {
		WORD n=ReadPCXLine(fp,LineBuffer,bytes,palchange);
		if(n>width) n=width;

		container = ClipRegion.HeadNode;
		while(container) {
			temprect=container->Rect.CalcOverlap(rect);
			if(temprect.IsValid() && temprect.top<=i && i<=temprect.bottom)
				_WriteImageLine(JKPoint(temprect.left,i),LineBuffer+(temprect.left-p.x),temprect.Width()+1,putmode);
			container=container->next;
		}
	}
	delete[] LineBuffer;

	fclose(fp);
	return TRUE;
}

BOOL JKDC::PutIMG(int8 *fname,JKPoint p,JKRect rect,BYTE putmode)
{
	p+=TempOrgPos;
	rect.Offset(TempOrgPos);

	if(!fname) return FALSE;
	FILE *fp=fopen(fname,"rb");
	if (!fp) return FALSE;

	WORD width;
	fread(&width,sizeof(width),1,fp);	width++;
		WORD height;
	fread(&height,sizeof(height),1,fp);	height++;


//////////////////////////////////////////////////////////////////////

	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);
	JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
	rect=MaxRect.CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return TRUE;
	}

	JKRect ImageRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=ImageRect.CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}
	rect=JKRect(0,0,GetMaxX(),GetMaxY()).CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}

	fseek(fp,(int32)width*(rect.top-p.y),SEEK_CUR);
	RectContainer *container;
	JKRect temprect;
	BYTE *LineBuffer = new BYTE[width];
	if(LineBuffer){
		for(uint16 i=rect.top;i<=rect.bottom;i++) {
			fread(LineBuffer,width,1,fp);
			container = ClipRegion.HeadNode;
			while(container) {
				temprect=container->Rect.CalcOverlap(rect);
				if(temprect.IsValid() && temprect.top<=i && i<=temprect.bottom)
					_WriteImageLine(JKPoint(temprect.left,i),LineBuffer+(temprect.left-p.x),temprect.Width()+1,putmode);
				container=container->next;
			}
		}
		delete[] LineBuffer;
	}

////////////////////////////////////////////////////////////////////////
/*

	uint32 imgsize=width*height+2*2;
	BYTE *ImageBuffer = new BYTE[imgsize];
	if(!ImageBuffer){	fclose(fp);		return FALSE;	}

	fseek(fp,0,SEEK_SET);
	fread(ImageBuffer,imgsize,1,fp);

	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		JKRect temp=container->Rect.CalcOverlap(rect);
		if(temp.IsValid())
			_PutImage(p,ImageBuffer,temp,putmode);
		container=container->next;
	}
	delete ImageBuffer;
*/
	fclose(fp);
	return TRUE;
}

void JKDC::ScrollUp(JKRect rect,uint16 lines)
{
	int16 count=rect.Height()+1-lines;
	if(count>0){
		int16  width=rect.Width()+1;
		if(width<=0) return;

		BYTE *buff= new BYTE[width*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.top+=lines;
		if(GetImage(temp,buff)){
			temp=rect;
			temp.bottom-=lines;
			PutImage(temp.LeftTop(),buff,0);
			rect.top=rect.bottom-lines+1;
		}
		delete[] buff;
	}
	SolidBar(rect,BackColor);
}

void JKDC::ScrollDown(JKRect rect,WORD lines)
{
	int16 count=rect.Height()+1-lines;
	if(count>0){
		int16  width=rect.Width()+1;
		if(width<=0) return;

		BYTE *buff= new BYTE[width*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.bottom-=lines;
		if(GetImage(temp,buff)){
			temp=rect;
			temp.top+=lines;
			PutImage(temp.LeftTop(),buff,0);
			rect.bottom=rect.top+lines-1;
		}
		delete[] buff;
	}
	SolidBar(rect,BackColor);
}

void JKDC::ScrollLeft(JKRect rect,uint16 lines)
{
	int16 count=rect.Width()+1-lines;
	if(count>0){
		int16  height=rect.Height()+1;
		if(height<=0) return;

		BYTE *buff= new BYTE[height*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.left+=lines;
		if(GetImage(temp,buff)){
			temp=rect;
			temp.right-=lines;
			PutImage(temp.LeftTop(),buff,0);
			rect.left=rect.right-lines+1;
		}
		delete[] buff;
	}
	SolidBar(rect,BackColor);
}

void JKDC::ScrollRight(JKRect rect,uint16 lines)
{
	int16 count=rect.Width()+1-lines;
	if(count>0){
		int16  height=rect.Height()+1;
		if(height<=0) return;

		BYTE *buff= new BYTE[height*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.right-=lines;
		if(GetImage(temp,buff)){
			temp=rect;
			temp.left+=lines;
			PutImage(temp.LeftTop(),buff,0);
			rect.right=rect.left+lines-1;
		}
		delete[] buff;
	}
	SolidBar(rect,BackColor);
}

//-----------------------------------------------------------//

void JKDC::EngPutCh(JKPoint p,BYTE ch)
{
	p+=TempOrgPos;
	JKRect charrect(p.x,p.y,p.x+8,p.y+15);

	JKRect temprect;
	JKPoint temppos;
	WORD stline=0,ycount=16;
	BYTE lshift=0,rshift=0;
	BYTE TextImage[16];

	JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
	RectContainer *container = ClipRegion.HeadNode;
	while(container) {
		temprect=MaxRect.CalcOverlap(container->Rect);
		if(temprect.IsValid()){
			temprect=temprect.CalcOverlap(charrect);
			if(temprect.IsValid()){
				stline=WORD(temprect.top-charrect.top);
				ycount=WORD(temprect.bottom-temprect.top+1);
				lshift=BYTE(temprect.left-charrect.left);
				rshift=BYTE(charrect.right-temprect.right);
				temppos.Set(p.x,p.y+stline);
				if(IsOverWrite) _PutEngImage(temppos,BackFont+stline,BackColor,ycount,lshift,rshift);
				if(HanMan->GetEnglishImage(TextImage,ch))
					_PutEngImage(temppos,TextImage+stline,TextColor,ycount,lshift,rshift);
				if(ycount==16&&!lshift&&!rshift) break;
			}
		}
		container=container->next;
	}
}

void JKDC::HanPutCh(JKPoint p,BYTE firstbyte,BYTE secondbyte)
{
	p+=TempOrgPos;

	BYTE TextImage[32];
	if(HanMan->GetWORDImage(TextImage,firstbyte,secondbyte)){
		JKRect charrect(p.x,p.y,p.x+15,p.y+15);

		JKRect temprect;
		JKPoint temppos;
		WORD stline,ycount;
		BYTE lshift,rshift;

		JKRect MaxRect(0,0,GetMaxX(),GetMaxY());
		RectContainer *container = ClipRegion.HeadNode;
		while(container) {
			temprect=MaxRect.CalcOverlap(container->Rect);
			if(temprect.IsValid()){
				temprect=temprect.CalcOverlap(charrect);
				if(temprect.IsValid()){
					stline=WORD(temprect.top-charrect.top);
					ycount=WORD(temprect.bottom-temprect.top+1);
					lshift=BYTE(temprect.left-charrect.left);
					rshift=BYTE(charrect.right-temprect.right);
					temppos.Set(p.x,p.y+stline);
					if(IsOverWrite) _PutHanImage(temppos,BackFont+stline*2,BackColor,ycount,lshift,rshift);
					_PutHanImage(temppos,TextImage+stline*2,TextColor,ycount,lshift,rshift);
					if(ycount==16&&!lshift&&!rshift) break;
				}
			}
			container=container->next;
		}
	}
}

void JKDC::PutCh(JKPoint p,WORD code)
{
	BYTE firstbyte=HIBYTE(code);
	BYTE secondbyte=LOBYTE(code);
	if(firstbyte)	HanPutCh(p,firstbyte,secondbyte);
	else EngPutCh(p,secondbyte);
}

void JKDC::TextOut(JKPoint p,int8 *str)
{
	TextOut(p,strlen(str),str);
}

void JKDC::TextOut(JKPoint p,WORD n,int8 *str)
{
	if(!str||!str[0]) return;
	if(n>strlen(str)) n=strlen(str);
	WORD i=0;
	while(i<n && *str){
		if(i+1<n && *str&0x80) {
			HanPutCh(p,*str,*(str+1));
			str+=2; p.x+=16; i+=2;
		}
		else {
			EngPutCh(p,*str);
			str++; p.x+=8; i++;
		}
	}
}

void JKDC::TextOutInRect(JKRect rect,JKPoint p,int8 *str)
{
	WORD n=strlen(str);
	TextOutInRect(rect,p,n,str);
}

void JKDC::TextOutInRect(JKRect rect,JKPoint p,WORD n,int8 *str)
{
	if(!n) return;
	JKPoint Extent(8,16);
	if(rect.top>p.y || rect.bottom< p.y+Extent.y-1) return;
	int16 count1= (p.x>=rect.left)? (rect.left-p.x)/Extent.x : (rect.left-p.x)/Extent.x+1;
	int16 count2= (p.x<=rect.right)? (rect.right-p.x-7)/Extent.x : (rect.right-p.x)/Extent.x-1;
	if(count2>=n) count2=n-1;
	if(count1<0) count1=0;
	if(HanMan->IsHangul2nd(str,count1)) count1++;
	if(HanMan->IsHangul1st(str,count2)) count2--;
	if(count1>count2) return;
	if(count1<n) {
		TextOut(JKPoint(p.x+count1*Extent.x,p.y),count2-count1+1,str+count1);
	}
}

void JKDC::TextOutX(JKRect rect,int8 *str,BYTE adjflag,BOOL IsWrapping)
{
	if(str&&str[0]) {
		unsigned CharPerLine[100];
		WORD LineCount=0;
		int8 *tempstr=str;
		WORD i=0;
		while(*tempstr){
			if(*tempstr=='\n'){
				CharPerLine[LineCount++]=i;
				i=0;
			}
			else i++;
			tempstr++;
		}
		CharPerLine[LineCount++]=i;

		JKPoint p,adj;
		adj=JKPoint(rect.top,rect.bottom);
		p.y=adj.Adjust((adjflag>>4)&0x0f,LineCount*16);

		adj=JKPoint(rect.left,rect.right);
		tempstr=str;
		for(i=0;i<LineCount;i++){
			if(CharPerLine[i]){
				p.x = adj.Adjust(adjflag&0x0f,8*CharPerLine[i]);
				if(IsWrapping)
					TextOutInRect(rect,JKPoint(p.x,p.y+i*16),CharPerLine[i],tempstr);
				else
					TextOut(JKPoint(p.x,p.y+i*16),CharPerLine[i],tempstr);
				tempstr+=CharPerLine[i]+1;
			}
			else tempstr+=1;
		}
	}
}

void JKDC::_GetImage(JKPoint p,BYTE *buffer,JKRect rect)
{
	if(!buffer) return;

	uint16 width=*((uint16 *)buffer)+1;
	uint16 height=*((uint16 *)(buffer+2))+1;
	if(!width || !height) return;

	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=JKRect(0,0,GetMaxX(),GetMaxY()).CalcOverlap(rect);
	if(!rect.IsValid()) return;

	JKRect bufferRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=bufferRect.CalcOverlap(rect);
	if(!rect.IsValid()) return;

	buffer+=4+width*(rect.top-p.y)+(rect.left-p.x);
	for(uint16 i=rect.top;i<=rect.bottom;i++) {
		p.Set(rect.left,i);
		_ScanImageLine(p,buffer,rect.Width()+1);
		buffer+=width;
	}
}

void JKDC::_PutImage(JKPoint p, BYTE *buffer,JKRect rect,BYTE putmode)
{
	if(!buffer) return;
	uint16 width=*((uint16 *)buffer)+1;
	uint16 height=*((uint16 *)(buffer+2))+1;
	if(!width || !height) return;

	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);
  rect=JKRect(0,0,GetMaxX(),GetMaxY()).CalcOverlap(rect);
	if(!rect.IsValid()) return;

	JKRect bufferRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=bufferRect.CalcOverlap(rect);
	if(!rect.IsValid()) return;

	buffer+=4+width*(rect.top-p.y)+(rect.left-p.x);
	for(uint16 i=rect.top;i<=rect.bottom;i++) {
		p.Set(rect.left,i);
		_WriteImageLine(p,buffer,rect.Width()+1,putmode);
		buffer+=width;
	}
}

