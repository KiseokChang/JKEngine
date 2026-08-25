#ifndef _VECTAPP_H
#define _VECTAPP_H

#include "jkapp.h"


class VectorApplication : public JKApplication
{
public:
	VectorApplication();
 ~VectorApplication();
	void SetupWindow();

	void PaintClient(JKDC& dc,JKRect& rect);

	void EvRightClick(JKPoint&);
};

#endif