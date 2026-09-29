#include "modules/clipboard_history.h"

#include <algorithm>
#include <cstring>
#include <chrono>

#include "util.h"

using nlohmann::json;

namespace {

constexpr size_t kMaxTextChars = 100000;  // on ignore les énormes copies

long long NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
}

bool OpenClipboardRetry(HWND hwnd) {
  for (int i = 0; i < 10; ++i) {
    if (OpenClipboard(hwnd)) return true;
    Sleep(20);  // une autre app a peut-être le presse-papiers ouvert
  }
  return false;
}

}  // namespace

ClipboardHistory::ClipboardHistory(HWND hwnd, std::function<void()> on_change, std::function<void()> on_hotkey)
    : hwnd_(hwnd), on_change_(std::move(on_change)), on_hotkey_(std::move(on_hotkey)) {}

ClipboardHistory::~ClipboardHistory() { Stop(); }

void ClipboardHistory::Start() {
  if (running_) return;
  running_ = AddClipboardFormatListener(hwnd_) != FALSE;
  RegisterShortcut();
}

void ClipboardHistory::Stop() {
  UnregisterShortcut();
  if (!running_) return;
  RemoveClipboardFormatListener(hwnd_);
  running_ = false;
}

void ClipboardHistory::RegisterShortcut() {
  UnregisterShortcut();
  hotkey_error_.clear();
  if (!hotkey_enabled_ || !hotkey_vk_) return;
  hotkey_registered_ = RegisterHotKey(hwnd_, kHotkeyId, hotkey_mods_ | MOD_NOREPEAT, hotkey_vk_) != FALSE;
  if (!hotkey_registered_) hotkey_error_ = "Ce raccourci est déjà utilisé par une autre app. Choisis-en un autre.";
}

void ClipboardHistory::UnregisterShortcut() {
  if (hotkey_registered_) UnregisterHotKey(hwnd_, kHotkeyId);
  hotkey_registered_ = false;
}

void ClipboardHistory::LoadConfig(const json& cfg) {
  max_items_ = std::clamp(cfg.value("maxItems", 50), 10, 500);
  keep_after_restart_ = cfg.value("keepAfterRestart", false);
  hotkey_enabled_ = cfg.value("hotkeyEnabled", true);
  hotkey_mods_ = cfg.value("hotkeyMods", static_cast<UINT>(MOD_CONTROL | MOD_ALT));
  hotkey_vk_ = cfg.value("hotkeyVk", static_cast<UINT>('V'));
  hotkey_label_ = cfg.value("hotkeyLabel", std::string("Ctrl + Alt + V"));
  auto_paste_ = cfg.value("autoPaste", true);
  items_.clear();
  if (keep_after_restart_) {
    for (const auto& e : cfg.value("items", json::array())) {
      if (!e.is_object()) continue;
      items_.push_back({next_id_++, util::FromUtf8(e.value("text", "")), e.value("time", 0LL), e.value("pinned", false)});
    }
  } else {
    // Même sans sauvegarde complète, les éléments épinglés sont conservés.
    for (const auto& e : cfg.value("items", json::array())) {
      if (e.is_object() && e.value("pinned", false)) {
        items_.push_back({next_id_++, util::FromUtf8(e.value("text", "")), e.value("time", 0LL), true});
      }
    }
  }
}

json ClipboardHistory::SaveConfig() const {
  json items = json::array();
  for (const auto& e : items_) {
    if (keep_after_restart_ || e.pinned) {
      items.push_back({{"text", util::ToUtf8(e.text)}, {"time", e.time}, {"pinned", e.pinned}});
    }
  }
  return json{{"maxItems", max_items_}, {"keepAfterRestart", keep_after_restart_}, {"items", items},
              {"hotkeyEnabled", hotkey_enabled_}, {"hotkeyMods", hotkey_mods_}, {"hotkeyVk", hotkey_vk_},
              {"hotkeyLabel", hotkey_label_}, {"autoPaste", auto_paste_}};
}

json ClipboardHistory::State() const {
  json items = json::array();
  for (const auto& e : items_) {
    items.push_back({{"id", e.id}, {"text", util::ToUtf8(e.text)}, {"time", e.time}, {"pinned", e.pinned}});
  }
  return json{{"maxItems", max_items_}, {"keepAfterRestart", keep_after_restart_}, {"items", items},
              {"hotkeyEnabled", hotkey_enabled_}, {"hotkeyLabel", hotkey_label_}, {"hotkeyError", hotkey_error_},
              {"autoPaste", auto_paste_}};
}

