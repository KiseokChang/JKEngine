#ifndef _EQPLSTBX_H
	#define _EQPLSTBX_H

#include "listbox.h"

class EquipListBox:public JKListBox
{
public:
	EquipListBox(JKRect& rect,WORD ctlid,JKControl *parent=0);
  ~EquipListBox();

  BOOL Add(char *str);
  BOOL Destroy(int16 index);
  BOOL DestoryAll();
};
#endif