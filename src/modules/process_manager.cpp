#include "modules/process_manager.h"

#include <dwmapi.h>
#include <psapi.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "util.h"

using nlohmann::json;

namespace {

constexpr ULONGLONG kWatchMs = 6000;      // l'interface redemande toutes les ~3 s
constexpr ULONGLONG kSnapshotMs = 2000;
constexpr size_t kMaxStopped = 30;

// Processus indispensables à Windows (en plus de tout ce qui est dans C:\Windows).
const std::set<std::wstring> kProtected = {
    L"system", L"registry", L"smss.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe", L"services.exe",
    L"lsass.exe", L"svchost.exe", L"dwm.exe", L"explorer.exe", L"sihost.exe", L"fontdrvhost.exe", L"ctfmon.exe",
    L"runtimebroker.exe", L"searchhost.exe", L"searchapp.exe", L"startmenuexperiencehost.exe",
    L"shellexperiencehost.exe", L"textinputhost.exe", L"taskhostw.exe", L"conhost.exe", L"dllhost.exe",
    L"audiodg.exe", L"spoolsv.exe", L"msmpeng.exe", L"nissrv.exe", L"securityhealthservice.exe",
    L"securityhealthsystray.exe", L"msedgewebview2.exe", L"toolbox.exe", L"widgets.exe", L"lockapp.exe",
    L"applicationframehost.exe", L"systemsettings.exe", L"smartscreen.exe", L"wudfhost.exe",
};

struct Proc {
  DWORD pid;
  std::wstring path;
  std::wstring path_lower;
  std::wstring exe_lower;
};

long long NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
}

ULONGLONG FtToU64(const FILETIME& ft) { return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }

std::wstring WinDirLower() {
  wchar_t buf[MAX_PATH] = {};
  GetWindowsDirectoryW(buf, MAX_PATH);
  return util::ToLower(buf) + L"\\";
}

// Processus de la session de l'utilisateur.
std::vector<Proc> ListProcesses() {
  std::vector<Proc> out;
  DWORD my_session = 0;
  ProcessIdToSessionId(GetCurrentProcessId(), &my_session);
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return out;
  PROCESSENTRY32W pe{sizeof(pe)};
  for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
    DWORD session = 0;
    if (pe.th32ProcessID == 0 || !ProcessIdToSessionId(pe.th32ProcessID, &session) || session != my_session) continue;
    Proc p{pe.th32ProcessID, {}, {}, util::ToLower(pe.szExeFile)};
    if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID)) {
      wchar_t path[MAX_PATH * 2];
      DWORD size = MAX_PATH * 2;
      if (QueryFullProcessImageNameW(h, 0, path, &size)) p.path.assign(path, size);
      CloseHandle(h);
    }
    p.path_lower = util::ToLower(p.path);
    out.push_back(std::move(p));
  }
  CloseHandle(snap);
  return out;
}

// Fenêtres principales visibles, par processus.
std::map<DWORD, std::vector<HWND>> ListWindows() {
  std::map<DWORD, std::vector<HWND>> out;
  EnumWindows(
      [](HWND hwnd, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) || GetWindowTextLengthW(hwnd) == 0) return TRUE;
        BOOL cloaked = FALSE;  // fenêtres d'apps UWP suspendues
        DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        if (cloaked) return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        (*reinterpret_cast<std::map<DWORD, std::vector<HWND>>*>(lp))[pid].push_back(hwnd);
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&out));
  return out;
}

}  // namespace

void ProcessManager::LoadConfig(const json& cfg) {
  useless_.clear();
  for (const auto& u : cfg.value("useless", json::array())) {
    if (u.is_string()) useless_.insert(util::ToUtf8(util::ToLower(util::FromUtf8(u.get<std::string>()))));
  }
  stopped_.clear();
  for (const auto& s : cfg.value("stopped", json::array())) {
    if (s.is_object()) stopped_.push_back({s.value("path", ""), s.value("name", ""), s.value("time", 0LL)});
  }
}

json ProcessManager::SaveConfig() const {
  json stopped = json::array();
  for (const auto& s : stopped_) stopped.push_back({{"path", s.path}, {"name", s.name}, {"time", s.time}});
  return json{{"useless", useless_}, {"stopped", stopped}};
}

