#ifndef _CONTROL_H
#define _CONTROL_H

#include <stdio.h>
#include <alloc.h>
#include <string.h>
#include "jkprog.h"
#include "jkscrndc.h"
#include "tmpltdll.h"

#define BORDER_TOP			1
#define BORDER_BOTTOM		2
#define BORDER_LEFT			3
#define BORDER_RIGHT		4

#define BORDER_CORNER1	5
#define BORDER_CORNER2	6
#define BORDER_CORNER3	7
#define BORDER_CORNER4	8

#define TITLEBOX				9
#define SYSTEMBOX				10

enum ProcessResult
{
	PROCESS_OK = 0,
	PROCESS_SKIP,
	PROCESS_FINISH
};



#define WA_NOHASFOCUS				0x0001

#define WA_NOINCLIENT   		0x0002
#define WA_NORESTRICT   		0x0004
#define WA_FIRSTSHOW				0x0008

#define WA_ISMOST						0x0010
#define WA_ISTOP						0x0020

#define WA_TITLEMOVEABLE		0x0040
#define WA_BORDERRESIZABLE 	0x0080
#define WA_CHANGEPLACE			0x0100


#define WW_WINDOWMOVE    	0x01
#define WW_WINDOWRESIZE 	0x02

class JKControl : public DLLFrame<JKControl>
{
public:
	JKControl(JKRect& rect,WORD ctrlid,JKControl *parent=0);
	virtual ~JKControl();

	BOOL IsEqual(JKControl *node,WORD compkind);
	JKControl *Duplicate();

	virtual	void InitWindow();
	virtual	void OpenWindow();
	virtual	void SetupWindow(){}
	virtual	void CloseWindow();
	virtual	BOOL CanClose();

	virtual void ShowWindow();
	virtual void HideWindow();
	virtual void EvShowWindow();
	virtual void EvHideWindow(BOOL isreal=TRUE);

	virtual WORD DonateRegion(JKRect winrect,RectQue& rectque,BOOL haschild=TRUE);
	virtual WORD GainRegion(RectQue& rectque,BOOL haschild=TRUE);
	BOOL IsContainPoint(JKPoint& p,BOOL isall=FALSE);

	virtual ProcessResult RespondMessage(JKMSG& msg);
	virtual ProcessResult ProcessMessage(JKMSG msg);

	virtual void SendMessage(WORD main,DWORD sub=0,DWORD detail=0,WORD option=0);
	virtual void ExecuteMessage(WORD main,DWORD sub=0,DWORD detail=0,WORD option=0);
	virtual void SendCommand(WORD cmdid,WORD data=0,DWORD detail=0,WORD option=0);
	virtual void ExecuteCommand(WORD cmdid,WORD data=0,DWORD detail=0,WORD option=0);
	virtual void SendControlMessage(WORD main,WORD data=0,DWORD detail=0,WORD option=0);
	virtual void ExecuteControlMessage(WORD main,WORD data=0,DWORD detail=0,WORD option=0);
	virtual void SendControlCommand(WORD cmdid,WORD data=0,DWORD detail=0,WORD option=0);
	virtual void ExecuteControlCommand(WORD cmdid,WORD data=0,DWORD detail=0,WORD option=0);


	virtual JKRect CalcClientRect();
	virtual int16 FindWindowElement(JKPoint& p);

	virtual BOOL RedrawWindow();
	virtual BOOL RedrawClient(BOOL iserase=TRUE,JKRect rect=JKRect());
	void Repaint(RectQue& rectque,DWORD winid=0,BOOL iserase=TRUE,JKRect cliprect=JKRect(0,0,-1,-1));
	void Repaint(JKRect rect,DWORD winid=0,BOOL iserase=TRUE);
	BOOL EvRepaint(JKRect rect,BOOL iserase=TRUE);
	virtual void DrawEmptyClient(JKDC& dc,JKRect rect);
	virtual void PaintWindow(JKDC&,JKRect& rect){}
	virtual void PaintClient(JKDC&,JKRect& rect){}

