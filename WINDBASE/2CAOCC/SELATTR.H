#ifndef _SELATTR_H
#define _SELATTR_H

#include "numentry.h"
#include "boolent.h"
#include "mt_char.h"
#include "jkdialog.h"

class SelectAttributeDialog:public JKDialog
{
public:
	SelectAttributeDialog(JKPoint p,MetaTextOut& mttext,JKControl *parent=0);
 ~SelectAttributeDialog();

	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect&);

	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	MetaTextOut& MtText;

	NumberEntry XSizeEntry;
	NumberEntry YSizeEntry;
	BooleanEntry TextAlignEntry;
	NumberEntry RotateAngleEntry;
	NumberEntry SlantAngleEntry;
};
#endif

