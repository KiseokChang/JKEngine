#ifndef _JANGOAPP_H
#define _JANGOAPP_H

#include "ctrlres.h"
#include "jkapp.h"


#define START_INSA			0x0100+1
#define START_EQUIP			0x0100+2
#define START_EQUIP24 	0x0100+3
#define START_MOTOR   	0x0100+4

class JangoApplication: public JKApplication
{
public:
	JangoApplication();
 ~JangoApplication();
	void InitResourceMan();
	void SetupWindow();
	BOOL CanClose();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	ProcessResult RespondMessage(JKMSG& msg);
	void EvShowWindow();
	void EvCommand(WORD cmdid,WORD data,DWORD detail,WORD option);

	void EvButtonClick(WORD ctrlid);
	void EvCtrlOnlyMove(WORD ctrlid);

	void EvDeadKeyDown(uint16 ,WORD );

	CtrlResManager CtrlResMan;
};
#endif
