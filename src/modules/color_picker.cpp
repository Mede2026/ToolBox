#include "modules/color_picker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "color_pick.h"
#include "util.h"

using nlohmann::json;

namespace {

constexpr size_t kMaxHistory = 24;

long long NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void ToHsl(int r, int g, int b, int& h, int& s, int& l) {
  const double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
  const double mx = std::max({rf, gf, bf}), mn = std::min({rf, gf, bf});
  const double d = mx - mn;
  double hh = 0, ss = 0;
  const double ll = (mx + mn) / 2;
  if (d > 1e-9) {
    ss = d / (1 - std::fabs(2 * ll - 1));
    if (mx == rf) hh = std::fmod((gf - bf) / d, 6.0);
    else if (mx == gf) hh = (bf - rf) / d + 2;
    else hh = (rf - gf) / d + 4;
    hh *= 60;
    if (hh < 0) hh += 360;
  }
  h = static_cast<int>(std::lround(hh)) % 360;
  s = static_cast<int>(std::lround(ss * 100));
  l = static_cast<int>(std::lround(ll * 100));
}

}  // namespace

ColorPickerModule::ColorPickerModule(HWND hwnd, HINSTANCE instance, std::function<void()> on_change,
                                     std::function<void()> on_hotkey, Notify notify)
    : hwnd_(hwnd),
      instance_(instance),
      on_change_(std::move(on_change)),
      on_hotkey_(std::move(on_hotkey)),
      notify_(std::move(notify)),
      hotkey_(hwnd, 0x4204, MOD_CONTROL | MOD_ALT, 'C', "Ctrl + Alt + C") {}

void ColorPickerModule::Start() {
  if (running_) return;
  running_ = true;
  hotkey_.Register();
}

void ColorPickerModule::Stop() {
  hotkey_.Unregister();
  running_ = false;
}

std::string ColorPickerModule::Format(const Swatch& s, const std::string& format) const {
  char buf[48];
  if (format == "rgb") {
    snprintf(buf, sizeof(buf), "rgb(%d, %d, %d)", s.r, s.g, s.b);
  } else if (format == "hsl") {
    int h, sat, l;
    ToHsl(s.r, s.g, s.b, h, sat, l);
    snprintf(buf, sizeof(buf), "hsl(%d, %d%%, %d%%)", h, sat, l);
  } else {
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", s.r, s.g, s.b);
  }
  return buf;
}

void ColorPickerModule::LoadConfig(const json& cfg) {
  hotkey_.Load(cfg);
  format_ = cfg.value("format", std::string("hex"));
  history_.clear();
  for (const auto& e : cfg.value("history", json::array())) {
    if (e.is_array() && e.size() == 4) {
      history_.push_back({next_id_++, e[0].get<int>(), e[1].get<int>(), e[2].get<int>(), e[3].get<long long>()});
    }
  }
}

json ColorPickerModule::SaveConfig() const {
  json history = json::array();
  for (const auto& s : history_) history.push_back({s.r, s.g, s.b, s.time});
  json cfg{{"format", format_}, {"history", history}};
  hotkey_.Save(cfg);
  return cfg;
}

json ColorPickerModule::State() const {
  json history = json::array();
  for (const auto& s : history_) {
    history.push_back({{"id", s.id}, {"hex", Format(s, "hex")}, {"rgb", Format(s, "rgb")}, {"hsl", Format(s, "hsl")},
                       {"time", s.time}});
  }
  json st{{"format", format_}, {"history", history}};
  hotkey_.AddState(st);
  return st;
}

void ColorPickerModule::HandleAction(const std::string& action, const json& payload) {
  if (hotkey_.HandleAction(action, payload, running_)) return;
  if (action == "setFormat") {
    const std::string f = payload.value("value", std::string("hex"));
    if (f == "hex" || f == "rgb" || f == "hsl") format_ = f;
  } else if (action == "copy") {
    const auto id = payload.value("id", 0ULL);
    for (const auto& s : history_) {
      if (s.id == id) util::SetClipboardText(hwnd_, util::FromUtf8(Format(s, payload.value("format", format_))));
    }
  } else if (action == "delete") {
    const auto id = payload.value("id", 0ULL);
    std::erase_if(history_, [&](const Swatch& s) { return s.id == id; });
  } else if (action == "clear") {
    history_.clear();
  }
}

void ColorPickerModule::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM) {
  if (running_ && hotkey_.Matches(msg, wparam) && on_hotkey_) on_hotkey_();
}

void ColorPickerModule::BeginPick(std::function<void()> after) {
  if (!running_ || !ColorPicker::Start(instance_, [this, after](bool ok, COLORREF c) {
        if (after) after();
        if (ok) Picked(c);
      })) {
    if (after) after();
  }
}

void ColorPickerModule::Picked(COLORREF color) {
  Swatch s{next_id_++, GetRValue(color), GetGValue(color), GetBValue(color), NowSeconds()};
  // Même couleur déjà dans l'historique : on la remonte en haut.
  std::erase_if(history_, [&](const Swatch& x) { return x.r == s.r && x.g == s.g && x.b == s.b; });
  history_.push_front(s);
  while (history_.size() > kMaxHistory) history_.pop_back();

  const std::string text = Format(s, format_);
  util::SetClipboardText(hwnd_, util::FromUtf8(text));
  Emit("colorPick");
  if (notify_) notify_("Couleur copiée", text);
  if (on_change_) on_change_();
}
