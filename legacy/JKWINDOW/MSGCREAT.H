#ifndef MSGCREAT_H
#define MSGCREAT_H

#include "jkwindef.h"
#include "msgque.h"

class MessageCreator
{
public:
	MessageCreator();
	virtual ~MessageCreator();

	virtual BOOL CreateMessage(MessageQue *msgque=::SystemMsgQue)=0;

	virtual BOOL SetMessagable(BOOL isenable);
	BOOL GetMessagable();

	BOOL CreationError;
	BOOL IsMessagable;
};

#endif