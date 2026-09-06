#ifndef _BARGRAPH_H
	#define _BARGRAPH_H

#include "control.h"
#include "bombman.h"

class BarGraph:public JKControl
{
public:
	BarGraph(JKRect rect,BombBase* bombbase,WORD count,WORD maxvalue,WORD ctrid,JKControl *parent=0);
	~BarGraph();
	JKRect CalcClientRect();
	void InitWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);
	void PaintWindow(JKDC& dc,JKRect& rect);
	void CalBarSize(int16 step=6);
	void SetBarSize(int16 barwidth,int16 ratio);
	void SetValue(BombBase* bombbase,WORD MaxValue);
	void DrawBar(JKDC& dc);

	int16 BarWidth;
	int16 Ratio;
	int16 Step;
	WORD MaxValue;
	WORD Count;
	BombBase* BBase;

};
#endif
