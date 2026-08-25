#ifndef _EVNTHAND_H
#define _EVNTHAND_H

#include "control.h"

class EventHandler
{
public:
	EventHandler(JKControl *main);
	virtual ~EventHandler(){}

	virtual WORD Run();
	virtual ProcessResult PreRespondMessage(JKMSG& msg);
	virtual BOOL PreProcessMessage(JKMSG msg);

	JKControl *MainWindow;
	BOOL LoopBreak;
	BOOL CreationError;
};
#endif