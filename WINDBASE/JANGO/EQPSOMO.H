#ifndef _EQPSOMO_H
	#define _EQPSOMO_H

#include "jkdialog.h"
#include "scrllist.h"
#include "eqfilemn.h"

#define ID_RECDVIEW2		 USERMESSAGE
#define	ID_MODECHANGE  USERMESSAGE+1

class EquipSomoWindow:public JKDialog
{
public:
	EquipSomoWindow(JKRect rect,JKControl *parent=0);
	~EquipSomoWindow();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void EvLBTouchEntry(WORD ctrlid,uint16 index);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);
	ProcessResult RespondMessage(JKMSG& msg);

	void CreatListEntry();
	void CreatSomoEntry();
	void ChangeSomoTitle();
	void RecDelete(int16 index);
	void ReForm();
	void InitListBomb();
	BOOL FindBomb(BOMB& bm);
	void SetButtonState(BOOL flag);
	void PrintButtonChange(BOOL flag=FALSE);

public:
	int16 CurrentIndex;
	BOMB ListBomb;
};
#endif