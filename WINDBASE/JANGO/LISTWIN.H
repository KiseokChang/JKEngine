#ifndef _LISTWIN_H
#define _LISTWIN_H

#include "persnman.h"
#include "jkwindow.h"

class ListWindow:public JKWindow
{
public:
	ListWindow(JKRect rect,WORD ctrlid,PersonManager& recman,WORD *& srcharray,uint16& srchcount);
 ~ListWindow();

	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);
	void EvLBCharDown(WORD ctrlid,uint16 index,uint16 key,WORD modifier);
	void EvLBKeyDown(WORD ctrlid,uint16 index,uint16 key,WORD modifier);
	void EvCommand(WORD cmdid,WORD,DWORD,WORD);

	ProcessResult RespondMessage(JKMSG& msg);
	void EvFinishAdd(uint16 index);
	void EvFinishModify(uint16 index);
	void EvFinishSearch(BOOL isscope);
	void EvFinishDelete(uint16 index);
	void EvFinishBackup(uint16 index);

	void InitList();
	void SortView(uint16 index);
	void SortView(WORD *compoder,uint16 ordercount);

	void SortFile(uint16 index);
	void SortFile(WORD *compoder,uint16 ordercount);


	char *CreateListString(PersonRecord& rec);
	BOOL PrintList();

	PersonManager& RecordMan;
	WORD *& SearchArray;
	uint16& SearchCount;
};

#endif