#include "wancode.h"
#include "vfontman.h"

/*********************************************************************/
VectorFontManager::VectorFontManager(char *fontpath):
	CreationError(FALSE),TextAlign(FALSE)
{
	nEmSize = 256;
	nMaxSubdivDepth = 3;

	lpFontData = 0;
	for(int16 i=0;i<MAX_BITMAPHEIGHT*2;i++) alpPoint[i] = 0;

	if(InitBuffers()) {
		nCurFontSizeX = 1;
		nCurFontSizeY = 1;
		strcpy(FontDir, fontpath);

		int16		i;
		for (i = 0; i < MAX_HANGULFONT; i++)
			OpenFontFile(&HangulFontInfo[i], HangulFontName[i]);
		for (i = 0; i < MAX_HANJAFONT; i++)
			OpenFontFile(&HanjaFontInfo[i], HangulFontName[i]);
		for (i = 0; i < MAX_SPECFONT; i++)
			OpenFontFile(&SpecFontInfo[i], SpecFontName[i]);
		for (i = 0; i < MAX_ENGFONT; i++)
			OpenFontFile(&EngFontInfo[i], EngFontName[i]);

		lpCurHangulFont = &HangulFontInfo[0];
		lpCurHanjaFont = &HanjaFontInfo[0];
		lpCurSpecFont = &SpecFontInfo[0];
		lpCurEngFont = &EngFontInfo[0];

		ResetCTM();
		SetFont(HANGUL, FN_MYUNGJO);
	}
	else CreationError=TRUE;
}

VectorFontManager::~VectorFontManager()
{
	if(!CreationError) FreeBuffers();;
}


BOOL VectorFontManager::OpenFontFile(FONTINFO *fi, char *filename)
{
	char	s[100];
	uint16 	len;

	sprintf(s, "%s\\%s", FontDir, filename);
	FILE *fp = fopen(s,"rb");
	if(!fp) return FALSE;
	if(fread(fi,sizeof(FONTINFO),1,fp)) {
		if(fi->Pitch == FONT_VARIABLE) {
			len = fi->CharNum * sizeof(int16);
			fi->WidthTable = new int16[len];
			if(!fi->WidthTable){ fclose(fp); return FALSE; }
			fseek(fp,fi->WidthTblOffset,SEEK_SET);
			if(!fread(fi->WidthTable,len,1,fp)){
				delete[] fi->WidthTable; fi->WidthTable=0;
				fclose(fp);
				return FALSE;
			}
		}
		fclose(fp);
		strcpy(fi->FileName, s);
		return TRUE;
	}
	fclose(fp);
	return FALSE;
}

BOOL VectorFontManager::InitBuffers()
{
	lpFontData = new BYTE[MAX_FONTDATA];
	if(lpFontData) {
		for(int16 i=0;i<MAX_BITMAPHEIGHT*2;i++) {
			alpPoint[i] = new int16[30];
			if(!alpPoint[i]) {
				FreeBuffers();
				return FALSE;
			}
		}
		return TRUE;
	}
	return FALSE;
}

void VectorFontManager::FreeBuffers()
{
	if(lpFontData) delete[] lpFontData;
	for (int16 i=0;i<MAX_BITMAPHEIGHT*2;i++)
		if(alpPoint[i])	{	delete[] alpPoint[i];		alpPoint[i]=0;	}
}

void VectorFontManager::ResetCTM()
{
	CTM.a = 1024;	CTM.b = 0;
	CTM.c = 0;		CTM.d = 1024;
	CTM.tx = 0; 	CTM.ty = 0;
}

void VectorFontManager::SetFontSize(int16 xSize, int16 ySize)
{
	nCurFontSizeX = xSize;
	nCurFontSizeY = ySize;

	if (xSize > 128 || ySize > 128)	nMaxSubdivDepth = 4;
	else                           	nMaxSubdivDepth = 3;
}

#include "sintbl.h"

