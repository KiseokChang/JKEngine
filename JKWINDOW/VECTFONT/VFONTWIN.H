#ifndef _VFONTWIN_H
#define _VFONTWIN_H

#include "jkwindow.h"

class VectorFontWindow:public JKWindow
{
public:
	VectorFontWindow();
 ~VectorFontWindow();
	void EvShowWindow();
	void PaintClient(JKDC& dc,JKRect& rect);
	void EvKeyDown(WORD key,WORD);

  JKPoint FontSize;
};

#endif