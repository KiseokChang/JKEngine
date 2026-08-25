#include "occmsg.h"
#include "battman.h"
#include "podaectl.h"

PodaeControl::PodaeControl(int16 index,int32 balsu,int32 interval,JKControl *parent):
	JKControl(JKRect(-2,-2,-1,-1),0,parent),Index(index),Balsu(balsu),Interval(interval)
{
	if(Index>=0&&Balsu&&Interval){
		REFENTRY refentry;
		if(!BattalionMan->TableMan.GetRefEntry(Index,refentry))
			CreationError=TRUE;
	}
	else CreationError=TRUE;
}

PodaeControl::~PodaeControl()
{}

void PodaeControl::InitWindow()
{
	JKControl::InitWindow();
	if(Index>=0){
		BattalionRecord battrec;
		BattalionMan->RecordMan.GetRecord(Index,battrec);
		int32 battbalsu;
		battrec.GetBalsu(battbalsu);
		battbalsu-=Balsu;
		battrec.SetBalsu(battbalsu);
		battrec.SetStatus(BATTSTAT_FIRE);
		BattalionMan->SetRecord(Index,battrec);
		SetTimer(Interval);
	}
}

void PodaeControl::EvTimerHappen(DWORD)
{
	KillTimer();
	SendControlCommand(FIRE_FINISH,Index);
	CloseWindow();
}
