#include "jkapp.h"
#include "resbttn.h"
#include "inwonctl.h"

InwonControl::InwonControl(JKPoint pos,WORD ctrlid,uint16 entlen,InwonManager *cur,InwonManager *inga,uint16 linecount,JKControl *parent):
	JKControl(JKRect(pos,pos+JKPoint(185+2+70*4-1,2+20*(linecount+2)-1)),ctrlid,parent),
	LineCount(linecount),EntryLen(entlen),CurrentInwon(cur),IngaInwon(inga),
	ShowFirstIndex(0),SelectLine(-1),SelectKind(-1)
{
	BackColor=LtGray;
	if(CurrentInwon){
		for(uint16 i=0;i<LineCount;i++){
			CellRegion.PutRect(JKRect(2   ,2+20*(i+2),     185-1,20*(i+3)-1));
			CellRegion.PutRect(JKRect(185+2     ,2+20*(i+2),185+70*1-1,20*(i+3)-1));
			CellRegion.PutRect(JKRect(185+2+70*1,2+20*(i+2),185+70*2-1,20*(i+3)-1));
			CellRegion.PutRect(JKRect(185+2+70*2,2+20*(i+2),185+70*3-1,20*(i+3)-1));
			CellRegion.PutRect(JKRect(185+2+70*3,2+20*(i+2),185+70*4-1,20*(i+3)-1));
		}
	}
	else CreationError=TRUE;
}

InwonControl::~InwonControl()
{}

void InwonControl::SetupWindow()
{
	JKButton *bttn;
	bttn=new JKButton(JKRect(2,2,185+70*4-1,20-1),ID_BUTTON+1);
	bttn->SetText(WinText);
	bttn->BackColor=LtBlue;
	bttn->TextColor=White;
	bttn->Depth=-1;
	bttn->WinAttr|=WA_NOHASFOCUS;
	AddControl(bttn);

	bttn=new JKButton(JKRect(2+1,2+20,185-35,2+20+15),ID_STATIC+1);
	bttn->SetEnable(FALSE);
	bttn->BackColor=White;
	bttn->TextColor=LtRed;
	bttn->Depth=-1;
	AddControl(bttn);

	bttn=new JKResButton(JKRect(185-1-15-16,2+20,185-1-16,2+20+15),ID_BUTTON+2,InternImageID[ArrowDown]);
	bttn->WinAttr|=WA_NOHASFOCUS;
	AddControl(bttn);
	bttn=new JKResButton(JKRect(185-1-15,2+20,185-1,2+20+15),ID_BUTTON+3,InternImageID[ArrowUp]);
	bttn->WinAttr|=WA_NOHASFOCUS;
	AddControl(bttn);
	bttn=new JKButton(JKRect(185+2,2+20*1,185+70*1-1,20*2-1),ID_BUTTON+4);
	bttn->SetText("HQ");
	bttn->Depth=-1;
	bttn->WinAttr|=WA_NOHASFOCUS;
	AddControl(bttn);
	bttn=new JKButton(JKRect(185+2+70*1,2+20*1,185+70*2-1,20*2-1),ID_BUTTON+5);
	bttn->SetText("A");
	bttn->Depth=-1;
	bttn->WinAttr|=WA_NOHASFOCUS;
	AddControl(bttn);
	bttn=new JKButton(JKRect(185+2+70*2,2+20*1,185+70*3-1,20*2-1),ID_BUTTON+6);
	bttn->SetText("B");
	bttn->Depth=-1;
	bttn->WinAttr|=WA_NOHASFOCUS;
	AddControl(bttn);
	bttn=new JKButton(JKRect(185+2+70*3,2+20*1,185+70*4-1,20*2-1),ID_BUTTON+7);
	bttn->SetText("C");
	bttn->Depth=-1;
	bttn->WinAttr|=WA_NOHASFOCUS;
	AddControl(bttn);
}

void InwonControl::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.Box3D(rect,1,BackColor);
	for(uint16 i=0;i<LineCount*5;i++){
		DrawCell(dc,i);
	}
	DisplaySelectSum();
}

void InwonControl::DrawCell(JKDC& dc,uint16 index)
{
	RectContainer *container=CellRegion.GetNode(index);
	if(container){
		uint16 Line=index/5;
		Line+=ShowFirstIndex;

		BOOL IsSelected=IsSelectedCell(index+5*ShowFirstIndex);
		dc.Box3D(container->Rect,-1,IsSelected? Black:White);
		dc.IsOverWrite=FALSE;

		InwonBase *node=CurrentInwon->GetNode(Line);
		if(node){
			uint16 entbufsize=EntryLen;
			uint16 inwonbufsize=7;
			uint16 len=max(inwonbufsize,entbufsize);
			BYTE *Buffer = new BYTE[len+1];
			Buffer[0]=0;

			InwonBase *inganode=IngaInwon? IngaInwon->GetInwonBaseByText(node->Text):0;
			uint16 Kind=index%5;
			if(!Kind){
				strncpy(Buffer,node->Text,entbufsize);
				Buffer[entbufsize]=0;
				if(!node->IsEmpty()) dc.SetTextColor(IsSelected? White:LtBlue);
				else           dc.SetTextColor(IsSelected? Gray:LtGray);
			}
			else{
				uint16 inwoncount=node->InwonCount[Kind-1];
				if(!inganode)
					sprintf(Buffer,"%3d",inwoncount);
				else
					sprintf(Buffer,"%3d/%3d",inwoncount,inganode->InwonCount[Kind-1]);
				if(inwoncount) dc.SetTextColor(IsSelected? White:LtBlue);
				else           dc.SetTextColor(IsSelected? Gray:LtGray);
			}
			dc.TextOutX(container->Rect.ExpandBy(-1),Buffer,ADJ_YCENTER,TRUE);
			delete[] Buffer;
		}
	}
}


