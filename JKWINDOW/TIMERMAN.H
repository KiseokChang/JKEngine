#ifndef _TIMERMAN_H
#define _TIMERMAN_H

#include <dos.h>
#include <stdio.h>
#include "msgcreat.h"

#define TIMER_ISR 0x08

class TimerMember
{
public:
	TimerMember(DWORD winid=0,DWORD interval=100):
		prev(0),next(0),WinID(winid),Interval(interval)
	{}
	BOOL IsEqual(TimerMember *node,WORD /*compkind*/)
	{
		if(node) return WinID==node->WinID? TRUE : FALSE;
		return FALSE;
	}

	TimerMember *Duplicate()
	{	return new TimerMember(WinID,Interval);	}

	DWORD WinID;
	DWORD Interval;
	struct time LastTime;
	TimerMember *prev;
	TimerMember *next;
};


class TimerManager : public MessageCreator, public DLLFrame<TimerMember>
{
public:
	TimerManager();
 ~TimerManager();
	void UserTimerIsr();
	DWORD CalcValidCount(struct time& oldtime,struct time& curtime,DWORD interval);

	BOOL CreateMessage(MessageQue *msgque=::SystemMsgQue);

	BOOL SetTimer(DWORD winid,DWORD interval);
	void KillTimer(DWORD winid);
	TimerMember *GetTimer(DWORD winid);
private:
	BOOL IsTickActived;
};
extern TimerManager * TimerMan;
#endif