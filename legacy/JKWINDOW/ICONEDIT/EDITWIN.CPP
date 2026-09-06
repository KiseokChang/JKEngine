#include "basefunc.h"
#include "jkstatic.h"
#include "jkedit.h"
#include "drawbrd.h"
#include "showbrd.h"
#include "editwin.h"

EditWindow::EditWindow(uint16 width,uint16 height,int16 bkcolor,char *fname,char *imgname):
	JKWindow(JKRect(0,0,740,760),0)
{
	PackImage[0]=0;PackImage[1]=0;
	BorderSize=5;
	SetText("ICON ¸b¬÷ WINDOW");
	BackColor=bkcolor;
	if(!width) width=24;
	if(!height) height=24;
	WinAttr|=WA_FIRSTSHOW | WA_TITLEMOVEABLE;
	MakeImageBuffer(width,height,BackColor);
	if(fname)	strcpy(FileName,fname);
	else		  strcpy(FileName,"NONAME");
	if(imgname)	strcpy(ImageName,imgname);
	else       	strcpy(ImageName,"Image_Noname");
}

EditWindow::~EditWindow()
{
	DestroyImageBuffer();
}

void EditWindow::InitWindow()
{
	JKWindow::InitWindow();
	SendMessage(READIMAGE);
}

void EditWindow::SetupWindow()
{
	JKRect rect = ClientRect.SetLeftTopBy();
	rect.bottom=rect.top+IconHeight-1;
	rect.right=rect.left+IconWidth-1;
	AddControl(new ShowBoard(rect,ID_USERCONTROL+2,PackImage[0]));

	JKControl *ctrl;
	rect.left=rect.right+2;
	rect.right=200;

	rect.bottom=rect.top+19;
	ctrl = new JKEdit(rect,ID_EDIT+1,0,8,FALSE,FALSE);
	ctrl->SetText("NONAME");
	AddControl(ctrl);

	rect.top=rect.bottom+1;
	rect.bottom=rect.top+19;
	ctrl = new JKEdit(rect,ID_EDIT+2,0,20);
	ctrl->SetText("Image_Noname");
	AddControl(ctrl);

	rect = ClientRect.SetLeftTopBy();
	rect.top += IconHeight>40? IconHeight:40;
	AddControl(new DrawBoard(rect,ID_USERCONTROL+1,IconWidth,IconHeight,PackImage[0],PackImage[1]));
}

void EditWindow::MakeImageBuffer(uint16 width,uint16 height,BYTE color)
{
	DestroyImageBuffer();
	uint32 size=width*height;
	if(size) {
		IconWidth=width;	IconHeight= height;
		width--;	height--;
		for(int16 i=0;i<2;i++){
			PackImage[i] = new BYTE[size+4];
			if(PackImage[i]){
				*((uint16 *)(PackImage[i]))=width;
				*((uint16 *)(PackImage[i]+2))=height;
				memset(PackImage[i]+4,(!i? color:0),IconWidth*IconHeight);
			}
			else{
				IconWidth=IconHeight=0;
				CloseWindow();
			}
		}
	}
}

void EditWindow::DestroyImageBuffer()
{
	for(int16 i=0;i<2;i++){
		if(PackImage[i])	{
			delete[] PackImage[i];
			PackImage[i]=0; //Important Initialization By 0
		}
	}
}

void EditWindow::EvCtrlLeftClick(WORD ctrlid)
{
	JKControl *ctrl;
	switch(ctrlid){
		case ID_USERCONTROL+1:
			ctrl=GetControl(ID_USERCONTROL+2);
			if(ctrl) ctrl->RedrawClient(FALSE);
			break;
		default:
			break;
	}
}

void EditWindow::EvCtrlLeftDragging(WORD ctrlid)
{
	EvCtrlLeftClick(ctrlid);
}

void EditWindow::EvKeyDown(uint16 key,WORD )
{
	switch(key){
		case ALT_N:
			SendMessage(CLEARIMAGE);
			break;
		case ALT_R:
			SendMessage(READIMAGE);
			break;
		case ALT_S:
			SendMessage(SAVEIMAGE);
			break;
		case ALT_X:
			CloseWindow();
			break;
		default:
			break;
	}
}

BOOL EditWindow::CanClose()
{
	if(!JKWindow::CanClose()) return FALSE;
	return TRUE;
}

void EditWindow::EvEditFinish(WORD ctrlid)
{
	JKControl *ctrl=GetControl(ctrlid);
	if(ctrl){
		char buffer[21];
		if(ctrlid==ID_EDIT+1) {
			strcpy(buffer,ctrl->GetText());
			DeleteWhiteSpace(buffer);
			if(!buffer)	strcpy(buffer,"NONAME");
			strcpy(FileName,buffer);
			ctrl->SetText(buffer);
			ctrl->RedrawClient();
		}
		else if(ctrlid==ID_EDIT+2) {
			strcpy(buffer,ctrl->GetText());
			DeleteWhiteSpace(buffer);
			if(!buffer) strcpy(buffer,"Image_Noname");
			strcpy(ImageName,buffer);
			ctrl->SetText(buffer);
			ctrl->RedrawClient();
		}
	}
}

void EditWindow::EvCtrlKillFocus(WORD ctrlid)
{
	if(ctrlid==ID_EDIT+1||ctrlid==ID_EDIT+2) EvEditFinish(ctrlid);
}

