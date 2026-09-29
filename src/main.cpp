#include <windows.h>
#include <objbase.h>

#include <string>

#include "app.h"
#include "updater.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdline, int) {
  const std::wstring args = cmdline ? cmdline : L"";
  const bool start_hidden = args.find(L"--tray") != std::wstring::npos;
  const bool after_update = args.find(L"--updated") != std::wstring::npos;

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  // Une seule ToolBox à la fois. Après une mise à jour, on attend que l'ancienne se ferme.
  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\ToolBox.SingleInstance");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (!after_update || WaitForSingleObject(mutex, 15000) == WAIT_TIMEOUT) {
      if (HWND existing = FindWindowW(kWindowClass, nullptr)) PostMessageW(existing, WM_APP_SHOW, 0, 0);
      CloseHandle(mutex);
      return 0;
    }
  }

  if (after_update) Updater::CleanupOldExe();

  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  int rc = 0;
  {
    App app(instance, start_hidden);
    rc = app.Run();
  }
  CoUninitialize();

  ReleaseMutex(mutex);
  CloseHandle(mutex);
  return rc;
}
