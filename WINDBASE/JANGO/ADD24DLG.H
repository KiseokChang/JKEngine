#ifndef _ADDEQP24DLG_H
#define _ADDEQP24DLG_H

#include "eq24dman.h"
#include "eqp24glb.h"
#include "jkdialog.h"

static char *BudaeName2[4]={"¥¥¦Í¡”","A Í¡”","B Í¡”","C Í¡”"};

class AddEquip24Dialog:public JKDialog
{
public:
	AddEquip24Dialog(WORD ctrlid);
 ~AddEquip24Dialog();

	void SetupWindow();
	int16 SearchFile(char *filename);
	void ChangeStatic(WORD ctrlid,char *filename);
	void SetStaticName();
	void MakePodaeName();
	void AddFile(char *ndest,char *kdest,char *nscr,char *kscr,char *s);
	BOOL MakeBackupFile(char *file,char *backfile);
	void FileUnity();
	void MakeFileName();
	void MakeAllBackupFile();

	void EvCharDown(uint16 key,WORD modifier);
	void EvButtonClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);

public:
	BOOL NameExistFile[4];
	BOOL KindExistFile[4];
	BOOL SNameExistFile[4];
	BOOL SKindExistFile[4];
	BOOL RNameExistFile[4];
	BOOL RKindExistFile[4];

	char DaeDaeName[4];
	char PodaeName[4][8];

	char FileName[6][13];
	char BackFileName[6][13];

	char KindFileName[4][13];
	char NameFileName[4][13];
	char SKindFileName[4][13];
	char SNameFileName[4][13];
	char RKindFileName[4][13];
	char RNameFileName[4][13];

	Equip24Global Eqp24Glb;

	Equip24DataManager EqpDest;
	Equip24DataManager EqpScr;

};

#endif
