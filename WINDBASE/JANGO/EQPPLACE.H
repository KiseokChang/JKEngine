#ifndef _EQPPLACE_H
	#define _EQPPLACE_H

#include <stdio.h>
#include <string.h>
#include "jkwindef.h"

class EqpPlace
{
public:
	EqpPlace(char* filename=0);
	~EqpPlace();

	int16 Read(char &buf,int16 index);
	int16 Write(char& str);

	char FileName[13];
};

#endif