#include "eqptable.h"
#include "jkbutton.h"

EqpTable::EqpTable(WORD ctrlid,BombManager *cur,InwonManager *inga,JKControl *parent):
	JKControl(JKRect(2,75,829,504),ctrlid,parent),
	CurrentBomb(cur),IngaInwon(inga),
	ShowFirstIndex(0),SelectLine(-1),SelectKind(-1)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));

	BackColor=Gray;
	rect.top+=43;
	rect.left=100;
	rect.right=rect.left+90;
	rect.bottom=rect.top+33;

	for(int16 i=0;i<8;i++) {
		CellRegion.PutRect(rect);
		rect.top=rect.bottom+1;
		rect.bottom=rect.top+33;
		for(int16 j=0;j<10;j++) {
			CellRegion.PutRect(rect);
			rect.top=rect.bottom+1;
			rect.bottom=rect.top+33;
		}
		rect.left=rect.right+1;
		rect.right=rect.left+90;
		rect.top=43;
		rect.bottom=rect.top+33;
	}
}

EqpTable::~EqpTable()
{}

void EqpTable::SetupWindow()
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	JKButton *btn;
	rect.right=rect.left+40;
	rect.top+=76;rect.bottom=rect.top+67;
	btn=new JKButton(rect,ID_BUTTON+2);
	btn->SetText("HQ");
	btn->BackColor=Blue;
	btn->TextColor=LtGray;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.top=rect.bottom+1;rect.bottom=rect.top+67;
	btn=new JKButton(rect,ID_BUTTON+3);
	btn->SetText("A");
	btn->BackColor=Red;
	btn->TextColor=LtGray;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.top=rect.bottom+1;rect.bottom=rect.top+67;
	btn=new JKButton(rect,ID_BUTTON+4);
	btn->SetText("B");
	btn->BackColor=Yellow;
	btn->TextColor=LtGray;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.top=rect.bottom+1;rect.bottom=rect.top+67;
	btn=new JKButton(rect,ID_BUTTON+5);
	btn->SetText("C");
	btn->BackColor=Cyan;
	btn->TextColor=LtGray;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.top=rect.bottom+1;rect.bottom=rect.top+67;
	btn=new JKButton(rect,ID_BUTTON+6);
	btn->SetText("‰");
	btn->BackColor=LtMagenta;
	btn->TextColor=LtGray;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.top=43;rect.bottom=rect.top+33;
	rect.left=0;rect.right=99;
	btn=new JKButton(rect,ID_BUTTON+1);
	btn->SetText("¶w•¡:¸åÁA");
	btn->BackColor=Yellow;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.top=rect.bottom+1;rect.bottom=rect.top+33;
	rect.left=41;rect.right=rect.left+58;
	for (int16 i=0;i<5;i++) {
		btn=new JKButton(rect,0);
		btn->SetEnable(FALSE);
    btn->SetText("·¥ˆa");
		btn->Depth=1;
		AddControl(btn);
		rect.top=rect.bottom+1;
		rect.bottom=rect.top+33;
		btn=new JKButton(rect,0);
		btn->SetEnable(FALSE);
    btn->SetText("¥¡·A");
		btn->Depth=1;
		AddControl(btn);
		rect.top=rect.bottom+1;
		rect.bottom=rect.top+33;
	}

	rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.bottom=40;
	rect.left=rect.right-45;
	btn=new JKButton(rect,ID_BUTTON+10);
	btn->SetText(">>");
	btn->BackColor=LtGray;
	btn->TextColor=Red;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.right=rect.left-1;
	rect.left=rect.right-45;
	btn=new JKButton(rect,ID_BUTTON+9);
	btn->SetText("<<");
	btn->BackColor=LtGray;
	btn->TextColor=Red;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.right=rect.left-1;
	rect.left=rect.right-45;
	btn=new JKButton(rect,ID_BUTTON+8);
	btn->SetText(">");
	btn->BackColor=LtGray;
	btn->TextColor=Red;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);

	rect.right=rect.left-1;
	rect.left=rect.right-45;
	btn=new JKButton(rect,ID_BUTTON+7);
	btn->SetText("<");
	btn->BackColor=LtGray;
	btn->TextColor=Red;
	btn->WinAttr|=WA_NOHASFOCUS;
	btn->Depth=1;
	AddControl(btn);
}

void EqpTable::PaintClient(JKDC& dc,JKRect&)
{
	for(int16 i=0;i<88;i++) {
		DrawCell(dc,i);
	}
}

