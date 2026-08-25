#ifndef _RECOGWIN_H
#define _RECOGWIN_H

#include "recoprog.h"
#include "jkwindow.h"

#define ID_INPUTBOARD		0x0010

class RecognitionWindow:public JKWindow
{
public:
	RecognitionWindow();
 ~RecognitionWindow();
	void SetupWindow();

	void EvShowWindow();

	ProcessResult RespondMessage(JKMSG& msg);
	void FinishStrokeInput();
	void FinishCharacterInput();

	JKPoint  CalcThreshold01(unsigned width,unsigned height);
	unsigned Resize_Filter(JKRect region,JKRect normal,JKPoint *points,unsigned pointcount);
	unsigned FindStroke(JKRect region,JKPoint *points,unsigned pointcount);
	BOOL ProcessInput(StrokeChain& stroke);
	void ProcessStroke();
};
#endif