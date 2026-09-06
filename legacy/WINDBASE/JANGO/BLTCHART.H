#ifndef _BLTCHSRT_H
#define _BLTCHSRT_H

#include "jkwindow.h"

typedef struct {
	JKRect Rect;
	int16 Color;
	int16 Brush;
}BELTSTRUCT;

class BeltChart:public JKWindow
{
public:
	BeltChart(JKRect rect,WORD ctrid,JKControl *parent=0);
	~BeltChart();

	JKRect CalcClientRect();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	BOOL SetValue(uint16 valuecount,uint16 *valuearray);
	void ChangeBkColor(BYTE bkcolor);

	void DrawBelt(JKDC& dc,uint16 index);
	void RedrawChart(uint16 number);

protected:
	JKRect ChartRect;
	uint16 *ValueArray;
	uint16 ValueCount;
	uint16 Sum;
	BOOL SaveFlag;
	uint16 SaveNumber;
	JKRect SaveRect;
	BELTSTRUCT *BELTCHART;
};
#endif