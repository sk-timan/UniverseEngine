// OpenSpecTest.cpp: application entry point.
//

#include "app/GameApp.h"

int WINAPI wWinMain(HINSTANCE InInstance, HINSTANCE, PWSTR, int InShowCommand)
{
	GameApp App;
	return App.Run(InInstance, InShowCommand);
}
