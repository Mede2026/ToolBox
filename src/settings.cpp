#include "settings.h"

#include <fstream>

#include "util.h"

using nlohmann::json;

Settings::Settings() : path_(util::DataDir() / L"settings.json"), data_(Defaults()) {}

json Settings::Defaults() {
  return json{
      {"enabled", true},
      {"startWithWindows", false},
      {"minimizeToTray", true},
      {"autoUpdate", true},
      {"modules", json::object()},
  };
}

void Settings::Load() {
  std::ifstream in(path_);
  if (!in) return;
  json loaded = json::parse(in, nullptr, /*allow_exceptions=*/false);
  if (loaded.is_discarded() || !loaded.is_object()) return;
  // Fusion : les nouvelles clés par défaut restent présentes.
  data_.merge_patch(loaded);
}

void Settings::Save() const {
  auto tmp = path_;
  tmp += L".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) return;
    out << data_.dump(2);
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path_, ec);
}

json& Settings::Module(const std::string& id) {
  auto& modules = data_["modules"];
  if (!modules.is_object()) modules = json::object();
  auto& m = modules[id];
  if (!m.is_object()) m = json::object();
  return m;
}
