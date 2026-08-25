#ifndef _EQPPRINT_H
	#define _EQPPRINT_H

#include "eqfilemn.h"
#include "printman.h"
#include "jkdialog.h"

class EquipPrint:public JKDialog
{
public:
	EquipPrint();
  ~EquipPrint();
  void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& paintrect);

  void EvButtonClick(WORD ctrlid);

	BOOL PrintHeadLine();
	BOOL PrintHeadLine2();
  BOOL PrintTailLine();
  BOOL PrintListString(BOMB& bm);
  BOOL PrintListString2(BOMB& bm);
  BOOL PrintState();
  BOOL FindBomb(BOMB& bm);

public:
	BOMB Bomb,CompareBomb;
  BOOL SomoFlag;
};
#endif