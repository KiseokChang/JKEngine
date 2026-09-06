#ifndef _JKDC_H
#define _JKDC_H

#include <stdio.h>
#include <string.h>
#include "rectque.h"
#include "kgraphic.h"
#include "hanman.h"

typedef struct {
	BYTE Manufacturer;     	/* Ðw¬w 10 (0x0a) */
	int8 ver;       				/*                */
	int8 encod;     				/* RLE ·¡¡e 1     */
	int8 bitpx;     				/* Ðe¸ñ·i Îa¯¡Ða“e §¡Ëa ® */
	int16 x1,y1,x2,y2;				/* ‹aŸ±· ¹ÁÎa    */
	WORD HRes,VRes;			/* Ð¬w•¡         */
	BYTE Colormap[16][3];
	BYTE reserved;     			/* ‹aŸ±·i ¤h·e §¡—¡µ¡ ¡¡—a ? */
	BYTE NPlanes;   				/* ¬a¶w–E ÏiA·¥® */
	WORD BytesperLine;  /* Ðeœa·¥”w ¤a·¡Ëa ® */
	int16  palinfo;   				/* 1=COLOR, 0=GREY */
	int16  shres;     				/* ¯aÄá Ð¬w•¡?*/
	int16  svres;
	int8 xtra[54];  				/* ¬a¶w ´g·q */
} PCXHDR;    							/* PCX Ñw¯¢ ÑÁ·©· ÑA”á */


#define NO_PCX				0
#define PCX_HERCULES 	1
#define PCX_VGA16			2
#define PCX_VGA256		3

enum writemode{     // BitBlt operators for putimage
		COPY_PUT=0,       // MOV
		XOR_PUT,        // XOR
		OR_PUT,         // OR
		AND_PUT,        // AND
		NOT_PUT         // NOT
};

class JKPen
{
public:
	JKPen()
	{
		Pen=0xffff;
		Thick=1;
	}
	JKPen(WORD pattern,WORD thick=1)
	{
		Pen=pattern;
		Thick=thick;
	}

	JKPen& operator = (const JKPen& other)
	{
		Pen=other.Pen;
		Thick=other.Thick;
		return *this;
	}

	WORD Pen;
	WORD Thick;
};

const WORD StdPenCount=4;
static WORD StdPen[StdPenCount]={
	0xffff,0xcccc,0xfc78,0xf8f8
};
enum StdPenName {
	SOLID_PEN,DOTTED_PEN,CENTER_PEN,DASHED_PEN
};

//==================================================================

class JKBrush
{
public:
	JKBrush()
	{
		memcpy(Brush,"\0xff\0xff\0xff\0xff\0xff\0xff\0xff\0xff",8);
	}
	JKBrush(BYTE pattern[8])
	{
		memcpy(Brush,pattern,8);
	}

	JKBrush& operator = (const JKBrush& other)
	{
		memcpy(Brush,other.Brush,8);
		return *this;
	}

	int8 Brush[8];
};

const WORD StdBrushCount=13;

static BYTE StdBrush[StdBrushCount][8]=
{
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	"\xff\xff\xff\xff\xff\xff\xff\xff",
	"\xff\xaa\xff\xaa\xff\xaa\xff\xaa",
	"\xff\xff\x00\x00\xff\xff\x00\x00",
	"\x80\x40\x20\x10\x80\x40\x20\x10",
	"\x87\xc3\xe1\xf0\x78\x3c\x1e\x0f",
	"\x0f\x1e\x3c\x78\xf0\xe1\xc3\x87",
	"\xa5\x4b\x96\x2d\x5a\xb4\x69\xd2",
	"\xff\x11\x11\x11\xff\x11\x11\x11",
	"\x18\x24\x42\x81\x81\x42\x24\x18",
	"\x77\x88\x77\x88\x77\x88\x77\x88",
	"\x30\x30\x03\x03\x30\x30\x03\x03",
	"\x33\x33\xcc\xcc\x33\x33\xcc\xcc"
};
enum StdBrushName {
	EMPTY_BRUSH=0,	SOLID_BRUSH,			SMOOTH_BRUSH,     LINE_BRUSH,
	LTSLASH_BRUSH,  SLASH_BRUSH,  		BKSLASH_BRUSH,  	LTBKSLASH_BRUSH,
	HATCH_BRUSH,  	XHATCH_BRUSH,			int16ERLEAVE_BRUSH, WIDE_DOT_BRUSH,
	CLOSE_DOT_BRUSH
};

