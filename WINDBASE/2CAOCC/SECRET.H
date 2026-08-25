#ifndef _SECRET_H
#define _SECRET_H

#include "control.h"

class Secret:public JKControl
{
public:
	Secret(JKRect& rect);
	void PaintClient(JKDC& dc,JKRect& rect);
};

#endif