#ifndef _JOCLOCK_H
#define _JOCLOCK_H

#include <time.h>

#include "jkstatic.h"

#define CLOCK_CHANGEDATE	(USERMESSAGE+0x500+1)

class JOClock:public JKStatic
{
public:
	JOClock(JKRect& rect,WORD ctlid,JKControl *parent=0);
	void EvTimerHappen(DWORD happencount);
	struct time CurrentTime;
};
#endif