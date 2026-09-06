#include "resbttn.h"

JKResButton::JKResButton(JKRect& rect,WORD ctlid,WORD resid,JKControl *parent):
	JKButton(rect,ctlid,parent),ResID(resid)
{
	Depth=0;
}

void JKResButton::DrawReleased(JKDC& dc)
{
	ResourceBase *res=ResMan->GetResource(ResID);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(JKPoint(0,0),image,COPY_PUT);
			res->FreeRealData(image);
		}
	}
}

void JKResButton::DrawPressed(JKDC& dc)
{
	ResourceBase *res=ResMan->GetResource(ResID+0x1000);
	if(!res) res = ResMan->GetResource(ResID);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(JKPoint(0,0),image,COPY_PUT);
			res->FreeRealData(image);
		}
	}
}

void JKResButton::DrawHasFocus(JKDC& dc)
{
	ResourceBase *res=ResMan->GetResource(ResID+0x2000);
	if(!res) res = ResMan->GetResource(ResID);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(JKPoint(0,0),image,COPY_PUT);
			res->FreeRealData(image);
		}
	}
}

void JKResButton::DrawDisable(JKDC& dc)
{
	ResourceBase *res=ResMan->GetResource(ResID+0x3000);
	if(!res) res = ResMan->GetResource(ResID);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(JKPoint(0,0),image,COPY_PUT);
			res->FreeRealData(image);
		}
	}
}

void JKResButton::PaintClient(JKDC& dc,JKRect&)
{
	if(!Status){
		if(!HasFocus()) DrawReleased(dc);
		else DrawHasFocus(dc);
	}
	else DrawPressed(dc);
}

//===========================================================

ResourceManager *ResMan=0;