#ifndef _INPBALSU_H
#define _INPBALSU_H

#include "firerec.h"
#include "jkdialog.h"

class InputBalusuDialog:public JKDialog
{
public:
	InputBalusuDialog(FireRecord& rec);
 ~InputBalusuDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);
	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	FireRecord& RefRecord;
};
#endif

