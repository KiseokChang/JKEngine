#ifndef _EQPSMIP_H
	#define _EQPSMIP_H

#include "jkdialog.h"
#include "jkedit.h"
#include "eqfilemn.h"

class EquipSomoInputWindow:public JKDialog
{
public:
	EquipSomoInputWindow(JKControl *parent);
	~EquipSomoInputWindow();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void Modify();
	void InitSomo();
	void EvButtonClick(WORD ctrlid);
	void EvEditFinish(WORD /*ctrlid*/);

public:
	int16 Index;
	uint16 BombCount;
	uint16 TempCount1;
	uint16 TempCount2;
	BOMB SomoBomb;
	char Count[5];
	char SomoYear[11];
};
#endif