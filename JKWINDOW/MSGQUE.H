#ifndef MSGQUE_H
#define MSGQUE_H

#include "tmpltdll.h"

class JKMSG{
public:
	JKMSG(WORD m=0,DWORD	s=0,DWORD	d=0,DWORD w=0,WORD o=0)
	{ Set(m,s,d,w,o);	}

	void Set(WORD m=0,DWORD	s=0,DWORD	d=0,DWORD w=0,WORD o=0)
	{
		main=m;		sub=s; 		detail=d;		winid=w;		option=o;
	}

	JKMSG& operator =(const JKMSG& other)
	{
		main=other.main;
		sub=other.sub;
		detail=other.detail;
		winid=other.winid;
		option=other.option;
		return *this;
	}

	BOOL IsEqual(JKMSG& other,BYTE compkind=0x1f)
	{
		BOOL isequal=TRUE;
		if(isequal&&compkind&0x01&& main  !=other.main) isequal=FALSE;
		if(isequal&&compkind&0x02&& sub   !=other.sub) isequal=FALSE;
		if(isequal&&compkind&0x04&& detail!=other.detail) isequal=FALSE;
		if(isequal&&compkind&0x08&& winid !=other.winid) isequal=FALSE;
		if(isequal&&compkind&0x10&& option!=other.option) isequal=FALSE;
		return isequal;
	}

	WORD 	main;
	DWORD	sub;
	DWORD	detail;
	DWORD winid;
	WORD option;
};

class MSGContainer
{
public:
	MSGContainer(JKMSG msg=JKMSG()):
		Message(msg),prev(0),next(0)
	{}

	BOOL IsEqual(MSGContainer *node,WORD compkind=0x001f)
	{
		if(!node) return FALSE;
		return Message.IsEqual(node->Message,LOBYTE(compkind));
	}

	MSGContainer *Duplicate()
	{
		return new MSGContainer(Message);
	}


	JKMSG Message;
	MSGContainer *prev;
	MSGContainer *next;
};


class MessageQue: public DLLFrame<MSGContainer>
{
public:
	MessageQue();
 ~MessageQue();

	BOOL GetMessage(JKMSG& msg,BOOL isdel=TRUE);
	BOOL GetMessageLast(JKMSG& msg,BOOL isdel=TRUE);

	BOOL PutMessage(JKMSG& msg);
	BOOL PutMessageFirst(JKMSG& msg);

	void SendMessage(DWORD winid,WORD main,DWORD sub=0,DWORD detail=0,WORD option=0);
	void ExecuteMessage(DWORD winid,WORD main,DWORD sub=0,DWORD detail=0,WORD option=0);
};

extern MessageQue *MsgQue;
extern MessageQue *SystemMsgQue;

#endif