class GDISet
{
public:
	GDISet(){}
	JKPen GetStdPen(WORD index,WORD thick=1)
	{
		return JKPen(StdPen[index%StdPenCount],thick);
	};
	JKBrush GetStdBrush(WORD index)
	{
		return JKBrush(StdBrush[index%StdBrushCount]);
	};

	JKPen CurrentPen;
	JKBrush CurrentBrush;
	BOOL WriteMode;
	BYTE TextColor;
	BYTE BackColor;
	BOOL IsOverWrite;

	GDISet& AssignGDI(const GDISet& other)
	{
		CurrentPen  =other.CurrentPen;
		CurrentBrush=other.CurrentBrush;
		WriteMode   =other.WriteMode;
		TextColor   =other.TextColor;
		BackColor   =other.BackColor;
		IsOverWrite =other.IsOverWrite;
		return *this;
	}
};

//=====================================================================//

class JKDC : public GDISet
{
public:
	JKDC();
	virtual ~JKDC();

	virtual uint16 GetMaxX()=0;
	virtual uint16 GetMaxY()=0;
	BOOL InitDC(JKPoint p,JKRect rect);
	virtual void InitAttribute();
	BOOL InitClipRegion(JKPoint p,JKRect rect);
	BOOL InitDC(JKPoint p,RectQue& region);
	BOOL InitClipRegion(JKPoint p,RectQue& region);

	virtual void SetWriteMode(BYTE wmode);
	virtual void SetTextColor(int16 color);
	virtual void SetBackColor(int16 color);
	virtual void SetPen(JKPen pen);
	virtual void SetBrush(JKBrush brush);
	virtual void PutPixel(JKPoint p,int16 color);

	virtual BOOL GetImage(JKRect rect, BYTE *buffer);
	virtual BOOL PutImage(JKPoint pos, BYTE *buffer,BYTE op=COPY_PUT,JKRect cliprect=JKRect(0,0,-1,-1));
	virtual BOOL PutPCX(int8 *fname,JKPoint pos,JKRect rect=JKRect(0,0,-1,-1),BYTE putmode=COPY_PUT,BOOL palchange=FALSE);
	virtual BOOL PutIMG(int8 *fname,JKPoint p,JKRect rect=JKRect(0,0,-1,-1),BYTE putmode=COPY_PUT);

	virtual void Circle(JKPoint p,int16 radius,int16 color);
	virtual void Arc(JKPoint p, double startangle, double endangle, int16 radius,int16 color);
	virtual void Pieslice(JKPoint p, double startangle, double endangle, int16 radius,int16 color);

	virtual void Circle(JKPoint p,JKPoint radius,int16 color);
	virtual void Arc(JKPoint p, double startangle, double endangle, JKPoint radius,int16 color);
	virtual void Pieslice(JKPoint p, double startangle, double endangle, JKPoint radius,int16 color);

	virtual void DrawPolygon(JKPoint *polypoint,WORD numpoints,int16 color);
	virtual void FillPolygon(JKPoint *polypoint,WORD numpoints,int16 color);

