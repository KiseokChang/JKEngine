#ifndef _SAGOMAN_H
#define _SAGOMAN_H

#include <stdio.h>
#include "tmpltdll.h"
#include "persnman.h"

class SagoNode
{
public:
	SagoNode(uint16 index=0);
 ~SagoNode();
	BOOL IsEqual(SagoNode *node,uint16 compkind);
	SagoNode *Duplicate();

	void Assign(SagoNode& other);

	void Get(FILE *fp);
	void Put(FILE *fp);

	uint16 Index;

	SagoNode *prev,*next;
};


class SagoBase : public DLLFrame<SagoNode>
{
public:
	SagoBase(char *title=0);
 ~SagoBase();

	void SetText(char *str);

	SagoNode *GetSagoNode(uint16 index);
	BOOL DeleteSagoNode(uint16 budae,uint16 index,BOOL isganbu);
	BOOL AddSagoNode(uint16 budae,uint16 index,BOOL isganbu);

	BOOL IsEqual(SagoBase *node,uint16 compkind);
	void Assign(SagoBase& other);
	SagoBase *Duplicate();

	void Get(FILE *fp);
	void Put(FILE *fp);

	char Text[9];
	uint16 SagoCount[4][2];
	SagoBase *prev,*next;
};

class SagoManager : public DLLFrame<SagoBase>
{
public:
	SagoManager(PersonManager& recman,char *fname=0);
 ~SagoManager();
	SagoBase *GetSagoBase(char *title);
	SagoBase *GetSagoBase(uint16 index);
	SagoNode *GetSagoNode(uint16 index);
	BOOL DeleteSagoBase(char *title);
	BOOL DeleteSagoNode(uint16 budae,uint16 index,BOOL delempty=TRUE);
	BOOL AddSagoBase(char *title);
	BOOL AddSagoNode(char *title,uint16 budae,uint16 index);
	void Read();
	void Write();
	void Get(FILE *fp);
	void Put(FILE *fp);

	uint16 GetTotalCount();
	uint16 GetGanbuCount();
	uint16 GetSabyungCount();


	char FileName[13];
	PersonManager& PersonMan;
};

#endif