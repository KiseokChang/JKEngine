#include "joglobal.h"
#include "bltchart.h"
#include "fltbttn.h"
#include "scrllist.h"
#include "inwonctl.h"
#include "insamsg.h"
#include "incommon.h"

CommonCondWin::CommonCondWin(JKRect rect,WORD ctrlid,WORD entryid,PersonManager& recman,WORD *& srcharray,uint16& srchcount):
	JKWindow(rect,0,ctrlid),CondEntryID(entryid),RecordMan(recman),SearchArray(srcharray),SearchCount(srchcount),
	CommonMan(0),IngaMan(0)
{
	InwonTextLen=RecordMan.AddedRecord->GetEntry(CondEntryID)->BufferSize;
	CommonMan.InwonTextLen=InwonTextLen;
	IngaMan.InwonTextLen=InwonTextLen;
	WinForm=0x01;
	BackColor=LtGray;
}

CommonCondWin::~CommonCondWin()
{}

void CommonCondWin::SetupWindow()
{
	InwonControl *inwonctrl= new InwonControl(JKPoint(20,10),ID_USERCONTROL+0x0100,InwonTextLen,&CommonMan,&IngaMan,27);
	inwonctrl->SetText(WinText);
	AddControl(inwonctrl);

	BeltChart *beltchart;
	uint16 maxwidth=100;
	JKPoint p(600,200);
	JKRect rect(0,0,70,400);
	beltchart = new BeltChart(rect.OffsetBy(p),ID_USERCONTROL+1);
	beltchart->SetText("¥¥  ¦");
	AddControl(beltchart);
	rect.left +=maxwidth;	rect.right+=maxwidth;
	beltchart = new BeltChart(rect.OffsetBy(p),ID_USERCONTROL+2);
	beltchart->SetText("´i  Ìa");
	AddControl(beltchart);
	rect.left +=maxwidth;	rect.right+=maxwidth;
	beltchart = new BeltChart(rect.OffsetBy(p),ID_USERCONTROL+3);
	beltchart->SetText("¦œa¥¡");
	AddControl(beltchart);
	rect.left +=maxwidth;	rect.right+=maxwidth;
	beltchart = new BeltChart(rect.OffsetBy(p),ID_USERCONTROL+4);
	beltchart->SetText("À¡  Ÿ¡");
	AddControl(beltchart);

	JKButton *bttn;
	rect.Set(600,10,700,40);
	bttn=new JKFloatingButton(rect,ID_BUTTON+1);
	bttn->SetText("·¥¶¥ ¥¡‹¡");
	AddControl(bttn);
	rect.Set(600,60,700,90);
	bttn=new JKFloatingButton(rect,ID_BUTTON+2);
	bttn->SetText("Â‰     b");
	AddControl(bttn);
	rect.Set(600,110,700,140);
	bttn=new JKFloatingButton(rect,ID_BUTTON+3);
	bttn->SetText("a ˆa ‹¡");
	AddControl(bttn);

}

void CommonCondWin::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.VLine(JKPoint(550),rect.Height()+1,Gray);
	dc.VLine(JKPoint(551),rect.Height()+1,White);
}

void CommonCondWin::EvShowWindow()
{
	InitList();

	JKWindow::EvShowWindow();
}

