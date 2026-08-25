#include "piechart.h"

PieChart::PieChart(JKRect rect,WORD ctrid,JKControl *parent):
	JKWindow(rect,parent,ctrid),ValueCount(0),ValueArray(0),PIECHART(0),SaveFlag(0)
{
	WinForm=0x01;
	WinAttr|=WA_FIRSTSHOW;
}

PieChart::~PieChart()
{
	if(ValueArray) delete[] ValueArray;
	if(PIECHART) delete[] PIECHART;
}

JKRect PieChart::CalcClientRect()
{
	JKRect rect=JKWindow::CalcClientRect();
	JKPoint pos((rect.Width()+1)/2,(rect.Height()+1)/2);
	Radius=(pos.x>pos.y)? pos.y*3/5:pos.x*3/5;
	return rect;
}

void PieChart::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(SaveFlag&&ValueArray[SaveNumber]){
		dc.Rectangle3D(rect,4,Black,Black);
		if(Sum){
			BYTE buffer[11];
			sprintf(buffer,"%-3.1f%",(double)ValueArray[SaveNumber]/Sum*100.);
			rect.top=rect.bottom-16;
			dc.SetBackColor(BackColor);
			dc.SetTextColor(LtMagenta);
			dc.TextOutX(rect,buffer,ADJ_XYCENTER);
		}
	}
	for(int16 i=0;i<ValueCount;i++)	Pie(dc,i);
}


BOOL PieChart::SetValue(uint16 valuecount,uint16 *valuearray)
{
	if(valuecount&&valuearray){
		Sum=0;
		ValueCount=valuecount;
		if(ValueArray) delete[] ValueArray;
		ValueArray = new uint16[ValueCount];
		memcpy(ValueArray,valuearray,ValueCount*sizeof(uint16));

		if(PIECHART) delete[] PIECHART;
		PIECHART=new PIESTRUCT[ValueCount];

		JKPoint pos((ClientRect.Width()+1)/2,(ClientRect.Height()+1)/2);
		uint16 MaxValue=0;
		uint16 i,color;
		for(i=0;i<ValueCount;i++)	{
			PIECHART[i].Pos=pos;
			color=15-i%15;
			if(BackColor<=color) PIECHART[i].Color=(color-1+16)%16;
			else PIECHART[i].Color=color;
			PIECHART[i].Brush=(i/15+1)%13;
			if (MaxValue<valuearray[i]) MaxValue=valuearray[i];
			Sum+=valuearray[i];
		}
		for(i=0;i<ValueCount;i++)	{
			PIECHART[i].StartAngle= i? PIECHART[i-1].EndAngle : 0;
			if (i==ValueCount-1) PIECHART[i].EndAngle=360;
			else {
				PIECHART[i].EndAngle= Sum? (double)valuearray[i]*360/Sum+PIECHART[i].StartAngle : 0 ;
			}
		}
		SaveFlag=FALSE;
		RedrawClient();
		return TRUE;
	}
	return FALSE;
}

void PieChart::Pie(JKDC& dc,uint16 index)
{
	if(index<ValueCount){
		PIESTRUCT&st =PIECHART[index];
		if(ValueArray[index]){
			if(SaveFlag&&index==SaveNumber)	dc.SetTextColor(Black);
			else dc.SetTextColor(st.Color);
			dc.SetBrush(dc.GetStdBrush(st.Brush));
			dc.Pieslice(st.Pos,st.StartAngle,st.EndAngle,Radius,st.Color);
		}
	}
}

void PieChart::ChangeBkColor(BYTE bkcolor)
{
	if(bkcolor<16){
		BackColor=bkcolor;
		int16 color;
		for(uint16 i=0;i<ValueCount;i++)	{
			color=15-i%15;
			if(BackColor>=color) PIECHART[i].Color=(color-1+16)%16;
			else PIECHART[i].Color=color;
		}
	}
}

void PieChart::RedrawChart(uint16 number)
{
	if(number<ValueCount) {
		if((SaveFlag&&number!=SaveNumber)||!SaveFlag) {
			if(SaveFlag) PIECHART[SaveNumber].Pos=SavePos;

			SavePos=PIECHART[number].Pos;
			SaveNumber=number;
			int16 radius=Radius/3;
			int16 x=int16(radius*cos((double)(PIECHART[number].StartAngle+PIECHART[number].EndAngle)*M_PI/360.)+0.5);
			int16 y=-int16(radius*sin((double)(PIECHART[number].StartAngle+PIECHART[number].EndAngle)*M_PI/360.)+0.5);
			JKPoint pos=JKPoint(x,y);
			PIECHART[number].Pos+=pos;
			SaveFlag=TRUE;
		}
	}
	else{
		if(SaveFlag) {
			PIECHART[SaveNumber].Pos=SavePos;
			SaveFlag=FALSE;
		}
	}
	RedrawClient();
}

