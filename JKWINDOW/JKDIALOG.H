#ifndef _JKDIALOG_H
#define _JKDIALOG_H

#include "evnthand.h"
#include "jkwindow.h"

class JKDialog:public EventHandler,public JKWindow
{
public:
	JKDialog(JKRect rect,JKControl *parent=0,WORD ctrlid=0);
	~JKDialog();

	void EvShowWindow();
	BOOL PreProcessMessage(JKMSG msg);

	WORD Run();

	virtual void EvDlgOk(){}
	virtual void EvDlgCancel(){}
	virtual void EvDlgYes(){}
	virtual void EvDlgNo(){}

	ProcessResult RespondMessage(JKMSG& msg);
	BOOL LoopBreak;
	WORD ReturnValue;
	MessageFilter SaveMsgFilter;

	BOOL IsModal;
};
#endif