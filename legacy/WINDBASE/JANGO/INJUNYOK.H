#ifndef _INJUNYOK_H
#define _INJUNYOK_H

#include "incommon.h"

class JunYokCondWin:public CommonCondWin
{
public:
	JunYokCondWin(JKRect rect,WORD ctrlid,PersonManager& recman,WORD *& srcharray,uint16& srchcount);

	void SetupWindow();
	void EvButtonClick(WORD ctrlid);
	void EvEditFinish(WORD ctrlid);

	void InitPresentCondition();
	void CreatePresentCondition();

	int16 ViewYear;
};

#endif