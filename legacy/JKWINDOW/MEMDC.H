#ifndef _MEMDC_H
#define _MEMDC_H

#include "jkdc.h"

class JKMemoryDC:public JKDC
{
public:
	JKMemoryDC(uint16 xsize,uint16 ysize,BYTE initcolor=0);
	JKMemoryDC(JKDC& dc,BYTE initcolor=0);
 ~JKMemoryDC();

	uint16 GetMaxX(){ return XSize? XSize-1:XSize; }
	uint16 GetMaxY(){ return YSize? YSize-1:YSize; }

	void ClearBuffer(BYTE initcolor);


	void _PutPixel(JKPoint p,int16 color);
	void _Line(JKRect rect,int16 x1,int16 y1,int16 x2,int16 y2,int16 color);
	void _HLine(JKPoint p,WORD delta,int16 color);
	void _VLine(JKPoint p,WORD delta,int16 color);
	void _SolidBar(JKRect rect,int16 color);
	void _Arc(JKRect rect,JKPoint p, int16 stangle, int16 endangle, int16 radius,int16 color);
	void _Circle(JKRect rect,JKPoint p,int16 radius,int16 color);
	void _Pieslice(JKRect rect,JKPoint p, int16 stangle, int16 endangle, int16 radius,int16 color);
	void _DrawPolygon(JKRect rect,JKPoint *polypoint,WORD numpoints,int16 color);
	void _FillPolygon(JKRect rect,JKPoint *polypoint,WORD numpoints,int16 color);

	void _WriteImageLine(JKPoint& p,BYTE *buff,int16 n,BYTE putmode);
	void _ScanImageLine(JKPoint& p,BYTE *buff,int16 n);

	BOOL _PutEngImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount=16,BYTE lshift=0,BYTE rshift=0);
	BOOL _PutHanImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount=16,BYTE lshift=0,BYTE rshift=0);

	BYTE *DataBuffer;
	uint16 XSize;
	uint16 YSize;

protected:
	void Set_PutMode(BYTE *dest,BYTE value,uint16 count,BYTE putmode);
	void Set_PutMode(BYTE *dest,BYTE *src,uint16 count,BYTE putmode);

	BYTE *ScreenBuffer;
};

#endif

