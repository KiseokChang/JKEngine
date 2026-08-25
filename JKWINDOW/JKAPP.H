#ifndef _JKAPP_H
#define _JKAPP_H

#include "evnthand.h"
#include "control.h"

#define InternPaletteCount 1
enum InternPaletteName{
	JKPalette=0
};

extern WORD InternPaletteID[InternPaletteCount];



#define InternImageCount 26
enum InternImageName{
	OkUp=0,
	OkDown,
	OkFocus,
	CancelUp,
	CancelDown,
	CancelFocus,
	YesUp,
	YesDown,
	YesFocus,
	NoUp,
	NoDown,
	NoFocus,
	ComboUp,
	ComboDown,
	SpinUp,
	SpinDown,
	SpinLeft,
	SpinRight,
	ArrowUp,
	ArrowUp2,
	ArrowDown,
	ArrowDown2,
	ArrowLeft,
	ArrowLeft2,
	ArrowRight,
	ArrowRight2
};
extern WORD InternImageID[InternImageCount];

class JKApplication : public EventHandler,public JKControl
{
public:
	JKApplication();
	~JKApplication();

	void DestroySubWindow(DWORD winid);

	virtual void InitApplication();
	virtual void CloseApplication();


	WORD Run();
	virtual void 	InitResourceMan();

	BOOL IsInitApp;
	BYTE OldPalette[768];
};
#endif
