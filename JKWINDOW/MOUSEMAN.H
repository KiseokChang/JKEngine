#ifndef MOUSEMAN_H
#define MOUSEMAN_H

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include "jkpoint.h"
#include "msgcreat.h"

//  Declare and Define for Class Mouse & MouseManager

typedef struct {
	uint16 xkey,ykey;
	BYTE CursorImage[2][260];
}CursorType;

#define MOUSE_INTERRUPT		0x33


#define InternalCursorCount 10
enum InternalCursorName{
	CURSOR_ARROW=0,CURSOR_CHECK,CURSOR_CROSS,CURSOR_GLOVE,CURSOR_IBEAM,CURSOR_CLOCK,
	CURSOR_BORDER1,CURSOR_BORDER2,CURSOR_BORDER3,CURSOR_BORDER4

};
extern CursorType InternalCursor[InternalCursorCount];


class MouseManager : public MessageCreator
{
public:
	enum ButtonEvent
	{
		NAVIGATE=0,
		STAYING,
		CLICK,
		PRESSING,
		RELEASE,
		DRAGGING,
		NOACTION
	};


	MouseManager();
 ~MouseManager();

	BOOL InitEventHandler();

	JKPoint LastDrawPos;
	JKPoint LastCheckPos;
	int16 CursorShowFlag;

	void ShowCursor();
	void HideCursor();

	void SetCursorPos(JKPoint pos);
	JKPoint GetCursorPos();
	BOOL LeftButtonPressed();
	BOOL RightButtonPressed();
	BOOL LeftButtonRelease();
	BOOL RightButtonRelease();

	void SetActiveRegion(JKRect rect);
	JKRect& GetActiveRegion(){ return ActiveRegion;}

	BOOL SetCursorID(WORD resid);
	WORD GetCursorID(){return CursorID; }

	BOOL GetCurrentStatus();
	BOOL SetMessagable(BOOL isenable);
	BOOL CreateMessage(MessageQue *msgque=::SystemMsgQue);

	uint16 PressingDelay;
	uint16 PressingInterval;

	void DrawCursor(JKPoint& p,BYTE kind=0x03);
private:
	void SubDrawCursor(JKPoint& p);

	JKRect ActiveRegion;
	WORD CursorID;

	class Event
	{
	public:
		ButtonEvent EventID;
		int16 DelayCount;

		Event():
			EventID(NAVIGATE),DelayCount(0)
		{}

		Event& operator = (Event& other)
		{
			EventID = other.EventID;
			DelayCount=other.DelayCount;
			return *this;
		}
	} LeftEvent,RightEvent;
	BOOL LeftButton;     // FALSE => UP, TRUE => DOWN
	BOOL RightButton;

	CursorType CursorBuffer;
	BYTE *CursorImgeBuffer;
};

//===================================//
extern MouseManager * MouseEvents;//////
//===================================//

#endif