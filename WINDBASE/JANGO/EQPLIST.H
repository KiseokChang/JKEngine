#ifndef _EQPLIST_H
	#define _EQPLIST_H

#define ID_RECDELETE  USERMESSAGE+1
#define ID_RECMODIFY  USERMESSAGE+2
#define ID_RECINPUT   USERMESSAGE+3
#define	ID_RECDVIEW   USERMESSAGE+4
#define ID_REFLUSH    USERMESSAGE+5

#include "jkwindow.h"
#include "scrllist.h"
#include "eqfilemn.h"

class EquipListWindow:public JKWindow
{
public:
	EquipListWindow(JKRect rect,WORD ctrlid=0,JKControl *parent=0);
	~EquipListWindow();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void EvLBTouchEntry(WORD ctrlid,uint16 index);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);
	ProcessResult RespondMessage(JKMSG& msg);

	void CreatListEntry();
	void RecDelete(int16 index);
	void ReForm();
	void InitListBomb();
	BOOL FindBomb(BOMB& bm);

public:
	int16 CurrentIndex;
	BOMB ListBomb;
};
#endif