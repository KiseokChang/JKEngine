#ifndef _EQPTABLE_H
	#define _EQPTABLE_H

#include "inwonman.h"
#include "bombman.h"
#include "control.h"

#define INWONCTRLMESSAGE			(USERMESSAGE+0x200)
#define INWON_SELECTCELL			(INWONCTRLMESSAGE+1)
#define INWON_SETFIRSTINDEX   (INWONCTRLMESSAGE+2)


class EqpTable:public JKControl
{
public:
	EqpTable(WORD ctrlid,BombManager *cur,InwonManager *inga=0,JKControl *parent=0);
	~EqpTable();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void DrawCell(JKDC& dc,uint16 index);
	BOOL IsSelectedCell(uint16 index);
	int16 CheckCellIndex(JKPoint p);
	void EvButtonClick(WORD ctrlid);
	void EvLeftClick(JKPoint& p);
	void ChangeSelectCell(int16 line,int16 kind);
	BOOL SetShowFirstIndex(int16 index);
	void RedrawCell(JKDC& dc,int16 line=-1,int16 kind=-1);
	void RedrawTable();

	uint16 ShowFirstIndex;
	int16 SelectLine;
	int16 SelectKind;

	RectQue CellRegion;
	InwonManager *IngaInwon;
	BombManager *CurrentBomb;
};
#endif
