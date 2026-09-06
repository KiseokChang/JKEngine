#include "control.h"

JKControl::JKControl(JKRect& rect,WORD ctrlid,JKControl *parent):
	DLLFrame<JKControl>(),prev(0),next(0),
	WinRect(rect),ClientOrgPos(),Parent(parent),ControlID(ctrlid),
	CreationError(FALSE),MinimumSize(2,2),
	IsInit(FALSE),IsShow(FALSE),WinID(0),WinAttr(0),
	TextColor(Black),BackColor(White),WinText(0),CursorID(0),
	WorkID(0),SelectBorder(0x00),IsRemoveable(TRUE),PrevImage(0)
{
	WinID = (DWORD)this;
	WinAttr|=WA_FIRSTSHOW;
}

JKControl::~JKControl()
{
	KillTimer();
	if(WinText) delete[] WinText;
	DestroyAllSubWindow();
	if(PrevImage) delete PrevImage;
}

void JKControl::InitWindow()
{
	if(!IsInit){
		OpenWindow();
		if(WinAttr&WA_FIRSTSHOW) ShowWindow();
		SetupWindow();
		IsInit=TRUE;
	}
}

BOOL JKControl::IsEqual(JKControl *node,WORD compkind)
{
	if(node) {
		if(compkind&0x01 && WinID!=node->WinID) return FALSE;
		if(compkind&0x02 && ControlID!=node->ControlID) return FALSE;
		return TRUE;
	}
	return FALSE;
}

JKControl *JKControl::Duplicate()
{
	return 0;
}


void JKControl::OpenWindow()
{
	ClientRect = CalcClientRect();
}

JKRect JKControl::CalcClientRect()
{
	JKRect rect =WinRect.SetLeftTopBy(JKPoint(0,0));
	return rect;
}

int16 JKControl::FindWindowElement(JKPoint& p)
{
	JKRect rect;
	int16 ret =0;
	p=ScreenToClient(p);
	p+=ClientRect.LeftTop();
	if(WinRect.SetLeftTopBy(JKPoint(0,0)).IsContain(p)){
		if(ClientRect.IsContain(p)) {	p-=ClientRect.LeftTop();	ret=-1;	}
		else { p=ClientToScreen(p); ret=0; }
	}
	else{
		p-=ClientRect.LeftTop();
		p=ClientToScreen(p);	ret=0;
	}
	return ret;
}

void JKControl::CloseWindow()
{
	SendMessage(CLOSEWINDOW);
}

BOOL JKControl::EvCloseWindow()
{
	JKControl *ctrl = HeadNode;
	while(ctrl) {
		if(!ctrl->IsRemoveable||!ctrl->EvCloseWindow()) return FALSE;
		ctrl=ctrl->next;
	}
	return CanClose();
}

BOOL JKControl::CanClose()
{
	return TRUE;
}

void JKControl::SendMessage(WORD main,DWORD sub,DWORD detail,WORD option)
{
	MsgQue->SendMessage(WinID,main,sub,detail,option);
}

void JKControl::ExecuteMessage(WORD main,DWORD sub,DWORD detail,WORD option)
{
	MsgQue->ExecuteMessage(WinID,main,sub,detail,option);
}

void JKControl::SendCommand(WORD cmdid,WORD data,DWORD detail,WORD option)
{
	MsgQue->SendMessage(WinID,COMMAND,MAKEDWORD(cmdid,data),detail,option);
}

void JKControl::ExecuteCommand(WORD cmdid,WORD data,DWORD detail,WORD option)
{
	MsgQue->ExecuteMessage(WinID,COMMAND,MAKEDWORD(cmdid,data),detail,option);
}

void JKControl::SendControlMessage(WORD main,WORD data,DWORD detail,WORD option)
{
	if(Parent) Parent->SendMessage(main,MAKEDWORD(ControlID,data),detail,option);
}
void JKControl::ExecuteControlMessage(WORD main,WORD data,DWORD detail,WORD option)
{
	if(Parent) Parent->ExecuteMessage(main,MAKEDWORD(ControlID,data),detail,option);
}

void JKControl::SendControlCommand(WORD cmdid,WORD data,DWORD detail,WORD option)
{
	if(Parent) Parent->SendCommand(cmdid,data,detail,option);
}

void JKControl::ExecuteControlCommand(WORD cmdid,WORD data,DWORD detail,WORD option)
{
	if(Parent) Parent->ExecuteCommand(cmdid,data,detail,option);
}

void JKControl::ShowWindow()
{
	if(!IsShow&&(!Parent||Parent->IsShow))
		SendMessage(SHOWWINDOW);
}

void JKControl::EvShowWindow()
{
	if(!IsShow&&(!Parent||Parent->IsShow)){
		JKRect winrect=GetWindowRectOnScreen();
		IsShow=TRUE;
		AllocShowRegion();
		RedrawWindow();

		JKControl *ctrl = HeadNode;
		while(ctrl) {
			if(ctrl->WinAttr&WA_FIRSTSHOW) ctrl->EvShowWindow();
			ctrl=ctrl->next;
		}
		SendControlMessage(CONTROL_SHOWWINDOW);
	}
}

void JKControl::HideWindow()
{
	if(IsShow) {
		EndCapture();
		KillFocus();
		SendMessage(HIDEWINDOW);
	}
}

void JKControl::EvHideWindow(BOOL isreal)
{
	if(IsShow){
		JKControl *ctrl = TailNode;
		while(ctrl) {
			ctrl->EvHideWindow(FALSE);
			ctrl=ctrl->prev;
		}

		RedrawWindow();
		if(!isreal && Parent){
			Parent->ShowRegion.AppendRectQue(ShowRegion);
			Parent->ShowRegion.MinimizeCount();
		}
		else{
			FreeShowRegion();
		}

		IsShow=FALSE;
		SendControlMessage(CONTROL_HIDEWINDOW);
	}
}

