#include "podaectl.h"
#include "prfcctrl.h"

PrefaceControl::PrefaceControl(JKRect& rect,WORD ctlid,BOOL kind,JKControl *parent):
	JKControl(rect,ctlid,parent),Kind(kind)
{
	BackColor=LtGray;
}

PrefaceControl::~PrefaceControl()
{}

void PrefaceControl::SetupWindow()
{
	PodaeControl *podaectrl;
	JKPoint p(5,40);
	podaectrl=new PodaeControl(JKRect(0,25*0,24,25*1-1).OffsetBy(p),0,Kind,0,2,0,0);
	AddControl(podaectrl);
	podaectrl=new PodaeControl(JKRect(0,25*1,24,25*2-1).OffsetBy(p),0,Kind,0,3,0,0);
	AddControl(podaectrl);
	podaectrl=new PodaeControl(JKRect(0,25*2,24,25*3-1).OffsetBy(p),0,Kind,0,1,0,0);
	AddControl(podaectrl);
	if(!Kind){
		podaectrl=new PodaeControl(JKRect(0,25*3,24,25*4-1).OffsetBy(p),0,Kind,0,1,1,1);
		AddControl(podaectrl);
		podaectrl=new PodaeControl(JKRect(0,25*4,24,25*5-1).OffsetBy(p),0,Kind,0,1,2,1);
		AddControl(podaectrl);
	}
}

void PrefaceControl::PaintClient(JKDC&dc,JKRect&)
{
	char *msg[7]={
		" < ¤ñ     >    ",
		"   ¬a‰b º—       ",
		"   ·¡•· º—       ",
		"   ¬a‰b ¦‰“w     ",
		"   ¬a‰b ˆa“w-¥¡É·",
		"   ¬a‰b ˆa“w-RAP ",
		"   ¬a‰b ”‹¡     "
	};

	JKRect rect=ClientRect.SetLeftTopBy();
	dc.Box3D(rect,-1,BackColor);

	dc.IsOverWrite=FALSE;

	dc.SetTextColor(Black);
	dc.TextOut(JKPoint(10+2,10+2),msg[0]);
	dc.SetTextColor(White);
	dc.TextOut(JKPoint(10,10),msg[0]);

	dc.SetTextColor(Black);
	for(uint16 i=1;i<3;i++)
		dc.TextOut(JKPoint(10,43+(i-1)*25),msg[i]);
	if(Kind){
		dc.TextOut(JKPoint(10,43+2*25),msg[6]);
	}
	else{
		for(uint16 i=3;i<6;i++)
			dc.TextOut(JKPoint(10,43+(i-1)*25),msg[i]);
	}
}
