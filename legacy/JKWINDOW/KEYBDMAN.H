#ifndef KEYBDMAN_H
#define KEYBDMAN_H

#include <dos.h>
#include <bios.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include "msgcreat.h"

uint16 GetChX();

class KeyboardManager : public MessageCreator
{
public:
	KeyboardManager();
	~KeyboardManager();
	BOOL GetCurrentStatus();
	BOOL CreateMessage(MessageQue *msgque=::SystemMsgQue);


	uint16 CharCode;
	uint16 KeyKind;
	uint16 Modifier;
};

/////////////////////////////////////////
extern KeyboardManager * KeyboardEvents;//
/////////////////////////////////////////
#endif