void JKControl::VirtualShowWindow()
{
	if(IsShow){
		JKRect winrect=GetWindowRectOnScreen();
		AllocShowRegion();
		JKControl *ctrl = HeadNode;
		while(ctrl) {
			ctrl->VirtualShowWindow();
			ctrl=ctrl->next;
		}

		RectQue *tempque=PrevRegion.Duplicate();
		if(tempque){
				NotRedrawRegion.AppendRectQue(*tempque);
				delete tempque;
		}
		NotRedrawRegion.MinusRect(ShowRegion);

		JKRect clientrect=GetClientRectOnScreen();
		JKDC *dc=GlobalDC;
		dc->InitDC(clientrect.LeftTop(),ShowRegion);
		dc->GetClipRegion().ClippingRegion(clientrect);
		dc->GetClipRegion().MinimizeCount();
		if(PrevImage) {
			MouseEvents->HideCursor();
			dc->PutImage(JKPoint(0,0),PrevImage);
			MouseEvents->ShowCursor();
			delete PrevImage;
			PrevImage=0;
		}

		JKPoint offset=clientrect.LeftTop()-PrevRect.LeftTop();
		PrevRegion.Offset(offset);
		dc->GetClipRegion().MinusRect(PrevRegion);
		NotRedrawRegion.AppendRectQue(dc->GetClipRegion());

		RectQue *winregion=ShowRegion.Duplicate();
		if(winregion){
			winregion->MinusRect(clientrect);
			NotRedrawRegion.AppendRectQue(*winregion);
			delete winregion;
		}
		PrevRegion.ClearNode();
	}
}

void JKControl::VirtualHideWindow(BOOL isreal)
{
	if(IsShow){
		if(PrevImage) delete PrevImage;

		PrevRect=GetClientRectOnScreen();
		PrevRegion=ShowRegion;
		PrevRegion.ClippingRegion(PrevRect);
		int width=PrevRect.Width()+1;
		int height=PrevRect.Height()+1;
		PrevImage=new BYTE[4+width*height];
		memset(PrevImage+4,BackColor,width*height);
		JKDC *dc=GlobalDC;
		dc->InitDC(PrevRect.LeftTop(),PrevRegion);
		dc->GetClipRegion().MinimizeCount();
		MouseEvents->HideCursor();
		dc->GetImage(PrevRect.SetLeftTopBy(),PrevImage);
		MouseEvents->ShowCursor();

		RectQue *tempque=ShowRegion.Duplicate();
		tempque->MinusRect(PrevRect);
		NotRedrawRegion.AppendRectQue(*tempque);
		delete tempque;

		JKControl *ctrl = TailNode;
		while(ctrl) {
			ctrl->VirtualHideWindow(FALSE);
			ctrl=ctrl->prev;
		}
		if(!isreal && Parent){
			Parent->ShowRegion.AppendRectQue(ShowRegion);
			Parent->ShowRegion.MinimizeCount();
		}
		else{
			FreeShowRegion();
		}
	}
}

// Get Max Rect if No NextWindow and No ChildWindow,but PrevWindow and ParentWindow
JKRect JKControl::GetVirtualWindowRect()
{
	if(IsShow){
		JKRect rect=GetWindowRectOnScreen().CalcOverlap(JKRect(0,0,getmaxx(),getmaxy()));
		if(rect.IsValid()){
			if(Parent && !(WinAttr&WA_NORESTRICT)){
				JKRect temp;
				if(WinAttr&WA_NOINCLIENT)	temp=Parent->GetVirtualWindowRect();
				else                     	temp=Parent->GetVirtualClientRect();
				if(temp.IsValid()) rect=rect.CalcOverlap(temp);
				else return JKRect(0,0,-1,-1);
			}
			return rect;
		}
	}
	return JKRect(0,0,-1,-1);
}

JKRect JKControl::GetVirtualClientRect()
{
	if(IsShow){
		JKRect rect=GetClientRectOnScreen().CalcOverlap(JKRect(0,0,getmaxx(),getmaxy()));
		if(rect.IsValid()){
			if(Parent && !(WinAttr&WA_NORESTRICT)){
				JKRect temp;
				if(WinAttr&WA_NOINCLIENT)	temp=Parent->GetVirtualWindowRect();
				else                     	temp=Parent->GetVirtualClientRect();
				if(temp.IsValid()) rect=rect.CalcOverlap(temp);
				else return JKRect(0,0,-1,-1);
			}
			return rect;
		}
	}
	return JKRect(0,0,-1,-1);
}

WORD JKControl::AllocShowRegion()
{
	if(Parent && WinAttr&WA_ISMOST){
		if(WinAttr&WA_ISTOP) Parent->SetNodeLast(Parent->GetIndex(this));
		else                 Parent->SetNodeFirst(Parent->GetIndex(this));
	}
	JKRect rect=GetVirtualWindowRect();
	if(rect.IsValid()){
		if(Parent){
			BOOL haschild,isb;
			JKControl *win=this;
			while(1){
				if(win->prev) {
					win=win->prev;
					haschild=TRUE;
				}
				else if(win->Parent) {
					win=win->Parent;
					haschild=FALSE;
				}
				else break;
				win->DonateRegion(rect,ShowRegion,haschild);
				if(ShowRegion.NodeCount==1 && ShowRegion.HeadNode->Rect==rect) return 1;
			}
		}
		else ShowRegion.PutRect(rect);
		ShowRegion.MinimizeCount();
	}
	return ShowRegion.NodeCount;
}

void JKControl::FreeShowRegion()
{
	if(Parent && WinAttr&WA_ISMOST){
		if(WinAttr&WA_ISTOP) Parent->SetNodeLast(Parent->GetIndex(this));
		else                 Parent->SetNodeFirst(Parent->GetIndex(this));
	}

	if(Parent){
		BOOL haschild;
		JKControl *win=this;
		while(1){
			if(win->prev){
				win=win->prev;
				haschild=TRUE;
			}
			else if(win->Parent){
				win=win->Parent;
				haschild=FALSE;
			}
			else break;

			if(!win->GainRegion(ShowRegion,haschild)) return;
		}
	}
	ShowRegion.ClearNode();
}

