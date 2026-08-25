#ifndef _JKSCRNDC_H
#define _JKSCRNDC_H

#include "jkdc.h"

class JKScreenDC:public JKDC
{
public:
	JKScreenDC();
 ~JKScreenDC();

	uint16 GetMaxX(){ return 1023; }
	uint16 GetMaxY(){ return 767; }

	void _PutPixel(JKPoint p,int16 color);
	void _Line(JKRect rect,int16 x1,int16 y1,int16 x2,int16 y2,int16 color);
	void _HLine(JKPoint p,WORD delta,int16 color);
	void _VLine(JKPoint p,WORD delta,int16 color);
	void _SolidBar(JKRect rect,int16 color);

	void _WriteImageLine(JKPoint& p,BYTE *buff,int16 n,BYTE putmode);
	void _ScanImageLine(JKPoint& p,BYTE *buff,int16 n);

	BOOL _PutEngImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount=16,BYTE lshift=0,BYTE rshift=0);
	BOOL _PutHanImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount=16,BYTE lshift=0,BYTE rshift=0);
};

extern JKScreenDC * GlobalDC;
#endif
