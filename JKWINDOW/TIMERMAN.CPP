#include "timerman.h"

void /*interrupt*/ (*OldVect)(...);

void /*interrupt*/ TimerIsr(...)
{
	static BOOL inflag = FALSE;
	if (OldVect != NULL)
			OldVect();
	if (inflag) return;
	inflag = TRUE;
	if(TimerMan) TimerMan->UserTimerIsr();
	inflag = FALSE;
}

TimerManager::TimerManager():
	MessageCreator(),
	DLLFrame<TimerMember>(),
	IsTickActived(FALSE)
{
//	OldVect = getvect(TIMER_ISR);
//	setvect(TIMER_ISR, TimerIsr);
}

TimerManager::~TimerManager()
{
//	setvect(TIMER_ISR, OldVect);
}

BOOL TimerManager::CreateMessage(MessageQue *msgque)
{
	///////////////////////
	IsTickActived=TRUE;////
	///////////////////////

	BOOL ExistFlag=FALSE;
	if(IsTickActived&&msgque){
		JKMSG msg;
		struct time CurrentTime;
		gettime(&CurrentTime);
		TimerMember *memb = HeadNode;
		while(memb) {
			DWORD count=CalcValidCount(memb->LastTime,CurrentTime,memb->Interval);
			if(count){
				if(IsMessagable) msgque->SendMessage(memb->WinID,TIMERHAPPEN,count);
				memb->LastTime=CurrentTime;
				ExistFlag=TRUE;
			}
			memb=memb->next;
		}
	}
	IsTickActived=FALSE;
	return ExistFlag;
}

void TimerManager::UserTimerIsr()
{
	IsTickActived=TRUE;
}

DWORD TimerManager::CalcValidCount(struct time& oldtime,struct time& curtime,DWORD interval)
{
	int32 totalcount=DWORD((curtime.ti_hour-oldtime.ti_hour+24)%24)*60;
	totalcount+=int32(curtime.ti_min-oldtime.ti_min);	 totalcount*=60;
	totalcount+=int32(curtime.ti_sec-oldtime.ti_sec);	 totalcount*=100;
	totalcount+=int32(curtime.ti_hund-oldtime.ti_hund); totalcount*=10;
	return (DWORD)totalcount/interval;
}

//==============================================================

BOOL TimerManager::SetTimer(DWORD winid,DWORD interval)
{
	TimerMember *memb = new TimerMember(winid,interval);
	if(memb){
		TimerMember *tempmemb=GetNode(memb,0);
		if(!tempmemb) {
			PutNodeLast(memb);
			tempmemb=memb;
		}
		else delete memb;
		struct time CurrentTime;
		gettime(&CurrentTime);
		tempmemb->LastTime=CurrentTime;
		SystemMsgQue->SendMessage(winid,TIMERACTIVE);
		return TRUE;
	}
	return FALSE;
}

void TimerManager::KillTimer(DWORD winid)
{
	TimerMember *findnode = new TimerMember(winid);
	DeleteNode(findnode,0);
	if(findnode) delete findnode;
}

TimerMember *TimerManager::GetTimer(DWORD winid)
{
	TimerMember *findnode = new TimerMember(winid);
	TimerMember *node=GetNode(findnode,0);
	if(findnode) delete findnode;
	return node;
}

/////////////////////////
TimerManager *TimerMan=0;
/////////////////////////