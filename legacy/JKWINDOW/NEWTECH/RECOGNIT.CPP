#include "recogapp.h"

EventHandler *CreateApplication()
{
	return new RecognitionApplication();
}