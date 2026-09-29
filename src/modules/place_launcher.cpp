#include "modules/place_launcher.h"

#include <shellapi.h>
#include <wlanapi.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "util.h"

using nlohmann::json;

namespace {

constexpr long long kGpsEverySec = 60;       // fréquence des demandes de position
constexpr long long kGpsMaxAgeSec = 10 * 60; // au-delà, la position est trop vieille

long long NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string NewId() {
  static unsigned counter = 0;
  return "r" + std::to_string(NowSeconds()) + "_" + std::to_string(++counter);
}

}  // namespace

PlaceLauncher::PlaceLauncher(std::function<void()> on_change)
    : on_change_(std::move(on_change)), geo_(std::make_unique<GeoLocator>(on_change_)) {}

PlaceLauncher::~PlaceLauncher() { Stop(); }

void PlaceLauncher::Start() {
  if (running_) return;
  DWORD negotiated = 0;
  wifi_error_.clear();
  if (WlanOpenHandle(2, nullptr, &negotiated, &wlan_) != ERROR_SUCCESS) {
    wlan_ = nullptr;
    wifi_error_ = "noWifi";
  }
  running_ = true;
  connected_.clear();
  for (auto& r : rules_) r.known = r.inside = r.gps_inside = false;
  last_geo_request_ = 0;
  Tick();
}

void PlaceLauncher::Stop() {
  if (wlan_) {
    WlanCloseHandle(wlan_, nullptr);
    wlan_ = nullptr;
  }
  running_ = false;
  connected_.clear();
}

void PlaceLauncher::LoadConfig(const json& cfg) {
  rules_.clear();
  if (auto it = cfg.find("rules"); it != cfg.end() && it->is_array()) {
    for (const auto& r : *it) {
      if (!r.is_object()) continue;
      Rule rule;
      rule.id = r.value("id", NewId());
      rule.place = r.value("place", "");
      rule.ssid = r.value("ssid", "");
      rule.use_gps = r.value("useGps", false);
      rule.lat = r.value("lat", 0.0);
      rule.lon = r.value("lon", 0.0);
      rule.radius_m = std::clamp(r.value("radius", 150), 30, 5000);
      rule.path = r.value("path", "");
      rule.enabled = r.value("enabled", true);
      rule.last_run = r.value("lastRun", 0LL);
      rules_.push_back(std::move(rule));
    }
  }
}

json PlaceLauncher::SaveConfig() const {
  json rules = json::array();
  for (const auto& r : rules_) {
    rules.push_back({{"id", r.id}, {"place", r.place}, {"ssid", r.ssid}, {"useGps", r.use_gps},
                     {"lat", r.lat}, {"lon", r.lon}, {"radius", r.radius_m}, {"path", r.path},
                     {"enabled", r.enabled}, {"lastRun", r.last_run}});
  }
  return json{{"rules", rules}};
}

json PlaceLauncher::State() const {
  json s = SaveConfig();
  for (size_t i = 0; i < rules_.size(); ++i) s["rules"][i]["inside"] = rules_[i].inside;
  s["connected"] = json(std::vector<std::string>(connected_.begin(), connected_.end()));
  s["wifiError"] = wifi_error_;
  s["lastEvent"] = last_event_;

  const auto fix = geo_->Last();
  s["position"] = fix.valid ? json{{"lat", fix.lat}, {"lon", fix.lon}, {"accuracy", fix.accuracy_m}, {"time", fix.time}}
                            : json(nullptr);
  s["gpsError"] = fix.error;
  return s;
}

void PlaceLauncher::HandleAction(const std::string& action, const json& payload) {
  if (action == "saveRule") {
    const json r = payload.value("rule", json::object());
    const std::string id = r.value("id", "");
    auto it = std::find_if(rules_.begin(), rules_.end(), [&](const Rule& x) { return x.id == id; });
    const bool is_new = (it == rules_.end());
    Rule& rule = is_new ? rules_.emplace_back() : *it;
    if (rule.id.empty()) rule.id = NewId();
    rule.place = r.value("place", "");
    rule.ssid = r.value("ssid", "");
    rule.use_gps = r.value("useGps", false);
    rule.lat = r.value("lat", 0.0);
    rule.lon = r.value("lon", 0.0);
    rule.radius_m = std::clamp(r.value("radius", 150), 30, 5000);
    rule.path = r.value("path", "");
    rule.enabled = r.value("enabled", true);
    // Si on est déjà sur place, pas de lancement immédiat : seulement à la prochaine arrivée.
    // On suppose donc « déjà sur place » ; le prochain Tick corrige sans lancer.
    rule.known = true;
    rule.inside = true;
    if (is_new) rule.gps_inside = false;
    Tick();
  } else if (action == "deleteRule") {
    const std::string id = payload.value("id", "");
    std::erase_if(rules_, [&](const Rule& x) { return x.id == id; });
  } else if (action == "testRule") {
    const std::string id = payload.value("id", "");
    for (auto& r : rules_) {
      if (r.id == id) Launch(r);
    }
  } else if (action == "locate") {
    geo_->RequestAsync();
    last_geo_request_ = NowSeconds();
  } else if (action == "refresh") {
    Tick();
  }
}

