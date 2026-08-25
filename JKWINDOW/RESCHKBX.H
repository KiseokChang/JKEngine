#ifndef _RESCHKBX_H
#define _RESCHKBX_H

#include "checkbox.h"

class ResCheckBox:public JKCheckBox
{
public:
	ResCheckBox(JKRect rect,WORD ctrlid,WORD trueres,WORD falseres);
 ~ResCheckBox();

	JKRect CalcClientRect();

	void PaintClient(JKDC& dc,JKRect& paintrect);

	void SetCheckResource(WORD trueres,WORD falseres);

	void EvSetFocus(DWORD lostid);
	void EvKillFocus(DWORD gainid);

	WORD TrueResID,FalseResID;
};
#endif