void VectorFontManager::RotateFont(int16 angle)
{
	int32	SinVal, CosVal;
	int32	a, b, c, d, tx, ty;

	angle %= 360;
	if (angle < 0) angle += 360;
	if (angle == 0) return;

	CosVal = CosTbl[angle];
	SinVal = SinTbl[angle];

	a = CTM.a * CosVal - CTM.b * SinVal;
	b = CTM.a * SinVal + CTM.b * CosVal;
	c = CTM.c * CosVal - CTM.d * SinVal;
	d = CTM.c * SinVal + CTM.d * CosVal;
	tx = CTM.tx * CosVal - CTM.ty * SinVal;
	ty = CTM.tx * SinVal + CTM.ty * CosVal;

	CTM.a = a >> 10;
	CTM.b = b >> 10;
	CTM.c = c >> 10;
	CTM.d = d >> 10;
	CTM.tx = tx >> 10;
	CTM.ty = ty >> 10;
}

void VectorFontManager::SlantFont(int16 angle)
{
	int32	SinVal, CosVal;

	angle %= 360;
	if (angle >= 90 || angle <= -90) return;
	if (angle < 0) angle += 360;
	if (angle == 0) return;

	SinVal = SinTbl[angle];
	CosVal = CosTbl[angle];

	CTM.a -= CTM.c * SinVal / CosVal;
	CTM.b -= CTM.d * SinVal / CosVal;
	CTM.tx -= CTM.ty * SinVal / CosVal;
}

void VectorFontManager::SetFont(int16 CodeArea, int16 FontId)
{
	switch (CodeArea) {
		case ENGLISH :
			if (FontId >= 0 && FontId < MAX_ENGFONT)
				lpCurEngFont = &EngFontInfo[FontId];
			break;
		case HANGUL	 :
			if (FontId >= 0 && FontId < MAX_HANGULFONT)
				lpCurHangulFont = &HangulFontInfo[FontId];
			break;
		case HANJA	 :
			if (FontId >= 0 && FontId < MAX_HANJAFONT)
				lpCurHanjaFont = &HanjaFontInfo[FontId];
			break;
		case SPECIAL :
			if (FontId >= 0 && FontId < MAX_SPECFONT)
				lpCurSpecFont = &SpecFontInfo[FontId];
			break;
	}
}

void VectorFontManager::DrawString(JKDC& dc,JKPoint p, BYTE *str)
{
	int16	xinc, yinc;

	while(*str) {
		if(*str<128) {
			DrawChar(dc,p,*str,xinc,yinc);
			str++;
		}
		else {
			DrawChar(dc,p,MAKEWORD(*(str+1),*str),xinc,yinc);
			str += 2;
		}
		p+=JKPoint(xinc,yinc);
	}
}


void VectorFontManager::DrawChar(JKDC& dc,JKPoint p,BYTE16 ch,int16& xInc,int16& yInc)
{
	BYTE16	  ch2;
	int16		  row, nWidth;
	FONTINFO *fi;
	int16		  index;

	xInc=yInc=0;
	if(ch<0x80) fi = lpCurEngFont;
	else if(ch>=0x8440 && ch<=0xd3fe) fi = lpCurHangulFont;
	else if(ch>=0xd931 && ch<=0xdefe) fi = lpCurSpecFont;
	else if(ch>=0xe031 && ch<=0xf9fe) fi = lpCurHanjaFont;
	else return;

	FILE *fp=fopen(fi->FileName,"rb");
	if(!fp) return;

	switch(fi->CodingSystem) {

		case FONT_ENG:
			if(ch < fi->StartCode || ch >= fi->StartCode + fi->CharNum){
				fclose(fp);
				return;
			}
			index = ch - fi->StartCode;
			_DrawChar(dc,p,fi,fp,index);
			break;

		case FONT_KSCODE:
			if((ch2 = KSSM2KS(ch))==0xffff) {
				fclose(fp);
				return;
			}
			row = (ch2 >> 8) - fi->StartCode;
			if(row < 0 || row >= fi->RowNum) {
				fclose(fp);
				return;
			}
			index = row*94+(ch2&0x00ff)-0xa1;
			_DrawChar(dc,p,fi,fp,index);
			break;

		default:
			fclose(fp);
			return;
	}
	fclose(fp);

	if(fi->Pitch==FONT_FIXED)	nWidth = fi->DefaultWidth;
	else nWidth = fi->WidthTable[index];
	GetCharNext(nWidth,xInc, yInc);
}

