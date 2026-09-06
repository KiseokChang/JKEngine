#ifndef _INGADLG_H
#define _INGADLG_H

#include "inwonman.h"
#include "jkdialog.h"


class IngaDialog : public JKDialog
{
public:
	IngaDialog(uint16 len,char *fname=0);
 ~IngaDialog();

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
	BYTE FileName[13];
};
#endif