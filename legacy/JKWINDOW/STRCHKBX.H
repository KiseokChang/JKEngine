#ifndef _STRCHKBX_H
#define _STRCHKBX_H

#include "checkbox.h"

class StringCheckBox:public JKCheckBox
{
public:
	StringCheckBox(JKRect rect,WORD ctrlid,char *truestr,char *falsestr);
 ~StringCheckBox();

  JKRect CalcClientRect();

	void PaintClient(JKDC& dc,JKRect& paintrect);

	void SetCheckString(char *truestr,char *falsestr);

	char *TrueString;
	char *FalseString;
};
#endif