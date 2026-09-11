#include "app/Application.h"
#include <windows.h>

// shellapi.h only declares this when NTDDI_VERSION >= NTDDI_WIN7; declare it
// explicitly so the app builds regardless of SDK macro defaults.
extern "C" HRESULT WINAPI SetCurrentProcessExplicitAppUserModelID(PCWSTR);

int main(int argc, char** argv) {
    SetCurrentProcessExplicitAppUserModelID(L"StockWidget.1");
    Application app(argc, argv);
    return app.exec();
}
