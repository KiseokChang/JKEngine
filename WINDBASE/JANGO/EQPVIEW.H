#ifndef _EQPVIEW_H
	#define _EQPVIEW_H

#include "jkdialog.h"
#include "eqfilemn.h"

class EqpView:public JKDialog
{
public:
	EqpView(JKControl *parent=0);
	~EqpView();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void SetBomb(int16 index);

public:
	BOMB Bomb;
};
#endif