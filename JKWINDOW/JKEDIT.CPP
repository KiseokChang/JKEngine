#include <conio.h>
#include <ctype.h>
#include <mem.h>
#include "hanjadlg.h"
#include "jkedit.h"


JKEdit::JKEdit(JKRect& rect,WORD ctlid,int8 *buffer,WORD bufsize,BOOL ismultiline,BOOL ishangulkbd,JKControl *parent)
	:JKControl(rect,ctlid,parent),HangulAutomata(ishangulkbd),
	TabSize(4),LineSpace(16),EOLChar(20),IsShowEOLChar(FALSE),IsShowCaret(FALSE),
	HanStart(FALSE),SaveBuffer(0),IsEnableCaret(FALSE),IsMultiLine(ismultiline),IsPassword(FALSE),IsCancel(FALSE)
{
	CursorID=4;

	InsertMode=TRUE;
	IndentMode=FALSE;
	EditBuffer=0;
	EditEndPointer=0;
	//=================================

	EditBufferSize = bufsize;
	if(!buffer) {
		EditBuffer = new int8[EditBufferSize+1];
		EditBuffer[EditBufferSize]=0;
		HasOwnership=TRUE;
	}
	else {
		EditBuffer = buffer;
		EditBuffer[EditBufferSize]=0;
		HasOwnership=FALSE;
	}
	if(EditBuffer) {
		EditEndPointer=EditBuffer+EditBufferSize;
		InitEditorVariable();
	}
	else CreationError=TRUE;
}

JKEdit::~JKEdit()
{
	if(SaveBuffer) delete[] SaveBuffer;
	if(HasOwnership&&EditBuffer) delete[] EditBuffer;
}

void JKEdit::InitWindow()
{
	JKControl::InitWindow();

	TextWidth =(ClientRect.Width()+1)/8;
	TextHeight=(ClientRect.Height()+1)/LineSpace;
	if(!TextWidth || !TextHeight) CloseWindow();
}

JKRect JKEdit::CalcClientRect()
{
	JKRect rect = JKControl::CalcClientRect();
	rect.Expand(-1);
	return rect;
}

void JKEdit::PaintWindow(JKDC& dc,JKRect&)
{
	JKRect rect=WinRect.SetLeftTopBy(JKPoint(0,0));
	dc.Box3D(rect,-1,BackColor);
}

void JKEdit::PaintClient(JKDC& dc,JKRect&)
{
	if(TextWidth&&TextHeight){
		dc.SetTextColor(TextColor);
		dc.SetBackColor(BackColor);
		dc.SolidBar(ClientRect.SetLeftTopBy(JKPoint(0,0)),BackColor);
		DisplayPage(dc);
	}
}


void JKEdit::EvSetFocus(DWORD)
{
	WORD buffersize=GetTextSize();
	if(SaveBuffer) delete[] SaveBuffer;
	SaveBuffer=new int8[buffersize+1];
	if(SaveBuffer) GetText(SaveBuffer,buffersize);
	UserTask = TASK_DISPLAYPAGE;
	UserFunc();
	SetTimer(300);
	IsShowCaret=FALSE;
	SetEnableCaret(TRUE);
	ShowCaret();
	IsCancel=FALSE;
}

void JKEdit::EvKillFocus(DWORD)
{
	KillTimer();
	HideCaret();
	SetEnableCaret(FALSE);

	if(IsWaitAddKey) IsWaitAddKey=FALSE;
	if(CurHanState)	{
		*CurrentPointer    =(InpStack[InpSP-1].charcode>>8)&0xFF;
		*(CurrentPointer+1)=InpStack[InpSP-1].charcode&0xFF;
		InitAutomata();
	}
	if(SaveBuffer) delete[] SaveBuffer;
	SaveBuffer=0;
}

void JKEdit::EvPreFinish()

{
	if(IsWaitAddKey) IsWaitAddKey=FALSE;
	if(CurHanState)	{
		*CurrentPointer    =(InpStack[InpSP-1].charcode>>8)&0xFF;
		*(CurrentPointer+1)=InpStack[InpSP-1].charcode&0xFF;
		InitAutomata();
	}
	if(SaveBuffer) delete[] SaveBuffer;
	SaveBuffer=0;

	if(IsCancel) SendControlMessage(EDIT_CANCEL);
	else         SendControlMessage(EDIT_FINISH);
	IsCancel=FALSE;
}

void JKEdit::EvTimerHappen(DWORD)
{
	XPutCaret();
}


ProcessResult JKEdit::RespondMessage(JKMSG& msg)
{
	switch(msg.main){
		case EDIT_BEGIN:
			if(IsShow)	SetFocus();
			break;
		default:
			break;
	}
	return JKControl::RespondMessage(msg);
}

WORD JKEdit::FindHanja(WORD hangulcode)
{
	WORD hanjacode=hangulcode;

	if(HanjaSelectDialog(JKPoint(200,200),hanjacode).Run()!=ID_OK){
		hanjacode=hangulcode;
	}
	return hanjacode;
}

void JKEdit::SetInsertMode(BOOL mode)
{
	InsertMode=mode;
}

void JKEdit::SetIndentMode(BOOL mode)
{
	if(IsMultiLine)	IndentMode=mode;
}

//================================================================

