#include "editctrl.h"

EditControl::EditControl(JKRect& rect,WORD ctlid,int8 *buffer,WORD bufsize,BOOL ismultiline,BOOL ishangulkbd,JKControl *parent):
	JKEdit(rect,ctlid,buffer,bufsize,ismultiline,ishangulkbd,parent)
{}

EditControl::~EditControl()
{}

JKRect EditControl::CalcClientRect()
{
	return JKControl::CalcClientRect();
}

void EditControl::PaintWindow(JKDC&,JKRect&)
{}

