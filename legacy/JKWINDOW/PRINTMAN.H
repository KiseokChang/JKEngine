#ifndef _PRINTMAN_H
#define _PRINTMAN_H

#include"typedef.h"

#define PRT_INTERRUPT		0x17

#define PRT_TIMEOUT			0x01
#define PRT_BUSYBIT			0x80

#define LPT1						0
#define LPT2						1
#define LPT3						2

class PrintManager
{
public:
	PrintManager();
	~PrintManager();

	void InitPrinterPort();
	BYTE GetPrinterStatus();
	BOOL PrintCharacter(BYTE ch);
	BOOL PrintString(char *str);
	BOOL PrintString(BYTE *str);
	BOOL PrintString(char *str,WORD count);
	BOOL PrintString(BYTE *str,WORD count);

	void ChangePrintPort(WORD port);

	WORD PrinterPort;

};

////////////////////////////////
extern PrintManager * PrintMan;//
////////////////////////////////
#endif