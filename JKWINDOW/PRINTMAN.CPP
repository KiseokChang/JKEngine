#include<dos.h>
#include<stdio.h>
#include "printman.h"


PrintManager::PrintManager()
{
	PrinterPort=LPT1;
}

PrintManager::~PrintManager()
{
}

void PrintManager::InitPrinterPort()
{
	union REGS regs;
	regs.h.ah=0x01;
	regs.w.dx=PrinterPort;
	int386(PRT_INTERRUPT,&regs,&regs);
}

BYTE PrintManager::GetPrinterStatus()
{
	union REGS regs;
	regs.h.ah=0x02;
	regs.w.dx=PrinterPort;
	int386(PRT_INTERRUPT,&regs,&regs);
	return regs.h.ah;
}

BOOL PrintManager::PrintCharacter(BYTE ch)
{
	BYTE status = GetPrinterStatus();
	if(!(status&0x10)) return FALSE;
	union REGS regs;
	regs.h.ah=0x00;
	regs.w.dx=PrinterPort;
	regs.h.al=ch;
	int386(PRT_INTERRUPT,&regs,&regs);
	return (regs.h.ah&PRT_TIMEOUT)? FALSE:TRUE;
}

BOOL PrintManager::PrintString(char *str)
{
	return PrintString((BYTE *)str);
}
BOOL PrintManager::PrintString(BYTE *str)
{
	if(!str) return FALSE;

	while(*str)
		if(!PrintCharacter(*str++)) return FALSE;
	return TRUE;
}

BOOL PrintManager::PrintString(char *str,WORD count)
{
	return PrintString((BYTE *)str,count);
}

BOOL PrintManager::PrintString(BYTE *str,WORD count)
{
	if(!str||!count) return FALSE;

	while(*str && count){
		if(!PrintCharacter(*str++)) return FALSE;
		count--;
	}
	return TRUE;
}


void PrintManager::ChangePrintPort(WORD port)
{
	if(port<=LPT3){
		PrinterPort=port;
	}
}

//////////////////////////
PrintManager *PrintMan=0;
//////////////////////////