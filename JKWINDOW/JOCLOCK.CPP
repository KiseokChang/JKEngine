#include "joclock.h"

JOClock::JOClock(JKRect& rect,WORD ctlid,JKControl *parent):
	JKStatic(rect,ctlid,parent)
{
	SetTimer(10);
	gettime(&CurrentTime);
}

void JOClock::EvTimerHappen(DWORD /*happencount*/)
{
	struct time curtime;
	gettime(&curtime);
	if(curtime.ti_sec==CurrentTime.ti_sec
			&&curtime.ti_min==CurrentTime.ti_min
			&&curtime.ti_hour==CurrentTime.ti_hour) return;
	if(CurrentTime.ti_hour>curtime.ti_hour)
		SendControlMessage(CLOCK_CHANGEDATE);
	CurrentTime = curtime;
	if(IsShow){
		int8 Buffer[30];
		sprintf(Buffer,"%2d:%02d.%02d",CurrentTime.ti_hour,CurrentTime.ti_min,CurrentTime.ti_sec);
		SetText(Buffer);
		RedrawClient();
	}
}
