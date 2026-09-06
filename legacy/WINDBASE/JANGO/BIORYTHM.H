#ifndef _BIORYTHM_H
#define _BIORYTHM_H

#define EMOTION			24
#define PHYSICAL    28
#define INTELLECT   32

#include <dos.h>
#include <math.h>
#include "jkdialog.h"
#include "jkstatic.h"
#include "jkbutton.h"

const int16 MONTH[12]={31,28,31,30,31,30,31,31,30,31,30,31};

class BioRythm:public JKDialog
{
public:
	BioRythm(char *name,int16 year,int16 month,int16 day,int16 xx,int16 yy,JKControl *parent=0);
	void SetupWindow();
	void PaintClient(JKDC& dc);
	void DrawCurve(JKDC& dc);
	void GetPassDay();
	int16 LeapYear(int16 year);

	void EvCtrlClick(WORD ctrlid);
	void EvCharDown(uint16 key,WORD modifier);

protected:
	int16 Year,Month,Day,BornYear,BornMonth,BornDay;
	uint16 PassDay;
	struct date Date;
	char Name[9];
};
#endif