BOOL InwonControl::IsSelectedCell(uint16 index)
{
	uint16 Line=index/5,Kind=index%5;
	if(Line>=CurrentInwon->NodeCount) return FALSE;
	BOOL IsSelLine = Line==SelectLine? TRUE:FALSE;
	BOOL IsSelKind = Kind==SelectKind? TRUE:FALSE;
	if((SelectLine!=-1)^(SelectKind!=-1)){
		if(IsSelLine||IsSelKind)
			return TRUE;
	}
	else if(SelectLine!=-1&&SelectKind!=-1){
		if(IsSelLine&&IsSelKind)
			return TRUE;
	}
	return FALSE;
}

int16 InwonControl::CheckCellIndex(JKPoint p)
{
	JKRect rect;
	uint16 count=0;
	RectContainer *container=CellRegion.HeadNode;
	while(container){
		rect =container->Rect;
		if(rect.IsContain(p)) return count;
		count++;
		container=container->next;
	}
	return -1;
}

void InwonControl::EvButtonClick(WORD ctrlid)
{
	uint16 selkind;
	switch(ctrlid){
		case ID_BUTTON+1:
      {
			ChangeSelectCell(-1,-1);
			JKDC *dc=CreateClientDC();
			if(dc) {
				MouseEvents->HideCursor();
				RedrawCell(*dc);
				MouseEvents->ShowCursor();
			}
			}
			break;
		case ID_BUTTON+2:
			SetShowFirstIndex(ShowFirstIndex+1);
			break;
		case ID_BUTTON+3:
			if(ShowFirstIndex) SetShowFirstIndex(ShowFirstIndex-1);
			break;
		case ID_BUTTON+4:
		case ID_BUTTON+5:
		case ID_BUTTON+6:
		case ID_BUTTON+7:
			selkind=ctrlid-(ID_BUTTON+4)+1;
			ChangeSelectCell(-1,selkind);
			break;
		default:
			break;
	}
}

void InwonControl::EvButtonPressing(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+2 || ctrlid==ID_BUTTON+3) EvButtonClick(ctrlid);
}

void InwonControl::EvLeftClick(JKPoint& p)
{
	int16 index=CheckCellIndex(p);
	if(index>=0){
		if(!(index%5)) ChangeSelectCell(index/5+ShowFirstIndex,-1);
		else ChangeSelectCell(index/5+ShowFirstIndex,index%5);
	}
}

void InwonControl::ChangeSelectCell(int16 line,int16 kind)
{
	if(line>=(int16)CurrentInwon->NodeCount) return;

	if(SelectLine!=line || SelectKind!=kind){
		SelectLine=line;
		SelectKind=kind;
		JKDC *dc=CreateClientDC();
		if(dc) {
			MouseEvents->HideCursor();
			RedrawCell(*dc);
			MouseEvents->ShowCursor();
		}
		SendControlMessage(INWON_SELECTCELL,0,MAKEDWORD(SelectLine,SelectKind));
	}
	else SendControlMessage(INWON_RESELECTCELL,0,MAKEDWORD(SelectLine,SelectKind));
}

BOOL InwonControl::SetShowFirstIndex(uint16 index)
{
	if(index+LineCount>=CurrentInwon->NodeCount)
		index=int16(CurrentInwon->NodeCount-LineCount)>0? CurrentInwon->NodeCount-LineCount:0;

	if(ShowFirstIndex!=index){
		ShowFirstIndex=index;
		JKDC *dc=CreateClientDC();
		if(dc) {
			MouseEvents->HideCursor();
			RedrawCell(*dc);
			MouseEvents->ShowCursor();
		}
		SendControlMessage(INWON_SETFIRSTINDEX,ShowFirstIndex);
		return TRUE;
	}
	return FALSE;
}

void InwonControl::RedrawCell(JKDC& dc,int16 line,int16 kind)
{
	uint16 stline,endline, stkind,endkind;
	if(line>=ShowFirstIndex&&line<=ShowFirstIndex+LineCount-1) stline=endline=line-ShowFirstIndex;
	else{	stline=0;	endline=LineCount-1; }
	if(kind>=0&&kind<=4) stkind=endkind=kind;
	else{	stkind=0;	endkind=4; }

	MouseEvents->HideCursor();
	uint16 i,j;
	for(i=stline;i<=endline;i++)
		for(j=stkind;j<=endkind;j++)
			DrawCell(dc,i*5+j);
	DisplaySelectSum();
	MouseEvents->ShowCursor();
}

void InwonControl::DisplaySelectSum()
{
	uint16 sum=0;
	if(CurrentInwon->NodeCount){
		uint16 stline,endline, stkind,endkind;
		if(SelectLine>=0&&SelectLine<=CurrentInwon->NodeCount-1) stline=endline=SelectLine;
		else{	stline=0;	endline=CurrentInwon->NodeCount-1; }
		if(SelectKind>=0&&SelectKind<=4) stkind=endkind=SelectKind;
		else{	stkind=0;	endkind=4; }

		uint16 i,j;
		for(i=stline;i<=endline;i++){
			InwonBase *node=CurrentInwon->GetNode(i);
			if(node){
				for(j=stkind;j<=endkind;j++){
					if(j) sum+=node->InwonCount[j-1];
				}
			}
		}
	}
	BYTE buffer[20];
	sprintf(buffer,"%5d ¡w",sum);
	JKControl *ctrl=GetControl(ID_STATIC+1);
	if(ctrl) {
		ctrl->TextColor = (SelectLine<0&&SelectKind<0)? Black:LtRed;
		ctrl->SetText(buffer);
		ctrl->RedrawClient();
	}
}
