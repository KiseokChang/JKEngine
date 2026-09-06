#include "mouseman.h"
#include "keybdman.h"
#include "timerman.h"
#include "msgfilt.h"

MessageFilter::MessageFilter():
		FocusWinID(0),CaptureWinID(0)
{}

BOOL MessageFilter::PatchMessage()
{
	BOOL ExistFlag = FALSE;
	if(KeyboardEvents->CreateMessage()) ExistFlag = TRUE;
	if(MouseEvents->CreateMessage()) ExistFlag = TRUE;
	if(TimerMan->CreateMessage()) ExistFlag = TRUE;
	return ExistFlag;
}

void MessageFilter::TranslateMessage()
{
	MsgQue->AppendDLLFrame(*SystemMsgQue);
}

void MessageFilter::MessageNormalize(MessageQue& msgque)
{
	MessageQue TempMsgQue;
	JKMSG msg;
	RepaintRegion.ClearNode();
	NopaintRegion.ClearNode();
	BOOL IsErase=FALSE;
	while(msgque.GetMessage(msg)){
		if(!msg.winid && msg.main==REPAINT){
			if(msg.option) IsErase=TRUE;
			RepaintRegion.PutRect(JKRect(msg.sub,msg.detail));
		}
		else TempMsgQue.PutMessage(msg);
	}

	if(RepaintRegion.NodeCount){
		RepaintRegion.MinimizeCount();
		JKRect rect=RepaintRegion.HeadNode->Rect;
		RectContainer *contain=RepaintRegion.HeadNode;
		while(contain){
			rect=rect.CalcPlusRegion(contain->Rect);
			contain=contain->next;
		}
		NopaintRegion.PutRect(rect);
		if(NopaintRegion.MinusRect(RepaintRegion)) NopaintRegion.MinimizeCount();

		msg.winid=0L;
		msg.main=REPAINT;
		msg.sub   = rect.LeftTop().GetDWORD();
		msg.detail= rect.RightBottom().GetDWORD();
		msg.option= IsErase? 1:0;
		msgque.PutMessage(msg);
	}
	msgque.AppendDLLFrame(TempMsgQue);
}

void MessageFilter::InitFilter()
{
	SetFocus(0);
	BeginCapture(0);
}

MessageFilter& MessageFilter::operator=(MessageFilter& other)
{
	FocusWinID   = other.FocusWinID;
	CaptureWinID = other.CaptureWinID;
	return *this;
}

BOOL MessageFilter::SetFocus(DWORD winid)
{
	if(FocusWinID!=winid){
		if(FocusWinID) MsgQue->SendMessage(0,KILLFOCUS,FocusWinID,winid);
		if(winid) MsgQue->SendMessage(0,SETFOCUS,winid,FocusWinID);
		FocusWinID=winid;
		return TRUE;
	}
	return FALSE;
}

DWORD MessageFilter::BeginCapture(DWORD winid)
{
	DWORD oldcapture=CaptureWinID;
	if(winid)
		CaptureWinID=winid;
	return oldcapture;
}

DWORD MessageFilter::EndCapture(DWORD winid)
{
	DWORD oldcapture=CaptureWinID;
	if(winid==CaptureWinID){
		CaptureWinID=0;
		return oldcapture;
	}
	return 0l;
}

/////////////////////////////
MessageFilter *MsgFilter=0;//
/////////////////////////////