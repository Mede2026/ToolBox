#include <windows.h>
#include <objbase.h>
#include <tlhelp32.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

#include "app.h"
#include "updater.h"
#include "util.h"
#include "version.h"

namespace {

HANDLE g_mutex = nullptr;
bool g_mutex_owned = false;

// Note l'erreur dans %APPDATA%\ToolBox\crash.log et prévient l'utilisateur.
void ReportCrash(const std::string& what) {
  {
    std::ofstream log(util::DataDir() / L"crash.log", std::ios::app);
    SYSTEMTIME t;
    GetLocalTime(&t);
    char when[32];
    snprintf(when, sizeof(when), "%04d-%02d-%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
             t.wSecond);
    log << when << "  ToolBox " << util::ToUtf8(TOOLBOX_VERSION_STR) << "  " << what << "\n";
  }
  const std::wstring msg = L"ToolBox a rencontré une erreur et doit se fermer :\n\n" + util::FromUtf8(what) +
                           L"\n\nDétails enregistrés dans %APPDATA%\\ToolBox\\crash.log.";
  MessageBoxW(nullptr, msg.c_str(), L"ToolBox", MB_OK | MB_ICONERROR);
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info) {
  char buf[96];
  const auto* rec = info->ExceptionRecord;
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  snprintf(buf, sizeof(buf), "exception 0x%08lX à ToolBox.exe+0x%llX", rec->ExceptionCode,
           static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(rec->ExceptionAddress) - base));
  ReportCrash(buf);
  return EXCEPTION_EXECUTE_HANDLER;
}

void OnTerminate() {
  std::string what = "erreur inconnue";
  if (auto e = std::current_exception()) {
    try {
      std::rethrow_exception(e);
    } catch (const std::exception& ex) {
      what = ex.what();
    } catch (...) {
    }
  }
  ReportCrash(what);
  ExitProcess(1);
}

// Ferme les autres ToolBox (ToolBox.exe, ou ToolBox.old.exe laissé par une mise à jour) bloquées.
void KillStuckInstances() {
  const std::wstring dir = util::ToLower(util::ExePath().parent_path().wstring());
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return;
  PROCESSENTRY32W pe{sizeof(pe)};
  for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
    if (pe.th32ProcessID == GetCurrentProcessId()) continue;
    const std::wstring exe = util::ToLower(pe.szExeFile);
    if (exe != L"toolbox.exe" && exe != L"toolbox.old.exe") continue;
    const std::wstring path = util::ToLower(util::ProcessPath(pe.th32ProcessID));
    if (!path.empty() && std::filesystem::path(path).parent_path().wstring() != dir) continue;  // autre dossier
    if (HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID)) {
      TerminateProcess(h, 1);
      WaitForSingleObject(h, 3000);
      CloseHandle(h);
    }
  }
  CloseHandle(snap);
}

// Une seule ToolBox à la fois. Retourne false si une autre, qui fonctionne, a été affichée à la place.
bool AcquireSingleInstance(bool after_update) {
  g_mutex = CreateMutexW(nullptr, TRUE, L"Local\\ToolBox.SingleInstance");
  if (GetLastError() != ERROR_ALREADY_EXISTS) {
    g_mutex_owned = true;
    return true;
  }
  // Une ToolBox qui répond : on l'affiche et on s'arrête là.
  if (!after_update) {
    if (HWND existing = FindWindowW(kWindowClass, nullptr)) {
      DWORD_PTR r = 0;
      if (SendMessageTimeoutW(existing, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 3000, &r)) {
        PostMessageW(existing, WM_APP_SHOW, 0, 0);
        return false;
      }
    }
  }
  // Sinon (mise à jour, ou ToolBox bloquée / invisible) : on attend un peu, puis on la remplace.
  DWORD wait = WaitForSingleObject(g_mutex, after_update ? 8000 : 4000);
  if (wait == WAIT_TIMEOUT) {
    KillStuckInstances();
    wait = WaitForSingleObject(g_mutex, 5000);
  }
  g_mutex_owned = (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED);
  return true;  // même sans le verrou, mieux vaut démarrer que ne rien afficher
}

}  // namespace

// Appelé avant de lancer la nouvelle version (mise à jour) : elle n'a pas à attendre notre fermeture.
void ReleaseSingleInstance() {
  if (g_mutex && g_mutex_owned) ReleaseMutex(g_mutex);
  g_mutex_owned = false;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdline, int) {
  SetUnhandledExceptionFilter(OnUnhandledException);
  std::set_terminate(OnTerminate);

  const std::wstring args = cmdline ? cmdline : L"";
  const bool start_hidden = args.find(L"--tray") != std::wstring::npos;
  const bool after_update = args.find(L"--updated") != std::wstring::npos;

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  if (!AcquireSingleInstance(after_update)) {
    CloseHandle(g_mutex);
    return 0;
  }
  Updater::CleanupOldExe();

  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  int rc = 0;
  {
    App app(instance, start_hidden);
    rc = app.Run();
    ReleaseSingleInstance();  // une nouvelle ToolBox peut démarrer pendant qu'on nettoie
  }
  CoUninitialize();
  CloseHandle(g_mutex);
  return rc;
}