bool PlaceLauncher::NeedsGps() const {
  return std::any_of(rules_.begin(), rules_.end(), [](const Rule& r) { return r.enabled && r.use_gps; });
}

bool PlaceLauncher::QueryConnectedSsids(std::set<std::string>& out) {
  if (!wlan_) return false;
  PWLAN_INTERFACE_INFO_LIST list = nullptr;
  if (WlanEnumInterfaces(wlan_, nullptr, &list) != ERROR_SUCCESS) return false;

  bool denied = false;
  for (DWORD i = 0; i < list->dwNumberOfItems; ++i) {
    const auto& intf = list->InterfaceInfo[i];
    if (intf.isState != wlan_interface_state_connected) continue;
    DWORD size = 0;
    PWLAN_CONNECTION_ATTRIBUTES attr = nullptr;
    const DWORD rc = WlanQueryInterface(wlan_, &intf.InterfaceGuid, wlan_intf_opcode_current_connection, nullptr,
                                        &size, reinterpret_cast<PVOID*>(&attr), nullptr);
    if (rc == ERROR_SUCCESS && attr) {
      const auto& ssid = attr->wlanAssociationAttributes.dot11Ssid;
      if (ssid.uSSIDLength > 0) {
        out.insert(std::string(reinterpret_cast<const char*>(ssid.ucSSID), ssid.uSSIDLength));
      }
      WlanFreeMemory(attr);
    } else if (rc == ERROR_ACCESS_DENIED) {
      denied = true;
    }
  }
  WlanFreeMemory(list);

  // Depuis Windows 11 24H2, lire le nom du Wi-Fi demande l'accès à la position.
  wifi_error_ = denied ? "locationDenied" : "";
  return !denied;
}

void PlaceLauncher::Tick() {
  if (!running_) return;
  const long long now_s = NowSeconds();

  if (NeedsGps() && now_s - last_geo_request_ >= kGpsEverySec) {
    last_geo_request_ = now_s;
    geo_->RequestAsync();
  }

  std::set<std::string> ssids;
  QueryConnectedSsids(ssids);
  bool changed = (ssids != connected_);
  connected_ = std::move(ssids);

  const auto fix = geo_->Last();
  const bool fix_fresh = fix.valid && now_s - fix.time <= kGpsMaxAgeSec;

  for (auto& rule : rules_) {
    if (!rule.enabled || rule.path.empty()) continue;

    const bool wifi_here = !rule.ssid.empty() && connected_.count(rule.ssid) > 0;
    if (rule.use_gps && fix_fresh) {
      const double d = GeoLocator::DistanceMeters(fix.lat, fix.lon, rule.lat, rule.lon);
      // Hystérésis : on entre à `radius`, on ne sort qu'à 1,5 × radius (évite les allers-retours).
      if (!rule.gps_inside && d <= rule.radius_m) rule.gps_inside = true;
      else if (rule.gps_inside && d > rule.radius_m * 1.5) rule.gps_inside = false;
    }
    const bool gps_known = !rule.use_gps || fix_fresh;
    const bool here = wifi_here || (rule.use_gps && rule.gps_inside);

    // Au démarrage, on attend une position GPS avant de décider (sauf si le Wi-Fi suffit).
    if (!rule.known && !here && !gps_known) continue;

    if (here && (!rule.known || !rule.inside)) Launch(rule);
    if (here != rule.inside || !rule.known) changed = true;
    rule.inside = here;
    rule.known = true;
  }
  if (changed && on_change_) on_change_();
}

void PlaceLauncher::Launch(Rule& rule) {
  const std::wstring path = util::FromUtf8(rule.path);
  const std::wstring dir = std::filesystem::path(path).parent_path().wstring();
  const auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, dir.empty() ? nullptr : dir.c_str(), SW_SHOWNORMAL));
  const std::string label = rule.place.empty() ? rule.ssid : rule.place;
  const std::string file = util::ToUtf8(std::filesystem::path(path).filename().wstring());
  if (rc > 32) {
    rule.last_run = NowSeconds();
    last_event_ = file + " ouvert (" + label + ")";
  } else {
    last_event_ = "Impossible d'ouvrir " + file;
  }
  if (on_change_) on_change_();
}
