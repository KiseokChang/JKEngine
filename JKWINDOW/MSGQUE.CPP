#include "msgque.h"

MessageQue::MessageQue():
	DLLFrame<MSGContainer>()
{}

MessageQue::~MessageQue()
{}

BOOL MessageQue::PutMessage(JKMSG& msg)
{
	MSGContainer *container = new MSGContainer(msg);
	return PutNodeLast(container);
}

BOOL MessageQue::PutMessageFirst(JKMSG& msg)
{
	MSGContainer *container = new MSGContainer(msg);
	return PutNodeFirst(container);
}

BOOL MessageQue::GetMessage(JKMSG& msg,BOOL isdel)
{
	MSGContainer *container=GetNodeFirst(isdel);
	if(container){
		msg=container->Message;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}


BOOL MessageQue::GetMessageLast(JKMSG& msg,BOOL isdel)
{
	MSGContainer *container=GetNodeLast(isdel);
	if(container){
		msg=container->Message;
		if(isdel) delete container;
		return TRUE;
	}
	return FALSE;
}

void MessageQue::SendMessage(DWORD winid,WORD main,DWORD sub,DWORD detail,WORD option)
{
	JKMSG msg;
	msg.main=main;
	msg.sub=sub;
	msg.winid = winid;
	msg.detail = detail;
	msg.option = option;
	PutMessage(msg);
}

void MessageQue::ExecuteMessage(DWORD winid,WORD main,DWORD sub,DWORD detail,WORD option)
{
	JKMSG msg;
	msg.main=main;
	msg.sub=sub;
	msg.winid = winid;
	msg.detail = detail;
	msg.option = option;
	PutMessageFirst(msg);
}


//--------------------------------------------//
MessageQue *SystemMsgQue=0;
MessageQue *MsgQue=0;
//--------------------------------------------//
