#ifndef _JKWINDEF_H
#define _JKWINDEF_H

#include "keydef.h"
// Default Color Definition

#define Black					0
#define White					1
#define Blue					2
#define Green   			3
#define Cyan        	4
#define Red         	5
#define Magenta       6
#define Brown         7
#define LtBlue        8
#define LtGreen       9
#define LtCyan        10
#define LtRed         11
#define LtMagenta     12
#define Yellow        13

#define Gray          240
#define LtGray        248


// Message Main Part Definition

#define NOEVENT							0
#define CHARDOWN						1
#define KEYDOWN							2
#define DEADCHARDOWN				3
#define DEADKEYDOWN					4

#define ONLYMOVE					(0x100+1)
#define ONLYSTAYING				(ONLYMOVE+1)
#define LEFTCLICK					(ONLYMOVE+2)
#define LEFTPRESSING			(ONLYMOVE+3)
#define LEFTRELEASE				(ONLYMOVE+4)
#define LEFTDRAGGING			(ONLYMOVE+5)
#define RIGHTCLICK				(ONLYMOVE+6)
#define RIGHTPRESSING			(ONLYMOVE+7)
#define RIGHTRELEASE			(ONLYMOVE+8)
#define RIGHTDRAGGING			(ONLYMOVE+9)

#define NCONLYMOVE				(RIGHTDRAGGING+1)
#define NCONLYSTAYING			(NCONLYMOVE+1)
#define NCLEFTCLICK				(NCONLYMOVE+2)
#define NCLEFTPRESSING		(NCONLYMOVE+3)
#define NCLEFTRELEASE			(NCONLYMOVE+4)
#define NCLEFTDRAGGING		(NCONLYMOVE+5)
#define NCRIGHTCLICK			(NCONLYMOVE+6)
#define NCRIGHTPRESSING		(NCONLYMOVE+7)
#define NCRIGHTRELEASE		(NCONLYMOVE+8)
#define NCRIGHTDRAGGING		(NCONLYMOVE+9)

#define ELONLYMOVE				(NCRIGHTDRAGGING+1)
#define ELONLYSTAYING			(ELONLYMOVE+1)
#define ELLEFTCLICK				(ELONLYMOVE+2)
#define ELLEFTPRESSING		(ELONLYMOVE+3)
#define ELLEFTRELEASE			(ELONLYMOVE+4)
#define ELLEFTDRAGGING		(ELONLYMOVE+5)
#define ELRIGHTCLICK			(ELONLYMOVE+6)
#define ELRIGHTPRESSING		(ELONLYMOVE+7)
#define ELRIGHTRELEASE		(ELONLYMOVE+8)
#define ELRIGHTDRAGGING		(ELONLYMOVE+9)

#define TIMERMESSAGE			0x500
#define TIMERACTIVE				(TIMERMESSAGE+1)
#define TIMERHAPPEN				(TIMERMESSAGE+2)

#define CLOSEWINDOW								(0x1000+ 1)
#define CANCLOSE								  (0x1000+ 2)
#define DESTROYSUBWINDOW					(0x1000+ 3)

#define SHOWWINDOW								(0x1000+ 4)
#define HIDEWINDOW								(0x1000+ 5)

#define STARTWINDOWMOVE           (0x1000+ 6)
#define STARTWINDOWRESIZE         (0x1000+ 7)
#define MOVEWINDOW                (0x1000+ 8)
#define RESIZEWINDOW							(0x1000+ 9)
#define PLACEFORWARD							(0x1000+10)
#define PLACEBACKWARD							(0x1000+11)

#define REPAINT										(0x1000+12)
#define REDRAWCLIENT							(0x1000+13)

#define EV_EXIT										(0x1000+ 0x0100)

#define COMMAND										0x2000

#define SETFOCUS						  		(0x3000+ 1)
#define KILLFOCUS 								(0x3000+ 2)

#define SHOWCURSOR								(0x3000+ 3)
#define HIDECURSOR								(0x3000+ 4)
#define SETMOUSECURSOR						(0x3000+ 5)

#define APPEXIT										(0x3000+ 6)

#define CONTROLMESSAGE						0x4000

