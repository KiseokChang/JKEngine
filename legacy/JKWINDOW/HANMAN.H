#ifndef _HANMAN_H
#define _HANMAN_H

#include "jkwindef.h"

class HangulManager
{
public:
	HangulManager();
	virtual ~HangulManager();

	BOOL CreationError;

	BYTE *HanFirstFont;			/* first 	*/
	BYTE *HanMiddleFont;		/* mid   	*/
	BYTE *HanLastFont;			/* las   	*/
	BYTE *EnglishFont;			/* english 	*/

	WORD HanjaPos2HanjaCode(WORD hanjapos);
	WORD HanjaPos2HangulCode(WORD hanjapos);
	int16 HanjaCode2HanjaPos(WORD hanjacode);
	int16 HangulCode2HanjaIndex(WORD hanjacode);

	WORD HanjaCode2HangulCode(WORD hanjacode);

	BOOL IsHangul1st(BYTE *s, int16 pos,BOOL includehanja=TRUE);
	BOOL IsHangul2nd(BYTE *s, int16 pos);
	BOOL IsHanja(BYTE ch1,BYTE ch2);
	WORD RefHanCode(WORD a,WORD b);

	BOOL IsGraphic(BYTE ch1,BYTE ch2);
	BOOL IsSpecial(BYTE ch1,BYTE ch2);

	BOOL GetWORDImage(BYTE *buffer,BYTE ch1,BYTE ch2,BOOL onlyeng=FALSE);
	BOOL GetEnglishImage(BYTE *buffer,BYTE ch);
	BOOL GetSpecialImage(BYTE *buffer,BYTE ch);
	BOOL GetHanjaImage(BYTE *buffer,BYTE ch1,BYTE ch2);
	BOOL GetHangulImage(BYTE *buffer,BYTE ch1,BYTE ch2);

private:
	void OrHangulImage(int8 *dest, int8 *src);

	static int8 RefHanCodeFile[13];

	static int8 IndexHF[3][32];
	static int8 DLast[22];
	static int8 DMiddle[20*2];
	static int8 DFirst[22*2];
	static int8 HangulFontFile[13];
	static int8 HanjaFontFile[13];
	static int8 EngFontFile[13];
	static int8 SpcFontFile[13];
};

////////////////////////////////
extern HangulManager *HanMan;///
////////////////////////////////
#endif