#include "kgraphic.h"

BOOL IsInitGraph=FALSE;

BOOL InitGraph()
{
	if(!IsInitGraph){
		SetVesaMode();
		IsInitGraph=TRUE;
		return TRUE;
	}
  return FALSE;
}

void CloseGraph()
{
	if(IsInitGraph){
		EndVesaMode();
		IsInitGraph=FALSE;
	}
}

int16 getmaxx()
{
	return 1023;
}
int16 getmaxy()
{
	return 767;
}

BOOL Put256IMG(JKPoint p,BYTE *Image,JKRect rect,BYTE putmode)
{
	if(!Image) return FALSE;
	uint16 width=*((uint16 *)Image)+1;
	uint16 height=*((uint16 *)(Image+2))+1;
	if(!width || !height) return FALSE;

	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);

	JKRect ImageRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=ImageRect.CalcOverlap(rect);
	if(!rect.IsValid()) return FALSE;
	rect=JKRect(0,0,getmaxx(),getmaxy()).CalcOverlap(rect);
	if(!rect.IsValid()) return FALSE;

	Image+=4+width*(rect.top-p.y)+(rect.left-p.x);
	for(uint16 i=rect.top;i<=rect.bottom;i++) {
		p.Set(rect.left,i);
		writeimageline(p.x,p.y,Image,rect.Width()+1,putmode);
		Image+=width;
	}
	return TRUE;
}

BOOL Get256IMG(JKPoint p,BYTE *Image,JKRect rect)
{
	if(!Image) return FALSE;

	uint16 width=*((uint16 *)Image)+1;
	uint16 height=*((uint16 *)(Image+2))+1;
	if(!width || !height) return FALSE;

	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);

	JKRect ImageRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=ImageRect.CalcOverlap(rect);
	if(!rect.IsValid()) return FALSE;
	rect=JKRect(0,0,getmaxx(),getmaxy()).CalcOverlap(rect);
	if(!rect.IsValid()) return FALSE;

	Image+=4+width*(rect.top-p.y)+(rect.left-p.x);
	for(uint16 i=rect.top;i<=rect.bottom;i++) {
		p.Set(rect.left,i);
		scanimageline(p.x,p.y,Image,rect.Width()+1);
		Image+=width;
	}
	return TRUE;
}

BOOL Get256Image(JKRect rect,BYTE *Image)
{
	if(!Image) return FALSE;

	*((uint16 *)Image)=rect.Width();
	*((uint16 *)(Image+2))=rect.Height();

	return Get256IMG(rect.LeftTop(),Image,rect);
}

//////////////////////////////////////////////////////////////////////////////
void SetAllPalette(BYTE palette[768])
{
	for(uint16 i=0;i<256;i++)
		SetPalette(i,palette[i*3],palette[i*3+1],palette[i*3+2]);
}

void GetAllPalette(BYTE palette[768])
{
	for(uint16 i=0;i<256;i++)
		GetPalette(i,palette[i*3],palette[i*3+1],palette[i*3+2]);
}

void SetPalette(uint16 color,BYTE r,BYTE g,BYTE b)
{
	OutPortb(0x3c8,color);
	OutPortb(0x3c9,r);	OutPortb(0x3c9,g);	OutPortb(0x3c9,b);
}

void GetPalette(uint16 color,BYTE& r,BYTE& g,BYTE& b)
{
	OutPortb(0x3c7,color);
	r=InPortb(0x3c9);	g=InPortb(0x3c9);	b=InPortb(0x3c9);
}


BYTE IndexFromEachRGB(BYTE rgb)
{
	uint16 index=0;

	if(rgb<32){
		if(rgb<10) index=0;
		else if(rgb<21) index=1;
		else index=2;
	}
	else {
		if(rgb<43) index=3;
		else if(rgb<54) index=4;
		else index=5;
	}
	return index;
}


BYTE FindNearestColor(BYTE r,BYTE g,BYTE b)
{
	BYTE RGBIndex[3];
	r&=0x3f;	g&=0x3f;	b&=0x3f;
	RGBIndex[0]=IndexFromEachRGB(r);
	RGBIndex[1]=IndexFromEachRGB(g);
	RGBIndex[2]=IndexFromEachRGB(b);

	uint16 color=0;
	if(RGBIndex[0]==RGBIndex[1] && RGBIndex[0]==RGBIndex[2]){
		color=14+210;
		uint16 value=(uint16(r)*30+uint16(g)*59+uint16(b)*11)/100;
		color+=value/2;
	}
	else{
		color=14;
		color+=RGBIndex[0]*6*6+RGBIndex[1]*6+RGBIndex[2];
		color-=RGBIndex[0];
		if(RGBIndex[0]<RGBIndex[1] ||(RGBIndex[0]==RGBIndex[1] && RGBIndex[1]<RGBIndex[2])) color--;
	}
	return color;
}


