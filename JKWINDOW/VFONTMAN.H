#ifndef _VFONTMAN_H
#define _VFONTMAN_H

#include "jkdc.h"


#define		FN_MYUNGJO	0

#define		ENGLISH		0
#define		HANGUL		1
#define		HANJA		2
#define		SPECIAL		3


// set tab size = 4 to view this source

typedef struct _FONTINFO {
	char	FontName[32];		  		// typeface name
	WORD	Version;			  			// version #
	WORD	CharSet;          		// Hangul or English
	WORD	CodingSystem;         // English, KS, Johap, etc
	WORD	Pitch;                // fixed or variable
	WORD	StartCode;            // starting code (if KS, starting row #)
	WORD  CharNum;              // numer of character
	WORD	RowNum;				  			// row # if KS code
	WORD  EmSize; 		      		// design em size
	WORD  DefaultWidth;         // default width (relative to DesignMeshSize)
	int32  MapOffset;           // map table offset
	int32  WidthTblOffset;      // width table offset
	int32  GlyphOffset;         // glyph data offset
	char	FileName[128];		  	// actual filename
	int16	*WidthTable;		  		// width table in memory
	char	Reserved[48];
} FONTINFO;


//  Pitch
#define		FONT_FIXED		0
#define		FONT_VARIABLE	1

// CodingSystem
#define		FONT_ENG		0
#define		FONT_KSCODE		100
#define		FONT_JOHAP		200

// CharSet
#define		FONT_LATIN		0
#define		FONT_HANGUL		1
#define		FONT_HANJA		2
#define		FONT_KSSYMBOL	3

// Map Entry
typedef struct _MAPENTRY {
	int16	nGlyphSize;
	int32	lOffset;
} MAPENTRY;


// private definitions
#define		MAX_FONTDATA			4096
#define		MAX_BITMAPHEIGHT	4096

#define		MAGIC_PROP		0xAA

#define		MOVETO			0x10
#define		LINETO			0x20
#define		CURVETO			0x40
#define		ENDCHAR			0x80


typedef struct _BITMAPMETRIC {
	int16	Width;
	int16	Height;
	int16	xOffset;
	int16	yOffset;
} BITMAPMETRIC;

#define		MAX_HANGULFONT		1
#define		MAX_HANJAFONT		1
#define		MAX_SPECFONT		1
#define		MAX_ENGFONT			1

typedef struct _MATRIX {
	int32	a, b, c, d;
	int32	tx, ty;
} MATRIX;

static BYTE mask[8]  = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

/**********************************************************************/

static char	*HangulFontName[MAX_HANGULFONT] = {
	"hanmoon.vft"
};
static char	*HanjaFontName[MAX_HANJAFONT]= {
	"hjmj.vft"
};
static char	*SpecFontName[MAX_SPECFONT] = {
	"sym.vft",
};
static char	*EngFontName[MAX_ENGFONT] = {
	"english.vft"
};


class VectorFontManager
{
public:
	VectorFontManager(char *fontpath=".");
 ~VectorFontManager();

	BOOL InitFontMachinery(char *fontpath);
	void CloseFontMachinery();
	void SetFont(int16 CodeArea, int16 FontId);
	void ResetCTM();
	void SetFontSize(int16 x, int16 y);
	void RotateFont(int16 Angle);
	void SlantFont(int16 Angle);
	void DrawString(JKDC& dc,JKPoint p,BYTE *str);
	void DrawChar(JKDC& dc,JKPoint p,uint16 ch, int16& xInc, int16& yInc);
	int16	GetCharWidth(uint16 ch);
	void GetCharNext(int16 nWidth, int16& xInc, int16& yInc);

	BOOL TextAlign;

	BOOL CreationError;
private:
	BOOL OpenFontFile(FONTINFO * fi, char *filename);
	BOOL InitBuffers();
	void FreeBuffers();

	BOOL CalcCharMetric(BYTE *&fontdata,BITMAPMETRIC *lpMet);
	BOOL _DrawChar(JKDC& dc,JKPoint p,FONTINFO *fi,FILE *fp, int16 index);

	int16	GetWord(BYTE *&fontdata);
	void ScaleCoord(int32& X, int32& Y);
	void TogglePixel(int16 x, int16 y);
	void DrawLine(int32 x1, int32 y1, int32 x2, int32 y2);
	void DrawBezier(int32 x0, int32 x1, int32 x2, int32 x3,
							int32 y0, int32 y1, int32 y2, int32 y3, int16 n);
	BOOL DrawGlyph(BYTE *fontdata);
	BOOL GetPremapFromOutline(BYTE *fontdata,BITMAPMETRIC *lpMet);

	void PutCharacterImage(JKDC& dc,JKPoint p);

	void Sort(int16 a[], int16 n);

	FONTINFO HangulFontInfo[MAX_HANGULFONT];
	FONTINFO HanjaFontInfo[MAX_HANJAFONT];
	FONTINFO SpecFontInfo[MAX_SPECFONT];
	FONTINFO EngFontInfo[MAX_ENGFONT];

	int16	*alpPoint[MAX_BITMAPHEIGHT * 2];
	BYTE anCount[MAX_BITMAPHEIGHT * 2];

	FONTINFO	*lpCurHangulFont,	*lpCurEngFont,	*lpCurHanjaFont,	*lpCurSpecFont;
	char FontDir[100];

	BYTE *lpFontData;
	int16 nEmSize;
	int16 nMaxSubdivDepth;
	int16 nCurFontSizeX, nCurFontSizeY;
	int16 nCharBitmapWidth, nCharBitmapHeight, nCharXBytes;
	int32	xShift, yShift;

	MATRIX	CTM;
};


extern VectorFontManager *GlobalVFontMan;
#endif