#define CONTROL_SHOWWINDOW				(CONTROLMESSAGE+ 1)
#define CONTROL_HIDEWINDOW				(CONTROLMESSAGE+ 2)
#define CONTROL_SETFOCUS					(CONTROLMESSAGE+ 3)
#define CONTROL_KILLFOCUS					(CONTROLMESSAGE+ 4)

#define CTRLMOUSEMESSAGE					(CONTROLMESSAGE+ 0x050)

#define CONTROL_ONLYMOVE					(CTRLMOUSEMESSAGE+ 1)
#define CONTROL_ONLYSTAYING				(CTRLMOUSEMESSAGE+ 2)
#define CONTROL_LEFTCLICK					(CTRLMOUSEMESSAGE+ 3)
#define CONTROL_LEFTPRESSING			(CTRLMOUSEMESSAGE+ 4)
#define CONTROL_LEFTRELEASE				(CTRLMOUSEMESSAGE+ 5)
#define CONTROL_LEFTDRAGGING			(CTRLMOUSEMESSAGE+ 6)
#define CONTROL_RIGHTCLICK				(CTRLMOUSEMESSAGE+ 7)
#define CONTROL_RIGHTPRESSING			(CTRLMOUSEMESSAGE+ 8)
#define CONTROL_RIGHTRELEASE			(CTRLMOUSEMESSAGE+ 9)
#define CONTROL_RIGHTDRAGGING			(CTRLMOUSEMESSAGE+10)

#define BUTTONMESSAGE							(CONTROLMESSAGE+ 0x0100)
#define BUTTON_CLICK							(BUTTONMESSAGE+ 1)
#define BUTTON_PRESSING						(BUTTONMESSAGE+ 2)
#define BUTTON_RELEASE						(BUTTONMESSAGE+ 3)

#define CHECKMESSAGE							(CONTROLMESSAGE+ 0x0110)
#define CHECK_TOGGLE							(CHECKMESSAGE+ 1)

#define SCROLLBARMESSAGE					(CONTROLMESSAGE+ 0x0150)
#define SB_SCROLLUP								(SCROLLBARMESSAGE+1)
#define SB_SCROLLDOWN							(SCROLLBARMESSAGE+2)
#define SB_SCROLLLEFT							(SCROLLBARMESSAGE+3)
#define SB_SCROLLRIGHT						(SCROLLBARMESSAGE+4)
#define SB_SCROLLJUMP							(SCROLLBARMESSAGE+5)

#define LISTBOXMESSAGE						(CONTROLMESSAGE+ 0x0200)
#define LB_TOUCHENTRY							(LISTBOXMESSAGE+1)
#define LB_SELECTENTRY						(LISTBOXMESSAGE+2)
#define LB_SETFIRSTINDEX					(LISTBOXMESSAGE+3)
#define LB_CHARDOWN								(LISTBOXMESSAGE+4)
#define LB_KEYDOWN								(LISTBOXMESSAGE+5)

#define EDITMESSAGE								(CONTROLMESSAGE+ 0x0300)
#define EDIT_BEGIN								(EDITMESSAGE+ 1)
#define EDIT_FINISH								(EDITMESSAGE+ 2)
#define EDIT_CANCEL								(EDITMESSAGE+ 3)
#define EDIT_SETTEXT							(EDITMESSAGE+ 4)


#define SPINMESSAGE								(CONTROLMESSAGE+ 0x0350)
#define SPIN_UP										(SPINMESSAGE+1)
#define SPIN_DOWN									(SPINMESSAGE+2)


#define DIALOGMESSAGE							0x5000
#define ID_OK											(DIALOGMESSAGE+1)
#define ID_CANCEL                 (DIALOGMESSAGE+2)
#define ID_YES                    (DIALOGMESSAGE+3)
#define ID_NO                     (DIALOGMESSAGE+4)

#define USERMESSAGE								 0xa000

/*=====================================================*/
#define ID_CHECKBOX				0x1000
#define ID_STATIC					0x2000
#define ID_BUTTON					0x3000
#define ID_EDIT						0x4000
#define ID_POPUP					0x5000
#define ID_LISTBOX				0x6000
#define ID_SCROLLBAR			0x7000
#define ID_SPIN						0x8000
#define ID_SUBWINDOW  		0x9000
#define ID_USERCONTROL		0xa000
/*=====================================================*/
#endif
