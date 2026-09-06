#include "numentry.h"
#include "seltgdlg.h"
#include "tgrefctl.h"

TargetRefEntryControl::TargetRefEntryControl(JKPoint& p,WORD ctlid,TargetRefEntry& ent):
	EntryControl(JKRect(0,0,200,200).OffsetBy(p),ctlid,ent)
{}

TargetRefEntryControl::~TargetRefEntryControl()
{}

void TargetRefEntryControl::PaintClient(JKDC& dc,JKRect& )
{
	JKRect rect=ClientRect.SetLeftTopBy();
	dc.Rectangle3D(rect,-1);
	rect.Expand(-1);
	dc.IsOverWrite=FALSE;

	TargetRefEntry& ent= (TargetRefEntry&)RefEntry;
	if(!ent.GetTargetRecord(SelectTarget)) {
		dc.SolidBar(rect,Gray);
    dc.SetTextColor(Yellow);
    dc.TextOutX(rect,"Ð”w Îa¸â·¡\n"
                     "¹¥¸Ða»¡   \n"
                     "´g¯s“¡”a.  ",ADJ_XYCENTER);
		return;
	}

	JKPoint p(5,5);
	dc.TextOutX(JKRect(  0,  0, 80, 20).OffsetBy(p),"Îa¸â¤åÑ¡:",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(  0, 25, 80, 45).OffsetBy(p),"Îa¸â¹ÁÎa:",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(  0, 50, 80, 70).OffsetBy(p),"Îa¸â¬÷»©:",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(  0, 75, 80, 95).OffsetBy(p),"Îa    ‰¡:",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(  0,100, 80,120).OffsetBy(p),"§¡    ‰¡:",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(  0,125, 80,145).OffsetBy(p),"¸å Ê b:",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(170,125,180,145).OffsetBy(p),"%",ADJ_RIGHT|ADJ_YCENTER);
	dc.TextOutX(JKRect(  0,150, 80,180).OffsetBy(p),"”e ” Ñ¡:",ADJ_RIGHT|ADJ_YCENTER);

	dc.TextOutX(JKRect(85,  0,180, 20).OffsetBy(p),SelectTarget.GetEntry((uint16)0)->DataBuffer,ADJ_YCENTER,TRUE);
	dc.TextOutX(JKRect(85, 25,180,45).OffsetBy(p),SelectTarget.GetEntry(1)->DataBuffer,ADJ_YCENTER,TRUE);
	dc.TextOutX(JKRect(85, 50,180,70).OffsetBy(p),SelectTarget.GetEntry(2)->DataBuffer,ADJ_YCENTER,TRUE);
	dc.TextOutX(JKRect(85, 75,180,95).OffsetBy(p),SelectTarget.GetEntry(3)->DataBuffer,ADJ_YCENTER,TRUE);
	dc.TextOutX(JKRect(85,100,180,120).OffsetBy(p),SelectTarget.GetEntry(4)->DataBuffer,ADJ_YCENTER,TRUE);
	dc.TextOutX(JKRect(85,125,170,145).OffsetBy(p),SelectTarget.GetEntry(5)->DataBuffer,ADJ_YCENTER,TRUE);

	NumberEntry& dandent=(NumberEntry&)*SelectTarget.GetEntry(6);
	int32 resid;
	dandent.GetNumber(resid);
	ResourceBase *res=ResMan->GetResource(resid);
	if(res){
		BYTE *image = res->AllocRealData();
		if(image)	{
			dc.PutImage(JKRect(85,150,180,180).OffsetBy(p).Adjust(ADJ_XYCENTER,JKPoint(24,24)),image+24*24+4,AND_PUT,rect);
			dc.PutImage(JKRect(85,150,180,180).OffsetBy(p).Adjust(ADJ_XYCENTER,JKPoint(24,24)),image,OR_PUT,rect);
			res->FreeRealData(image);
		}
	}
}

void TargetRefEntryControl::EvLeftClick(JKPoint& )
{
	int32 index;
	TargetRefEntry& ent= (TargetRefEntry&)RefEntry;
	ent.GetNumber(index);
	if(SelectTargetDialog(JKPoint(0,0),index,this).Run()==ID_OK){
		ent.SetNumber(index);
		if(ent.GetTargetRecord(SelectTarget))	RedrawWindow();
	}
}

void TargetRefEntryControl::ModifyEntry()
{}

void TargetRefEntryControl::ReLoad()
{
	RedrawClient();
}