void CommonCondWin::EvButtonClick(WORD ctrlid)
{
	InwonControl *inwonctrl;
	switch(ctrlid){
		case ID_BUTTON+1:
			inwonctrl = (InwonControl *)GetControl(ID_USERCONTROL+0x100);
			if(inwonctrl){
				SearchCount=0;
				if(inwonctrl->SelectKind!=-1){
					BYTE buffer[10];
					strcpy(buffer,BudaeName);
					strcat(buffer,inwonctrl->SelectKind==4? "-C":inwonctrl->SelectKind==3? "-B":inwonctrl->SelectKind==2?"-A":"-HQ");
					RecordMan.AddedRecord->SetData(2,buffer);
					SearchArray[SearchCount++]=2;
				}
				if(inwonctrl->SelectLine!=-1){
					InwonBase *base=CommonMan.GetNode(inwonctrl->SelectLine);
					if(base) {
						RecordMan.AddedRecord->SetData(CondEntryID,base->Text);
						SearchArray[SearchCount++]=CondEntryID;
					}
				}
				SendControlMessage(FINISH_SEARCH);
				CloseWindow();
			}
			break;
		case ID_BUTTON+2:
			PrintPresentCondition();
			break;
		case ID_BUTTON+3:
			CloseWindow();
			break;
		default:
			break;
	}
}

void CommonCondWin::InitPresentCondition()
{
	CommonMan.ClearNode();
	if(IngaMan.NodeCount){
		InwonBase *base = IngaMan.HeadNode;
		while(base){
			CommonMan.PutNodeLast(new InwonBase(InwonTextLen,base->Text));
			base=base->next;
		}
	}
	else{
		EntryBase *ent=RecordMan.AddedRecord->GetEntry(CondEntryID);
		if(ent&&ent->HuboMan){
			HuboManager *huboman=ent->HuboMan;
			HuboBase *hubobase;
			for(uint16 i=0;i<huboman->NodeCount;i++) {
				hubobase=(*huboman)[i];
				if(hubobase)
					CommonMan.PutNodeLast(new InwonBase(InwonTextLen,hubobase->TextBuff));
			}
		}
	}
}

void CommonCondWin::CreatePresentCondition()
{
	InitPresentCondition();

	REFENTRY refentry;
	int16	index=RecordMan.TableHead;
	while(index!=-1&&RecordMan.GetRefEntry(index,refentry)){
		if(refentry.HasData) {
			RecordMan.GetRecord(index,*RecordMan.AddedRecord);
			uint16 budae=0;
			budae=((PersonRecord *)RecordMan.AddedRecord)->GetBudae();
			EntryBase *ent=RecordMan.AddedRecord->GetEntry(CondEntryID);
			if(ent) CommonMan.Add(budae,(char *)ent->Data);

			index=refentry.next;
		}
		else index=-1;
	}
}

void CommonCondWin::InitList()
{
	CreatePresentCondition();

	InwonControl *inwonctrl=(InwonControl *)GetControl(ID_USERCONTROL+0x0100);
	if(inwonctrl) {
		inwonctrl->ShowFirstIndex=0;
		inwonctrl->SelectLine=-1;
		inwonctrl->SelectKind=-1;
		inwonctrl->RedrawClient();
	}

	uint16 i;
	uint16 *Value[4];
	uint16 count=0;

	for(i=0;i<4;i++)
		Value[i]=new uint16[CommonMan.NodeCount];

	InwonBase *base = CommonMan.HeadNode;
	while(base){
		for(i=0;i<4;i++)
			Value[i][count]=base->InwonCount[i];
		count++;
		base=base->next;
	}

	BeltChart *beltchart;
	for(i=0;i<4;i++) {
		beltchart=(BeltChart *)GetControl(ID_USERCONTROL+1+i);
		if(beltchart) beltchart->SetValue(count,Value[i]);
	}
	for(i=0;i<4;i++) delete[] Value[i];
}

ProcessResult CommonCondWin::RespondMessage(JKMSG& msg)
{

	if(msg.main==INWON_SELECTCELL)
		EvSelectCell(LOWORD(msg.detail),HIWORD(msg.detail));
	else if(msg.main==INWON_RESELECTCELL&&(int16(LOWORD(msg.detail))!=-1||int(HIWORD(msg.detail))!=-1))
		SendMessage(BUTTON_CLICK,ID_BUTTON+1);

	return JKWindow::RespondMessage(msg);
}

