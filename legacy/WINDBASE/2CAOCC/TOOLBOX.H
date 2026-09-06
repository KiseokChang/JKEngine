#ifndef _TOOLBOX_H
#define _TOOLBOX_H

#include "jkwindow.h"

class ToolBox:public JKWindow
{
public:
	ToolBox(JKRect& rect,WORD ctlid,JKControl *parent=0);
	~ToolBox();

	void SetupWindow();

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);
	void EvButtonClick(WORD ctrlid);

};
#endif