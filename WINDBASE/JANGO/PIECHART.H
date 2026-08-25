#ifndef _PIECHSRT_H
#define _PIECHSRT_H

#include <math.h>
#include "jkwindow.h"

typedef struct {
	JKPoint Pos;
	int16 StartAngle;
	int16 EndAngle;
	int16 Color;
	int16 Brush;
}PIESTRUCT;

class PieChart:public JKWindow
{
public:
	PieChart(JKRect rect,WORD ctrid,JKControl *parent=0);
	~PieChart();

	JKRect CalcClientRect();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	BOOL SetValue(uint16 valuecount,uint16 *valuearray);
	void ChangeBkColor(BYTE bkcolor);

	void Pie(JKDC& dc,uint16 index);
	void RedrawChart(uint16 number);

protected:
	uint16 Radius;
	uint16 *ValueArray;
	uint16 ValueCount;
	uint16 Sum;
	BOOL SaveFlag;
	uint16 SaveNumber;
	JKPoint SavePos;
	PIESTRUCT *PIECHART;
};
#endif