#ifndef _TESTAPP_H
#define _TESTAPP_H

#include "jkapp.h"


class TestApplication : public JKApplication
{
public:
	TestApplication();
 ~TestApplication();
	void SetupWindow();

	void PaintClient(JKDC& dc,JKRect& rect);

	void EvRightClick(JKPoint&);
};

#endif