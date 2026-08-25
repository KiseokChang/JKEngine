#include <stdio.h>
#include <conio.h>
#include "jkpoint.h"
#include "graph_pm.h"

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
		WriteImageLine(p.x,p.y,Image,rect.Width()+1,putmode);
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
		ScanImageLine(p.x,p.y,Image,rect.Width()+1);
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

BOOL PutIMG(int8 *fname,JKPoint p,JKRect rect,BYTE putmode)
{
	if(!fname) return FALSE;
	FILE *fp=fopen(fname,"rb");
	if (!fp) return FALSE;

	WORD tmp;
	fread(&tmp,sizeof(tmp),1,fp);
	WORD width=tmp+1;
	fread(&tmp,sizeof(tmp),1,fp);
	WORD height=tmp+1;

	if(!rect.IsValid()) rect.Set(p.x,p.y,p.x+width-1,p.y+height-1);

	JKRect ImageRect(p.x,p.y,p.x+width-1,p.y+height-1);
	rect=ImageRect.CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}

	rect=JKRect(0,0,getmaxx(),getmaxy()).CalcOverlap(rect);
	if(!rect.IsValid()) {
		fclose(fp);
		return FALSE;
	}


	fseek(fp,(int32)width*(rect.top-p.y),SEEK_CUR);

	BYTE *LineBuffer = new int8[width];
	if(LineBuffer){
		for(WORD i=rect.top;i<=rect.bottom;i++) {
			fread(LineBuffer,width,1,fp);
			WriteImageLine(rect.left,i,LineBuffer+(rect.left-p.x),rect.Width()+1,putmode);
		}
		delete[] LineBuffer;
	}

	fclose(fp);
	return TRUE;
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

uint8 EngImage[16]={
	0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f,0xff,
	0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f,0xff
};
uint8 HanImage[32]={
	0x00,0x01,0x00,0x03,0x00,0x07,0x00,0x0f,
	0x00,0x1f,0x00,0x3f,0x00,0x7f,0x00,0xff,
	0x01,0xff,0x03,0xff,0x07,0xff,0x0f,0xff,
	0x1f,0xff,0x3f,0xff,0x7f,0xff,0xff,0xff
};

void ScrollUp(JKRect rect,uint16 lines,BYTE bkcolor)
{
	int16 count=rect.Height()+1-lines;
	if(count>0){
		int16  width=rect.Width()+1;
		if(width<=0) return;

		BYTE *buff= new BYTE[width*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.top+=lines;
		if(Get256Image(temp,buff)){
			temp=rect;
			temp.bottom-=lines;
			Put256IMG(temp.LeftTop(),buff,temp,0);
			rect.top=rect.bottom-lines+1;
		}
		delete[] buff;
	}
	BYTE temp=_writemode;
	_writemode=0;
	bar(rect.left,rect.top,rect.right,rect.bottom,bkcolor);
	_writemode=temp;
}

void ScrollDown(JKRect rect,uint16 lines,BYTE bkcolor)
{
	int16 count=rect.Height()+1-lines;
	if(count>0){
		int16  width=rect.Width()+1;
		if(width<=0) return;

		BYTE *buff= new BYTE[width*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.bottom-=lines;
		if(Get256Image(temp,buff)){
			temp=rect;
			temp.top+=lines;
			Put256IMG(temp.LeftTop(),buff,temp,0);
			rect.bottom=rect.top+lines-1;
		}
		delete[] buff;
	}
	BYTE temp=_writemode;
	_writemode=0;
	bar(rect.left,rect.top,rect.right,rect.bottom,bkcolor);
	_writemode=temp;
}

void ScrollLeft(JKRect rect,uint16 lines,BYTE bkcolor)
{
	int16 count=rect.Width()+1-lines;
	if(count>0){
		int16  height=rect.Height()+1;
		if(height<=0) return;

		BYTE *buff= new BYTE[height*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.left+=lines;
		if(Get256Image(temp,buff)){
			temp=rect;
			temp.right-=lines;
			Put256IMG(temp.LeftTop(),buff,temp,0);
			rect.left=rect.right-lines+1;
		}
		delete[] buff;
	}
	BYTE temp=_writemode;
	_writemode=0;
	bar(rect.left,rect.top,rect.right,rect.bottom,bkcolor);
	_writemode=temp;
}

void ScrollRight(JKRect rect,uint16 lines,BYTE bkcolor)
{
	int16 count=rect.Width()+1-lines;
	if(count>0){
		int16  height=rect.Height()+1;
		if(height<=0) return;

		BYTE *buff= new BYTE[height*count+4];
		if(!buff) return;
		JKRect temp=rect;
		temp.right-=lines;
		if(Get256Image(temp,buff)){
			temp=rect;
			temp.left+=lines;
			Put256IMG(temp.LeftTop(),buff,temp,0);
			rect.right=rect.left+lines-1;
		}
		delete[] buff;
	}
	BYTE temp=_writemode;
	_writemode=0;
	bar(rect.left,rect.top,rect.right,rect.bottom,bkcolor);
	_writemode=temp;
}

void main()
{
	AllocVideoSelector();
	SetVesaMode();
	setwritemode(1);
	bar(100,100,800,500,3);
	getch();
	bar(0,0,getmaxx(),getmaxy(),3);
	getch();
	ScrollDown(JKRect(0,0,getmaxx(),getmaxy()),100,0);
	getch();
	ScrollUp(JKRect(0,0,getmaxx(),getmaxy()),100,0);
	getch();
	ScrollLeft(JKRect(0,0,getmaxx(),getmaxy()),100,0);
	getch();
  ScrollRight(JKRect(0,0,getmaxx(),getmaxy()),100,0);
	getch();
	EndVesaMode();
	FreeVideoSelector();
}