bool ClipboardHistory::CopyItem(unsigned long long id) {
  auto it = std::find_if(items_.begin(), items_.end(), [&](const Entry& e) { return e.id == id; });
  return it != items_.end() && CopyToClipboard(it->text);
}

void ClipboardHistory::HandleAction(const std::string& action, const json& payload) {
  const auto id = payload.value("id", 0ULL);
  auto it = std::find_if(items_.begin(), items_.end(), [&](const Entry& e) { return e.id == id; });

  if (action == "copy" && it != items_.end()) {
    CopyToClipboard(it->text);  // WM_CLIPBOARDUPDATE le remettra en haut de la liste
  } else if (action == "pin" && it != items_.end()) {
    it->pinned = !it->pinned;
  } else if (action == "delete" && it != items_.end()) {
    items_.erase(it);
  } else if (action == "clear") {
    std::erase_if(items_, [](const Entry& e) { return !e.pinned; });
  } else if (action == "setMax") {
    max_items_ = std::clamp(payload.value("value", 50), 10, 500);
    Trim();
  } else if (action == "setKeep") {
    keep_after_restart_ = payload.value("value", false);
  } else if (action == "hotkeyCaptureStart") {
    UnregisterShortcut();  // sinon la combinaison n'arrive pas jusqu'à l'interface
  } else if (action == "hotkeyCaptureCancel") {
    if (running_) RegisterShortcut();
  } else if (action == "setHotkey") {
    hotkey_mods_ = payload.value("mods", 0u) & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
    hotkey_vk_ = payload.value("vk", 0u) & 0xFF;
    hotkey_label_ = payload.value("label", std::string());
    hotkey_enabled_ = true;
    if (running_) RegisterShortcut();
  } else if (action == "setHotkeyEnabled") {
    hotkey_enabled_ = payload.value("value", true);
    if (running_) RegisterShortcut();
  } else if (action == "setAutoPaste") {
    auto_paste_ = payload.value("value", true);
  }
}

void ClipboardHistory::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM) {
  if (!running_) return;
  if (msg == WM_CLIPBOARDUPDATE) Capture();
  if (msg == WM_HOTKEY && wparam == kHotkeyId && on_hotkey_) on_hotkey_();
}

void ClipboardHistory::Capture() {
  // Formats posés par les gestionnaires de mots de passe (convention Windows).
  static const UINT exclude = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
  static const UINT no_history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
  if (IsClipboardFormatAvailable(exclude)) return;
  if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return;
  if (!OpenClipboardRetry(hwnd_)) return;

  std::wstring text;
  bool allowed = true;
  if (IsClipboardFormatAvailable(no_history)) {
    if (HANDLE h = GetClipboardData(no_history)) {
      if (auto* v = static_cast<const DWORD*>(GlobalLock(h))) {
        allowed = (*v != 0);
        GlobalUnlock(h);
      }
    }
  }
  if (allowed) {
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
      if (auto* p = static_cast<const wchar_t*>(GlobalLock(h))) {
        text.assign(p, wcsnlen(p, kMaxTextChars + 1));
        GlobalUnlock(h);
      }
    }
  }
  CloseClipboard();

  if (!allowed || text.empty() || text.size() > kMaxTextChars) return;
  if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos) return;

  // Déjà présent : on le remonte en haut (en gardant l'épingle).
  bool pinned = false;
  if (auto it = std::find_if(items_.begin(), items_.end(), [&](const Entry& e) { return e.text == text; });
      it != items_.end()) {
    if (it == items_.begin()) return;
    pinned = it->pinned;
    items_.erase(it);
  }
  items_.push_front({next_id_++, std::move(text), NowSeconds(), pinned});
  Trim();
  if (on_change_) on_change_();
}

void ClipboardHistory::Trim() {
  // On retire les plus vieux non épinglés.
  while (static_cast<int>(items_.size()) > max_items_) {
    auto it = std::find_if(items_.rbegin(), items_.rend(), [](const Entry& e) { return !e.pinned; });
    if (it == items_.rend()) break;
    items_.erase(std::next(it).base());
  }
}

bool ClipboardHistory::CopyToClipboard(const std::wstring& text) {
  if (!OpenClipboardRetry(hwnd_)) return false;
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
