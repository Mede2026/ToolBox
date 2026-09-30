#include "hotkey.h"

using nlohmann::json;

GlobalHotkey::GlobalHotkey(HWND hwnd, int id, UINT mods, UINT vk, std::string label, std::string prefix)
    : hwnd_(hwnd), id_(id), mods_(mods), vk_(vk), label_(std::move(label)), prefix_(std::move(prefix)) {}

void GlobalHotkey::Register() {
  Unregister();
  error_.clear();
  if (!enabled_ || !vk_) return;
  registered_ = RegisterHotKey(hwnd_, id_, mods_ | MOD_NOREPEAT, vk_) != FALSE;
  if (!registered_) error_ = "Le raccourci " + label_ + " est déjà utilisé par une autre app. Choisis-en un autre.";
}

void GlobalHotkey::Unregister() {
  if (registered_) UnregisterHotKey(hwnd_, id_);
  registered_ = false;
}

void GlobalHotkey::Load(const json& cfg) {
  enabled_ = cfg.value(Key("hotkeyEnabled"), enabled_);
  mods_ = cfg.value(Key("hotkeyMods"), mods_);
  vk_ = cfg.value(Key("hotkeyVk"), vk_);
  label_ = cfg.value(Key("hotkeyLabel"), label_);
}

void GlobalHotkey::Save(json& cfg) const {
  cfg[Key("hotkeyEnabled")] = enabled_;
  cfg[Key("hotkeyMods")] = mods_;
  cfg[Key("hotkeyVk")] = vk_;
  cfg[Key("hotkeyLabel")] = label_;
}

void GlobalHotkey::AddState(json& state) const {
  state[Key("hotkeyEnabled")] = enabled_;
  state[Key("hotkeyLabel")] = label_;
  state[Key("hotkeyError")] = error_;
}

bool GlobalHotkey::HandleAction(const std::string& action, const json& payload, bool active) {
  if (action.rfind("hotkeyCapture", 0) != 0 && action != "setHotkey" && action != "setHotkeyEnabled") return false;
  if (payload.value("hotkey", std::string()) != prefix_) return false;  // l'autre raccourci du module
  if (action == "hotkeyCaptureStart") {
    Unregister();  // sinon la combinaison n'arrive pas jusqu'à l'interface
  } else if (action == "hotkeyCaptureCancel") {
    if (active) Register();
  } else if (action == "setHotkey") {
    mods_ = payload.value("mods", 0u) & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
    vk_ = payload.value("vk", 0u) & 0xFF;
    label_ = payload.value("label", std::string());
    enabled_ = true;
    if (active) Register();
  } else if (action == "setHotkeyEnabled") {
    enabled_ = payload.value("value", true);
    if (active) Register();
  } else {
    return false;
  }
  return true;
}