void EqpTable::DrawCell(JKDC& dc,uint16 index)
{

	RectContainer *container=CellRegion.GetNode(index);
	if(container){
		uint16 Line=index/11;
		Line+=ShowFirstIndex;

		BOOL IsSelected=IsSelectedCell(index+11*ShowFirstIndex);
		dc.Box3D(container->Rect,1,IsSelected? Black:LtGray);
		dc.IsOverWrite=FALSE;

		uint16 entbufsize=10;
		uint16 inwonbufsize=9;
		uint16 len=max(inwonbufsize,entbufsize);
		BYTE *Buffer = new BYTE[len+1];
		Buffer[0]=0;

		BombBase *curbomb=CurrentBomb->GetNode(Line);
		if(curbomb){
			uint16 Kind=index%11;
			if(!Kind){
				strncpy(Buffer,curbomb->Text,entbufsize);
				Buffer[entbufsize]=0;
				if(!curbomb->IsEmpty()) dc.SetTextColor(IsSelected? White:LtBlue);
				else           dc.SetTextColor(IsSelected? Gray:Gray);
			}
			else{
				InwonBase *inganode=IngaInwon? IngaInwon->GetInwonBaseByText(curbomb->Text):0;
				uint16 inwoncount=0;
				if((Kind-1)/2==4){
					if((Kind-1)%2) {
						for(uint16 i=0;i<4;i++)
							inwoncount+= curbomb->BombCount[i];
					}
					else if(!((Kind-1)%2)&&inganode) {
						for(uint16 i=0;i<4;i++)
							inwoncount+= inganode->InwonCount[i];
					}
				}
				else{
					if((Kind-1)%2) inwoncount= curbomb->BombCount[(Kind-1)/2];
					else if(!((Kind-1)%2)&&inganode) inwoncount= inganode->InwonCount[(Kind-1)/2];
				}
				sprintf(Buffer,"%5d",inwoncount);
				if(inwoncount) dc.SetTextColor(IsSelected? White:LtBlue);
				else           dc.SetTextColor(IsSelected? Gray:Gray);
			}
			dc.TextOutX(container->Rect.ExpandBy(-2),Buffer,ADJ_XYCENTER);
			delete[] Buffer;
		}
	}
}

BOOL EqpTable::IsSelectedCell(uint16 index)
{
	uint16 Line=index/11,Kind=index%11;
	if(Kind) Kind=(Kind-1)/2+1;
	if(Line>=CurrentBomb->NodeCount) return FALSE;
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

int16 EqpTable::CheckCellIndex(JKPoint p)
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

void EqpTable::EvButtonClick(WORD ctrlid)
{
	uint16 selkind;
	switch(ctrlid){
		case ID_BUTTON+1:
			RedrawTable();
			break;
		case ID_BUTTON+2:
		case ID_BUTTON+3:
		case ID_BUTTON+4:
		case ID_BUTTON+5:
		case ID_BUTTON+6:
			selkind=ctrlid-(ID_BUTTON+2)+1;
			ChangeSelectCell(-1,selkind);
			break;
		case ID_BUTTON+7:
			SetShowFirstIndex(ShowFirstIndex-1);
			break;
		case ID_BUTTON+8:
			SetShowFirstIndex(ShowFirstIndex+1);
			break;
		case ID_BUTTON+9:
			SetShowFirstIndex(ShowFirstIndex-7);
			break;
		case ID_BUTTON+10:
			SetShowFirstIndex(ShowFirstIndex+7);
			break;
		default:
			break;
	}
}

void EqpTable::EvLeftClick(JKPoint& p)
{
	int16 index=CheckCellIndex(p);
	if(index>=0){
		if(!(index%11)) ChangeSelectCell(index/11+ShowFirstIndex,-1);
		else ChangeSelectCell(index/11+ShowFirstIndex,(index%11+1)/2);
	}
}

void EqpTable::ChangeSelectCell(int16 line,int16 kind)
{
	if(line>=(int16)CurrentBomb->NodeCount) return;

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
}

BOOL EqpTable::SetShowFirstIndex(int16 index)
{
	if(index<0)
		index=0;
	else if(index+8>=CurrentBomb->NodeCount)
		index=((int16)CurrentBomb->NodeCount-8)>0? ((int16)CurrentBomb->NodeCount-8):0;

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

void EqpTable::RedrawCell(JKDC& dc,int16 line,int16 kind)
{
	uint16 stline,endline, stkind,endkind;
	if(line>=ShowFirstIndex&&line<=ShowFirstIndex+7) stline=endline=line-ShowFirstIndex;
	else{	stline=0;	endline=7; }
	if(kind>=0&&kind<=4) stkind=endkind=kind;
	else{	stkind=0;	endkind=10; }

	MouseEvents->HideCursor();
	uint16 i,j;
	for(i=stline;i<=endline;i++)
		for(j=stkind;j<=endkind;j++)
			DrawCell(dc,i*11+j);
	MouseEvents->ShowCursor();
}

void EqpTable::RedrawTable()
{
	 ChangeSelectCell(-1,-1);
	 JKDC *dc=CreateClientDC();
	 if(dc) {
		MouseEvents->HideCursor();
		RedrawCell(*dc);
		MouseEvents->ShowCursor();
	 }
}
