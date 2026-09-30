#include "modules/app_launcher.h"

#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "util.h"

using nlohmann::json;

void AppLauncher::LoadConfig(const json& cfg) {
  groups_.clear();
  for (const auto& g : cfg.value("groups", json::array())) {
    if (!g.is_object()) continue;
    Group group{g.value("id", ""), g.value("name", ""), g.value("emoji", ""), {}};
    for (const auto& i : g.value("items", json::array())) {
      if (i.is_string()) group.items.push_back(i.get<std::string>());
    }
    groups_.push_back(std::move(group));
  }
}

json AppLauncher::SaveConfig() const {
  json groups = json::array();
  for (const auto& g : groups_) {
    groups.push_back({{"id", g.id}, {"name", g.name}, {"emoji", g.emoji}, {"items", g.items}});
  }
  return json{{"groups", groups}};
}

json AppLauncher::State() const {
  json s = SaveConfig();
  s["lastEvent"] = last_event_;
  return s;
}

bool AppLauncher::Open(const std::string& target) {
  const std::wstring w = util::FromUtf8(target);
  const bool is_url = w.rfind(L"https://", 0) == 0 || w.rfind(L"http://", 0) == 0;
  std::wstring dir;
  if (!is_url) dir = std::filesystem::path(w).parent_path().wstring();
  const auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, dir.empty() ? nullptr : dir.c_str(), SW_SHOWNORMAL));
  return rc > 32;
}

void AppLauncher::HandleAction(const std::string& action, const json& payload) {
  if (action == "saveGroup") {
    const json g = payload.value("group", json::object());
    std::string id = g.value("id", "");
    auto it = std::find_if(groups_.begin(), groups_.end(), [&](const Group& x) { return x.id == id; });
    Group& group = (it != groups_.end()) ? *it : groups_.emplace_back();
    if (group.id.empty()) {
      group.id = "g" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    group.name = g.value("name", "");
    group.emoji = g.value("emoji", "");
    group.items.clear();
    for (const auto& i : g.value("items", json::array())) {
      if (i.is_string() && !i.get<std::string>().empty()) group.items.push_back(i.get<std::string>());
    }
  } else if (action == "deleteGroup") {
    const std::string id = payload.value("id", "");
    std::erase_if(groups_, [&](const Group& x) { return x.id == id; });
  } else if (action == "launchGroup") {
    const std::string id = payload.value("id", "");
    for (const auto& g : groups_) {
      if (g.id != id) continue;
      int ok = 0;
      for (const auto& item : g.items) ok += Open(item) ? 1 : 0;
      Emit("appGroup");
      last_event_ = g.name + " : " + std::to_string(ok) + "/" + std::to_string(g.items.size()) + " ouvert(s)";
    }
  } else if (action == "launchItem") {
    Open(payload.value("target", ""));
  }
}
