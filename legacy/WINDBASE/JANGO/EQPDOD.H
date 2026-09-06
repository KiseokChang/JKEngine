#ifndef _EQPDOD_H
	#define _EQPDOD_H

#include "jkwindow.h"

#define EQPDODMESSAGE	USERMESSAGE+70

class EqpDOD:public JKWindow
{
public:
	EqpDOD(JKRect rect,WORD ctrlid,int16 len,JKControl *parent=0);
	~EqpDOD();

	void SetupWindow();
	void SetExplanText(char *s);
	void EvShowWindow();
	void EvButtonClick(WORD ctrlid);
	void EvCharDown(uint16 key,WORD modifier);
	void EvEditFinish(WORD /*ctrlid*/);
	void EvEditCancel(WORD /*ctrlid*/);

public:
	char *Buffer;
	int16  Length;
protected:
	char *ExplanText;

private:
	BOOL CreationError;
};


#endif