WORD JKControl::DonateRegion(JKRect winrect,RectQue& rectque,BOOL haschild)
{
	if(IsShow){
		if(haschild){
			JKControl *ctrl = TailNode;
			while(ctrl) {
				ctrl->DonateRegion(winrect,rectque);
				ctrl=ctrl->prev;
			}
		}

		RectQue *TempQue = new RectQue();
		if(TempQue){
			BOOL IsNonated=FALSE;
			RectQue tempque;
			JKRect rect,temp;
			TempQue->AppendRectQue(ShowRegion);
			while(TempQue->GetRect(rect)){
				if(winrect.IsContain(rect)){
					rectque.PutRect(rect);
					IsNonated=TRUE;
				}
				else if(winrect.IsTouch(rect)||winrect.IsContained(rect)){
					temp=winrect.CalcOverlap(rect);
					rectque.PutRect(temp);
					tempque.PutRect(rect);
					if(tempque.MinusRect(winrect))
						ShowRegion.AppendRectQue(tempque);
					IsNonated=TRUE;
				}
				else ShowRegion.PutRect(rect);
			}
			if(IsNonated) {
				ShowRegion.MinimizeCount();
				tempque.MinimizeCount();
			}
			delete TempQue;
		}
	}
	return rectque.NodeCount;
}

WORD JKControl::GainRegion(RectQue& rectque,BOOL haschild)
{
	if(IsShow){
		if(haschild){
			JKControl *ctrl = TailNode;
			while(ctrl) {
				if(!ctrl->GainRegion(rectque)) return 0;
				ctrl=ctrl->prev;
			}
		}
		JKRect winrect=GetVirtualWindowRect();
		if(winrect.IsValid()){
			RectQue tempque;
			RectQue *TempQue = new RectQue();
			if(TempQue){
				JKRect rect,temp;
				BOOL IsNonated=FALSE;
				TempQue->AppendRectQue(rectque);
				while(TempQue->GetRect(rect)){
					if(winrect.IsContain(rect)){
						ShowRegion.PutRect(rect);
						IsNonated=TRUE;
					}
					else if(winrect.IsTouch(rect)||winrect.IsContained(rect)){
						temp=winrect.CalcOverlap(rect);
						ShowRegion.PutRect(temp);
						tempque.PutRect(rect);
						if(tempque.MinusRect(winrect))
							rectque.AppendRectQue(tempque);
						IsNonated=TRUE;
					}
					else rectque.PutRect(rect);
				}
				if(IsNonated) {
					ShowRegion.MinimizeCount();
					rectque.MinimizeCount();
				}
				delete TempQue;
			}
		}
	}
	return rectque.NodeCount;
}


JKRect JKControl::GetWindowRectOnScreen()
{
	JKRect rect=WinRect;
	if(Parent) rect.SetLeftTop(Parent->ClientToScreen(rect.LeftTop()));
	return rect;
}

JKRect JKControl::GetClientRectOnScreen()
{
	return ClientRect.SetLeftTopBy(ClientToScreen(JKPoint(0,0)));
}

JKRect JKControl::GetClientRect()
{
	return ClientRect.SetLeftTopBy(ClientOrgPos);
}

JKDC *JKControl::CreateScreenDC(JKDC& dc)
{
	JKRect rect=JKRect(0,0,getmaxx(),getmaxy());
	if(dc.InitDC(rect.LeftTop(),rect)){
		return &dc;
	}
	return 0;
}

JKDC *JKControl::CreateWindowDC(JKRect rect,RectQue *minus,JKDC& dc)
{
	JKRect winrect=GetWindowRectOnScreen();
	dc.InitDC(winrect.LeftTop(),ShowRegion);

	if(dc.GetClipRegion().ClippingRegion(rect))
		if(dc.GetClipRegion().MinusRect(GetClientRectOnScreen())){
			if(minus && !dc.GetClipRegion().MinusRect(*minus)) return 0;
			dc.GetClipRegion().MinimizeCount();
			return &dc;
		}
	return 0;
}

JKDC *JKControl::CreateClientDC(JKRect rect,RectQue *minus,JKDC& dc)
{
	JKRect clntrect=GetClientRectOnScreen();
	dc.InitDC(clntrect.LeftTop(),ShowRegion);
	rect=clntrect.CalcOverlap(rect);
	if(rect.IsValid() && dc.GetClipRegion().ClippingRegion(rect)){
		if(minus && !dc.GetClipRegion().MinusRect(*minus)) return 0;
		dc.GetClipRegion().MinimizeCount();
		dc.TempOrgPos-=ClientOrgPos;
		return &dc;
	}
	return 0;
}


void JKControl::Repaint(RectQue& rectque,DWORD winid,BOOL iserase,JKRect cliprect)
{
	RectContainer *container=rectque.HeadNode;
	if(cliprect.IsValid()){
		JKRect temp;
		while(container){
			temp=container->Rect.CalcOverlap(cliprect);
			if(temp.IsValid()) Repaint(temp,winid,iserase);
			container=container->next;
		}
	}
	else{
		while(container){
			 Repaint(container->Rect,winid,iserase);
			 container=container->next;
		}
	}
}

void JKControl::Repaint(JKRect rect,DWORD winid,BOOL iserase)
{
	MsgQue->SendMessage(winid,REPAINT,rect.LeftTop().GetDWORD(),rect.RightBottom().GetDWORD(),iserase? 1:0);
}

BOOL JKControl::RedrawWindow()
{
	if(IsShow){
		Repaint(ShowRegion);
		JKControl *ctrl = HeadNode;
		while(ctrl) {
			ctrl->RedrawWindow();
			ctrl=ctrl->next;
		}
		return TRUE;
	}
	return FALSE;
}


BOOL JKControl::RedrawClient(BOOL iserase,JKRect rect)
{
	if(IsShow){
		JKRect temp=ClientRect.SetLeftTopBy();

		if(rect==JKRect()) rect=temp;
		else {
			rect=temp.CalcOverlap(rect);
			if(!rect.IsValid()) return TRUE;
		}
		rect.Offset(ClientToScreen(JKPoint(0,0)));

		Repaint(ShowRegion,0,iserase,rect);
		return TRUE;
	}
	return FALSE;
}