int16 VectorFontManager::GetCharWidth(BYTE16 ch)
{
	BYTE16		ch2;
	FONTINFO *fi;
	int16			row, index;

	if (ch<0x80) fi=lpCurEngFont;
	else if(ch>=0x8440 && ch<=0xd3fe) fi=lpCurHangulFont;
	else if(ch>=0xd931 && ch<=0xdefe) fi=lpCurSpecFont;
	else if(ch>=0xe031 && ch<=0xf9fe) fi=lpCurHanjaFont;
	else return -1;

	if(fi->Pitch == FONT_FIXED) return fi->DefaultWidth;

	switch(fi->CodingSystem) {
		case FONT_ENG:
			if(ch < fi->StartCode || ch >= fi->StartCode + fi->CharNum) return -1;
			index = ch-fi->StartCode;
			break;
		case FONT_KSCODE:
			if((ch2 = KSSM2KS(ch))==0xffff) return -1;
			row = (ch2>>8)-fi->StartCode;
			if(row<0 || row>=fi->RowNum) return -1;
			index = row*94+(ch2&0x00ff)-0xa1;
			break;
		default:
			return -1;
	}
	return fi->WidthTable[index];
}

void VectorFontManager::PutCharacterImage(JKDC& dc,JKPoint p)
{
	for(int16 i=0;i<nCharBitmapHeight;i++) {
		Sort(alpPoint[i], anCount[i]);
		for (int16 j=0;j<anCount[i];j+=2) {
			int16 x1 = alpPoint[i][j];
			int16 x2 = alpPoint[i][j+1];
			dc.HLine(p+JKPoint(x1,i),x2-x1+1,dc.TextColor);
		}
	}
}

BOOL VectorFontManager::_DrawChar(JKDC& dc,JKPoint p, FONTINFO *fi,FILE *fp, int16 index)
{
	MAPENTRY me;
	fseek(fp,fi->MapOffset+(int32)index*sizeof(MAPENTRY),SEEK_SET);
	if(!fread(&me,sizeof(MAPENTRY),1,fp)) return FALSE;

	fseek(fp,me.lOffset,SEEK_SET);
	if(!me.nGlyphSize) lpFontData[0]=ENDCHAR;
	else if(!fread(lpFontData,me.nGlyphSize,1,fp)) return FALSE;

	BITMAPMETRIC	Met;
	int16 nRet=GetPremapFromOutline(lpFontData,&Met);
	if(!nRet) return FALSE;

	p.x+=Met.xOffset;
	p.y-=Met.yOffset+Met.Height;
	if(!TextAlign) p.y+=nCurFontSizeY;
	PutCharacterImage(dc,p);
	return TRUE;
}

void VectorFontManager::GetCharNext(int16 nCharWidth, int16& dx, int16& dy)
{
	int32	DX=(int32)nCharWidth*nCurFontSizeX/nEmSize;
	int32	DY=0;
	dx = (DX*CTM.a+DY*CTM.b+CTM.tx)>>10;
	dy = -(DX*CTM.c+DY*CTM.d+CTM.ty)>>10;
}

void VectorFontManager::ScaleCoord(int32& X, int32& Y)
{
	int32	XX = X*(nCurFontSizeX*1024L/nEmSize);
	int32	YY = Y*(nCurFontSizeY*1024L/nEmSize);

	X=(XX*CTM.a+YY*CTM.b+CTM.tx)>>10;
	Y=(XX*CTM.c+YY*CTM.d+CTM.ty)>>10;
}

