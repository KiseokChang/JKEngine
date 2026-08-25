#include "edanddlg.h"
#include "danddlg.h"
#include "dandctrl.h"

DandaehoEntryControl::DandaehoEntryControl(JKRect& rect,WORD ctlid,DandaehoEntry& ent):
	EntryControl(rect,ctlid,ent)
{}
DandaehoEntryControl::~DandaehoEntryControl()
{}

void DandaehoEntryControl::PaintClient(JKDC& dc,JKRect& )
{
	JKRect rect=ClientRect.SetLeftTopBy();
	dc.Rectangle3D(rect,-1);
	rect.Expand(-1);

	DandaehoEntry& ent= (DandaehoEntry&)RefEntry;
	int32 resid;
	ent.GetNumber(resid);
	ResourceBase *res=ResMan->GetResource(resid);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(rect.Adjust(ADJ_XYCENTER,JKPoint(24,24)),image+24*24+4,AND_PUT,rect);
			dc.PutImage(rect.Adjust(ADJ_XYCENTER,JKPoint(24,24)),image,OR_PUT,rect);
			res->FreeRealData(image);
		}
	}
}

void DandaehoEntryControl::EvLeftClick(JKPoint& )
{
	WORD resid;
	DandaehoEntry& ent= (DandaehoEntry&)RefEntry;
	JKDialog *dlg;
	if(ent.ResKind==1)	dlg = new EDandaehoDialog(JKPoint(0,0),resid,this);
	else						dlg = new DandaehoDialog(JKPoint(0,0),resid,this);
	if(dlg){
		if(dlg->Run()==ID_OK){
			int32 temp=resid;
			ent.SetNumber(temp);
			RedrawClient();
			SendControlMessage(ENTRYCTRL_ENDMODIFY);
			delete dlg;
      return;
		}
		delete dlg;
	}
	SendControlMessage(ENTRYCTRL_CANCELMODIFY);
}

void DandaehoEntryControl::ModifyEntry()
{}

void DandaehoEntryControl::ReLoad()
{
	RedrawClient();
}

