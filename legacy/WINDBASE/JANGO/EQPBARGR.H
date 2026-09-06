#ifndef _EQPBARGR_H
	#define _EQPBARGR_H

#define GRAPHCOUNT 	8

#include "jkwindow.h"
#include "BombMan.h"

class EqpBarGr:public JKWindow
{
public:
	EqpBarGr(WORD ctrid,BombManager *bomb,JKControl *parent=0);
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void CalBarSize();
	void SetValue();
	void DrawBar(JKDC& dc,BombBase* bombbase,JKRect rect);
	void Bar(JKDC& dc,JKRect rect,WORD value,int16 color);

	void EvCtrlClick(WORD ctrlid);
//	void EvCharDown(uint16 key,WORD modifier);
//	void Bar();
	BombManager *BombMan;
	JKRect BarRect;
	WORD MaxValue;
	int16 Step;
	int16 BarWidth;
	int16 FirstShowIndex;
};
#endif
