#include <math.h>
#include "bltchart.h"

BeltChart::BeltChart(JKRect rect,WORD ctrid,JKControl *parent):
	JKWindow(rect,parent,ctrid),ValueCount(0),ValueArray(0),BELTCHART(0),SaveFlag(0)
{
	WinForm=0x03;
	WinAttr|=WA_FIRSTSHOW;
}

BeltChart::~BeltChart()
{
	if(ValueArray) delete[] ValueArray;
	if(BELTCHART) delete[] BELTCHART;
}

JKRect BeltChart::CalcClientRect()
{
	JKRect rect=JKWindow::CalcClientRect();

	ChartRect=rect.ExpandBy(-8);
	ChartRect.bottom-=15;
	ChartRect.right=
	(rect.left+rect.right)/3;
	ChartRect.SetLeftTop(JKPoint(8,8));
	return rect;
}

void BeltChart::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(SaveFlag&&ValueArray[SaveNumber]){
		dc.Rectangle3D(rect,4,Black,Black);
		if(Sum){
			BYTE buffer[11];
			sprintf(buffer,"%-3.1f%",(double)ValueArray[SaveNumber]/Sum*100.);
			rect.bottom-=2;
			rect.top=rect.bottom-16;
			dc.SetBackColor(BackColor);
			dc.SetTextColor(LtMagenta);
			dc.TextOutX(rect,buffer,ADJ_XYCENTER);
		}
	}
	for(int16 i=0;i<ValueCount;i++)	DrawBelt(dc,i);
}


BOOL BeltChart::SetValue(uint16 valuecount,uint16 *valuearray)
{
	if(valuecount&&valuearray){
		Sum=0;
		ValueCount=valuecount;
		if(ValueArray) delete[] ValueArray;
		ValueArray = new uint16[ValueCount];
		memcpy(ValueArray,valuearray,ValueCount*sizeof(uint16));

		if(BELTCHART) delete[] BELTCHART;
		BELTCHART=new BELTSTRUCT[ValueCount];

		uint16 MaxValue=0;
		for(uint16 i=0;i<ValueCount;i++)	{
			BELTCHART[i].Rect=ChartRect;
			uint16 color=15-i%15;
			if(BackColor<=color) BELTCHART[i].Color=(color-1+16)%16;
			else BELTCHART[i].Color=color;
			BELTCHART[i].Brush=(i/15+1)%13;
			BELTCHART[i].Rect.top=Sum;
			if (MaxValue<valuearray[i]) MaxValue=valuearray[i];
			Sum+=valuearray[i];
			BELTCHART[i].Rect.bottom=Sum;
		}
		for(i=0;i<ValueCount;i++)	{
			if(Sum){
				BELTCHART[i].Rect.top = ChartRect.top + uint16(ChartRect.Height()*(int32)BELTCHART[i].Rect.top/Sum);
				BELTCHART[i].Rect.bottom = ChartRect.top + uint16(ChartRect.Height()*(int32)BELTCHART[i].Rect.bottom/Sum);
			}
			else{
				BELTCHART[i].Rect.top = BELTCHART[i].Rect.bottom =ChartRect.top;
			}
		}
		SaveFlag=FALSE;
		RedrawClient();
		return TRUE;
	}
	return FALSE;
}

void BeltChart::DrawBelt(JKDC& dc,uint16 index)
{
	if(index<ValueCount){
		BELTSTRUCT&st =BELTCHART[index];
		if(ValueArray[index]){
			if(SaveFlag&&index==SaveNumber)	{
				dc.Rectangle(st.Rect,Black);
			}
			dc.SetBrush(dc.GetStdBrush(st.Brush));
			dc.SolidBar(st.Rect,st.Color);
			if(SaveFlag&&index==SaveNumber)	dc.Rectangle(st.Rect,Black);
		}
	}
}

void BeltChart::ChangeBkColor(BYTE bkcolor)
{
	if(bkcolor<16){
		BackColor=bkcolor;
		int16 color;
		for(uint16 i=0;i<ValueCount;i++)	{
			color=15-i%15;
			if(BackColor>=color) BELTCHART[i].Color=(color-1+16)%16;
			else BELTCHART[i].Color=color;
		}
	}
}

void BeltChart::RedrawChart(uint16 number)
{
	if(number<ValueCount) {
		if((SaveFlag&&number!=SaveNumber)||!SaveFlag) {
			if(SaveFlag) BELTCHART[SaveNumber].Rect=SaveRect;

			SaveRect=BELTCHART[number].Rect;
			SaveNumber=number;
			JKRect rect=ClientRect.SetLeftTopBy();
			BELTCHART[number].Rect.right=rect.right-8;
			SaveFlag=TRUE;
		}
	}
	else{
		if(SaveFlag) {
			BELTCHART[SaveNumber].Rect=SaveRect;
			SaveFlag=FALSE;
		}
	}
	RedrawClient();
}

