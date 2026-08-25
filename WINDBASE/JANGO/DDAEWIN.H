#ifndef _DDAEWIN_H
	#define _DDAEWIN_H

#include "jkdialog.h"
#define DAEDAESELECTED USERMESSAGE+1

class DaedaeDialog:public JKDialog
{
public:
	DaedaeDialog(char* daedae,WORD ctrlid=0,JKControl *parent=0);
	~DaedaeDialog();
	void SetupWindow();
	void PaintClient(JKDC& dc,JKRect& rect);
	void MakeFileName();

	void EvCharDown(uint16 key,WORD modifier);
	void EvButtonClick(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);
	void EvEditFinish(WORD ctrlid);
	void EvShowWindow();

public:
	char* DaeDaeName;
};
#endif

