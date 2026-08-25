#ifndef _GRAPH_PM_H
#define _GRAPH_PM_H

#include "jkwindef.h"

#define VESA_1024X768X256		0x105

typedef struct {
	int8 VESAsig[4];
	int16  VESAver;
	BYTE *OEMStr;
	int8 Capabilities[4];
	BYTE *VModePtr;
	int16  TotalMemory;
	int8 Reserved[236];
} SVGAInfo;

typedef struct{
	// mandatory infomation
	int16  ModeAttr;
	int8 WinAAttr;
	int8 WinBAttr;
	int16  WinGranularity;
	int16  WinSize;
	int16  WinASegment;
	int16  WinBSegment;
	void (*WinFuncPtr)();
	int16 ByteperScanLine;

	//extended infomation
	int16  XResolution;
	int16  YResolution;
	int8 XCharSize;
	int8 YCharSize;
	int8 NumberOfPalnes;
	int8 BitsPerPixel;
	int8 NumberOfBanks;
	int8 MemoryModel;
	int8 BankSize;
	int8 NumberOfImagePages;
	int8 Reserved1;

	//color infomation
	int8 RedMaskSize;
	int8 RedFieldPosition;
	int8 GreenMaskSize;
	int8 GreenFieldPosition;
	int8 BlueMaskSize;
	int8 BlueFieldPosition;
	int8 RsvMaskSize;
	int8 RsvFieldPosition;
	int8 DirecColorModeInfo;
	int8 Reserved2[216];

}ModeInfo;

void SetVesaMode();
void EndVesaMode();
void setpage();

////////////////////////////////////////////////////////////

int16  _getmaxx();
int16  _getmaxy();

extern BYTE  _writemode;
void  setwritemode(BYTE mode);
BYTE  getwritemode();

void  stosb_putmode();
void  REPstosb_putmode();
void  putpixel(int16 x,int16 y,BYTE color);
void  hline(int16 x,int16 y,uint16 delta,BYTE color);
void  vline(int16 x,int16 y,uint16 delta,BYTE color);
void  bar(int16 left, int16 top, int16 right, int16 bottom,BYTE color);
/*
void  getline256(int16 xwidth,BYTE *buff);
void  putline256(int16 xwidth,BYTE *buff);
void  scrollup(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor);
void  scrolldown(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor);
void  scrollleft(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor);
void  scrollright(int16 x1, int16 y1, int16 x2,int16 y2,uint16 lines,BYTE bkcolor);
*/
///////////////////////////////////////////////////////////////////////////////////
void  writeimageline(int16 x, int16 y,int8 *buff,int16 n,BYTE putmode);
void  scanimageline(int16 x, int16 y,int8 *buff,int16 n);
BOOL  putengimage(int16 x,int16 y,int8 *buff,BYTE color,uint16 ycount,BYTE lshift,BYTE rshift);
BOOL  puthanimage(int16 x,int16 y,int8 *buff,BYTE color,uint16 ycount,BYTE lshift,BYTE rshift);
///////////////////////////////////////////////////////////////////////////////////

void  OutPort(int16 portid,int16 value);
void  OutPortb(int16 portid,uint8 value);
int16 InPort(int16 portid);
BYTE  InPortb(int16 portid);
#endif
