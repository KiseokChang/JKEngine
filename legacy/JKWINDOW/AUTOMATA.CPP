#include <ctype.h>
#include "automata.h"

HangulAutomata::HangulAutomata(BOOL ishangulkbd)
{
	HanKbdState = ishangulkbd;
	InitAutomata();
}


void HangulAutomata::InitAutomata()
{
	InpSP = 0;                 // µ¡É¡ aÈa ·³b ¯aÈ‚ Í¡·¥Èá
	OutSP = 0;                 // µ¡É¡ aÈa Â‰b ¯aÈ‚ Í¡·¥Èá
	CurHanState = 0;           // µ¡É¡ aÈa· Ñe¸ ¬wÈ
}


WORD HangulAutomata::JoongsungPair(WORD& key)
{
	static BYTE JoongTable[7][3] = {
		0xad, 0xa3, 0xae,		/* µ¡, ´a, µÁ */
		0xad, 0xa4, 0xaf,		/* µ¡, ´, µá */
		0xad, 0xbd, 0xb2,		/* µ¡, ·¡, ¶A */
		0xb4, 0xa7, 0xb5,		/* ¶, ´á, ¶¡ */
		0xb4, 0xaa, 0xb6,		/* ¶, µA, ¶Á */
		0xb4, 0xbd, 0xb7, 		/* ¶, ·¡, ¶á */
		0xbb, 0xbd, 0xbc		/* ·a, ·¡, · */
	};
	for(WORD i=0;i<7;i++) {
		if(JoongTable[i][0]==OldKey && JoongTable[i][1]==key)
			return (key=JoongTable[i][2]);
	}
	return 0;
}

WORD HangulAutomata::JongsungPair(WORD& key)
{
	static BYTE dJongTable[11][3] = {
		0x82, 0x8b, 0xc4,		/* ‹¡µb, ¯¡µµ */
		0x84, 0x8e, 0xc6,		/* “¡·e, »¡·x */
		0x84, 0x94, 0xc7,		/* “¡·e, Ó¡·} */
		0x87, 0x82, 0xca,		/* Ÿ¡·i, ‹¡µb */
		0x87, 0x88, 0xcb,		/* Ÿ¡·i, £¡·q */
		0x87, 0x89, 0xcc,		/* Ÿ¡·i, §¡·s */
		0x87, 0x8b, 0xcd,		/* Ÿ¡·i, ¯¡µµ */
		0x87, 0x92, 0xce,		/* Ÿ¡·i, Ë¡·{ */
		0x87, 0x93, 0xcf,		/* Ÿ¡·i, Ï¡·| */
		0x87, 0x94, 0xd0,		/* Ÿ¡·i, Ó¡·} */
		0x89, 0x8b, 0xd4,		/* §¡·s, ¯¡µµ */
	};
	for(WORD i=0;i<11;i++) {
		if (dJongTable[i][0]==OldKey && dJongTable[i][1]==key)
			return (key=dJongTable[i][2]);
	}
	return 0;
}


