#include "keybdman.h"

uint16 GetChX()
{
	int16 key=getch();
	if(key)
		return MAKEWORD(key,0x00);
	else
		return MAKEWORD(0x00,getch());
}

KeyboardManager::KeyboardManager():
	MessageCreator()
{}

KeyboardManager::~KeyboardManager()
{}


BOOL KeyboardManager::GetCurrentStatus()
{
	if(kbhit())	{
		CharCode = ::GetChX();
		KeyKind = HIBYTE(CharCode)? KEYDOWN : CHARDOWN;
		Modifier = _bios_keybrd(2);
		return TRUE;
	}
	return FALSE;
}

BOOL KeyboardManager::CreateMessage(MessageQue *msgque)
{
	BOOL ExistFlag=FALSE;
	JKMSG msg;
	msg.winid=0;
	if(msgque){
		JKMSG msg;
		while(GetCurrentStatus()) {
			if(!IsMessagable) continue;
			DWORD Sub=MAKEDWORD(CharCode,Modifier);
			if(!msgque->GetMessageLast(msg,FALSE)||msg.main!=KeyKind||msg.sub!=Sub||msg.winid){
				msgque->SendMessage(0,KeyKind,Sub,1);
				ExistFlag=TRUE;
			}
			else{
				msgque->GetMessageLast(msg);
				msg.detail++;
				msgque->PutMessage(msg);
			}
		}
	}
	return ExistFlag;
}

////////////////////////////////////
KeyboardManager *KeyboardEvents=0;//
////////////////////////////////////