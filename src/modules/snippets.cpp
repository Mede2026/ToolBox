#include "modules/snippets.h"

#include <algorithm>
#include <chrono>

#include "util.h"

using nlohmann::json;

namespace {
constexpr size_t kMaxItems = 100;
constexpr size_t kMaxTextBytes = 20000;
}  // namespace

Snippets::Snippets(HWND hwnd, std::function<void()> on_hotkey)
    : hwnd_(hwnd), on_hotkey_(std::move(on_hotkey)), hotkey_(hwnd, 0x4205, MOD_CONTROL | MOD_ALT, 'Q', "Ctrl + Alt + Q") {}

Snippets::~Snippets() { Stop(); }

void Snippets::Start() {
  running_ = true;
  hotkey_.Register();
}

void Snippets::Stop() {
  hotkey_.Unregister();
  running_ = false;
}

void Snippets::LoadConfig(const json& cfg) {
  hotkey_.Load(cfg);
  auto_paste_ = cfg.value("autoPaste", true);
  items_.clear();
  const json items = cfg.value("items", json::array());
  for (const auto& i : items) {
    if (!i.is_object() || items_.size() >= kMaxItems) continue;
    Item item{i.value("id", ""), i.value("label", ""), i.value("text", "")};
    if (item.id.empty() || item.text.empty()) continue;
    items_.push_back(std::move(item));
  }
}

json Snippets::SaveConfig() const {
  json items = json::array();
  for (const auto& i : items_) items.push_back({{"id", i.id}, {"label", i.label}, {"text", i.text}});
  json cfg{{"items", items}, {"autoPaste", auto_paste_}};
  hotkey_.Save(cfg);
  return cfg;
}

json Snippets::State() const {
  json state = SaveConfig();
  hotkey_.AddState(state);
  return state;
}

std::wstring Snippets::Use(const std::string& id) {
  auto it = std::find_if(items_.begin(), items_.end(), [&](const Item& i) { return i.id == id; });
  if (it == items_.end()) return {};
  Emit("snippet");
  return util::FromUtf8(it->text);
}

void Snippets::HandleAction(const std::string& action, const json& payload) {
  if (hotkey_.HandleAction(action, payload, running_)) return;
  const std::string id = payload.value("id", "");
  auto it = std::find_if(items_.begin(), items_.end(), [&](const Item& i) { return i.id == id; });

  if (action == "saveItem") {
    const json p = payload.value("item", json::object());
    std::string text = p.value("text", "");
    if (text.empty()) return;
    if (text.size() > kMaxTextBytes) text.resize(kMaxTextBytes);
    const std::string item_id = p.value("id", "");
    auto found = std::find_if(items_.begin(), items_.end(), [&](const Item& i) { return i.id == item_id; });
    if (found == items_.end()) {
      if (items_.size() >= kMaxItems) return;
      found = items_.insert(items_.end(), Item{});
      found->id = "s" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    found->label = p.value("label", "");
    found->text = std::move(text);
  } else if (action == "deleteItem" && it != items_.end()) {
    items_.erase(it);
  } else if (action == "move" && it != items_.end()) {
    const int dir = payload.value("dir", 0);
    const auto pos = it - items_.begin();
    const auto to = pos + dir;
    if (dir != 0 && to >= 0 && to < static_cast<long long>(items_.size())) std::swap(items_[pos], items_[to]);
  } else if (action == "copy" && it != items_.end()) {
    util::SetClipboardText(hwnd_, util::FromUtf8(it->text));
    Emit("snippet");
  } else if (action == "setAutoPaste") {
    auto_paste_ = payload.value("value", true);
  }
}

void Snippets::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM) {
  if (running_ && hotkey_.Matches(msg, wparam) && on_hotkey_) on_hotkey_();
}
