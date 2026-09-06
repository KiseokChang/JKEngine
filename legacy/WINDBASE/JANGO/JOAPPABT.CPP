#include "joappabt.h"

JOAppAboutControl::JOAppAboutControl(JKRect& rect,WORD ctrlid,WORD resid,JKControl *parent):
 JKControl(rect,ctrlid,parent),ResourceID(resid)
{
	BackColor=White;
	TextColor=LtBlue;
}

JOAppAboutControl::~JOAppAboutControl()
{
}


JKRect JOAppAboutControl::CalcClientRect()
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	rect.top+=70;
	rect.left+=10;
	rect.right-=10;
	rect.bottom-=30;
	return rect;
}

void JOAppAboutControl::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect;
	rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.Box3D(rect,1,LtGray);
	rect.Expand(-4);
	dc.Rectangle3D(rect,-1);
	rect.Expand(-1);
	rect.bottom=rect.top+60;
	dc.Rectangle(rect,Black);
	dc.PutIMG("log60.img",JKPoint(20,10));

	dc.IsOverWrite=FALSE;
	dc.SetTextColor(Gray);
	dc.TextOut(JKPoint(90+1,20+1),"Í¡¥w ¸å¬eÑÁ");
	dc.SetTextColor(TextColor);
	dc.TextOut(JKPoint(90,20),"Í¡¥w ¸å¬eÑÁ");

	dc.SetTextColor(Gray);
	dc.TextOut(JKPoint(90+1,40+1),"21­A‹¡ ‰ÁÐbÑÁ–E 2 Í¡¥w µa”e");
	dc.SetTextColor(TextColor);
	dc.TextOut(JKPoint(90,40),"21­A‹¡ ‰ÁÐbÑÁ–E 2 Í¡¥w µa”e");
}

void JOAppAboutControl::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	if(ResourceID){
		ResourceBase *res = ResMan->GetResource(ResourceID,RES_STRING);
		if(res){
			BYTE *data = res->AllocRealData();
			if(data)	{
				dc.SetTextColor(TextColor);
				dc.SetBackColor(BackColor);
				dc.TextOutX(rect,data,ADJ_LEFT);
				res->FreeRealData(data);
			}
		}
	}
	else DrawAbout(dc);
}

void JOAppAboutControl::DrawAbout(JKDC& dc)
{

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top=rect.bottom-59;
	dc.Box3D(rect,-2,Black);
	dc.IsOverWrite=FALSE;
	dc.SetTextColor(White);
	dc.TextOutX(rect,"·¡‰a ‘½‰¡ °a¶“e           \n\n"
									 "        ñœòœ÷±ì­· í:ïÌ÷oéB",ADJ_XYCENTER);

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.top+=10;
	rect.bottom-=80;
	dc.PutIMG("orglog.img",JKPoint(80,5),rect);

}


void JOAppAboutControl::SetResourceID(WORD resid)
{
	if(ResourceID!=resid){
		ResourceID=resid;
		RedrawClient();
	}
}
