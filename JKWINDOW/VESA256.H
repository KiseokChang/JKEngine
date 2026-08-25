#ifndef _VESA256_H
#define _VESA256_H

#include "jkwindef.h"

#define VESA_640X480X256		0x101
#define VESA_800X600X256		0x103
#define VESA_1024X768X256		0x105

typedef struct {
	CHAR VESAsig[4];
	INT  VESAver;
	CHAR far *OEMStr;
	CHAR Capabilities[4];
	INT far *VModePtr;
	INT  TotalMemory;
	CHAR Reserved[236];
} SVGAInfo;

typedef struct{
	// mandatory infomation
	INT  ModeAttr;
	CHAR WinAAttr;
	CHAR WinBAttr;
	INT  WinGranularity;
	INT  WinSize;
	INT  WinASegment;
	INT  WinBSegment;
	void (*WinFuncPtr)();
	INT ByteperScanLine;

	//extended infomation
	INT  XResolution;
	INT  YResolution;
	CHAR XCharSize;
	CHAR YCharSize;
	CHAR NumberOfPalnes;
	CHAR BitsPerPixel;
	CHAR NumberOfBanks;
	CHAR MemoryModel;
	CHAR BankSize;
	CHAR NumberOfImagePages;
	CHAR Reserved1;

	//color infomation
	CHAR RedMaskSize;
	CHAR RedFieldPosition;
	CHAR GreenMaskSize;
	CHAR GreenFieldPosition;
	CHAR BlueMaskSize;
	CHAR BlueFieldPosition;
	CHAR RsvMaskSize;
	CHAR RsvFieldPosition;
	CHAR DirecColorModeInfo;
	CHAR Reserved2[216];

}ModeInfo;



void setpage();
BOOL IsVesa();
void SetMode(INT mode);
void EndMode();

void PutPixel(INT x,INT y,INT color);
INT GetPixel(INT x,INT y);
#endif

