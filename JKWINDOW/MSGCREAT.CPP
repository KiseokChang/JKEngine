#include "msgcreat.h"

MessageCreator::MessageCreator():
	CreationError(FALSE),IsMessagable(TRUE)
{}
MessageCreator::~MessageCreator()
{}

BOOL MessageCreator::SetMessagable(BOOL isenable)
{
	if(IsMessagable!=isenable){
		IsMessagable=isenable;
		return TRUE;
	}
	return FALSE;
}

BOOL MessageCreator::GetMessagable()
{
	return IsMessagable;
}



