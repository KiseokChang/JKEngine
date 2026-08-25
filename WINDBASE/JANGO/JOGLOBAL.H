#ifndef _JOGLOBAL_H
#define _JOGLOBAL_H

#include "jkwindef.h"

extern char BudaeName[3+1];

#define PasswordCount 10
extern char PasswordTable[PasswordCount][8+1];


BOOL InitGlobal();
void WriteGlobal();

#endif