#ifndef _VFONTAPP_H
#define _VFONTAPP_H

#include "jkapp.h"


class VectorFontApplication : public JKApplication
{
public:
	VectorFontApplication();
 ~VectorFontApplication();
	void SetupWindow();

	void PaintClient(JKDC& dc,JKRect& rect);

	void EvRightClick(JKPoint&);
};

#endif