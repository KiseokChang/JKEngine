#ifndef _JKEDIT_H
#define _JKEDIT_H

#include "automata.h"
#include "control.h"

#define  MAXLINEBUF         (255)          /* Ðeº‰· ÂA” Íe»³ˆa“w ‹©·¡ */
#define  DEFAULTBUFSIZE     (10000) 			   /* ¸åÁA Íe»³¶wœ·· ÂA”Ça‹¡  */
#define  MAXBLOCKSIZE       (2000)         /* §iœâ·a¡ ”až‰ ® ·¶“e ÂA”Ça‹¡ */

#define  TASK_DISPLAYPAGE      0x0100    /* ¸å ÑÁ¡e ”a¯¡ Â‰b */
#define  TASK_SCROLLUPPAGE     0x0040    /* ¸åÁA ¶á¡ ÑÁ¡e ¯aÇa© */
#define  TASK_SCROLLUP         0x0020    /* Ñe¸ Äá¬á ´aœ ¦¦… e ¶á¡ ÐeÄe ¯aÇa© */
#define  TASK_SCROLLDOWN       0x0010    /* Ñe¸ Äá¬á ´aœ ¦¦… e ´aœ¡ ÐeÄe ¯aÇa© */
#define  TASK_DISPLAYUPLINE    0x0008    /* Ðeº‰ ¶á¡ ·¡•·Ð¬á ‹a º‰·i ”a¯¡ Îa¯¡ */
#define  TASK_DISPLAYDOWNLINE  0x0004    /* Ðeº‰ ´aœ¡ ·¡•·Ð¬á ‹a º‰·i ”a¯¡ Îa¯¡ */
#define  TASK_DISPLAYBOTLINE   0x0002    /*  … ´aœº‰ Îa¯¡ */
#define  TASK_DISPLAYLINE      0x0001    /* Ñe¸ º‰ ”a¯¡ Îa¯¡ */

class JKEdit:public JKControl,public HangulAutomata
{
public:
	JKEdit(JKRect& rect,WORD ctlid,int8 *buffer=0,WORD bufsize=DEFAULTBUFSIZE,BOOL ismultiline=TRUE,BOOL Hangul=TRUE,JKControl *parent=0);
 ~JKEdit();

	void InitWindow();
	JKRect CalcClientRect();
	void PaintWindow(JKDC& dc,JKRect&);
	void PaintClient(JKDC& dc,JKRect& rect);
	ProcessResult RespondMessage(JKMSG& msg);
	void EvSetFocus(DWORD);
	void EvKillFocus(DWORD);
	void EvTimerHappen(DWORD);
	void EvCharDown(WORD key,WORD modifier);
	void EvKeyDown(WORD key,WORD modifier);

	void EvKeyboardDown(WORD key,WORD modifier);
	void EvChar(WORD key,WORD modifier,int16 state,BOOL isleft);


	WORD FindHanja(WORD hangulcode);
	void InitEditorVariable();

	void SetEnableCaret(BOOL enable);
	void ShowCaret();
	void HideCaret();

	void GetCursorType(BOOL& CursorState,int16& CursorCode);
	BOOL GetInverse();

	WORD GetTextSize();
	int8 *GetText(){ return EditBuffer; }
	BOOL GetText(int8 *str,int16 size=-1);
	BOOL SetText(int8 *str);

	void SetBlock(int8 *& block_ptr);
	WORD GetBlockSize();

	void SetInsertMode(BOOL mode);
	void SetIndentMode(BOOL mode);

	BOOL GetInsertMode(BOOL mode){ return InsertMode;}
	BOOL GetIndentMode(BOOL mode){ return IndentMode;}

	BOOL IsMultiLine;
	BOOL IsPassword;

protected:

	void DisplayPage(JKDC& dc);
	int8 *OneLine(JKDC& dc,int8 *start, int16 y);


	WORD EditBufferSize;
	WORD TextWidth, TextHeight;

	BOOL HasOwnership;
	int8 *EditBuffer,*EditEndPointer, *LastPosition, *LineStart, *CurrentPointer;
	int8 *BlockStart, *BlockEnd;
	int8 *EOFMem;
	int8 *SaveBuffer;
	int8 LineBuffer[MAXLINEBUF+1];
	BOOL IsCancel;

	BOOL InsertMode, IndentMode, IsSaved, BlockDisplay;
	WORD CurrentX, CurrentY, BaseX, BaseY;
	WORD UserTask, MaxLineCount;
	BOOL IsEnableCaret;
	BOOL IsShowCaret;
	BOOL HanStart;
	BOOL IsWaitAddKey;
	WORD CtrlFirstKey;

	int8 EOLChar;
	BOOL IsShowEOLChar;
	WORD CurrentMostX;
	int16 TabSize;
	int16 LineSpace;

	void Proc_Hanja();
	BOOL Hangul();
	BOOL Englisg(int8 ch);

	void Control_K(WORD key);
	void Control_Q(WORD key);
	void UserFunc();
	WORD GetLineCount(int8 *start, int8 *finish);
	BOOL IsInBlock(int8 *ptr);
	void ToPointer(int8 *destin);
	int8 *PrevPointer(int8 *ptr, int16 count);
	int8 *NextPointer(int8 *ptr, int16 count);
	WORD LineWidth(int8 *ptr);

	void DownLine(JKDC& dc,int16 delta);
	void InsertLine(JKDC& dc,int16 y);
	void DeleteLine(JKDC& dc,int16 y);

	BOOL ChangeLine(int16 mem_size, int16 buf_size, int8 *buf_ptr);
	BOOL Insert(WORD size, BYTE content);
	BOOL InsertPointer(WORD size, BYTE content,  int8 *ptr);
	void LineMoveAfterBlock();
	void MoveBlock();
	void CopyBlock();
	void DeleteBlock();
	WORD GetLineCountInBlock(int8 *start, int8 *finish);
	BOOL Delete(WORD size);
	void HLocate(WORD pos);
	void VLocate(WORD line_no,int8 *destin);
	void Proc_Left();
	BOOL IsEOL(int8 *ptr);
	BOOL IsEOF(int8 *ptr);
	void Han_Right();
	void Proc_Right();
	void Proc_Del();
	void Proc_Return();
	void Proc_UpDown(int16 Proc_Delta);
	void DeleteEOL();
	void DeleteWord();
	BOOL IsInWord(int8 *ch);
	void MoveLeftWord();
	void MoveRightWord();
	void RestoreLine();

	void EvPreFinish();
	void XPutCaret();
};
#endif