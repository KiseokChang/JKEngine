#include "moonctrl.h"

MoonControl::MoonControl(JKRect& rect,WORD ctlid,JKControl *parent):
	JKButton(rect,ctlid,parent)
{
	Depth=-1;
	BackColor=Black;
	SetTimer(10);
	gettime(&CurrentTime);
}

void MoonControl::PaintClient(JKDC& dc,JKRect& )
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(!Status) dc.Rectangle3D(rect,Depth);
	else dc.Rectangle3D(rect,-Depth);
	rect.Expand(-abs(Depth));

	struct date Solar,Lunar;
	getdate(&Solar);
	SolLuCvt().SolToLuna(Solar,Lunar);

	WORD resid=0x4000+0x100+(Lunar.da_day? (Lunar.da_day<=30? Lunar.da_day:30):1)-1;

	ResourceBase *res=ResMan->GetResource(resid);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(rect.Adjust(ADJ_XYCENTER,JKPoint(24,24)),image,COPY_PUT,rect);
			res->FreeRealData(image);
		}
	}
}

void MoonControl::EvTimerHappen(DWORD /*happencount*/)
{
	struct time curtime;
	gettime(&curtime);
	if(CurrentTime.ti_hour>curtime.ti_hour)	RedrawClient();
	CurrentTime = curtime;
}

