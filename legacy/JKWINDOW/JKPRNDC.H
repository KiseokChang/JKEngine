#ifndef _JKPRNDC_H
#define _JKPRNDC_H

#include <stdio.h>
#include <conio.h>
#include "jkdc.h"

#define PAPERXSIZE_A4			2480
#define PAPERYSIZE_A4   	3500

class JKPrintDC:public JKDC
{
public:
	JKPrintDC();
 ~JKPrintDC();

	void Make_SwapFile();
	void Delete_SwapFile();
	void OpenDC();
	void CloseDC();

	FILE *SwapStream;


	void SaveAttribute();
	void RestoreAttribute();
	void InitAttribute();
	void _SetTextColor(INT color);
	void _SetBackColor(INT color);
	void _PutPixel(JKPoint p,INT color);
	void _Line(INT x1,INT y1,INT x2,INT y2,INT color);
	void _MoveTo(INT x,INT y);
	void _HLine(JKPoint p,WORD delta,INT color);
	void _VLine(JKPoint p,WORD delta,INT color);
	void _SolidBar(JKRect rect,INT color);
	void _XorBar(JKRect rect,INT color);
	void _GetImage(JKRect rect, BYTE *buffer);
	void _PutImage(JKPoint pos, BYTE *buffer,INT op);
	void _EngPutCh(JKPoint p,BYTE ch);
	void _HanPutCh(JKPoint p,BYTE firstbyte,BYTE secondbyte);
	void _PutString(JKPoint p,WORD count,CHAR *str);

	CHAR SwapFileName[13];
	WORD PaperXByteSize;

	INT TextColor;
	INT BackColor;
};
#endif
