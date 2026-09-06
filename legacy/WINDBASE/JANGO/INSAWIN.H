#ifndef _INSAWIN_H
#define _INSAWIN_H

#include "ctrlres.h"
#include "persnman.h"
#include "sagoman.h"
#include "insamsg.h"
#include "jkdialog.h"

class InsaWindow:public JKDialog,public PersonManager
{
public:
	InsaWindow();
 ~InsaWindow();

	void InitWindow();
	void SetupWindow();

	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvShowWindow();
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	void EvCtrlOnlyMove(WORD ctrlid);
	void EvCtrlLeftClick(WORD ctrlid);
	void EvButtonClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);

	void ViewChange(int16 index);

	void EvRequireDelete(WORD ctrlid,int16 index);
	void EvRequireBackup(WORD ctrlid,int16 index);
	void EvFinishAdd();
	void EvFinishDelete();
	void EvFinishSearch();
	void EvFinishBackup();
	void EvFinishModify();
	void EvCancelAdd();
	void EvCancelDelete();
	void EvCancelSearch();
	void EvCancelBackup();
	void EvCancelModify();

	void InitByFile();
	BOOL FreeIndex(uint16 index);
	BOOL BackupRecord(uint16 index);
	int16 AddRecord(RecordBase& rec);
	int16 InsertRecord(uint16 index,RecordBase& rec);

	void AdjustTitleAmount();

	int16 CurrentView;
	WorkProcess WorkingProcess;

	WORD *SearchArray;
	uint16 SearchCount;


	JKRect SubWinRect;
	CtrlResManager CtrlResMan;
	int16 ExplainID;

	SagoManager SagoMan;
};

#endif