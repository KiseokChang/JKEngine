#include "bargraph.h"

BarGraph::BarGraph(JKRect rect,BombBase* bombbase,WORD count,WORD maxvalue,WORD ctrid,JKControl *parent):
					JKControl(rect,ctrid,parent),MaxValue(maxvalue),Count(count),BBase(bombbase),Step(0)
{
	WinAttr=(BYTE)WA_FIRSTSHOW;
	BackColor=Black;
}

BarGraph::~BarGraph()
{
}

JKRect BarGraph::CalcClientRect()
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.bottom-=25;
	return rect;
}

void BarGraph::InitWindow()
{
	JKControl::InitWindow();
	CalBarSize();
}

void BarGraph::PaintClient(JKDC& dc,JKRect& /*paintrect*/)
{

	if(Count) DrawBar(dc);
}

void BarGraph::PaintWindow(JKDC& dc,JKRect& /*rect*/)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.SolidBar(rect,LtGray);
	rect.top=rect.bottom-24;
	rect.bottom=rect.top+22;
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(LtMagenta);
	if(BBase)dc.TextOutX(rect,BBase->Text,ADJ_XYCENTER);
}

void BarGraph::CalBarSize(int16 step)
{
	int16 width=ClientRect.Width();
	if(Count)	BarWidth=(width-(Count+1)*step)/Count;
	Step=step;
}

void BarGraph::SetBarSize(int16 barwidth,int16 ratio)
{
	if(!barwidth) CalBarSize();
	else BarWidth=barwidth;
	Ratio=ratio;
}

void BarGraph::SetValue(BombBase* bombbase,WORD maxvalue)
{
	BBase=bombbase;
	MaxValue=maxvalue;
}

void BarGraph::DrawBar(JKDC& dc)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	int16	BarHeight=ClientRect.Height()-6;
	int16 Color,ratio;

	dc.Rectangle3D(rect,2,LtGray,White);
	rect.top+=2;
	if(BBase){
		for(int16 i=0;i<Count;i++) {
			if(MaxValue) ratio=(int32)BBase->BombCount[i]*BarHeight/MaxValue;
			rect.left+=Step;rect.right=rect.left+BarWidth;
			rect.top=rect.bottom-ratio+1;
			if(i==0) Color=Blue;
			else if(i==1) Color=Red;
			else if(i==2) Color=Yellow;
			else if(i==3) Color=Cyan;
			//dc.SolidBar(rect,Color);
			dc.Box3D(rect,1,Color);
			rect.left=rect.right;rect.right=rect.left+BarWidth;
		}
	}
}