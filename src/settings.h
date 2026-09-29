#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>

// Paramètres persistants, stockés dans %APPDATA%\ToolBox\settings.json.
class Settings {
 public:
  Settings();

  void Load();
  void Save() const;

  nlohmann::json& Root() { return data_; }
  const nlohmann::json& Root() const { return data_; }

  // Section d'un module (créée au besoin).
  nlohmann::json& Module(const std::string& id);

  template <typename T>
  T Get(const nlohmann::json& obj, const char* key, T fallback) const {
    auto it = obj.find(key);
    if (it == obj.end()) return fallback;
    try {
      return it->get<T>();
    } catch (...) {
      return fallback;
    }
  }

 private:
  static nlohmann::json Defaults();

  std::filesystem::path path_;
  nlohmann::json data_;
};
