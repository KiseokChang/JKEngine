#ifndef _PERSNMAN_H
#define _PERSNMAN_H

#include "obsman.h"
#include "persnrec.h"
#include "recman.h"

class PersonManager : public RecordManager
{
public:
	PersonManager(char *fname=0,char *obsvname=0);
 ~PersonManager();

	BOOL ChangeRecordBase(char *fname,RecordBase& rec);
	virtual void ChangeManagerMode(BOOL mode);

	void InitByFile();
	BOOL MakeEmptyFile(uint16 alloccount);
	void PutHeader(FILE *fp1,FILE *fp2);

	BOOL FreeIndex(uint16 index);
	virtual BOOL BackupRecord(uint16 index);
	int16 AddRecord(RecordBase& rec);
	int16 InsertRecord(uint16 index,RecordBase& rec);
	BOOL SetRecord(uint16 index,RecordBase& rec);
	BOOL PrintRecord(uint16 index);

	BOOL DeleteBudae(uint16 budae);
	BOOL AddPersonMan(PersonManager *srcman,uint16 budae);


	uint16 OfficerCount;

	BOOL ManagerMode;

	PersonRecord *AddedRecord;
	ObserveManager ObserveMan;
};
#endif