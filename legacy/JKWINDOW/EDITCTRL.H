#ifndef _EDITCTRL_H
#define _EDITCTRL_H

#include "jkedit.h"

class EditControl : public JKEdit
{
public:
	EditControl(JKRect& rect,WORD ctlid,int8 *buffer=0,WORD bufsize=DEFAULTBUFSIZE,BOOL ismultiline=TRUE,BOOL Hangul=TRUE,JKControl *parent=0);
 ~EditControl();

	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect&);
};

#endif
