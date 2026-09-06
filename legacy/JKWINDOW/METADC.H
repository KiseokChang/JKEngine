#ifndef _METADC_H
#define _METADC_H

#include <dir.h>
#include "jkdc.h"

#define META_SETWRITEMODE       1
#define META_SETTEXTCOLOR       2
#define META_SETBACKCOLOR       3
#define META_SETPEN             4
#define META_SETBRUSH           5
#define META_PUTPIXEL           6

#define META_GETIMAGE						7
#define META_PUTIMAGE           8
#define META_PUTPCX             9
#define META_PUTIMG             10

#define META_CIRCLE             11
#define META_ARC                12
#define META_PIESLICE           13
#define META_DRAWPOLYLINE       14
#define META_DRAWPOLYGON        15
#define META_FILLPOLYGON        16

#define META_LINE               17
#define META_MOVETO             18
#define META_LINETO             19
#define META_HLINE              20
#define META_HLINETO            21
#define META_VLINE              22
#define META_VLINETO            23
#define META_BEZIER             24
#define META_BEZIERTO           25

#define META_SOLIDBAR           26
#define META_RECTANGLE          27
#define META_RECTANGLE3D        28
#define META_BOX3D              29
#define META_SCROLLDOWN         30
#define META_SCROLLUP           31
#define META_SCROLLLEFT         32
#define META_SCROLLRIGHT        33

#define META_ENGPUTCH           34
#define META_HANPUTCH           35
#define META_PUTCH              36
#define META_TEXTOUT            37
#define META_TEXTOUTX           38

class JKMetaDC:public JKDC
{
public:
	JKMetaDC(char *filename);
 ~JKMetaDC();

	uint16 GetMaxX(){ return XSize;}
	uint16 GetMaxY(){ return YSize;}

	void SetWriteMode(BYTE wmode);
	void SetTextColor(int16 color);
	void SetBackColor(int16 color);
	void SetPen(JKPen pen);
	void SetBrush(JKBrush brush);
	void PutPixel(JKPoint p,int16 color);

	BOOL GetImage(JKRect rect, BYTE *buffer);
	BOOL PutImage(JKPoint pos, BYTE *buffer,BYTE op=COPY_PUT,JKRect cliprect=JKRect(0,0,-1,-1));
	BOOL PutPCX(int8 *fname,JKPoint pos,JKRect rect=JKRect(0,0,-1,-1),BYTE putmode=COPY_PUT,BOOL palchange=FALSE);
	BOOL PutIMG(int8 *fname,JKPoint p,JKRect rect=JKRect(0,0,-1,-1),BYTE putmode=COPY_PUT);

	void Circle(JKPoint p,int16 radius,int16 color);
	void Arc(JKPoint p, double startangle, double endangle, int16 radius,int16 color);
	void Pieslice(JKPoint p, double startangle, double endangle, int16 radius,int16 color);
	void DrawPolygon(JKPoint *polypoint,WORD numpoints,int16 color);
	void FillPolygon(JKPoint *polypoint,WORD numpoints,int16 color);

	void Line(int16 x1,int16 y1,int16 x2,int16 y2,int16 color);
	void MoveTo(int16 x,int16 y);
	void LineTo(int16 x,int16 y,int16 color);
	void HLine(JKPoint p,int16 delta,int16 color);
	void HLineTo(int16 delta,int16 color);
	void VLine(JKPoint p,int16 delta,int16 color);
	void VLineTo(int16 delta,int16 color);
	void Bezier(JKPoint ps[4],int16 color,BOOL iskbez=FALSE,double delta=0.05);
	void BezierTo(JKPoint ps[3],int16 color,BOOL iskbez=FALSE,double delta=0.05);


	void SolidBar(JKRect rect,int16 color);
	void Rectangle(JKRect rect,int16 color);
	void Rectangle3D(JKRect& rect,int16 depth,int16 color1=White,int16 color2=Black);
	void Box3D(JKRect& rect,int16 depth,int16 color=LtGray,int16 color1=White,int16 color2=Black);
	void ScrollDown(JKRect rect,WORD lines);
	void ScrollUp(JKRect rect,WORD lines);
	void ScrollLeft(JKRect rect,WORD lines);
	void ScrollRight(JKRect rect,WORD lines);

	void EngPutCh(JKPoint p,BYTE ch);
	void HanPutCh(JKPoint p,BYTE firstbyte,BYTE secondbyte);
	void PutCh(JKPoint p,WORD code);
	void TextOut(JKPoint p,WORD n,int8 *str);
	void TextOutX(JKRect rect,int8 *str,BYTE adjflag=0x00,BOOL IsWrapping=FALSE);

	BOOL BeginPaint();
	BOOL EndPaint();

	BOOL IsOpen(){ return DCFp? TRUE:FALSE;}
protected:
	uint16 XSize;
	uint16 YSize;

  uint32 InstructCount;

	FILE *DCFp;
  char MetaFileName[MAXPATH+1];
};

#endif