BOOL VectorFontManager::GetPremapFromOutline(BYTE *fontdata, BITMAPMETRIC *lpMet)
{
	if(!CalcCharMetric(fontdata,lpMet)) return FALSE;
	nCharBitmapWidth  = lpMet->Width;
	nCharBitmapHeight = lpMet->Height;

	if(DrawGlyph(fontdata)) return TRUE;
	return FALSE;
}


BOOL VectorFontManager::CalcCharMetric(BYTE *&fontdata,BITMAPMETRIC *lpMet)
{
	if(!fontdata) return FALSE;
	int16	x11, y11, x22, y22;
	if(*fontdata == MAGIC_PROP) {
		fontdata++;
		x11 = GetWord(fontdata)-4;
		y11 = GetWord(fontdata)-4;
		x22 = GetWord(fontdata)+4;
		y22 = GetWord(fontdata)+4;
		if(x11>=x22 || y11>=y22)
			return FALSE;
	}
	else {
		x11=y11=0;
		x22=y22=nEmSize;
	}

	int32	xUpLeft, yUpLeft, xUpRight, yUpRight;
	int32	xDownLeft, yDownLeft, xDownRight, yDownRight;

	xUpLeft  = xDownLeft = x11;
	xUpRight = xDownRight= x22;
	yUpLeft  = yUpRight  = y11;
	yDownLeft= yDownRight= y22;

	ScaleCoord(xUpLeft, yUpLeft);
	ScaleCoord(xUpRight,yUpRight);
	ScaleCoord(xDownLeft ,yDownLeft);
	ScaleCoord(xDownRight,yDownRight);

	int32	xMin = xUpLeft;
	if(xMin > xUpRight)   xMin = xUpRight;
	if(xMin > xDownLeft)  xMin = xDownLeft;
	if(xMin > xDownRight) xMin = xDownRight;
	int32	xMax = xUpLeft;
	if(xMax < xUpRight)   xMax = xUpRight;
	if(xMax < xDownLeft)  xMax = xDownLeft;
	if(xMax < xDownRight) xMax = xDownRight;
	int32	yMin = yUpLeft;
	if(yMin > yUpRight)   yMin = yUpRight;
	if(yMin > yDownLeft)  yMin = yDownLeft;
	if(yMin > yDownRight) yMin = yDownRight;
	int32	yMax = yUpLeft;
	if(yMax < yUpRight)   yMax = yUpRight;
	if(yMax < yDownLeft)  yMax = yDownLeft;
	if(yMax < yDownRight) yMax = yDownRight;

	lpMet->Width =((xMax-1)>>10)-(xMin>>10)+1;
	lpMet->Height=((yMax-1)>>10)-(yMin>>10)+1;
	lpMet->xOffset=int16(xMin>>10);
	lpMet->yOffset=int16(yMin>>10);

	xShift=(int32)lpMet->xOffset<<10;
	yShift=(int32)lpMet->yOffset<<10;

	return TRUE;
}

void VectorFontManager::TogglePixel(int16 x, int16 y)
{
	if(x>=0&&x<nCharBitmapWidth && y>=0&&y<nCharBitmapHeight)
		alpPoint[y][anCount[y]++] = x;
}

void VectorFontManager::DrawLine(int32 x1, int32 y1, int32 x2, int32 y2)
{
	x1=(x1+16)>>5;	y1=(y1+16)>>5;
	x2=(x2+16)>>5;	y2=(y2+16)>>5;

	int16	iy1=(int16)((y1+16)>>5),iy2=(int16)((y2+16)>>5);

	if(iy1 != iy2) {
		int16		starty=iy1, endy=iy2-1, incdec=1;
		if (y1>=y2) {				//  up
			starty = iy1 - 1;	endy = iy2;
			incdec = -1;
		}

		int32	dx=x2-x1,dy=y2-y1;
		int16	x;
		int32	xx;
		for(int16 y = starty;y!=endy+incdec;y+=incdec) {
			xx=((((int32)y<<5)+16-y1)*dx)/dy;
			x=(int16)((x1 + xx) >> 5);
			TogglePixel(x, y);
		}
	}
}