BOOL JKControl::EvRepaint(JKRect rect,BOOL iserase)
{
	if(IsShow){
		JKDC *dc;
		dc=CreateWindowDC(rect,&MsgFilter->NopaintRegion);
		if(dc){
			JKRect paintrect=GetWindowRectOnScreen();
			paintrect=paintrect.CalcOverlap(rect);
			if(paintrect.IsValid()){
				paintrect.Offset(-dc->GetOriginPos());
				MouseEvents->HideCursor();
				PaintWindow(*dc,paintrect);
				MouseEvents->ShowCursor();
			}
		}
		dc=CreateClientDC(rect,&MsgFilter->NopaintRegion);
		if(dc){
			JKRect paintrect=GetClientRectOnScreen();
			paintrect=paintrect.CalcOverlap(rect);
			if(paintrect.IsValid()){
				paintrect.Offset(-dc->GetOriginPos());
				MouseEvents->HideCursor();
				if(iserase) DrawEmptyClient(*dc,paintrect);
				PaintClient(*dc,paintrect);
				MouseEvents->ShowCursor();
			}
		}
		return TRUE;
	}
	return FALSE;
}

void JKControl::DrawEmptyClient(JKDC& dc,JKRect rect)
{
	dc.SolidBar(rect,BackColor);
}

// p is Absolute Coord
BOOL JKControl::IsContainPoint(JKPoint& p,BOOL isall)
{
	if(IsShow){
		JKRect temp;
		RectContainer *container = ShowRegion.HeadNode;
		while(container) {
			temp=container->Rect;
			if(temp.IsContain(p))	return TRUE;
			container=container->next;
		}
	}
	if(isall){
		JKControl *ctrl =(JKControl *)HeadNode;
		while(ctrl) {
			if(ctrl->IsContainPoint(p,isall))	return TRUE;
			ctrl=ctrl->next;
		}
	}
	return FALSE;
}


