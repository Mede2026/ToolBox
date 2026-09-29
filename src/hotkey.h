#pragma once

#include <windows.h>

#include <nlohmann/json.hpp>
#include <string>

// Raccourci clavier global (RegisterHotKey) configurable depuis l'interface.
// Réglages : hotkeyEnabled, hotkeyMods, hotkeyVk, hotkeyLabel.
// Actions : hotkeyCaptureStart, hotkeyCaptureCancel, setHotkey, setHotkeyEnabled.
class GlobalHotkey {
 public:
  GlobalHotkey(HWND hwnd, int id, UINT mods, UINT vk, std::string label);
  ~GlobalHotkey() { Unregister(); }

  void Register();
  void Unregister();

  void Load(const nlohmann::json& cfg);
  void Save(nlohmann::json& cfg) const;
  void AddState(nlohmann::json& state) const;

  // Retourne true si l'action concernait le raccourci. `active` : le module tourne.
  bool HandleAction(const std::string& action, const nlohmann::json& payload, bool active);

  bool Matches(UINT msg, WPARAM wparam) const { return msg == WM_HOTKEY && static_cast<int>(wparam) == id_; }

 private:
  HWND hwnd_;
  int id_;
  bool enabled_ = true;
  UINT mods_;
  UINT vk_;
  std::string label_;
  bool registered_ = false;
  std::string error_;
};
