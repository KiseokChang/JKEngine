#ifndef _PASSDLG_H
#define _PASSDLG_H

#include "jkdialog.h"

#define CHANGESTEP		(USERMESSAGE+0x0100)
#define CHANGESTEP1		(CHANGESTEP+1)
#define CHANGESTEP2		(CHANGESTEP+2)
#define CHANGESTEP3		(CHANGESTEP+3)
#define CHANGESTEP4		(CHANGESTEP+4)


class PasswordDialog:public JKDialog
{
public:
	PasswordDialog(JKRect rect,uint16 passindex,char *msg="",JKControl *parent=0);
	void SetupWindow();

	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);

	BOOL GetPassword(char *buffer);
	ProcessResult RespondMessage(JKMSG& msg);
	void EvChangeStep1();
	void EvChangeStep2();
	void EvChangeStep3();
	void EvChangeStep4();

	void EvDeadKeyDown(uint16 key,WORD /*modifier*/);


	uint16 PassIndex;
	char *Password;
	uint16 PasswordLength;
	char *MessageString;
	uint16 ChangeStep;
	BYTE NewPassword[9];
};
#endif