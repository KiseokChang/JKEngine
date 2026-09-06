#ifndef _PRFCCTRL_H
#define _PRFCCTRL_H

#include "control.h"

class PrefaceControl : public JKControl
{
public:
	PrefaceControl(JKRect& rect,WORD ctlid,BOOL kind,JKControl *parent=0);
 ~PrefaceControl();

	void SetupWindow();

	void PaintClient(JKDC&dc,JKRect&);

	BOOL Kind;
};

#endif