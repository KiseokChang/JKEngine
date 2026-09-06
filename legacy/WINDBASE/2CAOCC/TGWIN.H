#ifndef _TGWIN_H
#define _TGWIN_H

#include "firerec.h"
#include "jkwindow.h"

class TargetWindow : public JKWindow
{
public:
	TargetWindow(JKPoint& p,WORD ctrlid,uint16 index,JKControl *parent=0);
	~TargetWindow();

	void SetupWindow();

	ProcessResult RespondMessage(JKMSG& msg);

	void EvButtonClick(WORD ctrlid);

	char *CreateListString(FireRecord& rec);
	BOOL AddBattalion(uint16 podaeindex);

	uint16 Index;
};

#endif