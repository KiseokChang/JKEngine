#ifndef _INPUTBRD_H
#define _INPUTBRD_H

#include "recoprog.h"
#include "jkwindow.h"

#define FINISH_INPUT 		(USERMESSAGE+1)
#define CANCEL_INPUT 		(USERMESSAGE+2)

class InputBoard:public JKWindow
{
public:
	InputBoard(JKRect rect,WORD ctrlid,JKControl *parent=0);

	void EvLeftClick(JKPoint& p);
	void EvLeftDragging(JKPoint& p);
	void EvLeftRelease(JKPoint&);
	void EvRightClick(JKPoint&);
	void EvNcLeftDragging(JKPoint&);
	void EvNcLeftRelease(JKPoint&);

	ProcessResult RespondMessage(JKMSG& msg);
	BOOL IsStartInput;
};
#endif