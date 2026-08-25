#ifndef _PRESAPP_H
#define _PRESAPP_H

#include "jkapp.h"


class PresentApplication : public JKApplication
{
public:
	PresentApplication();
 ~PresentApplication();
	void SetupWindow();

	void PaintClient(JKDC& dc,JKRect& rect);

	void EvRightClick(JKPoint&);
};

#endif