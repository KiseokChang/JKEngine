#ifndef _PASSDLG_H
#define _PASSDLG_H

#include "jkdialog.h"

class PasswordDialog:public JKDialog
{
public:
	PasswordDialog(JKRect rect,char *passname,char *msg="",JKControl *parent=0);
	void SetupWindow();

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);

	BOOL GetPassword(char *buffer);

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);


	char *Password;
	uint16 PasswordLength;
	char *MessageString;
};
#endif