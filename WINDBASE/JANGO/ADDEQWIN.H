#ifndef _ADDEQUIPWIN_H
#define _ADDEQUIPWIN_H

//#include "jkwindow.h"
#include "eqfilemn.h"
#include "jkdialog.h"

static char *EqpBudaeName[4]={"¥¥¦Í¡”","A Í¡”","B Í¡”","C Í¡”"};

class AddEquipWindow:public JKDialog//JKWindow
{
public:
	AddEquipWindow(WORD ctrlid);
 ~AddEquipWindow();

	void SetupWindow();
	int16 SearchFile(char *filename);
	void ChangeStatic(WORD ctrlid,char *filename);
	void SetStaticName();
	void MakePodaeName();
	void AddFile(char *dest,char *scr,char *s);
	BOOL MakeBackupFile(char *file,char *backfile);
	void FileUnity();
	void MakeFileName();

	void EvCharDown(uint16 key,WORD modifier);
	void EvButtonClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);
public:
	BOOL ExistFile[10];
	char DaeDaeName[4];
	char PodaeName[4][8];

	char FileName[4][13];
	char SomoFileName[4][13];
	char MainEquipFile[2][13];
	char MainSomoFile[2][13];
};

#endif