json ProcessManager::State() const {
  json s = SaveConfig();
  json names = json::object();
  for (const auto& u : useless_) {
    auto it = names_cache_.find(util::FromUtf8(u));
    names[u] = it != names_cache_.end() ? it->second
                                        : util::ToUtf8(std::filesystem::path(util::FromUtf8(u)).stem().wstring());
  }
  s["uselessNames"] = names;
  s["lastEvent"] = last_event_;
  return s;
}

bool ProcessManager::IsProtected(const std::wstring& path_lower, const std::wstring& exe_lower) const {
  static const std::wstring windir = WinDirLower();
  if (path_lower.empty()) return true;  // inaccessible : processus système ou administrateur
  if (path_lower.rfind(windir, 0) == 0) return true;
  if (kProtected.count(exe_lower)) return true;
  return path_lower == util::ToLower(util::ExePath().wstring());
}

std::string ProcessManager::FriendlyName(const std::wstring& path) {
  const std::wstring key = util::ToLower(path);
  if (auto it = names_cache_.find(key); it != names_cache_.end()) return it->second;

  std::string name;
  DWORD dummy = 0;
  const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &dummy);
  if (size > 0) {
    std::vector<BYTE> data(size);
    if (GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
      struct Lang { WORD lang, codepage; }* langs = nullptr;
      UINT len = 0;
      if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&langs), &len) &&
          len >= sizeof(Lang)) {
        wchar_t sub[64];
        swprintf_s(sub, L"\\StringFileInfo\\%04x%04x\\FileDescription", langs[0].lang, langs[0].codepage);
        wchar_t* desc = nullptr;
        UINT dlen = 0;
        if (VerQueryValueW(data.data(), sub, reinterpret_cast<void**>(&desc), &dlen) && dlen > 1) {
          name = util::ToUtf8(std::wstring(desc, wcsnlen(desc, dlen)));
        }
      }
    }
  }
  while (!name.empty() && name.back() == ' ') name.pop_back();
  if (name.empty()) name = util::ToUtf8(std::filesystem::path(path).stem().wstring());
  names_cache_[key] = name;
  return name;
}

json ProcessManager::Snapshot() {
  const auto procs = ListProcesses();
  const auto windows = ListWindows();

  SYSTEM_INFO si;
  GetNativeSystemInfo(&si);
  FILETIME now_ft;
  GetSystemTimeAsFileTime(&now_ft);
  const ULONGLONG wall = FtToU64(now_ft);

  struct Group {
    std::wstring path;
    int count = 0;
    SIZE_T ram = 0;
    double cpu = 0;
    std::wstring title;
  };
  std::map<std::wstring, Group> groups;
  std::map<DWORD, CpuSample> cpu_now;

  for (const auto& p : procs) {
    if (IsProtected(p.path_lower, p.exe_lower)) continue;
    Group& g = groups[p.path_lower];
    g.path = p.path;
    g.count++;
    if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.pid)) {
      PROCESS_MEMORY_COUNTERS_EX pmc{};
      if (GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        g.ram += pmc.PrivateUsage;
      }
      FILETIME c, e, k, u;
      if (GetProcessTimes(h, &c, &e, &k, &u)) {
        const ULONGLONG t = FtToU64(k) + FtToU64(u);
        cpu_now[p.pid] = {t, wall};
        if (auto it = cpu_prev_.find(p.pid); it != cpu_prev_.end() && wall > it->second.wall) {
          g.cpu += 100.0 * double(t - it->second.cpu_time) / double(wall - it->second.wall) / si.dwNumberOfProcessors;
        }
      }
      CloseHandle(h);
    }
    if (g.title.empty()) {
      if (auto it = windows.find(p.pid); it != windows.end()) {
        wchar_t title[256];
        GetWindowTextW(it->second.front(), title, 256);
        g.title = title;
      }
    }
  }
  cpu_prev_ = std::move(cpu_now);

  json list = json::array();
  for (auto& [key, g] : groups) {
    const std::string key8 = util::ToUtf8(key);
    list.push_back({
        {"key", key8},
        {"path", util::ToUtf8(g.path)},
        {"name", FriendlyName(g.path)},
        {"exe", util::ToUtf8(std::filesystem::path(g.path).filename().wstring())},
        {"count", g.count},
        {"ram", g.ram},
        {"cpu", std::min(g.cpu, 100.0)},
        {"windowed", !g.title.empty()},
        {"title", util::ToUtf8(g.title)},
        {"useless", useless_.count(key8) > 0},
    });
  }
  return json{{"programs", list}};
}

