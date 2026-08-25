#include "strkque.h"

StrokeQue::StrokeQue():
	DLLFrame<StrokeContainer>()
{}

StrokeQue::~StrokeQue()
{}

BOOL StrokeQue::PutStroke(STROKE& strk)
{
	StrokeContainer *container = new StrokeContainer(strk);
	return PutNodeLast(container);
}

BOOL StrokeQue::PutStrokeFirst(STROKE& strk)
{
	StrokeContainer *container = new StrokeContainer(strk);
	return PutNodeFirst(container);
}

BOOL StrokeQue::GetStroke(STROKE& strk,BOOL isdel)
{
	StrokeContainer *container=GetNodeFirst(isdel);
	if(container){
		strk=container->Stroke;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}


BOOL StrokeQue::GetStrokeLast(STROKE& strk,BOOL isdel)
{
	StrokeContainer *container=GetNodeLast(isdel);
	if(container){
		strk=container->Stroke;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}