#include <stdio.h>
#include <conio.h>

#include "jkinit.h"
#include "jkapp.h"
#include "add24app.h"

EventHandler *CreateApplication()
{
	return new Equip24Application();
}