	JKDC *CreateScreenDC(JKDC& dc=*::GlobalDC);
	JKDC *CreateWindowDC(JKRect rect,RectQue *minus=0,JKDC& dc=*::GlobalDC);
	JKDC *CreateClientDC(JKRect rect,RectQue *minus=0,JKDC& dc=*::GlobalDC);
	JKDC *CreateWindowDC(RectQue *minus=0,JKDC& dc=*::GlobalDC)
	{ return CreateWindowDC(GetWindowRectOnScreen(),minus,dc); }
	JKDC *CreateClientDC(RectQue *minus=0,JKDC& dc=*::GlobalDC)
	{ return CreateClientDC(GetClientRectOnScreen(),minus,dc); }

	JKRect GetWindowRectOnScreen();
	JKRect GetClientRectOnScreen();
	JKRect GetClientRect();
	JKPoint ScreenToClient(JKPoint point=JKPoint(0,0));
	JKPoint ClientToScreen(JKPoint point=JKPoint(0,0));

	void ChangeWindowSize(JKRect rect);
	void PlaceForward();
	void PlaceBackward();

	virtual void EvMoveWindow(JKRect ){}
	virtual void EvResizeWindow(JKRect ){}
	virtual void EvPlaceForward(){}
	virtual void EvPlaceBackward(){}

	virtual void EvCharDown(WORD ,WORD ){}
	virtual void EvKeyDown(WORD ,WORD ){}
	virtual void EvDeadCharDown(WORD ,WORD ){}
	virtual void EvDeadKeyDown(WORD ,WORD ){}

	virtual void EvOnlyMove(JKPoint&){}
	virtual void EvOnlyStaying(JKPoint&){}
	virtual void EvLeftClick(JKPoint&){}
	virtual void EvLeftPressing(JKPoint&){}
	virtual void EvLeftRelease(JKPoint&){}
	virtual void EvLeftDragging(JKPoint&){}
	virtual void EvRightClick(JKPoint&){}
	virtual void EvRightPressing(JKPoint&){}
	virtual void EvRightRelease(JKPoint&){}
	virtual void EvRightDragging(JKPoint&){}

	virtual void EvNcOnlyMove(JKPoint&){}
	virtual void EvNcOnlyStaying(JKPoint&){}
	virtual void EvNcLeftClick(JKPoint&){}
	virtual void EvNcLeftPressing(JKPoint&){}
	virtual void EvNcLeftRelease(JKPoint&){}
	virtual void EvNcLeftDragging(JKPoint&){}
	virtual void EvNcRightClick(JKPoint&){}
	virtual void EvNcRightPressing(JKPoint&){}
	virtual void EvNcRightRelease(JKPoint&){}
	virtual void EvNcRightDragging(JKPoint&){}

	virtual void EvElOnlyMove(WORD ){}
	virtual void EvElOnlyStaying(WORD ){}
	virtual void EvElLeftClick(WORD ){}
	virtual void EvElLeftPressing(WORD ){}
	virtual void EvElLeftRelease(WORD ){}
	virtual void EvElLeftDragging(WORD ){}
	virtual void EvElRightClick(WORD ){}
	virtual void EvElRightPressing(WORD ){}
	virtual void EvElRightRelease(WORD ){}
	virtual void EvElRightDragging(WORD ){}

	virtual void EvTimerActive(){}
	virtual void EvTimerHappen(DWORD happencount){}

	virtual void EvCommand(WORD cmdid,WORD data,DWORD detail,WORD option){}

	virtual void EvSetFocus(DWORD lostid){}
	virtual void EvKillFocus(DWORD getid){}

//=====================================================================

	virtual void EvCtrlShowWindow(WORD ctrlid){}
	virtual void EvCtrlHideWindow(WORD ctrlid){}
	virtual void EvCtrlSetFocus(WORD ctrlid){}
	virtual void EvCtrlKillFocus(WORD ctrlid){}

	virtual void EvCtrlOnlyMove(WORD /*ctrlid*/){}
	virtual void EvCtrlOnlyStaying(WORD /*ctrlid*/){}
	virtual void EvCtrlLeftClick(WORD /*ctrlid*/){}
	virtual void EvCtrlLeftPressing(WORD /*ctrlid*/){}
	virtual void EvCtrlLeftRelease(WORD /*ctrlid*/){}
	virtual void EvCtrlLeftDragging(WORD /*ctrlid*/){}
	virtual void EvCtrlRightClick(WORD /*ctrlid*/){}
	virtual void EvCtrlRightPressing(WORD /*ctrlid*/){}
	virtual void EvCtrlRightRelease(WORD /*ctrlid*/){}
	virtual void EvCtrlRightDragging(WORD /*ctrlid*/){}