// key => Input code
// 0=> Complete      Combination
// 1=> On            Combination
// Can Get Character From Last Stack
BOOL HangulAutomata::Automata(WORD key)
{
	int16 ChKind, CanBeJongsung = FALSE;
	static BYTE Cho2Jong[] = {	/* Á¡¬÷ Å¡—aµA ”·wÐa“e ¹·¬÷ Å¡—a */
		0xc2,	/*  ‹¡µb				*/
		0xc3,	/*  °w‹¡µb				*/
		0xc5,	/*  “¡·e				*/
		0xc8,	/*  —¡‹h				*/
		0x00,	/*  °w—¡‹h (Ð”w ´ô·q)	*/
		0xc9,	/*  Ÿ¡·i				*/
		0xd1,	/*  £¡·q				*/
		0xd3,	/*  §¡·s				*/
		0x00,	/*  ¬w§¡·s (Ð”w ´ô·q)	*/
		0xd5,	/*  ¯¡µµ				*/
		0xd6,	/*  °w¯¡µµ				*/
		0xd7,	/*  ·¡·w				*/
		0xd8,	/*  »¡·x				*/
		0x00,	/*  °w»¡·x (Ð”w ´ô·q)	*/
		0xd9,	/*  Ã¡·y				*/
		0xda,	/*  Ç¡·z				*/
		0xdb,	/*  Ë¡·{				*/
		0xdc,	/*  Ï¡·|				*/
		0xdd	/*  Ó¡·}				*/
		};

	if ((key & 0x60) == 0x20) {
		ChKind = VOWEL;
	} else {
		ChKind = CONSONANT;
		if (!(key == 0x86 || key == 0x8A || key == 0x8F))
			CanBeJongsung = TRUE;
	}
	if (CurHanState) {
		CharCode = InpStack[InpSP-1].charcode;
		OldKey   = InpStack[InpSP-1].key;
	} else {
		CharCode = 0x8441;
		OldKey   = 0;
	}
	WORD KeyCode = key;
	switch (CurHanState) {
		case HS_Start:
			if (ChKind == CONSONANT)
				CurHanState = HS_Chosung;
			else
				CurHanState = HS_Joongsung;
			break;
		case HS_Chosung:
			if (ChKind == VOWEL)
				CurHanState = HS_Joongsung;
			else
				CurHanState = HS_End1;
			break;
		case HS_Joongsung:
			if (CanBeJongsung)
				CurHanState = HS_Jongsung;
			else if(JoongsungPair(KeyCode))
				CurHanState = HS_dJoongsung;
			else
				CurHanState = HS_End1;
			break;
		case HS_dJoongsung:
			if (CanBeJongsung)
				CurHanState = HS_Jongsung;
			else
				CurHanState = HS_End1;
			break;
		case HS_Jongsung:
			if (ChKind==CONSONANT && JongsungPair(KeyCode))
				CurHanState = HS_dJongsung;
			else if (ChKind == VOWEL)
				CurHanState = HS_End2;
			else
				CurHanState = HS_End1;
			break;
		case HS_dJongsung:
			if (ChKind == VOWEL)
				CurHanState = HS_End2;
			else
				CurHanState = HS_End1;
			break;
	}
	switch (CurHanState) {
		case HS_Chosung:
			CharCode = (CharCode&0x83FF)|((KeyCode-0x80)<<10);
			break;
		case HS_Joongsung:
		case HS_dJoongsung:
			CharCode = (CharCode&0xFC1F)|((KeyCode-0xA0)<<5);
			break;
		case HS_Jongsung:
			KeyCode=Cho2Jong[KeyCode-0x82];
		case HS_dJongsung:
			CharCode = (CharCode&0xFFE0)|(KeyCode-0xC0);
			break;
		case HS_End1:
			OutStack[OutSP++]=key;
			return TRUE;
		case HS_End2:
			OutStack[OutSP++]=key;
			OutStack[OutSP++]=OldKey;
			InpSP--;
			return TRUE;
	}
	InpStack[InpSP].curhanst = CurHanState;
	InpStack[InpSP].charcode = CharCode;
	InpStack[InpSP++].key    = key;
	return FALSE;
}


WORD HangulAutomata::ConvertKey(WORD key,WORD modifier)
{
	static BYTE HanKbrdTable[] = {
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  /*  !"#$%&' */
		0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,  /* ()*+,-./ */
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  /* 01234567 */
		0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,  /* 89:;<=>? */
		0x40, 0x88, 0xBA, 0x90, 0x8D, 0x86, 0x87, 0x94,  /* @ABCDEFG */
		0xAD, 0xA5, 0xA7, 0xA3, 0xBD, 0xBB, 0xB4, 0xA6,  /* HIJKLMNO */
		0xAC, 0x8A, 0x83, 0x84, 0x8C, 0xAB, 0x93, 0x8F,  /* PQRSTUVW */
		0x92, 0xB3, 0x91, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,  /* XYZ[\]^_ */
		0x60, 0x88, 0xBA, 0x90, 0x8D, 0x85, 0x87, 0x94,  /* `abcdefg */
		0xAD, 0xA5, 0xA7, 0xA3, 0xBD, 0xBB, 0xB4, 0xA4,  /* hijklmno */
		0xAA, 0x89, 0x82, 0x84, 0x8B, 0xAB, 0x93, 0x8E,  /* pqrstuvw */
		0x92, 0xB3, 0x91, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F   /* xyz{|}~  */
	};

	if(HanKbdState&&(key>32&&key<127)) {
		if(modifier&0x0040 && isalpha(key)) key = key^0x20;
		key = HanKbrdTable[key-32]&0xFF;
	}
	return key;
}
