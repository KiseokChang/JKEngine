#ifndef _INCOMMON_H
#define _INCOMMON_H

#include "inwonman.h"
#include "persnman.h"
#include "jkwindow.h"

class CommonCondWin:public JKWindow
{
public:
	CommonCondWin(JKRect rect,WORD ctrlid,WORD entryid,PersonManager& recman,WORD *& srcharray,uint16& srchcount);
 ~CommonCondWin();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);
	void EvSelectCell(int16 line,int16 kind);


	virtual void InitPresentCondition();
	virtual void CreatePresentCondition();

	virtual void InitList();
	BOOL PrintPresentCondition();

	WORD CondEntryID;
	WORD *& SearchArray;
	uint16& SearchCount;
	PersonManager& RecordMan;
	InwonManager CommonMan;
	InwonManager IngaMan;
	uint16 InwonTextLen;
};
#endif