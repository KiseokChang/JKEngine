#ifndef _MSGFILT_H
#define _MSGFILT_H

#include "rectque.h"
#include "msgque.h"

class MessageFilter
{
public:
	MessageFilter();
 ~MessageFilter(){}

	BOOL PatchMessage();
	void TranslateMessage();
	void MessageNormalize(MessageQue& msgque);

	void InitFilter();
	MessageFilter& operator=(MessageFilter& other);

	BOOL SetFocus(DWORD winid);

	DWORD BeginCapture(DWORD winid);
	DWORD EndCapture(DWORD winid);

	DWORD FocusWinID;
	DWORD CaptureWinID;

	RectQue RepaintRegion;
	RectQue NopaintRegion;

};

//////////////////////////////////
extern MessageFilter * MsgFilter;//
//////////////////////////////////
#endif