ProcessResult EditWindow::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case CLEARIMAGE:
			MakeImageBuffer(IconWidth,IconHeight,BackColor);
			RedrawWindow();
			break;
		case READIMAGE:
			ReadImageFile();
			break;
		case SAVEIMAGE:
			SaveImageFile();
			break;
		default:
			break;
	}
	return JKWindow::RespondMessage(msg);
}

void EditWindow::SaveImageFile()
{
	SaveResourceFile();
	SaveDataFile();
}
void EditWindow::SaveResourceFile()
{
	char Name[13];
	FILE *fp;
	uint16 size = IconWidth*IconHeight+4;

	strcpy(Name,FileName);
	strcat(Name,".krh");
	fp = fopen(Name,"w");
	if(fp){
		strupr(Name);
		Name[strlen(Name)-4]='_';
		fprintf(fp,"#ifndef _%s\n",Name);
		fprintf(fp,"#define _%s\n",Name);
		fprintf(fp,"#include \"jkwindef.h\"\n\t");
        fprintf(fp,"extern BYTE %s[2][%2d];\n",ImageName,size);
		fprintf(fp,"#endif");
		fclose(fp);
	}

	strcpy(Name,FileName);
	strcat(Name,".krc");
	fp = fopen(Name,"w");
	if(fp){
		fprintf(fp,"#include \"%s.krh\"\n",FileName);

        fprintf(fp,"BYTE %s[2][%2d] = {\n\t",ImageName,size);

		for(int16 j=0;j<2;j++){
			fprintf(fp,"{\n\t\t");
			for(uint16 i=0;i<4;i++){
				fprintf(fp,"0x%02x,", PackImage[j][i]);
				if(i+1<4) fprintf(fp," ");
			}
			fprintf(fp,"\n\t\t");

			for(;i<size;i++){
				if((i-4)%IconWidth==0) fprintf(fp,"\n\t\t");
				else fprintf(fp," ");
				fprintf(fp,"0x%02x",PackImage[j][i]);
				if(i+1<size) fprintf(fp,",");
			}
			fprintf(fp,"\n\t}");
			if(!j) fprintf(fp,",\n\t}");
		}

		fprintf(fp,"\n};");
		fclose(fp);
	}
}

void EditWindow::SaveDataFile()
{
	if(!PackImage[0] || !PackImage[1]) return;
	char Name[13];
	strcpy(Name,FileName);
	strcat(Name,".spr");
	FILE *fp = fopen(Name,"wb");
	if(fp){
		uint32 size = IconWidth*IconHeight+4;
		uint32 totalsize=size*2;
		fwrite(&totalsize,sizeof(totalsize),1,fp);
		fwrite(PackImage[0],size,1,fp);
		fwrite(PackImage[1],size,1,fp);
		fclose(fp);
	}
}

BOOL EditWindow::ReadImageFile()
{
	char Name[13];
	strcpy(Name,FileName);
	strcat(Name,".spr");
	FILE *fp = fopen(Name,"rb");
	if(fp){
		uint32 totalsize;
		fread(&totalsize,sizeof(totalsize),1,fp);
		uint32 size=totalsize/2;
		uint16 width,height;
		fread(&width,sizeof(width),1,fp);
		fread(&height,sizeof(height),1,fp);
		if(totalsize%2 || size==int32(width+1)*(height+1)+4) {
			ChangeImageSize(width+1,height+1);
			fread(PackImage[0]+4,IconWidth*IconHeight,1,fp);
			fread(PackImage[1],size,1,fp);
			fclose(fp);
			return TRUE;
		}
		fclose(fp);
	}
	return FALSE;
}

void EditWindow::ChangeImageSize(uint16 width,uint16 height)
{
	if(!width || !height) return;

	MakeImageBuffer(width,height,BackColor);

	JKControl *ctrl;
	JKRect rect = ClientRect.SetLeftTopBy();
	rect.bottom=rect.top+IconHeight-1;
	rect.right=rect.left+IconWidth-1;
	ctrl=GetControl(ID_USERCONTROL+2);
	if(ctrl) {
		ctrl->IsShow=FALSE;
		ctrl->ChangeWindowSize(rect);
		ctrl->IsShow=TRUE;
	}
	rect.left=rect.right+2;
	rect.right=200;
	rect.bottom=rect.top+19;
	ctrl=GetControl(ID_EDIT+1);
	if(ctrl) {
		ctrl->IsShow=FALSE;
		ctrl->ChangeWindowSize(rect);
		ctrl->IsShow=TRUE;
	}
	rect.top=rect.bottom+1;
	rect.bottom=rect.top+19;
	ctrl=GetControl(ID_EDIT+2);
	if(ctrl) {
		ctrl->IsShow=FALSE;
		ctrl->ChangeWindowSize(rect);
		ctrl->IsShow=TRUE;
	}
	rect = ClientRect.SetLeftTopBy();
	rect.top += IconHeight>40? IconHeight:40;
	ctrl=GetControl(ID_USERCONTROL+1);
	if(ctrl) {
		ctrl->IsShow=FALSE;
		ctrl->ChangeWindowSize(rect);
		ctrl->IsShow=TRUE;
	}

	RedrawWindow();
}

