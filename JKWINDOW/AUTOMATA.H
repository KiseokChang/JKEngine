#ifndef _AUTOMATA_H
#define _AUTOMATA_H

#include "jkwindef.h"

enum HANINPRESULTS {
				HR_NOKEY,
				HR_ASC,
				HR_HANST,
				HR_HANIN,
				HR_HANEND,
				HR_HANBS,
				HR_NOHAN
};
enum HAN_STATUS {              // ˆb ¬wÈŸi ¸÷·
				HS_Start,
				HS_Chosung,
				HS_Joongsung, HS_dJoongsung,
				HS_Jongsung,  HS_dJongsung,
				HS_End1,         HS_End2
};

enum HAN_CHKIND { CONSONANT,  VOWEL };

class HangulAutomata
{
public:
	HangulAutomata(BOOL ishangulkbd=TRUE);
	virtual ~HangulAutomata(){}

	void InitAutomata();
	WORD ConvertKey(WORD key,WORD modifier);
	WORD JoongsungPair(WORD& key);
	WORD JongsungPair(WORD& key);
	BOOL Automata(WORD key);

	typedef struct {
		WORD curhanst;
		WORD key;
		WORD charcode;
	} INPSTACK;

	INPSTACK InpStack[10];         // µ¡É¡ aÈa ·³b ¯aÈ‚
	WORD OutStack[5];          // µ¡É¡ aÈa Â‰b ¯aÈ‚
	WORD InpSP;
	WORD OutSP;
	WORD CurHanState;
	WORD CharCode;
	WORD OldKey;
	BOOL HanKbdState;
};

#endif