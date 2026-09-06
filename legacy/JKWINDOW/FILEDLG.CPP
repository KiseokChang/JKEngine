#include "scrllist.h"
#include "jkbutton.h"
#include "filedlg.h"

FileDialog::FileDialog(JKControl *parent):
	JKDialog(JKRect(100,100,539,379),parent)
{
	SetText("ÑÁ·© ¹¡¸b·i ¶áÐe Dialog");
	SaveDrive = getdisk();
	getcurdir(SaveDrive+1,SavePath);

	DriveCount=0;
	for(int16 i=0;i<26;i++){
		setdisk(i);
		if(i==getdisk()) Drives[DriveCount++]=i;
	}
	setdisk(SaveDrive);
}

FileDialog::~FileDialog()
{
	setdisk(SaveDrive);
	chdir("\\");
	chdir(SavePath);
}

void FileDialog::InitWindow()
{
	JKDialog::InitWindow();
	InitDriveList();
	InitFileList();
}

void FileDialog::SetupWindow()
{
	JKRect rect;
	rect.Set(30,60,160,240);
	ScrollListBox *scrllist=new ScrollListBox(rect,ID_LISTBOX+1);
	if(scrllist) {
		scrllist->DragScrollFlag=TRUE;
		AddControl(scrllist);
	}
	rect.Set(170,60,280,140);
	AddControl(new JKListBox(rect,ID_LISTBOX+2));

	rect.Set(170,220,260,240);
	JKButton *bttn=new JKButton(rect,ID_BUTTON+1);
	if(bttn){
		bttn->SetText("ÑÂ·¥");
		AddControl(bttn);
	}
}

void FileDialog::InitDriveList()
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+2);
	if(listbox) {
		listbox->DestroyAll();
		int8 buffer[13];
		for(int16 i=0;i<DriveCount;i++){
			sprintf(buffer,"%c: —aœa·¡§a",Drives[i]+'A');
			listbox->Add(i,new HuboBase(buffer,Drives[i]));
		}
		int16 index=listbox->GetIndexByNumber(SaveDrive);
		if(index>=0) listbox->SetCurrentIndex(index);
		listbox->RedrawWindow();
	}
}

void FileDialog::ClearFileList()
{
	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();
		listbox->RedrawWindow();
	}
}

void FileDialog::InitFileList()
{

	JKListBox *listbox = (JKListBox *)GetControl(ID_LISTBOX+1);
	if(listbox) {
		listbox->DestroyAll();
		struct ffblk ffblk;
		int16 index=0,color;
		HuboBase *hubo;

		if(!findfirst("*.*",&ffblk,FA_DIREC)){
			do{
				if(HardError){
					HardError=FALSE;
					break;
				}
				if(strcmp(ffblk.ff_name,".")){
					color=ffblk.ff_attrib&FA_DIREC? LtBlue:Black;
					uint16 entcount=listbox->HuboManager::NodeCount;
					int16 result;
					int8 attrib;
					for(index=0;index<entcount;index++){
						result=0;
						hubo=(*listbox)[index];
						if(hubo){
							attrib=hubo->Number;
							if(attrib&FA_DIREC && !(ffblk.ff_attrib&FA_DIREC)) result=-1;
							else if(!(attrib&FA_DIREC) && ffblk.ff_attrib&FA_DIREC) result=1;
							if(!result) result=CompareFileName(hubo->TextBuff,ffblk.ff_name);
							if(result==1) break;
						}
					}
					listbox->Add(index,new HuboBase(ffblk.ff_name,ffblk.ff_attrib,color));
				}
			} while(!findnext(&ffblk));
		}
		if(index>0) listbox->SetCurrentIndex(0);
		listbox->RedrawWindow();
	}
}

void FileDialog::EvButtonClick(WORD ctrlid)
{
	if(ctrlid==ID_BUTTON+1){
		SendMessage(ID_OK);
	}
}

void FileDialog::EvCharDown(WORD key,WORD)
{
	if(key==ENTER){
		SendMessage(ID_OK);
	}
}

void FileDialog::EvLBSelectEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid==ID_LISTBOX+1){
		JKListBox *scrlist = (JKListBox *)GetControl(ctrlid);
		if(scrlist){
			BYTE attrib;
			HuboBase *hubo=(*scrlist)[index];
			if(hubo) {
				attrib=LOBYTE(hubo->Number);
				if(attrib&FA_DIREC){
					chdir(hubo->TextBuff);
					InitFileList();
				}
			}
		}
	}
}

void FileDialog::EvLBTouchEntry(WORD ctrlid,uint16 index)
{
	if(ctrlid==ID_LISTBOX+2){
		JKListBox *scrlist = (JKListBox *)GetControl(ctrlid);
		if(scrlist){
			HuboBase *hubo=(*scrlist)[index];
			if(hubo) {
				setdisk(hubo->Number);
				InitFileList();
			}
		}
	}
}

int16 FileDialog::CompareFileName(int8 *fn1,int8 *fn2)
{
	int16 result=0;
	if(!fn1||!fn2) return 2;

	if(!strcmp(fn1,".")) return -1;
	if(!strcmp(fn2,".")) return 1;
	if(!strcmp(fn1,"..")) return -1;
	if(!strcmp(fn2,"..")) return 1;

	int8 *delim1=strchr(fn1,'.'),*delim2=strchr(fn2,'.');

	if(!delim1&&delim2) result=-1;
	else if(delim1&&!delim2) result=1;
	else if(delim1&&delim2) {
		result= strcmp(delim1+1,delim2+1);
		result= result>0? 1:result<0? -1:0;
	}
	if(!result){
		if(delim1) *delim1=0;
		if(delim2) *delim2=0;
		result= strcmp(fn1,fn2);
		result= result>0? 1:result<0? -1:0;
		if(delim1) *delim1='.';
		if(delim2) *delim2='.';
	}
	return result;
}
