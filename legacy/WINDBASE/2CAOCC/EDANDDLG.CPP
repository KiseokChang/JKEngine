#include "resbttn.h"
#include "edanddlg.h"

EDandaehoDialog::EDandaehoDialog(JKPoint& p,WORD& resid,JKControl *parent):
	JKDialog(JKRect(p,p+JKPoint(8*25+6,10*25+6)),parent),ResID(resid)
{
	WinForm=0x01;
}

EDandaehoDialog::~EDandaehoDialog()
{}

void EDandaehoDialog::PaintClient(JKDC& dc,JKRect&)
{
	for(uint16 i=0;i<80;i++){
		ResourceBase *res=ResMan->GetResource(0x4000+0x300+i);
		if(res){
			BYTE *image = res->AllocRealData();
			if(image)	{
				dc.PutImage(JKPoint((i/10)*25,(i%10)*25),image+24*24+4,AND_PUT);
				dc.PutImage(JKPoint((i/10)*25,(i%10)*25),image,OR_PUT);
				res->FreeRealData(image);
			}
		}
	}
}

void EDandaehoDialog::EvLeftClick(JKPoint& p)
{
	int16 GridX=p.x/25,GridY=p.y/25;
	if(GridX>=0&&GridX<8){
		if(GridY>=0&&GridY<10) {
			ResID=0x4000+0x300+GridX*10+GridY;
			SendMessage(ID_OK);
		}
	}
}

void EDandaehoDialog::EvNcLeftClick(JKPoint& )
{
	SendMessage(ID_CANCEL);
}

