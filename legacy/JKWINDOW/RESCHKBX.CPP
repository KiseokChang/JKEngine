#include "reschkbx.h"

ResCheckBox::ResCheckBox(JKRect rect,WORD ctrlid,WORD trueres,WORD falseres):
	JKCheckBox(rect,ctrlid),TrueResID(trueres),FalseResID(falseres)
{
	WinRect=rect;
}

ResCheckBox::~ResCheckBox()
{}

JKRect ResCheckBox::CalcClientRect()
{
	return JKControl::CalcClientRect();
}

void ResCheckBox::PaintClient(JKDC& dc,JKRect&)
{
	ResourceBase *res=ResMan->GetResource(Status? TrueResID:FalseResID);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(JKPoint(0,0),image,COPY_PUT);
			res->FreeRealData(image);
		}
	}
}

void ResCheckBox::SetCheckResource(WORD trueres,WORD falseres)
{
	if((int)trueres>=0) TrueResID = trueres;
	if((int)falseres>=0) FalseResID = falseres;
}

void ResCheckBox::EvSetFocus(DWORD lostid)
{
	JKControl::EvSetFocus(lostid);
}
void ResCheckBox::EvKillFocus(DWORD gainid)
{
	JKControl::EvKillFocus(gainid);
}
