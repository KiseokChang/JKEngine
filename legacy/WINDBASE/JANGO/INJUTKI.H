#ifndef _INJUTKI_H
#define _INJUTKI_H

#include "incommon.h"

class JutkiCondWin:public CommonCondWin
{
public:
	JutkiCondWin(JKRect rect,WORD ctrlid,PersonManager& recman,WORD *& srcharray,uint16& srchcount);

	void SetupWindow();
	void EvButtonClick(WORD ctrlid);
	void CreatePresentCondition();

	void ModifyInga();
};

#endif