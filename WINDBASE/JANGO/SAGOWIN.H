#ifndef _SAGOWIN_H
#define _SAGOWIN_H

#include "inwonman.h"
#include "sagoman.h"
#include "jkwindow.h"

class SagoWindow:public JKWindow
{
public:
	SagoWindow(JKRect rect,WORD ctrlid,SagoManager& recman,WORD *& srcharray,uint16& srchcount);
 ~SagoWindow();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);

	void EvSelectCell(int16 line,int16 kind);


	void CreatePresentCondition();

	void InitTable(int16 line=-1,int16 kind=-1);
	void InitList(int16 line=-1,int16 kind=-1);
	char *CreateListString(RecordBase& rec,uint16 count=4);

	WORD CondEntryID;
	WORD *& SearchArray;
	uint16& SearchCount;
	SagoManager& RecordMan;
	InwonManager CurrentMan;
	uint16 SagoTextLen;
	int16 SelectLine,SelectKind;
};

#endif