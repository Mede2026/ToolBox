#include "modules/stats.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "util.h"

using nlohmann::json;

namespace {

constexpr size_t kMaxDays = 30;
constexpr size_t kMaxHours = 7 * 24;
constexpr ULONGLONG kIdleMs = 2 * 60 * 1000;  // plus de 2 min sans clavier/souris = PC pas utilisé

long long NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string LocalDate() {
  SYSTEMTIME t;
  GetLocalTime(&t);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.wYear, t.wMonth, t.wDay);
  return buf;
}

ULONGLONG FtToU64(const FILETIME& ft) { return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }

// Les N apps les plus utilisées, triées.
json TopApps(const std::map<std::string, long long>& apps, size_t n) {
  std::vector<std::pair<std::string, long long>> v(apps.begin(), apps.end());
  std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  if (v.size() > n) v.resize(n);
  json out = json::array();
  for (const auto& [name, sec] : v) out.push_back({{"name", name}, {"sec", sec}});
  return out;
}

}  // namespace

Stats::Stats(std::function<void()> on_change) : on_change_(std::move(on_change)) {}

void Stats::Start() {
  running_ = true;
  last_tick_ = GetTickCount64();
  if (!since_) since_ = NowSeconds();
}

Stats::Day& Stats::Today() {
  const std::string today = LocalDate();
  if (days_.empty() || days_.front().date != today) {
    days_.push_front(Day{today});
    while (days_.size() > kMaxDays) days_.pop_back();
  }
  return days_.front();
}

std::string Stats::AppName(DWORD pid) {
  const std::wstring path = util::ProcessPath(pid);
  if (path.empty()) return {};
  const std::wstring key = util::ToLower(path);
  if (auto it = names_.find(key); it != names_.end()) return it->second;
  // Écran de verrouillage et bureau : ce n'est pas une app qu'on utilise.
  const std::wstring exe = util::ToLower(std::filesystem::path(path).filename().wstring());
  std::string name = (exe == L"lockapp.exe" || exe == L"searchhost.exe") ? std::string() : util::FileDescription(path);
  if (exe == L"explorer.exe") name = "Explorateur de fichiers";
  names_[key] = name;
  return name;
}

void Stats::Tick() {
  if (!running_) return;
  const ULONGLONG now = GetTickCount64();
  const long long dt = std::clamp<long long>(static_cast<long long>((now - last_tick_) / 1000), 0, 30);
  last_tick_ = now;

  // Processeur et mémoire, regroupés par heure.
  FILETIME idle, kernel, user;
  double cpu = -1;
  if (GetSystemTimes(&idle, &kernel, &user)) {
    const ULONGLONG i = FtToU64(idle), total = FtToU64(kernel) + FtToU64(user);
    if (prev_total_ && total > prev_total_) cpu = 100.0 * (1.0 - double(i - prev_idle_) / double(total - prev_total_));
    prev_idle_ = i;
    prev_total_ = total;
  }
  MEMORYSTATUSEX mem{sizeof(mem)};
  GlobalMemoryStatusEx(&mem);
  if (cpu >= 0) {
    const long long hour = NowSeconds() / 3600;
    if (hours_.empty() || hours_.back().hour != hour) {
      hours_.push_back(Hour{hour});
      while (hours_.size() > kMaxHours) hours_.pop_front();
    }
    Hour& h = hours_.back();
    h.cpu += std::clamp(cpu, 0.0, 100.0);
    h.ram += mem.dwMemoryLoad;
    h.n++;
  }

  // Temps d'écran : seulement si on a touché au clavier ou à la souris récemment.
  LASTINPUTINFO lii{sizeof(lii)};
  const bool active = GetLastInputInfo(&lii) && GetTickCount() - lii.dwTime < kIdleMs;
  if (active && dt > 0) {
    DWORD pid = 0;
    if (HWND fg = GetForegroundWindow()) GetWindowThreadProcessId(fg, &pid);
    const std::string app = pid ? AppName(pid) : std::string();
    if (!app.empty()) {
      Day& day = Today();
      day.active += dt;
      day.apps[app] += dt;
    }
  }

  if (++ticks_ % 6 == 0 && on_change_) on_change_();  // enregistre et met l'écran à jour chaque minute
}

void Stats::Count(const std::string& event) {
  if (!running_) return;
  Today().events[event]++;
  totals_[event]++;
  if (on_change_) on_change_();
}

void Stats::LoadConfig(const json& cfg) {
  days_.clear();
  for (const auto& d : cfg.value("days", json::array())) {
    if (!d.is_object()) continue;
    Day day{d.value("date", ""), d.value("active", 0LL)};
    // Copies locales : itérer sur le résultat temporaire de value() lirait de la mémoire déjà libérée.
    const json apps = d.value("apps", json::object());
    const json events = d.value("events", json::object());
    for (const auto& [k, v] : apps.items()) {
      if (v.is_number()) day.apps[k] = v.get<long long>();
    }
    for (const auto& [k, v] : events.items()) {
      if (v.is_number()) day.events[k] = v.get<long long>();
    }
    days_.push_back(std::move(day));
  }
  hours_.clear();
  for (const auto& h : cfg.value("hours", json::array())) {
    if (h.is_array() && h.size() == 4 && h[0].is_number() && h[1].is_number() && h[2].is_number() && h[3].is_number()) {
      hours_.push_back(Hour{h[0].get<long long>(), h[1].get<double>(), h[2].get<double>(), h[3].get<int>()});
    }
  }
  totals_.clear();
  const json totals = cfg.value("totals", json::object());
  for (const auto& [k, v] : totals.items()) {
    if (v.is_number()) totals_[k] = v.get<long long>();
  }
  since_ = cfg.value("since", 0LL);
}

json Stats::SaveConfig() const {
  json days = json::array();
  for (const auto& d : days_) {
    json apps = json::object();
    for (const auto& a : TopApps(d.apps, 40)) apps[a["name"].get<std::string>()] = a["sec"];
    days.push_back({{"date", d.date}, {"active", d.active}, {"apps", apps}, {"events", d.events}});
  }
  json hours = json::array();
  for (const auto& h : hours_) hours.push_back({h.hour, h.cpu, h.ram, h.n});
  return json{{"days", days}, {"hours", hours}, {"totals", totals_}, {"since", since_}};
}

json Stats::State() const {
  // 7 derniers jours, du plus ancien au plus récent.
  json days = json::array();
  std::map<std::string, long long> week;
  for (size_t i = std::min<size_t>(7, days_.size()); i-- > 0;) {
    const Day& d = days_[i];
    for (const auto& [k, v] : d.apps) week[k] += v;
    days.push_back({{"date", d.date}, {"active", d.active}, {"apps", TopApps(d.apps, 12)}, {"events", d.events}});
  }
  json hours = json::array();
  for (size_t i = hours_.size() > 48 ? hours_.size() - 48 : 0; i < hours_.size(); ++i) {
    const Hour& h = hours_[i];
    if (h.n) hours.push_back({{"hour", h.hour}, {"cpu", h.cpu / h.n}, {"ram", h.ram / h.n}});
  }
  return json{{"days", days}, {"week", TopApps(week, 12)}, {"hours", hours}, {"totals", totals_}, {"since", since_}};
}

void Stats::HandleAction(const std::string& action, const json&) {
  if (action == "reset") {
    days_.clear();
    hours_.clear();
    totals_.clear();
    since_ = NowSeconds();
  }
}