	virtual void EvButtonClick(WORD /*ctrlid*/){}
	virtual void EvButtonPressing(WORD /*ctrlid*/){}
	virtual void EvButtonRelease(WORD /*ctrlid*/){}
  virtual void EvCheckToggle(WORD /*ctrlid*/,BOOL /*status*/){}
	virtual void EvSBScrollUp(WORD /*ctrlid*/,WORD /*curpos*/){}
	virtual void EvSBScrollDown(WORD /*ctrlid*/,WORD /*curpos*/){}
	virtual void EvSBScrollLeft(WORD /*ctrlid*/,WORD /*curpos*/){}
	virtual void EvSBScrollRight(WORD /*ctrlid*/,WORD /*curpos*/){}
	virtual void EvSBScrollJump(WORD /*ctrlid*/,WORD /*curpos*/){}
	virtual void EvLBTouchEntry(WORD /*ctrlid*/,uint16 /*index*/){}
	virtual void EvLBSelectEntry(WORD /*ctrlid*/,uint16 /*index*/){}
	virtual void EvLBSetFirstIndex(WORD /*ctrlid*/,uint16 /*index*/){}
	virtual void EvLBCharDown(WORD /*ctrlid*/,uint16 /*index*/,uint16 /*key*/,WORD /*modifier*/){}
	virtual void EvLBKeyDown(WORD /*ctrlid*/,uint16 /*index*/,uint16 /*key*/,WORD /*modifier*/){}
	virtual void EvEditFinish(WORD /*ctrlid*/){}
	virtual void EvEditCancel(WORD /*ctrlid*/){}
	virtual void EvEditSetText(WORD /*ctrlid*/){}
	virtual void EvSpinUp(WORD /*ctrlid*/){}
	virtual void EvSpinDown(WORD /*ctrlid*/){}

	WORD ControlID;

	DWORD WinID;
	JKControl *Parent;
	JKControl *prev;
	JKControl *next;

	JKRect 	WinRect;
	JKRect 	ClientRect;
	JKPoint	ClientOrgPos;
	JKPoint	MinimumSize;
	WORD 		IsShow;
	BOOL  	IsInit;
	int16 	TextColor;
	int16 	BackColor;
	WORD		WinAttr;
	RectQue ShowRegion;
	WORD CursorID;
	char *WinText;
	BOOL IsRemoveable;
	BOOL CreationError;

	virtual BOOL SetText(char *buf);
	virtual char *GetText()	{	return WinText;	}

	void BeginCapture();
	DWORD EndCapture();
	BOOL HasFocus(){ return WinID==MsgFilter->FocusWinID? TRUE:FALSE;}
	void SetFocus();
	BOOL KillFocus();
	BOOL HasTimer();
	void SetTimer(int32 interval);
	void KillTimer();



	BOOL AddControl(JKControl *ctrl);
	JKControl *GetControl(WORD ctrlid);

	BOOL FindSubWindow(DWORD winid,WORD& ctrlid,BOOL isall=FALSE);
	BOOL TestSubWindow(DWORD winid,BOOL isall=FALSE);
	BOOL IsContainFocus();

	BOOL ScrollClient(JKPoint& offset);
	BOOL RepaintRestRegion(RectQue& oldregion,RectQue& newregion);

	BOOL StartWindowMove();
	BOOL StartWindowResize(BYTE selborder);

protected:
	BOOL PreEvElLeftClick(WORD element);
	BOOL PreEvLeftDragging(JKPoint& p);
	BOOL PreEvLeftRelease(JKPoint& p);

	JKRect GetVirtualWindowRect();
	JKRect GetVirtualClientRect();
	WORD AllocShowRegion();
	void FreeShowRegion();

	void VirtualShowWindow();
	void VirtualHideWindow(BOOL isreal=TRUE);

	virtual BOOL EvCloseWindow();
	virtual void DestroySubWindow(DWORD winid);
	virtual void DestroyAllSubWindow();


	WORD WorkID;
	JKPoint  WorkPos;
	JKRect ResizeRect;
	BYTE SelectBorder;

	RectQue PrevRegion;
	JKRect 	PrevRect;
	BYTE 	 *PrevImage;
};

extern RectQue NotRedrawRegion;
#endif