void VectorFontManager::DrawBezier(int32 x0, int32 x1, int32 x2, int32 x3,
						int32 y0, int32 y1, int32 y2, int32 y3, int16 n)
{
	if(n<nMaxSubdivDepth){
		int32 mx0=(x0+x1)/2,   mx1=(x1+x2)/2,   mx2=(x2+x3)/2;
		int32 mx3=(mx0+mx1)/2, mx4=(mx1+mx2)/2, mx5=(mx3+mx4)/2;
		int32 my0=(y0+y1)/2,   my1=(y1+y2)/2,   my2=(y2+y3)/2;
		int32 my3=(my0+my1)/2, my4=(my1+my2)/2, my5=(my3+my4)/2;

		DrawBezier(x0, mx0, mx3, mx5, y0, my0, my3, my5, n+1);
		DrawBezier(mx5, mx4, mx2, x3, my5, my4, my2, y3, n+1);
	}
	else DrawLine(x0, y0, x3, y3);
}

int16 VectorFontManager::GetWord(BYTE *&fontdata)
{
	int16	n = (*fontdata<<8)+*(fontdata+1);
	fontdata+=2;
	return n;
}

BOOL VectorFontManager::DrawGlyph(BYTE *fontdata)
{
	BOOL bFirstMove = TRUE;
	int32	prevx=0, prevy=0;
	int32	firstx = prevx,firsty = prevy;

	int32	yScaledSize = (int32)nCharBitmapHeight << 10;

	for (int16 i=0;i<nCharBitmapHeight;i++)	anCount[i] = 0;

	int32	x1, y1, x2, y2, x3, y3;
	BOOL LoopBreak=FALSE,ReturnValue=TRUE;
	while(!LoopBreak) {
		switch (*fontdata++) {
			case MOVETO:
				if (!bFirstMove) DrawLine(prevx, prevy, firstx, firsty);
				bFirstMove = FALSE;
				x1 = GetWord(fontdata);
				y1 = GetWord(fontdata);
				ScaleCoord(x1,y1);
				firstx = prevx = x1 - xShift;
				firsty = prevy = yScaledSize - y1 + yShift;
				break;
			case LINETO:
				x1 = GetWord(fontdata);
				y1 = GetWord(fontdata);
				ScaleCoord(x1,y1);
				x1 = x1 - xShift;
				y1 = yScaledSize - y1 + yShift;
				DrawLine(prevx, prevy, x1, y1);
				prevx = x1;
				prevy = y1;
				break;
			case CURVETO:
				x1 = GetWord(fontdata);
				y1 = GetWord(fontdata);
				ScaleCoord(x1,y1);
				x1 = x1-xShift;
				y1 = yScaledSize - y1 + yShift;
				x2 = GetWord(fontdata);
				y2 = GetWord(fontdata);
				ScaleCoord(x2,y2);
				x2 = x2-xShift;
				y2 = yScaledSize - y2 + yShift;
				x3 = GetWord(fontdata);
				y3 = GetWord(fontdata);
				ScaleCoord(x3,y3);
				x3 = x3-xShift;
				y3 = yScaledSize - y3 + yShift;
				DrawBezier(prevx, x2, x3, x1, prevy, y2, y3, y1, 0);
				prevx = x1;
				prevy = y1;
				break;
			case ENDCHAR:
				DrawLine(prevx, prevy, firstx, firsty);
				LoopBreak=TRUE;
				break;
			default:
				LoopBreak=TRUE;
				ReturnValue=FALSE;
				break;
		 }
	}
	return ReturnValue;
}

void VectorFontManager::Sort(int16 a[], int16 n)
{
	int16 t;
	for (int16 i=1;i<n;i++) {
		t = a[i];
		for (int16 j=i;j>0&&a[j-1]>t;j--) a[j]=a[j-1];
		a[j] = t;
	}
}


VectorFontManager *GlobalVFontMan=0;
