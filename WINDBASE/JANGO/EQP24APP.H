#ifndef _JANGOAPP_H
#define _JANGOAPP_H

#include "jkapp.h"

class Equip24Application: public JKApplication
{
public:
	Equip24Application();
 ~Equip24Application();
	void SetupWindow();

	void EvButtonClick(WORD ctrlid);
};
#endif
