#ifndef _INBIRTH_H
#define _INBIRTH_H

#include "incommon.h"

class BirthCondWin:public CommonCondWin
{
public:
	BirthCondWin(JKRect rect,WORD ctrlid,PersonManager& recman,WORD *& srcharray,uint16& srchcount);

	void SetupWindow();
	void EvButtonClick(WORD ctrlid);
	void EvEditFinish(WORD ctrlid);
	void InitPresentCondition();
	void CreatePresentCondition();

	int16 ViewYear;
};

#endif