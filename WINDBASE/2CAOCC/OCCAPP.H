#ifndef _OCCAPP_H
#define _OCCAPP_H

#include "jkapp.h"


class OCCApplication : public JKApplication
{
public:
	OCCApplication();
 ~OCCApplication();

	void SetupWindow();
  void 	InitResourceMan();
};

#endif