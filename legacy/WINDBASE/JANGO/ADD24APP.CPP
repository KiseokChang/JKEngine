#include "add24app.h"
#include "add24dlg.h"
#include "jkbutton.h"

Equip24Application::Equip24Application():
	JKApplication()
{
	BackColor=LtBlue;
}

Equip24Application::~Equip24Application()
{

}

void Equip24Application::SetupWindow()
{
	JKButton *btn=new JKButton(JKRect(100,100,300,300),ID_BUTTON+1);
	btn->SetText("¯¡¸bÐa‹¡");
	AddControl(btn);

	btn=new JKButton(JKRect(500,500,700,700),ID_BUTTON+2);
	btn->SetText("{¸w‹¡");
	AddControl(btn);
}
/*
void Equip24Application::PaintClient(JKDC& dc,JKRect&)
{
}
*/
void Equip24Application::EvButtonClick(WORD ctrlid)
{
	switch(ctrlid) {
		case ID_BUTTON+1:
			AddEquip24Dialog *eqp24=new AddEquip24Dialog(0);
			if(eqp24) {
				eqp24->Run();
				delete eqp24;
			}
			break;

		case ID_BUTTON+2:
			CloseWindow();
			break;
	}
}
