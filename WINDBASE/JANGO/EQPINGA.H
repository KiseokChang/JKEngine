#ifndef _EQPINGA_H
#define _EQPINGA_H

#include "inwonman.h"
#include "jkdialog.h"


class EqpInga : public JKDialog
{
public:
	EqpInga(char *filename,uint16 len);
 ~EqpInga();

	void InitWindow();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

	void EvButtonClick(WORD ctrlid);
	void EvLBTouchEntry(WORD ctrlid,uint16 index);
	void EvEditFinish(WORD ctrlid);
	void SetEditControl();
	void EvDlgOk();
	void EvDlgCancel();

	void SetInga();
	void DeleteInga();
	BOOL MakeListString(char *buffer,InwonBase& base);


	InwonManager IngaMan;
	InwonBase IngaBuffer;
	char FileName[13];
};
#endif