	virtual void Line(int16 x1,int16 y1,int16 x2,int16 y2,int16 color);
	void Line(JKPoint p1,JKPoint p2,int16 color);
	virtual void MoveTo(int16 x,int16 y);
	void MoveTo(JKPoint p);
	virtual void LineTo(int16 x,int16 y,int16 color);
	void LineTo(JKPoint p,int16 color);
	virtual void HLine(JKPoint p,int16 delta,int16 color);
	virtual void HLineTo(int16 delta,int16 color);
	virtual void VLine(JKPoint p,int16 delta,int16 color);
	virtual void VLineTo(int16 delta,int16 color);
	virtual void Bezier(JKPoint ps[4],int16 color,BOOL iskbez=FALSE,double delta=0.05);
	virtual void BezierTo(JKPoint ps[3],int16 color,BOOL iskbez=FALSE,double delta=0.05);


	virtual void SolidBar(JKRect rect,int16 color);
	virtual void Rectangle(JKRect rect,int16 color);
	virtual void Rectangle3D(JKRect& rect,int16 depth,int16 color1=White,int16 color2=Black);
	virtual void Box3D(JKRect& rect,int16 depth,int16 color=LtGray,int16 color1=White,int16 color2=Black);
	virtual void ScrollDown(JKRect rect,WORD lines);
	virtual void ScrollUp(JKRect rect,WORD lines);
	virtual void ScrollLeft(JKRect rect,WORD lines);
	virtual void ScrollRight(JKRect rect,WORD lines);

	virtual void EngPutCh(JKPoint p,BYTE ch);
	virtual void HanPutCh(JKPoint p,BYTE firstbyte,BYTE secondbyte);
	virtual void PutCh(JKPoint p,WORD code);
	void TextOut(JKPoint p,int8 *str);
	virtual void TextOut(JKPoint p,WORD n,int8 *str);
	virtual void TextOutX(JKRect rect,int8 *str,BYTE adjflag=0x00,BOOL IsWrapping=FALSE);

//////////////////////////////////////////////////////////////////////////////////////////

	virtual void _PutPixel(JKPoint p,int16 color){}
	virtual void _MoveTo(int16 x,int16 y){}
	virtual void _Line(JKRect rect,int16 x1,int16 y1,int16 x2,int16 y2,int16 color){}
	virtual void _HLine(JKPoint p,WORD delta,int16 color){}
	virtual void _VLine(JKPoint p,WORD delta,int16 color){}
	virtual void _SolidBar(JKRect rect,int16 color){};
	virtual void _GetImage(JKPoint p,BYTE *buffer,JKRect rect);
	virtual void _PutImage(JKPoint pos, BYTE *buffer,JKRect rect,BYTE putmode);

	virtual void _WriteImageLine(JKPoint& p,BYTE *buff,int16 n,BYTE putmode){};
	virtual void _ScanImageLine(JKPoint& p,BYTE *buff,int16 n){};

	virtual BOOL _PutEngImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount=16,BYTE lshift=0,BYTE rshift=0)=0;
	virtual BOOL _PutHanImage(JKPoint& p,BYTE *buff,int8 color,WORD ycount=16,BYTE lshift=0,BYTE rshift=0)=0;

	JKPoint& GetCurrentPos(){ return CurrentPos; }
	JKPoint& GetOriginPos(){ return TempOrgPos; }
	RectQue& GetClipRegion(){ return ClipRegion; }

	JKDC& AssignDC(const JKDC& other)
	{
		AssignGDI(other);
		ClipRegion	=	other.ClipRegion;
		TempOrgPos	=	other.TempOrgPos;
		CurrentPos	=	other.CurrentPos;
		return *this;
	}

	RectQue ClipRegion;
	JKPoint TempOrgPos;
	JKPoint CurrentPos;

protected:

	void TextOutInRect(JKRect rect,JKPoint p,int8 *str);
	void TextOutInRect(JKRect rect,JKPoint p,WORD n,int8 *str);

	BYTE ColorConvTable256[256];

	BOOL SkipPCXLine(FILE *fp,WORD bytes,WORD lines);
	WORD ReadPCXLine(FILE *fp,int8 *buf,int16 bytes,BOOL palchange);
};

#endif