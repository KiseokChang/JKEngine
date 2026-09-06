#ifndef _EDITWIN_H
#define _EDITWIN_H

#include "jkwindow.h"

#define CLEARIMAGE	(USERMESSAGE+1)
#define READIMAGE		(USERMESSAGE+2)
#define SAVEIMAGE		(USERMESSAGE+3)

class EditWindow:public JKWindow
{
public:
	EditWindow(uint16 width,uint16 height,int16 bkcolor=LtGray,char *fname="Noname",char *imgname="Image_Noname");
 ~EditWindow();
	void InitWindow();
	void SetupWindow();
	BOOL CanClose();
	void EvCtrlLeftClick(WORD ctrlid);
	void EvCtrlLeftDragging(WORD ctrlid);
	void EvKeyDown(uint16 key,WORD modifier);
	void EvEditFinish(WORD ctrlid);
  void EvCtrlKillFocus(WORD ctrlid);
	ProcessResult RespondMessage(JKMSG& msg);

	void MakeImageBuffer(uint16 width,uint16 height,BYTE color);
	void DestroyImageBuffer();
	void ChangeImageSize(uint16 width,uint16 height);
	BOOL ReadImageFile();
	void SaveImageFile();


	uint16 IconWidth;
	uint16 IconHeight;
	char FileName[9];
	char ImageName[21];
	BYTE *PackImage[2];

private:
	void SaveResourceFile();
	void SaveDataFile();
};
#endif