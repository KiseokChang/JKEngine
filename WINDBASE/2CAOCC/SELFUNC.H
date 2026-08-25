#ifndef _SELFUNC_H
#define _SELFUNC_H

#include "jkdialog.h"

class SelectFunctionDialog : public JKDialog
{
public:
	SelectFunctionDialog(JKRect& rect,JKControl* parent=0);
 ~SelectFunctionDialog();
};
#endif
