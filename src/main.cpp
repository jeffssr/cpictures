#include <windows.h>

#include "app/app.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    (void)commandLine;
    return cpictures::RunApplication(instance, showCommand, GetCommandLineW());
}
