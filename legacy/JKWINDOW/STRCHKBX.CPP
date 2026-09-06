#include "strchkbx.h"

StringCheckBox::StringCheckBox(JKRect rect,WORD ctrlid,char *truestr,char *falsestr):
	JKCheckBox(rect,ctrlid),TrueString(0),FalseString(0)
{
	SetCheckString(truestr,falsestr);
}

StringCheckBox::~StringCheckBox()
{
	if(TrueString) delete[] TrueString;
	if(FalseString) delete[] FalseString;
}

JKRect StringCheckBox::CalcClientRect()
{
	return JKControl::CalcClientRect();
}

void StringCheckBox::PaintClient(JKDC& dc,JKRect&)
{
	JKRect rect=ClientRect.SetLeftTopBy(JKPoint(0,0));
	dc.Box3D(rect,-1,BackColor);
	if(HasFocus()) dc.Rectangle3D(rect,-1,LtBlue,Blue);
	rect.Expand(-1);

	dc.IsOverWrite=FALSE;
	dc.SetTextColor(TextColor);
	if(Status && TrueString){
		dc.TextOutX(rect,TrueString,ADJ_XYCENTER);
	}
	else if(!Status && FalseString){
		dc.TextOutX(rect,FalseString,ADJ_XYCENTER);
	}
}

void StringCheckBox::SetCheckString(char *truestr,char *falsestr)
{
	if(TrueString) {
		delete[] TrueString;
		TrueString=0;
	}
	if(truestr){
		TrueString = new char[strlen(truestr)+1];
		if(TrueString) {
			strcpy(TrueString,truestr);
		}
	}

	if(FalseString) {
		delete[] FalseString;
		FalseString=0;
	}
	if(falsestr){
		FalseString = new char[strlen(falsestr)+1];
		if(FalseString) {
			strcpy(FalseString,falsestr);
		}
	}
}
