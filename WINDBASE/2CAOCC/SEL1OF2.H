#ifndef _SEL1OF2_H
#define _SEL1OF2_H

#include "jkdialog.h"

class SelectOneOfTwoDialog:public JKDialog
{
public:
	SelectOneOfTwoDialog(JKPoint p,char *text1,char *text2,JKControl *parent=0);
 ~SelectOneOfTwoDialog();

	void SetupWindow();
	void EvShowWindow();

	void EvButtonClick(WORD ctrlid);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);

	char Text1[100+1], Text2[100+1];
};
#endif

