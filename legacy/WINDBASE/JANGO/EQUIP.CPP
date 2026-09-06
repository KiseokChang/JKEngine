#include <stdio.h>
#include <conio.h>

#include "jkinit.h"
#include "jkapp.h"
#include "equipwin.h"

void main()
{
	if(!InitJKProgram()) return;

//	EquipWindow *MainFrame = new EquipWindow(8788);
  EquipWindow().Run();
//-----------------------------------------------//
	CloseJKProgram();
}
