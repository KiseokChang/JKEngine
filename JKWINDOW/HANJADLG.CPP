#include "hanjadlg.h"

HanjaSelectDialog::HanjaSelectDialog(JKPoint& p,WORD& hangulcode):
	JKDialog(JKRect(p,p+JKPoint(298,83))),Position(0),HanjaCodeSet(0),HanjaCode(hangulcode)
{
	WinForm=0x01;
	BackColor=LtGray;
	int16 index = HanMan->HangulCode2HanjaIndex(hangulcode);
	if(index>=0) {
		MaxHanjaCount = HanMan->RefHanCode(index,1);
		HanjaCodeSet = new WORD[MaxHanjaCount];
		if(HanjaCodeSet){
			for(WORD i=0;i<MaxHanjaCount;i++)
				HanjaCodeSet[i] = HanMan->HanjaPos2HanjaCode(HanMan->RefHanCode(index,2)+i);
			Position=0;
		}
		else EventHandler::CreationError=JKWindow::CreationError=TRUE;
	}
	else EventHandler::CreationError=JKWindow::CreationError=TRUE;
}

HanjaSelectDialog::~HanjaSelectDialog()
{
	if(HanjaCodeSet) delete[] HanjaCodeSet;
}

void HanjaSelectDialog::PaintClient(JKDC& dc,JKRect&)
{
	WORD i,x,y;
	dc.SetTextColor(TextColor);
	dc.SetBackColor(BackColor);
	for(i=0;i<MaxHanjaCount;i++){
		x=i%16; y=i/16;
		if(i==Position){
			dc.SetTextColor(BackColor);
			dc.SetBackColor(TextColor);
			dc.PutCh(JKPoint(4+x*18,4+y*18),HanjaCodeSet[i]);
			dc.SetTextColor(TextColor);
			dc.SetBackColor(BackColor);
		}
		else{
			dc.PutCh(JKPoint(4+x*18,4+y*18),HanjaCodeSet[i]);
		}
	}
}

void HanjaSelectDialog::EvCharDown(WORD key,WORD)
{
	switch(key){
		case ESC:
			SendMessage(ID_CANCEL);
			break;
		case ENTER:
			SendMessage(ID_OK);
			break;
		default:
			break;
	}
}

void HanjaSelectDialog::EvKeyDown(WORD key,WORD)
{
	switch(key){
		case RIGHTARROW:
			Position=(Position+1)%MaxHanjaCount;
			break;
		case LEFTARROW:
			Position=(Position-1+MaxHanjaCount)%MaxHanjaCount;
			break;
		case UPARROW:
			Position-= (int16)(Position-16)<0? 0 : 16;
			break;
		case DOWNARROW:
			Position+= (Position+16)<MaxHanjaCount? 16 : 0;
			break;
		default:
			break;
	}
	if(key==RIGHTARROW||key==LEFTARROW||key==UPARROW||key==DOWNARROW){
		JKDC *dc=CreateClientDC();
		if(dc) {
			MouseEvents->HideCursor();
			PaintClient(*dc,JKRect());
			MouseEvents->ShowCursor();
		}
	}

}

void HanjaSelectDialog::EvDlgOk()
{
	HanjaCode=HanjaCodeSet[Position];
}

