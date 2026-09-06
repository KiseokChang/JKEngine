#include "jkmodule.h"
#include "jkinit.h"
#include "editwin.h"
#include "jkapp.h"

class IconEditApp : public JKApplication
{
public:
	IconEditApp():
		JKApplication()
	{}
 ~IconEditApp(){}

	void SetupWindow()
	{
		int16 width=24,height=24;
      EditWindow *MainFrame;
      MainFrame = new EditWindow(width,height);
      AddControl(MainFrame);
    }
};


EventHandler *CreateApplication()
{
	return new IconEditApp();
}
