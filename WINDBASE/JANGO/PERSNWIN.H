#ifndef _PERSNWIN_H
#define _PERSNWIN_H

#include "persnman.h"
#include "recview.h"

class PersonWindow:public RecordViewBase
{
public:
	PersonWindow(PersonManager& recman,PersonRecord& recbase,int16 currec,
							 WORD *& srcharray,uint16& srchcount,BOOL enmodify,BOOL viewkmpi=FALSE,JKControl *parent=0);
 ~PersonWindow();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void EvShowWindow();
	void EvDeadKeyDown(uint16 key,WORD modifier);

	void AddSearchEntry(WORD index);
	void EvEntryCtrlEndModify(WORD ctrlid);
	BOOL PrintRecord();

	PersonManager& RecordMan;
	int16 CurrentIndex;

	WORD *& SearchArray;
	uint16& SearchCount;
	BOOL EnModify;
	BOOL ViewKMPI;
};

#endif