json ProcessManager::Live() {
  const ULONGLONG now = GetTickCount64();
  if (!running_ || now > watch_until_ || now - last_snapshot_ < kSnapshotMs) return nullptr;
  last_snapshot_ = now;
  return Snapshot();
}

void ProcessManager::RememberStopped(const std::wstring& path) {
  const std::string p8 = util::ToUtf8(path);
  std::erase_if(stopped_, [&](const Stopped& s) { return util::ToLower(util::FromUtf8(s.path)) == util::ToLower(path); });
  stopped_.insert(stopped_.begin(), {p8, FriendlyName(path), NowSeconds()});
  if (stopped_.size() > kMaxStopped) stopped_.resize(kMaxStopped);
}

int ProcessManager::StopProgram(const std::wstring& path_lower, bool force) {
  const auto procs = ListProcesses();
  const auto windows = ListWindows();
  std::vector<const Proc*> targets;
  for (const auto& p : procs) {
    if (p.path_lower == path_lower && !IsProtected(p.path_lower, p.exe_lower)) targets.push_back(&p);
  }
  if (targets.empty()) return 0;

  const bool has_windows = std::any_of(targets.begin(), targets.end(), [&](const Proc* p) { return windows.count(p->pid) > 0; });
  if (has_windows && !force) {
    // Fermeture polie : comme cliquer sur ✕ (le programme peut demander d'enregistrer).
    for (const Proc* p : targets) {
      if (auto it = windows.find(p->pid); it != windows.end()) {
        for (HWND w : it->second) PostMessageW(w, WM_CLOSE, 0, 0);
      }
    }
  } else {
    for (const Proc* p : targets) {
      if (HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, p->pid)) {
        TerminateProcess(h, 1);
        CloseHandle(h);
      }
    }
  }
  RememberStopped(targets.front()->path);
  Emit("programStop");
  return static_cast<int>(targets.size());
}

void ProcessManager::HandleAction(const std::string& action, const json& payload) {
  const std::wstring key = util::ToLower(util::FromUtf8(payload.value("key", "")));

  if (action == "watch") {
    const bool first = GetTickCount64() > watch_until_;
    watch_until_ = GetTickCount64() + kWatchMs;
    if (first) last_snapshot_ = 0;  // réponse immédiate à l'ouverture de la page
  } else if (action == "stop" || action == "forceStop") {
    const int n = StopProgram(key, action == "forceStop");
    last_event_ = n ? FriendlyName(util::FromUtf8(payload.value("key", ""))) + " arrêté" : "Programme déjà fermé";
    last_snapshot_ = 0;
  } else if (action == "toggleUseless") {
    const std::string k8 = util::ToUtf8(key);
    if (!useless_.erase(k8)) useless_.insert(k8);
    last_snapshot_ = 0;  // la liste est renvoyée tout de suite (l'étoile s'affiche sans attendre)
  } else if (action == "stopUseless") {
    int programs = 0;
    for (const auto& u : useless_) programs += StopProgram(util::FromUtf8(u), payload.value("force", false)) > 0;
    last_event_ = programs ? "Mode léger : " + std::to_string(programs) + " programme(s) arrêté(s)"
                           : "Aucun programme inutile ouvert";
    last_snapshot_ = 0;
  } else if (action == "relaunch" || action == "relaunchAll") {
    int ok = 0;
    for (auto it = stopped_.begin(); it != stopped_.end();) {
      if (action == "relaunch" && util::ToLower(util::FromUtf8(it->path)) != key) {
        ++it;
        continue;
      }
      const std::wstring path = util::FromUtf8(it->path);
      const std::wstring dir = std::filesystem::path(path).parent_path().wstring();
      if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL)) > 32) {
        ++ok;
        Emit("programRelaunch");
        it = stopped_.erase(it);
      } else {
        ++it;
      }
    }
    last_event_ = ok ? std::to_string(ok) + " programme(s) relancé(s)" : "Impossible de relancer";
    last_snapshot_ = 0;
  } else if (action == "forget") {
    std::erase_if(stopped_, [&](const Stopped& s) { return util::ToLower(util::FromUtf8(s.path)) == key; });
  }
}
