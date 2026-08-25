#include "resbttn.h"
#include "danddlg.h"

DandaehoDialog::DandaehoDialog(JKPoint& p,WORD& resid,JKControl *parent):
	JKDialog(JKRect(p,p+JKPoint(7*25+6,10*25+6)),parent),ResID(resid)
{
	WinForm=0x01;
}

DandaehoDialog::~DandaehoDialog()
{}

void DandaehoDialog::PaintClient(JKDC& dc,JKRect&)
{
	for(uint16 i=0;i<50;i++){
		ResourceBase *res=ResMan->GetResource(0x4000+0x200+i);
		if(res){
			BYTE *image = res->AllocRealData();
			if(image)	{
				dc.PutImage(JKPoint((i/10)*25,(i%10)*25),image+24*24+4,AND_PUT);
				dc.PutImage(JKPoint((i/10)*25,(i%10)*25),image,OR_PUT);
				res->FreeRealData(image);
			}
		}
	}
	for(i=0;i<6;i++){
		ResourceBase *res=ResMan->GetResource(0x4000+0x200+50+i);
		if(res){
			BYTE *image = res->AllocRealData();
			if(image)	{
				dc.PutImage(JKPoint(5*25,i*25),image+24*24+4,AND_PUT);
				dc.PutImage(JKPoint(5*25,i*25),image,OR_PUT);
				res->FreeRealData(image);
			}
		}
	}
	for(i=0;i<5;i++){
		ResourceBase *res=ResMan->GetResource(0x4000+0x200+56+i);
		if(res){
			BYTE *image = res->AllocRealData();
			if(image)	{
				dc.PutImage(JKPoint(6*25,i*25),image+24*24+4,AND_PUT);
				dc.PutImage(JKPoint(6*25,i*25),image,OR_PUT);
				res->FreeRealData(image);
			}
		}
	}
}

void DandaehoDialog::EvLeftClick(JKPoint& p)
{
	int16 GridX=p.x/25,GridY=p.y/25;
	if(GridX>=0&&GridX<5){
		if(GridY>=0&&GridY<10) {
			ResID=0x4000+0x200+GridX*10+GridY;
			SendMessage(ID_OK);
		}
	}
	else if(GridX==5&&GridY>=0&&GridY<6) {
		ResID=0x4000+0x200+5*10+GridY;
		SendMessage(ID_OK);
	}
	else if(GridX==6&&GridY>=0&&GridY<5) {
		ResID=0x4000+0x200+5*10+6+GridY;
		SendMessage(ID_OK);
	}
}

void DandaehoDialog::EvNcLeftClick(JKPoint& )
{
	SendMessage(ID_CANCEL);
}

