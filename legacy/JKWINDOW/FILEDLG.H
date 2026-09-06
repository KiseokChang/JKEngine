#ifndef _FILEDLG_H
#define _FILEDLG_H

#include <dir.h>
#include "jkdialog.h"

class FileDialog:public JKDialog
{
public:
	FileDialog(JKControl *parent=0);
 ~FileDialog();

	void InitWindow();
	void SetupWindow();

	void InitDriveList();
	void InitFileList();
	void ClearFileList();


	void EvButtonClick(WORD ctrlid);
	void EvLBSelectEntry(WORD ctrlid,uint16 index);
	void EvLBTouchEntry(WORD ctrlid,uint16 index);
	void EvCharDown(WORD key,WORD modifier);

	int16 CompareFileName(int8 *fn1,int8 *fn2);

	int16 SaveDrive;
	int8 SavePath[MAXDIR];
	int8 FullPath[MAXDIR];
	uint16 DriveCount;
	int16 Drives[26];
};

#endif