ProcessResult JKControl::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case DESTROYSUBWINDOW:
			DestroySubWindow(msg.sub);
			break;
		case CONTROL_SHOWWINDOW:
			EvCtrlShowWindow(LOWORD(msg.sub));
			break;
		case CONTROL_HIDEWINDOW:
			EvCtrlHideWindow(LOWORD(msg.sub));
			break;
		case CONTROL_SETFOCUS:
			EvCtrlSetFocus(LOWORD(msg.sub));
			break;
		case CONTROL_KILLFOCUS:
			EvCtrlKillFocus(LOWORD(msg.sub));
			break;

		case CONTROL_ONLYMOVE:
			EvCtrlOnlyMove(LOWORD(msg.sub));
			break;
		case CONTROL_ONLYSTAYING:
			EvCtrlOnlyStaying(LOWORD(msg.sub));
			break;
		case CONTROL_LEFTCLICK:
			EvCtrlLeftClick(LOWORD(msg.sub));
			break;
		case CONTROL_LEFTPRESSING:
			EvCtrlLeftPressing(LOWORD(msg.sub));
			break;
		case CONTROL_LEFTRELEASE:
			EvCtrlLeftRelease(LOWORD(msg.sub));
			break;
		case CONTROL_LEFTDRAGGING:
			EvCtrlLeftDragging(LOWORD(msg.sub));
			break;
		case CONTROL_RIGHTCLICK:
			EvCtrlRightClick(LOWORD(msg.sub));
			break;
		case CONTROL_RIGHTPRESSING:
			EvCtrlRightPressing(LOWORD(msg.sub));
			break;
		case CONTROL_RIGHTRELEASE:
			EvCtrlRightRelease(LOWORD(msg.sub));
			break;
		case CONTROL_RIGHTDRAGGING:
			EvCtrlRightDragging(LOWORD(msg.sub));
			break;

		case BUTTON_CLICK:
			EvButtonClick(LOWORD(msg.sub));
			break;
		case BUTTON_PRESSING:
			EvButtonPressing(LOWORD(msg.sub));
			break;
		case BUTTON_RELEASE:
			EvButtonRelease(LOWORD(msg.sub));
			break;
		case CHECK_TOGGLE:
			EvCheckToggle(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case SB_SCROLLUP:
			EvSBScrollUp(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case SB_SCROLLDOWN:
			EvSBScrollDown(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case SB_SCROLLLEFT:
			EvSBScrollLeft(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case SB_SCROLLRIGHT:
			EvSBScrollRight(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case SB_SCROLLJUMP:
			EvSBScrollJump(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case LB_TOUCHENTRY:
			EvLBTouchEntry(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case LB_SELECTENTRY:
			EvLBSelectEntry(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case LB_SETFIRSTINDEX:
			EvLBSetFirstIndex(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case LB_CHARDOWN:
			EvLBCharDown(LOWORD(msg.sub),HIWORD(msg.sub),LOWORD(msg.detail),HIWORD(msg.detail));
			break;
		case LB_KEYDOWN:
			EvLBKeyDown(LOWORD(msg.sub),HIWORD(msg.sub),LOWORD(msg.detail),HIWORD(msg.detail));
			break;
		case EDIT_FINISH:
			EvEditFinish(LOWORD(msg.sub));
			break;
		case EDIT_CANCEL:
			EvEditCancel(LOWORD(msg.sub));
			break;
		case EDIT_SETTEXT:
			EvEditSetText(LOWORD(msg.sub));
			break;
		case SPIN_UP:
			EvSpinUp(LOWORD(msg.sub));
			break;
		case SPIN_DOWN:
			EvSpinDown(LOWORD(msg.sub));
			break;

		case SHOWWINDOW:
			EvShowWindow();
			break;
		case HIDEWINDOW:
			EvHideWindow(TRUE);
			break;
		case STARTWINDOWMOVE:
			StartWindowMove();
			break;
		case STARTWINDOWRESIZE:
			StartWindowResize((BYTE)msg.sub);
			break;
		case MOVEWINDOW:
			EvMoveWindow(JKRect(msg.sub,msg.detail));
			break;
		case RESIZEWINDOW:
			EvResizeWindow(JKRect(msg.sub,msg.detail));
			break;
		case PLACEFORWARD:
			EvPlaceForward();
			break;
		case PLACEBACKWARD:
			EvPlaceBackward();
			break;
		case CLOSEWINDOW:
			if(EvCloseWindow()) {
				HideWindow();
				if(Parent) Parent->SendMessage(DESTROYSUBWINDOW,WinID);
				else MsgQue->SendMessage(0,DESTROYSUBWINDOW,WinID,0);
			}
			break;
		case REPAINT:
			if(EvRepaint(JKRect(msg.sub,msg.detail),msg.option))
				break;
			return PROCESS_SKIP;
		case CHARDOWN:
			if(IsShow)
				EvCharDown(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case KEYDOWN:
			if(IsShow)
				EvKeyDown(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case DEADCHARDOWN:
			if(IsShow)
				EvDeadCharDown(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case DEADKEYDOWN:
			if(IsShow)
				EvDeadKeyDown(LOWORD(msg.sub),HIWORD(msg.sub));
			break;
		case ONLYMOVE:
		case ONLYSTAYING:
		case LEFTCLICK:
		case LEFTPRESSING:
		case LEFTRELEASE:
		case LEFTDRAGGING:
		case RIGHTCLICK:
		case RIGHTPRESSING:
		case RIGHTRELEASE:
		case RIGHTDRAGGING:
			SendControlMessage(msg.main-ONLYMOVE+CONTROL_ONLYMOVE);
		case NCONLYMOVE:
		case NCLEFTCLICK:
		case NCLEFTPRESSING:
		case NCLEFTRELEASE:
		case NCLEFTDRAGGING:
		case NCRIGHTCLICK:
		case NCRIGHTPRESSING:
		case NCRIGHTRELEASE:
		case NCRIGHTDRAGGING:
			{
			JKPoint p=JKPoint(LOWORD(msg.sub),HIWORD(msg.sub));
			switch(msg.main){
				case ONLYMOVE:
					EvOnlyMove(p);
					break;
				case ONLYSTAYING:
					EvOnlyStaying(p);
					break;
				case LEFTCLICK:
					EvLeftClick(p);
					break;
				case LEFTPRESSING:
					EvLeftPressing(p);
					break;
				case LEFTRELEASE:
					if(!PreEvLeftRelease(p))
						EvLeftRelease(p);
					break;
				case LEFTDRAGGING:
					if(!PreEvLeftDragging(p))
						EvLeftDragging(p);
					break;
				case RIGHTCLICK:
					EvRightClick(p);
					break;
				case RIGHTPRESSING:
					EvRightPressing(p);
					break;
				case RIGHTRELEASE:
					EvRightRelease(p);
					break;
				case RIGHTDRAGGING:
					EvRightDragging(p);
					break;
				case NCONLYMOVE:
					EvNcOnlyMove(p);
					break;
				case NCONLYSTAYING:
					EvNcOnlyStaying(p);
					break;
				case NCLEFTCLICK:
					EvNcLeftClick(p);
					break;
				case NCLEFTPRESSING:
					EvNcLeftPressing(p);
					break;
				case NCLEFTRELEASE:
					EvNcLeftRelease(p);
					break;
				case NCLEFTDRAGGING:
					EvNcLeftDragging(p);
					break;
				case NCRIGHTCLICK:
					EvNcRightClick(p);
					break;
				case NCRIGHTPRESSING:
					EvNcRightPressing(p);
					break;
				case NCRIGHTRELEASE:
					EvNcRightRelease(p);
					break;
				case NCRIGHTDRAGGING:
					EvNcRightDragging(p);
					break;
				default:
					break;
			}
			}
			break;
		case ELONLYMOVE:
			EvElOnlyMove(LOWORD(msg.sub));
			break;
		case ELONLYSTAYING:
			EvElOnlyStaying(LOWORD(msg.sub));
			break;
		case ELLEFTCLICK:
			if(!PreEvElLeftClick(LOWORD(msg.sub)))
				EvElLeftClick(LOWORD(msg.sub));
			break;
		case ELLEFTPRESSING:
			EvElLeftPressing(LOWORD(msg.sub));
			break;
		case ELLEFTRELEASE:
			EvElLeftRelease(LOWORD(msg.sub));
			break;
		case ELLEFTDRAGGING:
			EvElLeftDragging(LOWORD(msg.sub));
			break;
		case ELRIGHTCLICK:
			EvElRightClick(LOWORD(msg.sub));
			break;
		case ELRIGHTPRESSING:
			EvElRightPressing(LOWORD(msg.sub));
			break;
		case ELRIGHTRELEASE:
			EvElRightRelease(LOWORD(msg.sub));
			break;
		case ELRIGHTDRAGGING:
			EvElRightDragging(LOWORD(msg.sub));
			break;
		case TIMERACTIVE:
			EvTimerActive();
			break;
		case TIMERHAPPEN:
			EvTimerHappen(msg.sub);
			break;
		case COMMAND:
			EvCommand(LOWORD(msg.sub),HIWORD(msg.sub),msg.detail,msg.option);
			break;
		case SETFOCUS:
			if(msg.sub==WinID || TestSubWindow(msg.sub,TRUE)){
				if(WinAttr&WA_CHANGEPLACE) PlaceForward();
				if(msg.sub==WinID) {
					SendControlMessage(CONTROL_SETFOCUS);
					EvSetFocus(msg.detail);
				}
			}
			break;
		case KILLFOCUS:
			if(msg.sub==WinID) {
				SendControlMessage(CONTROL_KILLFOCUS);
				EvKillFocus(msg.detail);
			}
			break;
		default:
			break;
	}
	if(msg.winid==WinID) return PROCESS_FINISH;
	else					return PROCESS_OK;
}



ProcessResult JKControl::ProcessMessage(JKMSG msg)
{
	JKMSG tempmsg=msg;
	ProcessResult result=PROCESS_OK;
	if(!IsShow && (tempmsg.main==REPAINT||(tempmsg.main>=ONLYMOVE && tempmsg.main<ONLYMOVE+(NCONLYMOVE-ONLYMOVE)*3)))
		return PROCESS_SKIP;

	if(!tempmsg.winid){
		if(tempmsg.main>=NCONLYMOVE && tempmsg.main<NCONLYMOVE+(NCONLYMOVE-ONLYMOVE)){
			JKPoint p=JKPoint(tempmsg.sub);
			if(IsContainPoint(p)){
				WORD id=0;
				JKPoint p=JKPoint(tempmsg.sub);
				int16 element=FindWindowElement(p);
				if(element<0){
					tempmsg.main-=NCONLYMOVE-ONLYMOVE;
					tempmsg.sub = p.GetDWORD();
					id=CursorID;
				}
				else if(element>0){
					tempmsg.main+=(NCONLYMOVE-ONLYMOVE);
					tempmsg.sub = MAKEDWORD(element,0);
					if(WinAttr&WA_TITLEMOVEABLE && element==TITLEBOX){
						id=CURSOR_CROSS;
					}
					else if(WinAttr&WA_BORDERRESIZABLE){
						if(element==BORDER_TOP || element==BORDER_BOTTOM) 		 		id=CURSOR_BORDER1;
						else if(element==BORDER_LEFT || element==BORDER_RIGHT) 		id=CURSOR_BORDER2;
						else if(element==BORDER_CORNER1 || element==BORDER_CORNER4) id=CURSOR_BORDER3;
						else if(element==BORDER_CORNER2 || element==BORDER_CORNER3) id=CURSOR_BORDER4;
					}

				}
				MouseEvents->SetCursorID(id);
			}
		}
		if(tempmsg.main==CHARDOWN||tempmsg.main==KEYDOWN)
			if(WinID!=MsgFilter->FocusWinID) tempmsg.main+=(DEADCHARDOWN-CHARDOWN);
		if(tempmsg.main==LEFTCLICK||tempmsg.main==ELLEFTCLICK)
			SetFocus();
	}

	if(!tempmsg.winid || tempmsg.winid==WinID)
		result=RespondMessage(tempmsg);

	if(result) return result;

	JKControl *ctrl = HeadNode;
	while(ctrl) {
		if(ctrl->ProcessMessage(msg)==PROCESS_FINISH) return PROCESS_FINISH;
		ctrl=ctrl->next;
	}
	return PROCESS_OK;
}

void JKControl::ChangeWindowSize(JKRect rect)
{
	if(rect!=WinRect){
		BOOL ischangesize=(WinRect.Width()==rect.Width() && WinRect.Height()==rect.Height())? FALSE : TRUE;
		BOOL IsShowed=IsShow;
		NotRedrawRegion.ClearNode();
		if(IsShow) VirtualHideWindow();
		WinRect = rect;
		ClientRect = CalcClientRect();
		if(IsShowed) VirtualShowWindow();
		NotRedrawRegion.MinimizeCount();
		Repaint(NotRedrawRegion);
		NotRedrawRegion.ClearNode();


		if(!ischangesize)
			SendMessage(MOVEWINDOW,rect.LeftTop().GetDWORD(),rect.RightBottom().GetDWORD());
		else
			SendMessage(RESIZEWINDOW,rect.LeftTop().GetDWORD(),rect.RightBottom().GetDWORD());
	}
}

void JKControl::PlaceForward()
{
	if(Parent&&Parent->TailNode!=this){
		BOOL IsShowed=IsShow;
		NotRedrawRegion.ClearNode();
		if(IsShow) VirtualHideWindow();
		Parent->SetNodeLast(Parent->GetIndex(this));
		if(IsShowed) VirtualShowWindow();
		NotRedrawRegion.MinimizeCount();
		Repaint(NotRedrawRegion);
		NotRedrawRegion.ClearNode();
		SendMessage(PLACEFORWARD);
	}
}

void JKControl::PlaceBackward()
{
	if(Parent&&Parent->HeadNode!=this){
		BOOL IsShowed=IsShow;
		NotRedrawRegion.ClearNode();
		if(IsShow) VirtualHideWindow();
		Parent->SetNodeFirst(Parent->GetIndex(this));
		if(IsShowed) VirtualShowWindow();
		NotRedrawRegion.MinimizeCount();
		Repaint(NotRedrawRegion);
		NotRedrawRegion.ClearNode();
		SendMessage(PLACEBACKWARD);
	}
}

JKPoint JKControl::ClientToScreen(JKPoint point)
{
	JKControl *win=this;
	JKPoint p=point;
	while(win){
		p+=win->ClientRect.LeftTop();
		p+=win->WinRect.LeftTop();
		win=win->Parent;
	}
	return p;
}

JKPoint JKControl::ScreenToClient(JKPoint point)
{
	JKControl *win=this;
	JKPoint p=point;
	while(win){
		p-=win->WinRect.LeftTop();
		p-=win->ClientRect.LeftTop();
		win=win->Parent;
	}
	return p;
}

BOOL JKControl::SetText(char *buf)
{
	if(buf){
		if(WinText) {
			delete[] WinText;
			WinText=0;
		}
		WinText = new char[strlen(buf)+1];
		if(WinText) {
			strcpy(WinText,buf);
			return TRUE;
		}
	}
	return FALSE;
}

void JKControl::BeginCapture()
{
	MouseEvents->SetCursorID(CursorID);
	MsgFilter->BeginCapture(WinID);
}
DWORD JKControl::EndCapture()
{
	DWORD result=MsgFilter->EndCapture(WinID);
	JKPoint p=MouseEvents->GetCursorPos();
	MsgQue->SendMessage(0,NCONLYMOVE,p.GetDWORD());
	return result;
}

void JKControl::SetFocus()
{
	if(!IsShow || WinAttr&WA_NOHASFOCUS){
		if(Parent) Parent->SetFocus();
		else	MsgFilter->SetFocus(0);
	}
	else	MsgFilter->SetFocus(WinID);
}

BOOL JKControl::KillFocus()
{
	DWORD lostid = MsgFilter->FocusWinID;
	if(lostid==WinID){
		if(Parent) Parent->SetFocus();
		else MsgFilter->SetFocus(0);
		return TRUE;
	}
	return FALSE;
}

void JKControl::SetTimer(int32 interval)
{
	TimerMan->SetTimer(WinID,interval);
}

void JKControl::KillTimer()
{
	TimerMan->KillTimer(WinID);
}

BOOL JKControl::HasTimer()
{
	TimerMember *memb=TimerMan->GetTimer(WinID);
	return memb? TRUE:FALSE;
}

//===========================================================

BOOL JKControl::AddControl(JKControl *ctrl)
{
	if(ctrl) {
		if(!CreationError && !ctrl->CreationError){
			ctrl->Parent=this;
			PutNodeLast(ctrl);
			ctrl->IsRemoveable=TRUE;
			if(!ctrl->IsInit) ctrl->InitWindow();
			return TRUE;
		}
		else delete ctrl;
	}
	return FALSE;
}

JKControl *JKControl::GetControl(WORD ctrlid)
{
	JKControl *find = new JKControl(JKRect(),ctrlid);
	if(find) {
		JKControl *ctrl=GetNode(find,0x02);
		delete find;
		return ctrl;
	}
	return 0;
}

void JKControl::DestroySubWindow(DWORD winid)
{
	JKControl *ctrl;
	JKControl *find = new JKControl(JKRect(),0);
	if(find) {
		find->WinID=winid;
		ctrl=GetNode(find,0x01,TRUE);
		if(ctrl&&ctrl->IsRemoveable) delete ctrl;
		find->WinID=DWORD(find);
		delete find;
	}
}

void JKControl::DestroyAllSubWindow()
{
	JKControl *ctrl = TailNode,*temp;
	while(ctrl) {
		temp=ctrl;
		ctrl=ctrl->prev;
		if(temp->IsRemoveable) delete temp;
	}
	NodeCount=0;
	TailNode=HeadNode=0;
}


BOOL JKControl::FindSubWindow(DWORD winid,WORD& ctrlid,BOOL isall)
{
	if(!winid) return 0;
	JKControl *find = new JKControl(JKRect(),0);
	if(find) {
		find->WinID=winid;
		JKControl *ctrl=GetNode(find,0x01);
		find->WinID=DWORD(find);
		delete find;
		if(ctrl) {
			ctrlid=ctrl->ControlID;
			return TRUE;
		}
		if(isall){
			ctrl =(JKControl *)HeadNode;
			while(ctrl) {
				if(ctrl->TestSubWindow(winid,isall)){
					ctrlid=ctrl->ControlID;
					return TRUE;
				}
				ctrl=ctrl->next;
			}
		}
	}
	return FALSE;
}

BOOL JKControl::TestSubWindow(DWORD winid,BOOL isall)
{
	if(!winid) return 0;
	JKControl *find = new JKControl(JKRect(),0);
	if(find) {
		find->WinID=winid;
		JKControl *ctrl=GetNode(find,0x01);
		find->WinID=DWORD(find);
		delete find;
		if(ctrl) return TRUE;
		if(isall){
			ctrl =(JKControl *)HeadNode;
			while(ctrl) {
				if(ctrl->TestSubWindow(winid,isall)) return TRUE;
				ctrl=ctrl->next;
			}
		}
	}
	return FALSE;
}

BOOL JKControl::IsContainFocus()
{
	if(HasFocus() || TestSubWindow(MsgFilter->FocusWinID,TRUE))
		return TRUE;
	return FALSE;
}
//-----------------------------------------------------------
BOOL JKControl::ScrollClient(JKPoint& offset)
{
	if(offset==JKPoint(0,0)) return FALSE;
	if(IsShow){
		JKRect rect=GetClientRect();
		JKRect offsetrect=rect.OffsetBy(-offset);
		JKRect overlap=rect.CalcOverlap(offsetrect);
		if(!overlap.IsValid()){
			RedrawClient();
			ClientOrgPos+=offset;
			return TRUE;
		}

//////////////////////////////////////////////////////////

		RectQue *offsetregion=ShowRegion.Duplicate();
		if(!offsetregion) return TRUE;
		JKRect clientrect=GetClientRectOnScreen();
		offsetregion->ClippingRegion(clientrect);

		JKDC *dc=GlobalDC;
		dc->InitDC(clientrect.LeftTop(),*offsetregion);
		dc->TempOrgPos-=ClientOrgPos;

		int16  width=overlap.Width()+1;
		int16  height=overlap.Height()+1;
		if(width<=0||height<=0) return FALSE;


		MouseEvents->HideCursor();
		BYTE *buff= new BYTE[width*height+4];
		if(buff) {
				memset(buff+4,BackColor,width*height);
			if(dc->GetImage(overlap.OffsetBy(offset),buff))
				dc->PutImage(overlap.LeftTop(),buff,0);
			delete[] buff;
		}
		MouseEvents->ShowCursor();

		ClientOrgPos+=offset;
		dc->TempOrgPos-=offset;

		offsetregion->Offset(-offset);
		uint32 count=dc->GetClipRegion().MinusRect(*offsetregion);
		delete offsetregion;
		if(count) {
			dc->GetClipRegion().MinimizeCount();
			clientrect.Offset(-dc->GetOriginPos());
			MouseEvents->HideCursor();
			DrawEmptyClient(*dc,clientrect);
			PaintClient(*dc,clientrect);
			MouseEvents->ShowCursor();
		}
		return TRUE;
	}
	return FALSE;
}

BOOL JKControl::RepaintRestRegion(RectQue& oldregion,RectQue& newregion)
{
	RectQue *restregion=oldregion.Duplicate();
	if(!restregion) return FALSE;
	restregion->MinusRect(newregion);
	Repaint(*restregion);
	delete restregion;
	return TRUE;
}

//-----------------------------------------------------------

BOOL JKControl::StartWindowMove()
{
	if(!WorkID) {
		BeginCapture();
		MouseEvents->SetCursorID(CURSOR_CROSS);

		WorkPos= MouseEvents->GetCursorPos();
		WorkID=WW_WINDOWMOVE;
		JKRect rect = GetWindowRectOnScreen();
		JKDC *dc = CreateScreenDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Rectangle3D(rect,3,0xff,0xff);
			MouseEvents->ShowCursor();
		}
		ResizeRect=rect;
		return TRUE;
	}
	return FALSE;
}

BOOL JKControl::StartWindowResize(BYTE selborder)
{
	if(!WorkID && selborder){
		WORD cursorid=0;
		if(selborder==0x01||selborder==0x02) 			cursorid=CURSOR_BORDER1;
		else if(selborder==0x04||selborder==0x08) cursorid=CURSOR_BORDER2;
		else if(selborder==0x05||selborder==0x0a) cursorid=CURSOR_BORDER3;
		else if(selborder==0x06||selborder==0x09) cursorid=CURSOR_BORDER4;
		else return FALSE;

		SelectBorder=selborder;
		BeginCapture();

		MouseEvents->SetCursorID(cursorid);
		WorkPos= MouseEvents->GetCursorPos();
		WorkID=WW_WINDOWRESIZE;
		JKRect rect = GetWindowRectOnScreen();
		JKDC *dc = CreateScreenDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Rectangle3D(rect,3,0xff,0xff);
			MouseEvents->ShowCursor();
		}
		ResizeRect=rect;
		return TRUE;
	}
	return FALSE;
}

BOOL JKControl::PreEvElLeftClick(WORD element)
{
	if(element==TITLEBOX && WinAttr&WA_TITLEMOVEABLE){
		SendMessage(STARTWINDOWMOVE);
		return TRUE;
	}
	else if(element>=BORDER_TOP&&element<=BORDER_CORNER4 && WinAttr&WA_BORDERRESIZABLE){
		BYTE selborder=0x00;
		if(element==BORDER_TOP) 					selborder=0x01;
		else if(element==BORDER_BOTTOM) 	selborder=0x02;
		else if(element==BORDER_LEFT)   	selborder=0x04;
		else if(element==BORDER_RIGHT)  	selborder=0x08;
		else if(element==BORDER_CORNER1) 	selborder=0x05;
		else if(element==BORDER_CORNER2) 	selborder=0x09;
		else if(element==BORDER_CORNER3) 	selborder=0x06;
		else if(element==BORDER_CORNER4) 	selborder=0x0a;

		SendMessage(STARTWINDOWRESIZE,selborder);
		return TRUE;
	}
	return FALSE;
}


BOOL JKControl::PreEvLeftDragging(JKPoint& p)
{
	if(WorkID==WW_WINDOWMOVE){
		JKPoint pos=ClientToScreen(p);
		JKRect rect=GetWindowRectOnScreen();
		rect.Offset(pos-WorkPos);

		JKDC *dc = CreateScreenDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Rectangle3D(ResizeRect,3,0xff,0xff);
			dc->Rectangle3D(rect,3,0xff,0xff);
			MouseEvents->ShowCursor();
		}
		ResizeRect=rect;
		return TRUE;
	}
	else if(WorkID==WW_WINDOWRESIZE){
		JKPoint pos=ClientToScreen(p);
		JKRect rect=GetWindowRectOnScreen();
		if(SelectBorder&0x01)	{
			rect.top+=pos.y-WorkPos.y;
			if(rect.top>rect.bottom-MinimumSize.y) rect.top=rect.bottom-MinimumSize.y;
		}
		if(SelectBorder&0x02)	{
			rect.bottom+=pos.y-WorkPos.y;
			if(rect.top>rect.bottom-MinimumSize.y) rect.bottom=rect.top+MinimumSize.y;
		}
		if(SelectBorder&0x04)	{
			rect.left+=pos.x-WorkPos.x;
			if(rect.left>rect.right-MinimumSize.x) rect.left=rect.right-MinimumSize.x;
		}
		if(SelectBorder&0x08)	{
			rect.right+=pos.x-WorkPos.x;
			if(rect.left>rect.right-MinimumSize.x) rect.right=rect.left+MinimumSize.x;
		}

		JKDC *dc = CreateScreenDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Rectangle3D(ResizeRect,3,0xff,0xff);
			dc->Rectangle3D(rect,3,0xff,0xff);
			MouseEvents->ShowCursor();
		}
		ResizeRect=rect;
		return TRUE;
	}
	return FALSE;
}

BOOL JKControl::PreEvLeftRelease(JKPoint& p)
{
	if(WorkID==WW_WINDOWMOVE){
		EndCapture();
		JKPoint pos=ClientToScreen(p);

		JKRect rect=GetWindowRectOnScreen();
		rect.Offset(pos-WorkPos);
		JKDC *dc = CreateScreenDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Rectangle3D(ResizeRect,3,0xff,0xff);
			MouseEvents->ShowCursor();
		}

		if(Parent) rect.SetLeftTop(Parent->ScreenToClient(rect.LeftTop()));
		ChangeWindowSize(rect);
		WorkID=0;
		return TRUE;
	}
	else if(WorkID==WW_WINDOWRESIZE){
		EndCapture();
		JKPoint pos=ClientToScreen(p);

		JKRect rect=GetWindowRectOnScreen();
		if(SelectBorder&0x01)	{
			rect.top+=pos.y-WorkPos.y;
			if(rect.top>rect.bottom-MinimumSize.y) rect.top=rect.bottom-MinimumSize.y;
		}
		if(SelectBorder&0x02)	{
			rect.bottom+=pos.y-WorkPos.y;
			if(rect.top>rect.bottom-MinimumSize.y) rect.bottom=rect.top+MinimumSize.y;
		}
		if(SelectBorder&0x04)	{
			rect.left+=pos.x-WorkPos.x;
			if(rect.left>rect.right-MinimumSize.x) rect.left=rect.right-MinimumSize.x;
		}
		if(SelectBorder&0x08)	{
			rect.right+=pos.x-WorkPos.x;
			if(rect.left>rect.right-MinimumSize.x) rect.right=rect.left+MinimumSize.x;
		}

		JKDC *dc = CreateScreenDC();
		if(dc){
			dc->SetWriteMode(XOR_PUT);
			MouseEvents->HideCursor();
			dc->Rectangle3D(ResizeRect,3,0xff,0xff);
			MouseEvents->ShowCursor();
		}
		if(Parent) rect.SetLeftTop(Parent->ScreenToClient(rect.LeftTop()));
		ChangeWindowSize(rect);
		WorkID=0;
		return TRUE;
	}
	return FALSE;
}

RectQue NotRedrawRegion;
