#ifndef _PERSNREC_H
#define _PERSNREC_H

#include "recbase.h"

class PersonRecord : public RecordBase
{
public:
	PersonRecord();
 ~PersonRecord();

	void InitData();
	RecordBase *CreateEqualRecord();
	int16 Compare(RecordBase *base,WORD entid);

	BOOL Assign(RecordBase *rec);
	void Get(FILE *fp);
	void Put(FILE *fp);
	uint16 GetFileSize();

	int16 ObserveIndex;


	uint16 GetBudae(char *str=0);
	BOOL IsGanbu();
	BOOL IsHasa();
	BOOL IsSabyong();


	BOOL GetBirthDay(struct date& birthday,BOOL IsSolar=TRUE,int16 ViewYear=0);
	BOOL GetJunYokDay(struct date& junyokday);

};
#endif