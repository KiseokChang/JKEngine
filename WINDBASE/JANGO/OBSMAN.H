#ifndef _OBSMAN_H
#define _OBSMAN_H

#include <stdio.h>
#include <string.h>
#include <dir.h>
#include "jkprog.h"

typedef struct {
	uint16 size;
	int16 prev;
	int16 next;
} OBSREFENTRY;

const uint16 MaxRefEntryCount=5000;

class ObserveManager
{
public:
	ObserveManager(char *fname=0);
	~ObserveManager();

	void IsInitFile();
	BOOL IsEmptyTable();
	void MakeEmptyTable();

	int16  FindHead(int16 index);
	int16  FindTail(int16 index);
	int16  AllocIndex(uint16 size);
	BOOL ReallocIndex(uint16 index,uint16 size);
	BOOL FreeIndex(uint16 index);
	void FreeAll(int16 index);
	BOOL GetRefEntry(uint16 index,OBSREFENTRY& refentry);
	BOOL PutRefEntry(uint16 index,OBSREFENTRY& refentry);
	int16  GetPrevIndex(uint16 index);
	int16  GetNextIndex(uint16 index);
	int32 CalcDataPosition(uint16 index);

	uint16 GetObserveData(uint16 index,BYTE *date,BYTE *& observe,BYTE *& treat);
	uint16 PutObserveData(uint16 index,BYTE *date,BYTE *observe,BYTE *treat);

	int16 AddObserveMan(ObserveManager *obsman,int16 index);
//=======================================================//
	BOOL PrintObserveData(uint16 index);
	BOOL PrintAll(uint16 index);

	char ObserveDataFile[13];
	char ObserveTableFile[13];
	char ObserveTempFile[13];
	int32 HeaderSize;
};
#endif