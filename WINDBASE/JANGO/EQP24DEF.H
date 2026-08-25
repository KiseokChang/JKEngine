#ifndef _EQP24DEF_H
#define _EQP24DEF_H

#include "dfilemn.h"

typedef struct eqp24name
{
	char Division[5];
	char Attached[7];
	char Name[9];
	char Number[12];
//	char UnUse[30];
	RPTR FirstIndex;
	RPTR LastIndex;
}NAME24;

typedef struct epq24kind
{
	WORD Order;
	char Name[15];
	char Number[20];
	WORD Inga;
	WORD A;
	WORD B;
	WORD C;
	char Date[11];
//	char UnUse[30];
	RPTR NameIndex;
	RPTR NextIndex;
	RPTR PrvIndex;
}KIND24;

typedef struct epq24
{
	NAME24 Name24;
	KIND24 Kind24;
}EQP24;

#endif
