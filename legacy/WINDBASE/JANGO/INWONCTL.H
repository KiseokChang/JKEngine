#ifndef _INWONCTL_H
#define _INWONCTL_H

#include "persnrec.h"
#include "inwonman.h"
#include "control.h"

#define INWONCTRLMESSAGE			(USERMESSAGE+0x200)
#define INWON_SELECTCELL			(INWONCTRLMESSAGE+1)
#define INWON_RESELECTCELL		(INWONCTRLMESSAGE+2)
#define INWON_SETFIRSTINDEX   (INWONCTRLMESSAGE+3)


class InwonControl : public JKControl
{
public:
	InwonControl(JKPoint pos,WORD ctrlid,uint16 entrylen,InwonManager *cur,InwonManager *inga=0,uint16 linecount=7,JKControl *parent=0);
	~InwonControl();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void EvButtonPressing(WORD ctrlid);
	void EvLeftClick(JKPoint& p);

	void DrawCell(JKDC& dc,uint16 index);
	BOOL IsSelectedCell(uint16 index);
	int16 CheckCellIndex(JKPoint p);
	void ChangeSelectCell(int16 line,int16 kind);
	BOOL SetShowFirstIndex(uint16 index);
	void RedrawCell(JKDC& dc,int16 line=-1,int16 kind=-1);
	void DisplaySelectSum();

	uint16 ShowFirstIndex;
	int16 SelectLine;
	int16 SelectKind;

	RectQue CellRegion;
	InwonManager *CurrentInwon;
	InwonManager *IngaInwon;
	uint16 EntryLen;

	uint16 LineCount;
};

#endif