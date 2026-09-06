#include "recogwin.h"
#include "recogapp.h"

RecognitionApplication::RecognitionApplication():
	JKApplication()
{
	BackColor=LtBlue;
}

RecognitionApplication::~RecognitionApplication()
{}

void RecognitionApplication::SetupWindow()
{
	AddControl(new RecognitionWindow());
}
