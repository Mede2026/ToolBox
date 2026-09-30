#include "util.h"

#include <shlobj.h>
#define SECURITY_WIN32
#include <security.h>
#include <secext.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <vector>
#include <cwctype>

namespace util {

std::string ToUtf8(const std::wstring& w) {
  if (w.empty()) return {};
  int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), len, nullptr, nullptr);
  return out;
}

std::wstring FromUtf8(const std::string& s) {
  if (s.empty()) return {};
  int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
  return out;
}

std::filesystem::path ExePath() {
  std::wstring buf(MAX_PATH, L'\0');
  for (;;) {
    DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n < buf.size()) {
      buf.resize(n);
      return buf;
    }
    buf.resize(buf.size() * 2);
  }
}

std::filesystem::path DataDir() {
  PWSTR raw = nullptr;
  std::filesystem::path dir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
    dir = std::filesystem::path(raw) / L"ToolBox";
  } else {
    dir = ExePath().parent_path();
  }
  CoTaskMemFree(raw);
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

std::wstring ToLower(std::wstring s) {
  std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
  return s;
}

std::wstring ForegroundProcessName() {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd) return {};
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!proc) return {};
  wchar_t path[MAX_PATH];
  DWORD size = MAX_PATH;
  std::wstring name;
  if (QueryFullProcessImageNameW(proc, 0, path, &size)) {
    name = ToLower(std::filesystem::path(path).filename().wstring());
  }
  CloseHandle(proc);
  return name;
}

std::string FileDescription(const std::wstring& exe_path) {
  std::string name;
  DWORD dummy = 0;
  const DWORD size = GetFileVersionInfoSizeW(exe_path.c_str(), &dummy);
  if (size > 0) {
    std::vector<BYTE> data(size);
    struct Lang {
      WORD lang, codepage;
    }* langs = nullptr;
    UINT len = 0;
    if (GetFileVersionInfoW(exe_path.c_str(), 0, size, data.data()) &&
        VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&langs), &len) &&
        len >= sizeof(Lang)) {
      wchar_t sub[64];
      swprintf_s(sub, L"\\StringFileInfo\\%04x%04x\\FileDescription", langs[0].lang, langs[0].codepage);
      wchar_t* desc = nullptr;
      UINT dlen = 0;
      if (VerQueryValueW(data.data(), sub, reinterpret_cast<void**>(&desc), &dlen) && dlen > 1) {
        name = ToUtf8(std::wstring(desc, wcsnlen(desc, dlen)));
      }
    }
  }
  while (!name.empty() && name.back() == ' ') name.pop_back();
  if (name.empty()) name = ToUtf8(std::filesystem::path(exe_path).stem().wstring());
  return name;
}

std::wstring ProcessPath(DWORD pid) {
  std::wstring out;
  if (HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
    wchar_t path[MAX_PATH * 2];
    DWORD size = MAX_PATH * 2;
    if (QueryFullProcessImageNameW(proc, 0, path, &size)) out.assign(path, size);
    CloseHandle(proc);
  }
  return out;
}

void LogError(const std::string& what) {
  std::ofstream log(DataDir() / L"crash.log", std::ios::app);
  SYSTEMTIME t;
  GetLocalTime(&t);
  char when[32];
  snprintf(when, sizeof(when), "%04d-%02d-%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
           t.wSecond);
  log << when << "  (non fatale) " << what << "\n";
}

bool SetClipboardText(HWND owner, const std::wstring& text) {
  for (int i = 0; i < 10 && !OpenClipboard(owner); ++i) Sleep(20);
  EmptyClipboard();
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  bool ok = false;
  if (mem) {
    memcpy(GlobalLock(mem), text.c_str(), bytes);
    GlobalUnlock(mem);
    ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
    if (!ok) GlobalFree(mem);
  }
  CloseClipboard();
  return ok;
}

std::wstring UserFirstName() {
  wchar_t buf[256];
  ULONG size = 256;
  std::wstring name;
  // Nom affiché du compte (compte Microsoft : « Prénom Nom »), sinon le nom d'utilisateur.
  if (GetUserNameExW(NameDisplay, buf, &size) && size > 0) {
    name.assign(buf, size);
  } else {
    DWORD n = 256;
    if (GetUserNameW(buf, &n) && n > 1) name.assign(buf, n - 1);
  }
  if (auto sp = name.find(L' '); sp != std::wstring::npos) name.resize(sp);
  return name;
}

}  // namespace util
