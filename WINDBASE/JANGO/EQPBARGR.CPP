#include "eqpbargr.h"

EqpBarGr::EqpBarGr(WORD ctrid,BombManager *bomb,JKControl *parent):
	JKWindow(JKRect(0,510,1010,715),parent,ctrid),MaxValue(0),BombMan(bomb),
	Step(7),BarWidth(0),FirstShowIndex(0)
{
	WinForm=0x01;
	WinAttr=(BYTE)WA_FIRSTSHOW;
	BackColor=LtGray;
	CalBarSize();
}

void EqpBarGr::SetValue()
{
	BombBase *bombbase;

	CalBarSize();
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left+=50;rect.right=rect.left+(ClientRect.Width()-(50+(GRAPHCOUNT+1)*20))/GRAPHCOUNT;
	rect.top+=20;rect.bottom-=35;
	BarRect=rect;

	int16 width=BarRect.Width();
	BarWidth=(width-(5)*Step)/4;
}

void EqpBarGr::PaintClient(JKDC& dc,JKRect&)
{
	BombBase *BBase;
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect=BarRect;
	for(int16 i=0;i<GRAPHCOUNT;i++){
		dc.SolidBar(rect,Black);
		BBase=BombMan->GetNode(i+FirstShowIndex);
		if(BBase){
			DrawBar(dc,BBase,rect);
		}
		rect.Offset(JKPoint(rect.Width()+20,0));
	}
	char buf[5];
	rect.bottom=rect.top+20;
	rect.left=5;rect.right=rect.left+40;
	sprintf(buf,"%d",MaxValue);
	if(MaxValue)dc.TextOutX(rect,buf,ADJ_YCENTER|ADJ_RIGHT);

	rect.top=BarRect.Height()/2+10;rect.bottom=rect.top+20;
	sprintf(buf,"%d",MaxValue/2);
	if(MaxValue)dc.TextOutX(rect,buf,ADJ_YCENTER|ADJ_RIGHT);
	rect.top=BarRect.bottom-20;rect.bottom=rect.top+20;
	if(MaxValue)dc.TextOutX(rect,"0",ADJ_YCENTER|ADJ_RIGHT);
}

void EqpBarGr::CalBarSize()
{
	BombBase* bombbase;

	MaxValue=0;

	for(int16 i=0;i<GRAPHCOUNT;i++) {
		bombbase=BombMan->GetNode(i+FirstShowIndex);
		if(bombbase)
			for(int16 j=0;j<4;j++)
				if(MaxValue<bombbase->BombCount[j]) MaxValue=bombbase->BombCount[j];
	}
}

void EqpBarGr::DrawBar(JKDC& dc,BombBase* bombbase,JKRect rect)
{
	//rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	int16	BarHeight=rect.Height()-6;
	int16 Color,ratio;
	JKRect rect1;
	rect1.top=rect.bottom+5;rect1.bottom=rect.top+20;
	rect1.left=rect.left;rect1.right=rect.right;
	dc.SetTextColor(LtMagenta);
	dc.IsOverWrite=FALSE;
	dc.TextOutX(rect1,bombbase->Text,ADJ_XCENTER);

	dc.Rectangle3D(rect,2,LtGray,White);
	rect.top+=2;
	for(int16 i=0;i<4;i++) {
		if(MaxValue) ratio=(int32)bombbase->BombCount[i]*BarHeight/MaxValue;
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