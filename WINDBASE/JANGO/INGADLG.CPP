#include "joglobal.h"
#include "jkbutton.h"
#include "scrllist.h"
#include "jkedit.h"
#include "ingadlg.h"

IngaDialog::IngaDialog(uint16 len,char *fname):
	JKDialog(JKRect(140,100,500,380)),
	IngaMan(len),IngaBuffer(len)
{
	SetText("·¥ˆa ·¥¶¥ ¥e‰w");
	if(fname){
		strncpy(FileName,fname,12);
		FileName[12]=0;
	}
	else{
		strcpy(FileName,BudaeName);
		strcat(FileName,"jtk.dat");
	}
}
IngaDialog::~IngaDialog()
{}

void IngaDialog::InitWindow()
{
	JKDialog::InitWindow();
	IngaMan.GetInwon(FileName);
	JKListBox *listbox= (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		char Buffer[30];
		InwonBase *base=IngaMan.HeadNode;
		while(base){
			MakeListString(Buffer,*base);
			listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(Buffer));
			base=base->next;
		}
		listbox->RedrawWindow();
	}

}

void IngaDialog::SetupWindow()
{
	JKButton *bttn;
	bttn=new JKButton(JKRect(10,220,80,240),ID_BUTTON+1);
	bttn->SetText("Âˆa");
	AddControl(bttn);
	bttn=new JKButton(JKRect(90,220,160,240),ID_BUTTON+2);
	bttn->SetText("¬b¹A");
	AddControl(bttn);
	bttn=new JKButton(JKRect(170,220,240,240),ID_BUTTON+3);
	bttn->SetText("ÑÂ·¥");
	AddControl(bttn);
	bttn=new JKButton(JKRect(250,220,320,240),ID_BUTTON+4);
	bttn->SetText("Âá­¡");
	AddControl(bttn);

	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=10;
	rect.right-=10;
	rect.top+=10;
	rect.bottom-=100;
	ScrollListBox *scrllist = new ScrollListBox(rect,ID_LISTBOX+1);
	scrllist->DragScrollFlag=TRUE;
	AddControl(scrllist);

	rect.top=rect.bottom+40;
	rect.bottom=rect.top+19;

	JKEdit *edit;
	edit = new JKEdit(JKRect(10,rect.top,80,rect.bottom),ID_EDIT,0,8,FALSE,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);
	edit = new JKEdit(JKRect(100,rect.top,140,rect.bottom),ID_EDIT+1,0,3,FALSE,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);
	edit = new JKEdit(JKRect(150,rect.top,190,rect.bottom),ID_EDIT+2,0,3,FALSE,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);
	edit = new JKEdit(JKRect(200,rect.top,240,rect.bottom),ID_EDIT+3,0,3,FALSE,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);
	edit = new JKEdit(JKRect(250,rect.top,290,rect.bottom),ID_EDIT+4,0,3,FALSE,FALSE);
	edit->SetInsertMode(FALSE);
	AddControl(edit);

}


void IngaDialog::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	rect.left=10;
	rect.right-=10;
	rect.top+=10;
	rect.bottom-=100;

	rect.top=rect.bottom+20;
	rect.bottom=rect.top+19;

	dc.SetBackColor(BackColor);
	dc.SetTextColor(LtMagenta);

	dc.TextOutX(JKRect(10,rect.top,80,rect.bottom),"ºËb‹¡",ADJ_XYCENTER);
	dc.TextOutX(JKRect(100,rect.top,140,rect.bottom),"¥¥¦",ADJ_XYCENTER);
	dc.TextOutX(JKRect(150,rect.top,190,rect.bottom),"´iÌa",ADJ_XYCENTER);
	dc.TextOutX(JKRect(195,rect.top,245,rect.bottom),"§aœa¥¡",ADJ_XYCENTER);
	dc.TextOutX(JKRect(250,rect.top,290,rect.bottom),"À¡Ÿ¡",ADJ_XYCENTER);
}

void IngaDialog::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid){
		case ID_BUTTON+1:
			SetInga();
			break;
		case ID_BUTTON+2:
			DeleteInga();
			break;
		case ID_BUTTON+3:
			SendMessage(ID_OK);
			break;
		case ID_BUTTON+4:
			SendMessage(ID_CANCEL);
			break;
		default:
			break;
	}
}

void IngaDialog::EvLBTouchEntry(WORD /*ctrlid*/,uint16 index)
{
	InwonBase *base=IngaMan.GetNode(index);
	if(base) {
		IngaBuffer.Assign(*base);
		SetEditControl();
	}
}

void IngaDialog::EvEditFinish(WORD ctrlid)
{
	JKEdit *edit = (JKEdit *)GetControl(ctrlid);
	if(edit){
		if(ctrlid==ID_EDIT){
			IngaBuffer.SetText(edit->GetText());
		}
		else{
			int16 n = atoi(edit->GetText());
			if(n<0) n=0;
			IngaBuffer.InwonCount[ctrlid-(ID_EDIT+1)]=n;
			char Buffer[4];
			sprintf(Buffer,"%-3d",n);
			edit->SetText(Buffer);
		}
	}
}

void IngaDialog::EvDlgOk()
{
	IngaMan.PutInwon(FileName);
}

void IngaDialog::EvDlgCancel()
{
}


void IngaDialog::SetEditControl()
{
	uint16 n;
	JKEdit *edit = (JKEdit *)GetControl(ID_EDIT);
	if(edit){
		edit->SetText(IngaBuffer.Text);
	}
	char Buffer[4];
	for(uint16 i=0;i<4;i++){
		edit = (JKEdit *)GetControl(ID_EDIT+1+i);
		if(edit){
			n=IngaBuffer.InwonCount[i];
			sprintf(Buffer,"%-3d",n);
			edit->SetText(Buffer);
		}
	}
}

void IngaDialog::SetInga()
{
	if(IngaBuffer.IsEmpty()) return;
	JKListBox *listbox= (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		char Buffer[30];
		InwonBase *base = IngaMan.GetNode(&IngaBuffer,0x01);
		if(base) {
			MakeListString(Buffer,IngaBuffer);
			base->Assign(IngaBuffer);
			uint16 index=IngaMan.GetIndex(base);
			listbox->ChangeHuboBase(index,new HuboBase(Buffer));
			listbox->SetCurrentIndex(index);
			listbox->RedrawClient();
		}
		else {
			MakeListString(Buffer,IngaBuffer);
			IngaMan.PutNodeLast(IngaBuffer.Duplicate());
			listbox->Add(listbox->HuboManager::NodeCount,new HuboBase(Buffer));
			listbox->SetCurrentIndex(listbox->HuboManager::NodeCount-1);
			listbox->RedrawWindow();
		}
	}
}

void IngaDialog::DeleteInga()
{
	JKListBox *listbox= (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox){
		int16 index = listbox->CurrentIndex;
		if(index>=0){
			IngaMan.DeleteNode(index);
			listbox->Destroy(index);
			listbox->RedrawWindow();
		}
	}
}

BOOL IngaDialog::MakeListString(char *buffer,InwonBase& base)
{
	if(buffer){
		sprintf(buffer,"%-8s  %3d  %3d  %3d  %3d",base.Text,
						base.InwonCount[0],base.InwonCount[1],base.InwonCount[2],base.InwonCount[3]);
		return TRUE;
	}
	return FALSE;
}