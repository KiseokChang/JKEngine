#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <dir.h>
#include "jkprndc.h"

JKPrintDC::JKPrintDC():
	JKDC()
{
	MaxRect=JKRect(0,0,PAPERXSIZE_A4-1,PAPERYSIZE_A4-1);
	PaperXByteSize=(PAPERXSIZE_A4+7)/8;
	SwapStream=0;
	SaveAttribute();
	OpenDC();
	InitDC(MaxRect.LeftTop(),MaxRect);
}

JKPrintDC::~JKPrintDC()
{
	CloseDC();
//	RestoreAttribute();
}

void JKPrintDC::SaveAttribute()
{
	strcpy(SwapFileName,"prnswap0.dat");
	Make_SwapFile();
}

void JKPrintDC::RestoreAttribute()
{
	Delete_SwapFile();
}

void JKPrintDC::Make_SwapFile()
{
	if(SwapStream) CloseDC();
	FILE *fp=fopen(SwapFileName,"wb");
	if(fp){
		BYTE *TempBuffer= new BYTE[PaperXByteSize];
		if(TempBuffer){
			memset(TempBuffer,0,PaperXByteSize);
			for(WORD i=0;i<PAPERYSIZE_A4;i++)
				fwrite(TempBuffer,PaperXByteSize,1,fp);
			delete[] TempBuffer;
		}
		fclose(fp);
	}
}

void JKPrintDC::Delete_SwapFile()
{
	if(SwapStream) CloseDC();
	struct ffblk ffblk;
	if(!findfirst(SwapFileName,&ffblk,0))
		remove(SwapFileName);
}

void JKPrintDC::OpenDC()
{
	if(SwapStream) CloseDC();
	SwapStream=fopen(SwapFileName,"rb+");
}

void JKPrintDC::CloseDC()
{
	if(SwapStream) {
		fclose(SwapStream);
		SwapStream=0;
	}
}

void JKPrintDC::InitAttribute()
{}

void JKPrintDC::_SetTextColor(INT color)
{
	TextColor=color;
}

void JKPrintDC::_SetBackColor(INT color)
{
	BackColor=color;
}

void JKPrintDC::_PutPixel(JKPoint p,INT color)
{
	if(SwapStream) {
		fseek(SwapStream,p.y*PaperXByteSize+p.x/8,SEEK_SET);
		BYTE data=getc(SwapStream);
		WORD mod = p.x%8;
		if(color>0x07)	data |= 0x01<<mod;
		else          data &=~(0x01<<mod);
		fseek(SwapStream,-1,SEEK_CUR);
		putc(data,SwapStream);
	}
}

void JKPrintDC::_Line(INT ,INT ,INT ,INT ,INT )
{
	if(SwapStream) {
	}
}

void JKPrintDC::_MoveTo(INT ,INT )
{}

void JKPrintDC::_HLine(JKPoint p,WORD delta,INT color)
{
	if(SwapStream) {
		WORD pos=p.y*PaperXByteSize+p.x/8;
		fseek(SwapStream,pos++,SEEK_SET);
		BYTE mask=0xff;
		BYTE data=getc(SwapStream);
		WORD count=delta;
		WORD mod = p.x%8;
		if(count<8-mod) {
			mask>>=(8-mod)-count;		count=0;
		}
		else count-=(8-mod);
		if(color>0x07)	data |= (0xff<<mod)&mask;
		else          data &=~((0xff<<mod)&mask);
		fseek(SwapStream,-1,SEEK_CUR);
		putc(data,SwapStream);
		while(count>=8){
			fseek(SwapStream,pos++,SEEK_SET);
			data=getc(SwapStream);
			if(color>0x07)	data =0xff;
			else          data =0x00;
			fseek(SwapStream,-1,SEEK_CUR);
			putc(data,SwapStream);
			count-=8;
		}
		if(count){
			fseek(SwapStream,pos,SEEK_SET);
			data=getc(SwapStream);
			if(color>0x07)	data |= 0xff>>(8-count);
			else          data &=~(0xff<<(8-count));
			fseek(SwapStream,-1,SEEK_CUR);
			putc(data,SwapStream);
		}
	}
}

void JKPrintDC::_VLine(JKPoint p,WORD delta,INT color)
{
	if(SwapStream) {
		if(delta){
			fseek(SwapStream,p.y*PaperXByteSize+p.x/8,SEEK_SET);
			for(WORD i=0;i<delta;i++){
				BYTE data=getc(SwapStream);
				WORD mod = p.x%8;
				if(color>0x7)	data |= 0x01<<mod;
				else          data &=~(0x01<<mod);
				fseek(SwapStream,-1,SEEK_CUR);
				putc(data,SwapStream);
				if(i!=delta-1) fseek(SwapStream,PaperXByteSize-1,SEEK_CUR);
			}
		}
	}
}

void JKPrintDC::_SolidBar(JKRect rect,INT color)
{
	if(SwapStream) {
		for(INT y=rect.top;y<=rect.bottom;y++)
			_HLine(JKPoint(rect.left,y),rect.Width()+1,color);
	}
}

void JKPrintDC::_GetImage(JKRect , BYTE *)
{
	if(SwapStream) {
	}
}

void JKPrintDC::_PutImage(JKPoint , BYTE *,INT )
{
	if(SwapStream) {
	}
}

//-----------------------------------------------------------//

void JKPrINTDC::_EngPutCh(JKPoint ,BYTE )
{
	if(SwapStream) {
	}
}

void JKPrintDC::_HanPutCh(JKPoint ,BYTE ,BYTE )
{
	if(SwapStream) {
	}
}

void JKPrintDC::_PutString(JKPoint ,WORD ,CHAR *)
{
	if(SwapStream) {
	}
}
