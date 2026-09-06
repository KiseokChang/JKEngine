#ifndef _EQPSTAT_H
	#define _EQPSTAT_H

#include "jkwindow.h"
#include "eqfilemn.h"

class EqpStat:public JKWindow
{
public:
	EqpStat(WORD ctrlid,JKControl *parent=0);
	~EqpStat();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);
	void EvButtonClick(WORD ctrlid);
	void SetBomb(BOMB& bm);

public:
	BOMB Bomb;
};
#endif