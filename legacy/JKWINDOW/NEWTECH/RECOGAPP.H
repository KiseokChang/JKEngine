#ifndef _RECOGAPP_H
#define _RECOGAPP_H

#include "jkapp.h"

class RecognitionApplication:public JKApplication
{
public:
	RecognitionApplication();
	~RecognitionApplication();

	void SetupWindow();
};

#endif