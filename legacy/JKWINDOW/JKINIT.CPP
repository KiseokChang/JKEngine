#include <dos.h>
#include "okdialog.h"
#include "jkinit.h"
/*
void interrupt c_break(...){}

void interrupt (*old_cbreak)( ... );

#define IGNORE  0
#define RETRY   1
#define ABORT   2

INT JKHardErrorHandler(INT ,INT ax,INT ,INT )
{
	HardError=TRUE;

	static CHAR msg[200];
//	WORD di= _DI;
	if (ax < 0){
		sprintf(msg,"Device µAœá");
		OKDialog *dlg = new OKDialog(JKRect(200,150,439,330),msg);
		dlg->SetText("µA     œá");
		dlg->Run();
		delete dlg;
		hardretn(ABORT);
	}
	INT drive = ax & 0x00FF;
//	INT errorno = di & 0x00FF;
	sprintf(msg, "%c: —aœa·¡§aµA¬á µAœáˆa ¤i¬—Ð–¯s“¡”a.", 'A' + drive);
	OKDialog *dlg = new OKDialog(JKRect(150,150,539,330),msg);
	dlg->SetText("µA     œá");
	dlg->Run();
	delete dlg;
	hardresume(IGNORE);
	return IGNORE;
}
*/

BOOL InitJKProgram()
{
	if(!InitGraph()) return FALSE;
	ResMan = new ResourceManager();
	if(!ResMan){
		return FALSE;
	}
	HanMan = new HangulManager();
	if(!HanMan||HanMan->CreationError){
		if(HanMan) delete HanMan;
		delete ResMan;
		return FALSE;
	}
	GlobalDC = new JKScreenDC();
	if(!GlobalDC){
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	GlobalVFontMan = new VectorFontManager(".");
	if(!GlobalVFontMan||GlobalVFontMan->CreationError){
		if(GlobalVFontMan) delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	SystemMsgQue = new MessageQue();
	if(!SystemMsgQue) {
		delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	MsgQue = new MessageQue();
	if(!MsgQue) {
		delete SystemMsgQue;
		delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	TimerMan = new TimerManager();
	if(!TimerMan){
		delete MsgQue;
		delete SystemMsgQue;
		delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	MouseEvents = new MouseManager();
	if(!MouseEvents) {
		delete TimerMan;
		delete MsgQue;
		delete SystemMsgQue;
		delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	KeyboardEvents = new KeyboardManager();
	if(!KeyboardEvents) {
		delete MouseEvents;
		delete TimerMan;
		delete MsgQue;
		delete SystemMsgQue;
		delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	PrintMan = new PrintManager();
	if(!PrintMan){
		delete KeyboardEvents;
		delete MouseEvents;
		delete TimerMan;
		delete MsgQue;
		delete SystemMsgQue;
		delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
	MsgFilter = new MessageFilter();
	if(!MsgFilter){
		delete PrintMan;
		delete KeyboardEvents;
		delete MouseEvents;
		delete TimerMan;
		delete MsgQue;
		delete SystemMsgQue;
		delete GlobalVFontMan;
		delete GlobalDC;
		delete HanMan;
		delete ResMan;
		return FALSE;
	}
//	old_cbreak=getvect(0x23);
//	setvect(0x23,c_break);
//	harderr(JKHardErrorHandler);
	return TRUE;
}

void CloseJKProgram()
{
	if(MsgFilter) 			delete MsgFilter;
	if(PrintMan) 				delete PrintMan;
	if(KeyboardEvents) 	delete KeyboardEvents;
	if(MouseEvents) 		delete MouseEvents;
	if(TimerMan) 				delete TimerMan;
	if(MsgQue) 					delete MsgQue;
	if(SystemMsgQue) 		delete SystemMsgQue;
  if(GlobalVFontMan)  delete GlobalVFontMan;
	if(GlobalDC) 				delete GlobalDC;
	if(HanMan) 					delete HanMan;
	if(ResMan) 					delete ResMan;

	CloseGraph();
}

