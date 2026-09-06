#ifndef _EQFILEMN_H
	#define _EQFILEMN_H

#ifndef _BOMB_
	#define _BOMB_
		 typedef struct {
				 char Name[9];
				 char Kind[9];
				 char Dod[11];
				 char Lot[22];
				 char Born[11];
				 char Place[15];
				 char Count[5];
				 char Use[5];
				 char HaveYear[11];
				 char SomoYear[11];
			}BOMB;
#endif

#include <stdio.h>
#include <string.h>
#include <io.h>
#include <dir.h>
#include <stdlib.h>
#include "jkwindef.h"

class EqiupFileManage
{
public:
	EqiupFileManage(char *filename);
	~EqiupFileManage();

	BOOL ReadOpen();
	BOOL WriteOpen();
	BOOL Read(BOMB& buf,int16 index);
	BOOL Modify(BOMB& buf,int16 index);
	BOOL Append(BOMB& buf);
	BOOL Delete(int16 index);
	BOOL Compare(BOMB& buf1,BOMB& buf2);
	void MakeFileName(char *name);

	void Close();

	BOOL CreationError;
	FILE *EqFP;
	char FileName[13];
	char BackFileName[13];
	uint16 HeadSize;
};
#endif