void CommonCondWin::EvSelectCell(int16 line,int16 kind)
{
	BeltChart *beltchart;
	for(uint16 i=0;i<4;i++){
		beltchart=(BeltChart *)GetControl(ID_USERCONTROL+1+i);
		if(beltchart) {
			if(kind>0) {
				beltchart->ChangeBkColor(i==(kind-1)? Yellow:White);
				beltchart->RedrawChart(i==(kind-1)? line:-1);
			}
			else {
				beltchart->ChangeBkColor(line>=0? Yellow:White);
				beltchart->RedrawChart(line);
			}
		}
	}
}

BOOL CommonCondWin::PrintPresentCondition()
{
	char linefeed[3]="\r\n";

	uint16 bufflen=InwonTextLen+10*4+1;
	BYTE *Buffer = new BYTE[bufflen+1];

	uint16 lineperpage=47;
	uint16 count=0;
	InwonBase *node=CommonMan.HeadNode;
	while(node){
		if(!(count%lineperpage)){
			if(count&&node->next){
				memset(Buffer,'-',bufflen);
				if(!PrintMan->PrintString(Buffer)) return FALSE;
				if(!PrintMan->PrintString(linefeed)) return FALSE;

				if(!PrintMan->PrintString("  * ”a·q ¸wµA  . . . . . ."))
					return FALSE;
				if(!PrintMan->PrintString(linefeed)) return FALSE;

				memset(Buffer,'=',bufflen);
				if(!PrintMan->PrintString(Buffer)) return FALSE;
				if(!PrintMan->PrintString(linefeed)) return FALSE;
				if(!PrintMan->PrintCharacter(FF)) return FALSE;
			}
			if(!PrintMan->PrintString("  <<  "))	return FALSE;
			if(!PrintMan->PrintString(WinText))	return FALSE;
			if(!PrintMan->PrintString("    >>"))	return FALSE;
			if(!PrintMan->PrintString(linefeed))	return FALSE;
			if(!PrintMan->PrintString(linefeed))	return FALSE;

			memset(Buffer,'=',bufflen);
			if(!PrintMan->PrintString(Buffer)) return FALSE;
			if(!PrintMan->PrintString(linefeed)) return FALSE;

			memset(Buffer,' ',InwonTextLen);
			Buffer[InwonTextLen]=0;
			if(!PrintMan->PrintString(Buffer))	return FALSE;
			if(!PrintMan->PrintString("  ¥¥  ¦    ´i  Ìa    ¦œa¥¡    À¡  Ÿ¡  ")) return FALSE;
			if(!PrintMan->PrintString(linefeed))	return FALSE;

			memset(Buffer,'-',bufflen);
			if(!PrintMan->PrintString(Buffer)) return FALSE;
			if(!PrintMan->PrintString(linefeed)) return FALSE;
		}

		uint16 len=strlen(node->Text);
		strncpy(Buffer,node->Text,len);
		if(len<InwonTextLen) memset(Buffer+len,' ',InwonTextLen-len);
		Buffer[InwonTextLen]=0;
		InwonBase *inganode= IngaMan.GetInwonBaseByText(node->Text);
		for(uint16 i=0;i<4;i++) {
			if(!inganode) sprintf(Buffer+InwonTextLen+10*i,"     %3d  ",node->InwonCount[i]);
			else sprintf(Buffer+InwonTextLen+10*i,"  %3d/%3d  ",node->InwonCount[i],inganode->InwonCount[i]);
		}
		if(!PrintMan->PrintString(Buffer))	return FALSE;
		if(!PrintMan->PrintString(linefeed))	return FALSE;
		count++;
		node = node->next;
	}
	memset(Buffer,'=',bufflen);
	if(!PrintMan->PrintString(Buffer)) return FALSE;
	if(!PrintMan->PrintString(linefeed)) return FALSE;
	if(!PrintMan->PrintCharacter(FF)) return FALSE;
	delete[] Buffer;
	if(!PrintMan->PrintCharacter(FF)) return FALSE;

	return TRUE;
}
