#include "jkinit.h"
#include "jkmain.h"

void main()
{
	if(InitJKProgram()) {
		MainApplication = CreateApplication();
		if(MainApplication) {
			MainApplication->Run();
			delete MainApplication;
		}
		CloseJKProgram();
	}
}