void JKEdit::InitEditorVariable()
{

	BOOL Temp;
	if(IsShowCaret) { HideCaret(); Temp=TRUE; }

	LineStart = CurrentPointer = LastPosition = BlockStart = BlockEnd = EOFMem = EditBuffer;
	*EditBuffer = *LineBuffer = CurrentX = CurrentY = CurrentMostX = 0;
	MaxLineCount = BaseY = BaseX = 0;
	IsSaved = BlockDisplay = TRUE;
	IsWaitAddKey=FALSE;
	IsEnableCaret=TRUE;
	InitAutomata();

	UserTask = TASK_DISPLAYPAGE;
	UserFunc();

	if(Temp) ShowCaret();
}

WORD JKEdit::GetTextSize()
{
	return WORD(EOFMem-EditBuffer);
}

BOOL JKEdit::GetText(int8 *str,int16 size)
{
	WORD textsize=GetTextSize();
	if(str){
		if(size<0||size>textsize){
			strncpy(str,EditBuffer,textsize);
			str[textsize]=0;
		}
		else{
			strncpy(str,EditBuffer,size);
			str[size]=0;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL JKEdit::SetText(int8 *str)
{
	if(str&&EditBuffer&&EditBufferSize){
		WORD len=0;
		if(IsMultiLine)
			len=  strlen(str);
		else{
			int8 *tmp1=strchr(str,'\r');
			int8 *tmp2=strchr(str,'\n');
			if(tmp1){
				if(tmp2) len = WORD(min(tmp1,tmp2)-str);
				else	len = WORD(tmp1-str);
			}
			else if(tmp2){
				len = WORD(tmp2-str);
			}
			else len=  strlen(str);
		}
		if(len>EditBufferSize)	len=EditBufferSize;
		strncpy(EditBuffer,str,len);
		EditBuffer[len]=0;

		LineStart=CurrentPointer=LastPosition=BlockStart=BlockEnd=EOFMem=EditBuffer;
		*LineBuffer=CurrentX=CurrentY=CurrentMostX=0;
		IsSaved=BlockDisplay=TRUE;
		BaseY=BaseX=0;
		IsWaitAddKey=FALSE;
		IsEnableCaret=TRUE;

		LastPosition+= len? (len-1):0;
		if(len>1&&HanMan->IsHangul1st(str,len-2))
			LastPosition--;
		EOFMem+=len;
		MaxLineCount = GetLineCount(EditBuffer,LastPosition);

		InitAutomata();

		UserTask = TASK_DISPLAYPAGE;
		UserFunc();
		SendControlMessage(EDIT_SETTEXT);
		return TRUE;
	}
	return FALSE;
}

BOOL JKEdit::GetInverse()
{
	return (BlockDisplay && IsInBlock(CurrentPointer))? TRUE:FALSE;
}


void JKEdit::EvCharDown(WORD key,WORD modifier)
{
	if(IsWaitAddKey){
		WORD ch=key;
		if(('A'<=ch&&ch<='Z')||('a'<=ch&&ch<= 'z'))
			ch&=0x1f;
		ch+=0x40;
		if(CtrlFirstKey=='K')
			Control_K(ch);
		else if(CtrlFirstKey=='Q')
			Control_Q(ch);
		if(UserTask) UserFunc();
		IsWaitAddKey=FALSE;
		SetEnableCaret(TRUE);
		return;
	}
	EvKeyboardDown(key,modifier);
}

void JKEdit::EvKeyDown(WORD key,WORD modifier)
{
	if(IsWaitAddKey){
		IsWaitAddKey=FALSE;
		SetEnableCaret(TRUE);
	}
	EvKeyboardDown(key,modifier);
}

void JKEdit::EvKeyboardDown(WORD key,WORD modifier)
{
	if(!HanStart){
		if(key&0xFF) key&=0xFF;
		key=ConvertKey(key,modifier);
	}
	HideCaret();
	SetEnableCaret(FALSE);
	WORD PushedKey;
	do{
		PushedKey=0;
		static int16 oldstate=0;
		int16 state;
		int16 ch;
		ch = key;
		if(key == '\b') {
			if(InpSP) {
				InpSP--;
				if(InpSP) {
					int16 i = InpSP-1;
					CurHanState = InpStack[i].curhanst;
					ch = InpStack[i].charcode;
					state = HR_HANBS;
				}
				else {
					state = HR_NOHAN;
					CurHanState = 0;
				}
			}
			else {
				state = HR_ASC;
			}
		}
		else if(key>=0x80 && key<=0xff) {
			WORD hanstate = CurHanState;
			BOOL ar = Automata(key);
			ch = InpStack[InpSP-1].charcode;
			if(ar){
				CurHanState = 0;
				InpSP = 0;
				state = HR_HANEND;
				if(OutSP) {
					HanStart = TRUE;
					while(OutSP) Automata(OutStack[--OutSP]);
				}
			}
			else if(!hanstate) {
				state = HR_HANST;
			}
			else{
				state = HR_HANIN;
			}
		}
		else if(CurHanState) {
			PushedKey=key;
			ch = InpStack[InpSP-1].charcode;
			state = HR_HANEND;
			CurHanState = InpSP = 0;
		}
		else {
			state = HR_ASC;
		}
		EvChar(ch,modifier,state,oldstate==HR_HANEND);
		oldstate = state;
		if (HanStart) {
			HanStart = FALSE;
			state = HR_HANST;
			ch = InpStack[InpSP-1].charcode;
			EvChar(ch,modifier,state,oldstate==HR_HANEND);
			oldstate = state;
		}
	}while((key=PushedKey)!=0);
	if(UserTask) UserFunc();
	SetEnableCaret(TRUE);
	ShowCaret();
}

void JKEdit::EvChar(WORD key,WORD modifier,int16 state,BOOL isleft)
{
	if(key==F9||key==LEFTARROW||key==RIGHTARROW||key==UPARROW||key==DOWNARROW)
		if(isleft) Proc_Left();

	switch(key) {
		case ALT_X:
			if(IsMultiLine) EvPreFinish();
			break;
		case ESC:
			if(SaveBuffer) SetText(SaveBuffer);
			IsCancel=TRUE;
			EvPreFinish();
			break;
		case F2:
			HanKbdState=!HanKbdState;
			break;
		case F9:
			Proc_Hanja();
			break;
		case ALT_N:
			if(IsMultiLine) InitEditorVariable();
			break;
		case CTRL_K:
			IsWaitAddKey=TRUE;
			CtrlFirstKey='K';
			SetEnableCaret(FALSE);
			break;
		case CTRL_Q:
			IsWaitAddKey=TRUE;
			CtrlFirstKey='Q';
			SetEnableCaret(FALSE);
			break;
		case LEFTARROW:
			Proc_Left();
			break;
		case RIGHTARROW:
			Proc_Right();
			break;
		case CTRL_LEFT:
			MoveLeftWord();
			break;
		case CTRL_RIGHT:
			MoveRightWord();
			break;
		case UPARROW:
			if(IsMultiLine) Proc_UpDown(-1);
			break;
		case DOWNARROW:
			if(IsMultiLine) Proc_UpDown(1);
			break;
		case PGUPKEY:
			if(IsMultiLine) Proc_UpDown(-TextHeight);
			break;
		case PGDNKEY:
			if(IsMultiLine) Proc_UpDown(TextHeight);
			break;
		case HOMEKEY:
			CurrentMostX = 0;
			HLocate(0);
			break;
		case ENDKEY:
			CurrentMostX = (WORD)LineWidth(LineStart);
			HLocate(CurrentMostX);
			break;
		case CTRL_HOME:
			if(IsMultiLine)	Proc_UpDown(-CurrentY);
			break;
		case CTRL_END:
			if(IsMultiLine)	Proc_UpDown(TextHeight-2-CurrentY);
			break;
		case CTRL_PGUP:
			if(IsMultiLine)	ToPointer(EditBuffer);
			break;
		case CTRL_PGDN:
			if(IsMultiLine)	ToPointer(EOFMem);
			break;
		case CTRL_L:
			if(IsMultiLine) {}
			break;
		case CTRL_T:
			DeleteWord();
			break;
		case CR:
			if(IsMultiLine){
				if(InsertMode)
					Proc_Return();
				else{
					CurrentMostX = 0;
					HLocate(0);
					Proc_UpDown(1);
				}
			}
			else {
				if(isleft) Proc_Left();
				EvPreFinish();
			}
			break;
		case CTRL_N:
			if(IsMultiLine&&Insert(2, CR))
				UserTask|=TASK_SCROLLDOWN+TASK_DISPLAYDOWNLINE+TASK_DISPLAYLINE;
			break;
		case INSKEY:
			SetInsertMode(!InsertMode);
			break;
		case BS:
			if(state==HR_NOHAN){
				Delete(2);
				break;
			}
			if(CurrentPointer==EditBuffer)
				break;
			Proc_Left();
			if(UserTask>1) UserFunc();
		case DELKEY:
			Proc_Del();
			break;
		case CTRL_X:
			IsShowEOLChar=!IsShowEOLChar;
			UserTask=TASK_DISPLAYPAGE;
			break;
		case CTRL_B:
			ToPointer(BlockStart);
			break;
		case CTRL_E:
			ToPointer(BlockEnd);
			break;
		case ALT_I:
			SetIndentMode(!IndentMode);
			break;
		case ALT_Y:
			DeleteEOL();
			break;
		case CTRL_Y:
			if(IsMultiLine){
				CurrentMostX=0;
				HLocate(0);
				if(BaseY+CurrentY==MaxLineCount)
					DeleteEOL();
				else if(Delete(LineWidth(CurrentPointer)+2)){
					UserTask |= TASK_SCROLLUP+TASK_DISPLAYBOTLINE;
				}
			}
			break;
		default:
			if(key==' '&&modifier&0x0003){
				HanKbdState=!HanKbdState;
				break;
			}
			if(state==HR_ASC&&key>=' '&&key<128){
				if(!InsertMode&&*CurrentPointer&0x80)	*(CurrentPointer + 1) = ' ';
				Englisg(key);
			}
			else if(state==HR_ASC&&key=='\t') {
				WORD tmp_TabSize=TabSize-((CurrentX+BaseX)%TabSize);
				if(InsertMode) Insert(tmp_TabSize,' ');
				while(tmp_TabSize--) Proc_Right();
			}
			else {
				if(EditEndPointer>CurrentPointer+1){
					if(state==HR_HANST){
						if(!Hangul()) {
							UserTask |= TASK_DISPLAYLINE;
							InitAutomata();
						}
					}
					else if(state==HR_HANIN || state==HR_HANBS) {
						*CurrentPointer    =(key>>8)&0xFF;
						*(CurrentPointer+1)=key&0xFF;
					}
					else if(state==HR_HANEND) {
						*CurrentPointer    =(key>>8)&0xFF;
						*(CurrentPointer+1)=key&0xFF;
						Proc_Right();
					}
				}
				else{
					UserTask |= TASK_DISPLAYLINE;
					InitAutomata();
				}
			}
			break;
	}
}

void JKEdit::Control_K(WORD key)
{
	if(strchr("CVY",key)&&(!GetBlockSize()|| !BlockDisplay))
		return;
	switch( key ) {
		case  'B':
			SetBlock(BlockStart);
			break;
		case 'K':
			SetBlock(BlockEnd);
			break;
		case  'H':
			BlockDisplay = !BlockDisplay;
			UserTask = TASK_DISPLAYPAGE;
			break;
		case 'C':
			CopyBlock();
			break;
		case 'V':
			MoveBlock();
			break;
		case 'Y':
			DeleteBlock();
			break;
	}
}

void JKEdit::Control_Q(WORD key)
{
	switch(key) {
		case 'A':
			if(IsMultiLine) {}
			break;
		case 'F':
			if(IsMultiLine) {}
			break;
		case 'P':
			ToPointer(LastPosition);
			break;
		case 'L':
			RestoreLine();
			break;
	}
}

void JKEdit::GetCursorType(BOOL& CursorState,int16& CursorCode)
{
	if(CurHanState){
		CursorCode =(!IsPassword)? InpStack[InpSP-1].charcode:((WORD)'*'<< 8)|'*';
		CursorState=TRUE;
	}
	else {
		WORD bch = (*CurrentPointer&0x80)? (*CurrentPointer << 8)|*(CurrentPointer+1) : *CurrentPointer;
		if (bch&0x8000){
			CursorCode = (!IsPassword)? bch:((WORD)'*'<< 8)|'*';
			CursorState = TRUE;
		}
		else {
			CursorCode = (!bch||bch=='\r')? (IsShowEOLChar? EOLChar:' ') : (bch=='\t'? ' ':(!IsPassword)?bch:'*');
			CursorState = FALSE;
		}
	}
}

void JKEdit::XPutCaret()
{
	if(!IsEnableCaret) return;

	BOOL CursorState;
	int16 CursorCode;
	GetCursorType(CursorState,CursorCode);

	IsShowCaret=!IsShowCaret;
	JKDC *dc=CreateClientDC();
	if(dc){
		if(IsShowCaret^GetInverse()){
			dc->SetTextColor(BackColor); dc->SetBackColor(TextColor);
		}
		else{
			dc->SetTextColor(TextColor); dc->SetBackColor(BackColor);
		}
		MouseEvents->HideCursor();
		if(CursorState){
			int8 ch1=(CursorCode>>8)&0xFF,ch2=CursorCode&0xFF;
			if(ch1&0x80){
				dc->HanPutCh(JKPoint(CurrentX*8,CurrentY*LineSpace),ch1,ch2);
			}
			else{
				dc->EngPutCh(JKPoint(CurrentX*8,CurrentY*LineSpace),ch1);
				dc->EngPutCh(JKPoint((CurrentX+1)*8,CurrentY*LineSpace),ch2);
			}
		}
		else
			dc->EngPutCh(JKPoint(CurrentX*8,CurrentY*LineSpace),CursorCode);
		MouseEvents->ShowCursor();
	}
}

void JKEdit::SetEnableCaret(BOOL enable)
{
	if(enable){
		if(!IsEnableCaret) IsEnableCaret=TRUE;
	}
	else{
		if(IsEnableCaret){
			HideCaret();
			IsEnableCaret=FALSE;
		}
	}
}

void JKEdit::ShowCaret()
{
	if(!IsShowCaret) XPutCaret();
}

void JKEdit::HideCaret()
{
	if(IsShowCaret) XPutCaret();
}

int8 *JKEdit::PrevPointer(int8 *ptr, int16 count)
{
 int16 line=0;

 while(line <= count && ptr > EditBuffer)
	 if(*(--ptr)==LF && *(ptr-1)==CR) line++;
 return line<=count? EditBuffer : ptr+1;
}

int8 *JKEdit::NextPointer(int8 *ptr, int16 count)
{
 int16 line=0;
 while(line<count&&ptr<EOFMem)
	 if(*ptr++==CR && *ptr==LF) { ++ptr; ++line; }
 return ptr;
}

WORD JKEdit::LineWidth(int8 *ptr)
{
	WORD size=0;

	while(EOFMem>ptr && !(*ptr++==CR&&*ptr==LF))	++size;
	return size;
}

void JKEdit::DownLine(JKDC& dc,int16 Proc_Delta)
{
	int16 y=CurrentY+Proc_Delta;
	if(y>=0&&y<TextHeight&&BaseY+y<=MaxLineCount){
		MouseEvents->HideCursor();
		OneLine(dc,NextPointer(LineStart,Proc_Delta),y);
		MouseEvents->ShowCursor();
	}
}

void JKEdit::InsertLine(JKDC& dc,int16 y)
{
	dc.SetTextColor(TextColor); dc.SetBackColor(BackColor);
	MouseEvents->HideCursor();
	dc.ScrollDown(JKRect(0,y*LineSpace,TextWidth*8-1,TextHeight*LineSpace-1),LineSpace);
	MouseEvents->ShowCursor();
}

void JKEdit::DeleteLine(JKDC& dc,int16 y)
{
	dc.SetTextColor(TextColor); dc.SetBackColor(BackColor);
	MouseEvents->HideCursor();
	dc.ScrollUp(JKRect(0,y*LineSpace,TextWidth*8-1,TextHeight*LineSpace-1),LineSpace);
	MouseEvents->ShowCursor();
}

void JKEdit::UserFunc()
{
	JKDC *dc=CreateClientDC();
	if(dc){
		MouseEvents->HideCursor();
		if(UserTask & TASK_DISPLAYPAGE)
			DisplayPage(*dc);
		else {
			if(UserTask & TASK_SCROLLUPPAGE)								DeleteLine(*dc,0);
			else if(UserTask & TASK_SCROLLUP)								DeleteLine(*dc,CurrentY);
			else if(UserTask & TASK_SCROLLDOWN)							InsertLine(*dc,CurrentY);
			if((UserTask & TASK_DISPLAYUPLINE) && CurrentY)	OneLine(*dc,PrevPointer(LineStart, 1), CurrentY-1);
			if(UserTask & TASK_DISPLAYDOWNLINE)							DownLine(*dc,1);
			else if(UserTask & TASK_DISPLAYBOTLINE)					DownLine(*dc,TextHeight-CurrentY-1);

			if(UserTask & TASK_DISPLAYLINE)	OneLine(*dc,LineStart,CurrentY);
		}
		MouseEvents->ShowCursor();
	}
	UserTask = 0;
}

BOOL JKEdit::Insert(WORD size, BYTE content)
{
	return InsertPointer(size,content,CurrentPointer);
}

BOOL JKEdit::InsertPointer(WORD size,BYTE content,int8 *ptr)
{
	if(EditBufferSize-GetTextSize()<size) return FALSE;

	movmem(ptr,ptr+size,WORD(EOFMem-ptr+1));
	setmem(ptr,size,content);
	if(*ptr==CR){
		*(ptr+1) = LF;
		MaxLineCount++;
	}
	else UserTask|=TASK_DISPLAYLINE;
	EOFMem += size;
	IsSaved = FALSE;
	if(LastPosition>ptr) LastPosition+=size;
	if(GetBlockSize()) {
		if(BlockStart>ptr) BlockStart += size;
		if(BlockEnd>ptr) BlockEnd += size;
	}
	return TRUE;
}

WORD JKEdit::GetLineCount(int8 *start, int8 *finish)
{
	WORD line=0;
	while(finish>start)
		if(*start++==CR && *start==LF) ++line;
	return line;
}

WORD JKEdit::GetLineCountInBlock(int8 *start, int8 *finish)
{
	if(DWORD(finish-start)<DWORD(EOFMem)-DWORD(finish-start))
		return GetLineCount(start,finish);
	else
		return MaxLineCount-GetLineCount(EditBuffer,start)-GetLineCount(finish,EOFMem);
}

BOOL JKEdit::Delete(WORD size)
{
	if(EOFMem==CurrentPointer || !size)
		return FALSE;
	int8 *DelEndPos = CurrentPointer + size;
//	WORD line=GetLineCountInBlock(CurrentPointer, DelEndPos);
	WORD line=GetLineCount(CurrentPointer, DelEndPos);
	if(line) MaxLineCount -= line;
	else UserTask |= TASK_DISPLAYLINE;
	movmem(DelEndPos,CurrentPointer,WORD(EOFMem-DelEndPos+1));
	EOFMem -= size;
	IsSaved = FALSE;
	if(LastPosition > CurrentPointer)
		LastPosition = LastPosition>DelEndPos? LastPosition-size:CurrentPointer;
	if(GetBlockSize()) {
		if(CurrentPointer<=BlockStart && BlockEnd <= DelEndPos) {
			BlockStart = BlockEnd = CurrentPointer;
		}
		else {
			if(DelEndPos<= BlockStart) {
				BlockStart -= size; BlockEnd -= size;
			}
			else if(CurrentPointer <= BlockStart) {
				BlockStart=CurrentPointer; BlockEnd -= size;
			}
			else if(DelEndPos <= BlockEnd)
				BlockEnd -= size;
			else if(CurrentPointer<=BlockEnd)
				BlockEnd = CurrentPointer;
		}
	}
	return TRUE;
}

BOOL JKEdit::IsInBlock(int8 *ptr)
{
	return(BlockStart<=ptr&&ptr<BlockEnd);
}

void JKEdit::SetBlock(int8 *& block_ptr)
{
	block_ptr = CurrentPointer;
	BlockDisplay = TRUE;
	UserTask = TASK_DISPLAYPAGE;
}

WORD JKEdit::GetBlockSize()
{
	return BlockStart>=BlockEnd? 0:WORD(BlockEnd-BlockStart);
}

void JKEdit::CopyBlock()
{
	WORD blocksize=GetBlockSize();
	if(!IsInBlock(CurrentPointer) && Insert(blocksize,' ')) {
		MaxLineCount += GetLineCountInBlock(BlockStart,BlockEnd) ;
		movmem(BlockStart,CurrentPointer,blocksize);
		BlockStart = CurrentPointer;
		BlockEnd = BlockStart+blocksize;
		UserTask = TASK_DISPLAYPAGE;
	}
	else if(CurrentPointer==BlockStart) {
		int8 *temp_ptr = CurrentPointer;
		CurrentPointer = BlockEnd;
		CopyBlock();
		BlockEnd = BlockStart;
		CurrentPointer = BlockStart = temp_ptr;
	}
}

void JKEdit::LineMoveAfterBlock()
{
	int8 *homep=PrevPointer(LineStart,CurrentY);
	if(homep<=BlockStart)
		CurrentY-=GetLineCount(BlockStart,BlockEnd);
	else if(homep<=BlockEnd) {
		BaseY-=GetLineCount(BlockStart,homep);
		CurrentY=GetLineCount(BlockEnd,CurrentPointer);
	}
	else BaseY-=GetLineCount(BlockStart,BlockEnd);
	IsSaved=FALSE;
}

void JKEdit::MoveBlock()
{
	int8 buffer[2001], *start, *mid, *endp;
	WORD count, len, size;
	WORD blocksize=GetBlockSize();

	if(IsInBlock(CurrentPointer)) return;
	if(BlockEnd<=CurrentPointer){
		start=BlockStart; mid=BlockEnd; endp=CurrentPointer;
		LineMoveAfterBlock();
		BlockStart += (WORD)(CurrentPointer - BlockEnd);
	}
	else {
		start = CurrentPointer;
		mid = BlockStart;
		endp = BlockEnd;
		BlockStart = CurrentPointer;
	}
	len = (WORD)(endp - start );
	count = (WORD) (mid - start );
	while(count) {
		size = (count>=MAXBLOCKSIZE)? MAXBLOCKSIZE :count ;
		count -= size;
		strncpy(buffer,start, size);
		movmem(start+size,start,len-size);
		strncpy(endp-size,buffer,size);
	}
	BlockEnd = BlockStart+blocksize;
	LineStart = PrevPointer(BlockStart,0);
	CurrentMostX=WORD(BlockStart-LineStart);
	HLocate(CurrentMostX);
	UserTask = TASK_DISPLAYPAGE;
	IsSaved=FALSE;
}

void JKEdit::DeleteBlock()
{
	if(IsInBlock(CurrentPointer))
		ToPointer(BlockStart);
	else if(BlockEnd<=CurrentPointer){
		LineMoveAfterBlock();
		CurrentPointer -= GetBlockSize();
	}
	int8 *temp = CurrentPointer;
	CurrentPointer = BlockStart;
	Delete(GetBlockSize());
	CurrentPointer = temp ;
	LineStart = PrevPointer(CurrentPointer,0);
	CurrentMostX = WORD(CurrentPointer-LineStart);
	HLocate(CurrentMostX);
	UserTask=TASK_DISPLAYPAGE;
}


int8 *JKEdit::OneLine(JKDC& dc,int8 *start, int16 y)
{
	int8 *ptr=start;
	int16 pos=0,tmp=0;
	WORD size=LineWidth(start)+(IsShowEOLChar? 1:0);
	if(BaseX<size) {
		BOOL isptrhan=HanMan->IsHangul2nd(ptr,BaseX);
		ptr += BaseX;
		int16 limit = (BaseX+TextWidth-1>= size) ? size-BaseX : TextWidth;
		if(isptrhan) {
			if(BlockDisplay&&IsInBlock(ptr)) {
				dc.SetTextColor(BackColor);	dc.SetBackColor(TextColor);
			}
			else {
				dc.SetTextColor(TextColor);	dc.SetBackColor(BackColor);
			}
			if(!IsPassword)
				dc.HanPutCh(JKPoint((pos-1)*8,y*LineSpace),*(ptr-1),*ptr);
			else
				dc.HanPutCh(JKPoint((pos-1)*8,y*LineSpace),'*','*');
			pos++;
			ptr++;
		}
		for(;pos<limit;++pos) {
			if(BlockDisplay&&IsInBlock(ptr)) {
				dc.SetTextColor(BackColor);	dc.SetBackColor(TextColor);
			}
			else {
				dc.SetTextColor(TextColor);	dc.SetBackColor(BackColor);
			}

			if(*ptr=='\t') {
				*ptr=' ';
				InsertPointer(TabSize-(pos%TabSize)-1,' ',ptr);
				limit+=TabSize-(pos%TabSize);
				tmp+=TabSize-(pos%TabSize)-1;
			}
			else {
				if(*ptr&0x80) {
					if(pos != limit-1) {
						if(!IsPassword){
							dc.HanPutCh(JKPoint(pos*8,y*LineSpace),*ptr, *(ptr+1));
						}
						else{
							dc.EngPutCh(JKPoint(pos*8,y*LineSpace),'*');
							dc.EngPutCh(JKPoint((pos+1)*8,y*LineSpace),'*');
						}
						ptr += 2;
						pos++;
					}
					else {
						dc.EngPutCh(JKPoint(pos*8,y*LineSpace),(!IsPassword)? ' ':'*');
					}
				}
				else {
					if(IsEOF(ptr)&&IsShowEOLChar) {
						dc.EngPutCh(JKPoint(pos*8,y*LineSpace),EOLChar);
						pos++;
						break;
					}
					else if(IsEOL(ptr)&&IsShowEOLChar){
						dc.EngPutCh(JKPoint(pos*8,y*LineSpace),EOLChar);
						ptr +=2;
						pos++;
						break;
					}
					else {
						dc.EngPutCh(JKPoint(pos*8,y*LineSpace),(!IsPassword)? *ptr:'*');
						ptr++;
					}
				}
			}
		}
		dc.SetTextColor(TextColor);	dc.SetBackColor(BackColor);
	}

	if(BlockDisplay&&IsInBlock(ptr)) {
		dc.SetTextColor(BackColor);	dc.SetBackColor(TextColor);
	}
	else {
		dc.SetTextColor(TextColor);	dc.SetBackColor(BackColor);
	}
	for(;pos < TextWidth;pos++)
		dc.EngPutCh(JKPoint(pos*8,y*LineSpace),' ');
	start += (size+2-(IsShowEOLChar? 1:0))+tmp;
	return start;
}

void JKEdit::DisplayPage(JKDC& dc)
{
	int8 *ptr;
	int16 y, last;

	OneLine(dc,LineStart,CurrentY);
	if(MaxLineCount+1>=BaseY+TextHeight)
		last = TextHeight-1;
	else {
		last = MaxLineCount-BaseY;
		dc.SolidBar(JKRect(0,last*LineSpace,TextWidth*8-1,TextHeight*LineSpace-1),BackColor);
	}
	ptr = PrevPointer(LineStart,CurrentY);
	for(y=0;y<=last;y++) ptr=OneLine(dc,ptr,y);
}

void JKEdit::HLocate(WORD pos)
{
	if (pos<BaseX||BaseX+TextWidth-1<=pos) {
		CurrentX = pos<BaseX? 0:TextWidth-2;
		BaseX = pos-CurrentX;
		UserTask = TASK_DISPLAYPAGE;
	}
	else CurrentX = pos-BaseX;
	LastPosition = CurrentPointer;
	CurrentPointer = LineStart + pos;
}

void JKEdit::VLocate(WORD line_no,int8 *destin)
{
	if(line_no<BaseY||BaseY+TextHeight<=line_no){
		if(!CurrentY&&line_no+1==BaseY)
			UserTask|=TASK_DISPLAYLINE|TASK_SCROLLDOWN;
		else if(CurrentY==TextHeight-1 && BaseY+TextHeight==line_no)
			UserTask|=TASK_DISPLAYLINE|TASK_SCROLLUPPAGE|TASK_DISPLAYDOWNLINE;
		else
			UserTask=TASK_DISPLAYPAGE;
		if(line_no<CurrentY&&line_no<BaseY)
			CurrentY=line_no;
		BaseY=line_no-CurrentY;
	}
	else CurrentY=line_no-BaseY;
	strncpy(LineBuffer, LineStart, MAXLINEBUF);
	HLocate(WORD(destin-LineStart));
}

void JKEdit::ToPointer(int8 *destin)
{
	WORD line_no=BaseY+CurrentY;

	if(destin <= CurrentPointer) {
		if(WORD(destin-EditBuffer)<WORD(CurrentPointer-destin))
			line_no=GetLineCount(EditBuffer,destin);
		else
			line_no-=GetLineCount(destin,CurrentPointer);
	}
	else {
		if(WORD(destin-CurrentPointer)<WORD(EOFMem-destin))
			line_no+=GetLineCount(CurrentPointer, destin);
		else
			line_no=MaxLineCount-GetLineCount(destin, EOFMem);
	}
	LineStart = PrevPointer(destin,0);
	CurrentMostX = WORD(destin-LineStart);
	VLocate(line_no,destin);
}

void JKEdit::Proc_Left()
{
	if(EditBuffer<CurrentPointer) {
		if(CurrentPointer==LineStart)
			ToPointer(CurrentPointer-2);
		else {
			if(HanMan->IsHangul2nd(LineStart,CurrentX+BaseX-1))
				CurrentMostX = BaseX+CurrentX-2;
			else
				CurrentMostX = BaseX+CurrentX-1;
			HLocate(CurrentMostX);
		}
	}
}

BOOL JKEdit::IsEOL(int8 *ptr)
{
	return (*ptr==CR && *(ptr+1)==LF);
}

BOOL JKEdit::IsEOF(int8 *ptr)
{
	return (*ptr==0x1a || !*ptr || ptr==EOFMem);
}

void JKEdit::Han_Right()
{
	if(CurrentPointer<EOFMem){
		CurrentMostX = BaseX+CurrentX+2;
		HLocate(CurrentMostX);
	}
}

void JKEdit::Proc_Right()
{
	if(CurrentPointer<EOFMem){
		if(IsEOL(CurrentPointer))
			ToPointer(CurrentPointer+2);
		else {
			if(HanMan->IsHangul1st(LineStart, CurrentX+BaseX))
				Han_Right();
			else {
				CurrentMostX = BaseX+CurrentX+1;
				HLocate(CurrentMostX);
			}
		}
	}
}

void JKEdit::Proc_Del()
{
	if(IsEOL(CurrentPointer)) {
		if(Delete(2)) UserTask |= TASK_SCROLLUP|TASK_DISPLAYLINE|TASK_DISPLAYBOTLINE;
	}
	else {
		if(HanMan->IsHangul1st(LineStart, CurrentX+BaseX))
			Delete(2);
		else
			Delete(1);
	}
}

BOOL JKEdit::Englisg(int8 ch)
{
	if(InsertMode||IsEOL(CurrentPointer)||IsEOF(CurrentPointer))
		if(!Insert(1, ' '))	return FALSE;

	if(EOFMem >= CurrentPointer) {
		if(HanMan->IsHangul1st(LineStart, CurrentX+BaseX))
			*(CurrentPointer+1) = ' ';
		*CurrentPointer =ch;
		UserTask |= TASK_DISPLAYLINE;
		IsSaved = FALSE;
		Proc_Right();
	}
	return TRUE;
}

BOOL JKEdit::Hangul()
{
	if(InsertMode||IsEOL(CurrentPointer)||IsEOF(CurrentPointer)) {
		if(!Insert(2, ' ')) return FALSE;
	}
	else if(!InsertMode){
		UserTask |= TASK_DISPLAYLINE;
	}
	if(HanMan->IsHangul1st(LineStart, CurrentX+BaseX+1)) {
		*(CurrentPointer+1)=*(CurrentPointer+2) = ' ';
		UserTask |= TASK_DISPLAYLINE;
	}
	IsSaved = FALSE;
	return TRUE;
}

void JKEdit::Proc_Hanja()
{
	WORD origincode, transcode;

	if(IsEOF(CurrentPointer))	return;

	if(EOFMem >= CurrentPointer+1) {
		if(HanMan->IsHangul1st(LineStart, CurrentX+BaseX)) {
			origincode = MAKEWORD(*(CurrentPointer+1),*CurrentPointer);
			if((BYTE)*CurrentPointer<224){
				transcode = FindHanja(origincode);
			}
			else{
				transcode = HanMan->HanjaCode2HangulCode(origincode);
			}
			if(transcode) {
				*CurrentPointer = HIBYTE(transcode);
				*(CurrentPointer+1) = LOBYTE(transcode);
				UserTask |= TASK_DISPLAYLINE;
				IsSaved = FALSE;
			}
		}
		Proc_Right();
	}
}

void JKEdit::Proc_UpDown(int16 Proc_Delta)
{
	int8 *destin;
	WORD line_no, size;

	if(Proc_Delta<=0) {
		Proc_Delta=-Proc_Delta;
		line_no = (Proc_Delta>BaseY+CurrentY) ? 0 : BaseY+CurrentY-Proc_Delta;
		LineStart = PrevPointer(LineStart, Proc_Delta);
	}
	else {
		line_no = BaseY+CurrentY;
		if(Proc_Delta > MaxLineCount-line_no)
			Proc_Delta = MaxLineCount-line_no;
		line_no += Proc_Delta;
		LineStart = NextPointer(LineStart, Proc_Delta);
	}
	size = LineWidth(LineStart);
	destin = LineStart + ((CurrentMostX > size) ? size : CurrentMostX);
	if(HanMan->IsHangul2nd(LineStart, (CurrentMostX > size) ? size : CurrentMostX))
		destin--;
	VLocate(line_no,destin);
}

void JKEdit::DeleteEOL()
{
	Delete(LineWidth(CurrentPointer));
}

void JKEdit::DeleteWord()
{
	while(EOFMem>CurrentPointer && IsInWord(CurrentPointer))  Proc_Del();
	while(EOFMem>CurrentPointer && !IsInWord(CurrentPointer))	Proc_Del();
}

void JKEdit::Proc_Return()
{
	WORD pos=0;

	if(IndentMode) {
		pos=strspn(LineStart," ");
		if(pos>=BaseX+CurrentX)
			pos=BaseX;
	}
	if(Insert(pos+2,CR)) {
		UserTask|=TASK_SCROLLDOWN+TASK_DISPLAYLINE+TASK_DISPLAYUPLINE;
		memset(CurrentPointer+2, ' ', pos);
		ToPointer(CurrentPointer+pos+2);
	}
}

BOOL JKEdit::IsInWord(int8 *ch)
{
	return !strchr(" \r\n<>,:.()[]^'*+-/$[", *ch);
}

void JKEdit::MoveLeftWord()
{
	int8 *ptr =  CurrentPointer;

	while(EditBuffer<ptr && !IsInWord(ptr-1))	ptr--;
	while(EditBuffer<ptr &&  IsInWord(ptr-1))	ptr--;
	ToPointer(ptr);
}

void JKEdit::MoveRightWord()
{
	int8 *ptr = CurrentPointer;

	while(EOFMem>ptr &&  IsInWord(ptr))	ptr++;
	while(EOFMem>ptr && !IsInWord(ptr))	ptr++;
	ToPointer(ptr);
}

void JKEdit::RestoreLine()
{
	int8 *ptr = LineBuffer;
	WORD buf_size=0;

	CurrentMostX=0;
	HLocate(0);
	while(buf_size<MAXLINEBUF&&*ptr&&!IsEOL(ptr)){
		++buf_size;
		++ptr;
	}
	ChangeLine(LineWidth(CurrentPointer), buf_size, LineBuffer);
}

BOOL JKEdit::ChangeLine(int16 mem_size, int16 buf_size, int8 *buf_ptr)
{
	BOOL success = buf_size<mem_size? Delete((WORD)(mem_size-buf_size)):Insert((WORD)(buf_size-mem_size),' ');
	if(success)	strncpy(CurrentPointer, buf_ptr, buf_size);
	return success;
}
