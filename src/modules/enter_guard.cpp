#include "modules/enter_guard.h"

#include <algorithm>

#include "util.h"

using nlohmann::json;

EnterGuard* EnterGuard::instance_ = nullptr;

EnterGuard::EnterGuard(std::function<void()> on_change) : on_change_(std::move(on_change)) {}

EnterGuard::~EnterGuard() { Stop(); }

void EnterGuard::Start() {
  if (hook_) return;
  instance_ = this;
  hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, HookProc, GetModuleHandleW(nullptr), 0);
  enter_down_ = guard_down_ = last_key_was_guard_ = false;
  suppress_guard_repeats_ = block_guard_up_ = false;
}

void EnterGuard::Stop() {
  if (hook_) {
    UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
  }
  capturing_ = false;
  if (instance_ == this) instance_ = nullptr;
}

void EnterGuard::LoadConfig(const json& cfg) {
  scan_ = cfg.value("scanCode", kDefaultScan);
  extended_ = cfg.value("extended", false);
  threshold_ms_ = std::clamp<DWORD>(cfg.value("thresholdMs", kDefaultThresholdMs), 10, 500);
  corrections_ = cfg.value("corrections", 0ULL);
  exclusions_.clear();
  if (auto it = cfg.find("exclusions"); it != cfg.end() && it->is_array()) {
    for (const auto& e : *it) {
      if (e.is_string()) exclusions_.push_back(util::ToLower(util::FromUtf8(e.get<std::string>())));
    }
  }
}

json EnterGuard::SaveConfig() const {
  json ex = json::array();
  for (const auto& e : exclusions_) ex.push_back(util::ToUtf8(e));
  return json{
      {"scanCode", scan_},
      {"extended", extended_},
      {"thresholdMs", threshold_ms_},
      {"corrections", corrections_},
      {"exclusions", ex},
  };
}

json EnterGuard::State() const {
  json s = SaveConfig();
  s["keyName"] = util::ToUtf8(KeyName());
  s["isDefaultKey"] = (scan_ == kDefaultScan && !extended_);
  s["capturing"] = capturing_;
  return s;
}

void EnterGuard::HandleAction(const std::string& action, const json& payload) {
  if (action == "setThreshold") {
    threshold_ms_ = std::clamp<DWORD>(payload.value("value", kDefaultThresholdMs), 10, 500);
  } else if (action == "captureKey") {
    capturing_ = true;
  } else if (action == "cancelCapture") {
    capturing_ = false;
  } else if (action == "resetKey") {
    scan_ = kDefaultScan;
    extended_ = false;
  } else if (action == "resetStats") {
    corrections_ = 0;
  } else if (action == "setExclusions") {
    json cfg = SaveConfig();
    cfg["exclusions"] = payload.value("list", json::array());
    LoadConfig(cfg);
  }
}

std::wstring EnterGuard::KeyName() const {
  wchar_t name[64] = {};
  LONG lparam = static_cast<LONG>((scan_ & 0xFF) << 16) | (extended_ ? (1 << 24) : 0);
  if (GetKeyNameTextW(lparam, name, 64) > 0) return name;
  wchar_t fallback[16];
  swprintf_s(fallback, L"0x%02X", scan_);
  return fallback;
}

bool EnterGuard::Excluded() const {
  if (exclusions_.empty()) return false;
  const std::wstring fg = util::ForegroundProcessName();
  return std::find(exclusions_.begin(), exclusions_.end(), fg) != exclusions_.end();
}

void EnterGuard::SendBackspaceThenEnter(const KBDLLHOOKSTRUCT& enter) {
  INPUT in[3] = {};
  in[0].type = INPUT_KEYBOARD;
  in[0].ki.wVk = VK_BACK;
  in[0].ki.wScan = static_cast<WORD>(MapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC));
  in[1] = in[0];
  in[1].ki.dwFlags = KEYEVENTF_KEYUP;
  in[2].type = INPUT_KEYBOARD;
  in[2].ki.wVk = VK_RETURN;
  in[2].ki.wScan = static_cast<WORD>(enter.scanCode);
  in[2].ki.dwFlags = (enter.flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0;
  // Le relâchement physique d'Enter passera normalement.
  SendInput(3, in, sizeof(INPUT));
}

LRESULT CALLBACK EnterGuard::HookProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == HC_ACTION && instance_) {
    if (instance_->OnKey(wparam, *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam))) return 1;
  }
  return CallNextHookEx(nullptr, code, wparam, lparam);
}

bool EnterGuard::OnKey(WPARAM wparam, const KBDLLHOOKSTRUCT& k) {
  if (k.flags & LLKHF_INJECTED) return false;  // nos propres touches (et celles des autres logiciels)

  const bool down = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN);
  const DWORD t = k.time;
  const bool is_enter = (k.vkCode == VK_RETURN);
  const bool ext = (k.flags & LLKHF_EXTENDED) != 0;

  // Mode « choisir une touche » : la prochaine touche devient la touche surveillée.
  if (swallow_up_scan_ && !down && k.scanCode == swallow_up_scan_) {
    swallow_up_scan_ = 0;
    return true;
  }
  if (capturing_ && down) {
    capturing_ = false;
    if (k.vkCode != VK_ESCAPE && !is_enter) {
      scan_ = k.scanCode;
      extended_ = ext;
    }
    swallow_up_scan_ = k.scanCode;
    if (on_change_) on_change_();
    return true;
  }

  const bool is_guard = !is_enter && k.scanCode == scan_ && ext == extended_;

  if (is_guard) {
    if (!down) {
      guard_down_ = false;
      suppress_guard_repeats_ = false;
      if (block_guard_up_) {
        block_guard_up_ = false;
        return true;
      }
      return false;
    }
    if (suppress_guard_repeats_) return true;  // touche maintenue après une correction

    // Cas 2 : Enter vient d'être pressé (ou l'est encore) -> on bloque la touche.
    if ((enter_down_ || t - last_enter_down_ < threshold_ms_) && !Excluded()) {
      suppress_guard_repeats_ = true;
      block_guard_up_ = true;
      guard_down_ = true;
      ++corrections_;
      Emit("enterFix");
      if (on_change_) on_change_();
      return true;
    }
    guard_down_ = true;
    last_guard_down_ = t;
    last_key_was_guard_ = true;
    return false;
  }

  if (is_enter) {
    if (!down) {
      enter_down_ = false;
      return false;
    }
    const bool repeat = enter_down_;
    enter_down_ = true;
    if (repeat) return false;
    last_enter_down_ = t;

    // Cas 1 : la touche vient d'être tapée (ou est encore enfoncée) -> on l'efface avant Enter.
    const bool accidental = last_key_was_guard_ && (guard_down_ || t - last_guard_down_ < threshold_ms_);
    last_key_was_guard_ = false;
    if (accidental && !Excluded()) {
      if (guard_down_) suppress_guard_repeats_ = true;
      SendBackspaceThenEnter(k);
      ++corrections_;
      Emit("enterFix");
      if (on_change_) on_change_();
      return true;
    }
    return false;
  }

  if (down) last_key_was_guard_ = false;
  return false;
}
