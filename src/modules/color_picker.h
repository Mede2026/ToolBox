#pragma once

#include <windows.h>

#include <deque>
#include <functional>
#include <string>

#include "hotkey.h"
#include "modules/module.h"

// Pipette : un raccourci affiche une loupe ; on clique sur un pixel et sa couleur
// est copiée (HEX, RGB ou HSL). Les dernières couleurs restent dans l'historique.
class ColorPickerModule : public Module {
 public:
  using Notify = std::function<void(const std::string& title, const std::string& text)>;

  ColorPickerModule(HWND hwnd, HINSTANCE instance, std::function<void()> on_change, std::function<void()> on_hotkey,
                    Notify notify);
  ~ColorPickerModule() override { Stop(); }

  std::string Id() const override { return "color"; }
  std::string Name() const override { return "Pipette"; }
  std::string Description() const override {
    return "Copie la couleur de n'importe quel pixel de l'écran (HEX, RGB ou HSL).";
  }

  void Start() override;
  void Stop() override;
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;
  void OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

  // Ouvre la loupe. `after` est appelé quand elle se ferme.
  void BeginPick(std::function<void()> after);

 private:
  struct Swatch {
    unsigned long long id;
    int r, g, b;
    long long time;
  };

  void Picked(COLORREF color);
  std::string Format(const Swatch& s, const std::string& format) const;

  HWND hwnd_;
  HINSTANCE instance_;
  std::function<void()> on_change_;
  std::function<void()> on_hotkey_;
  Notify notify_;
  bool running_ = false;
  GlobalHotkey hotkey_;  // Ctrl + Alt + C

  std::string format_ = "hex";  // hex, rgb, hsl
  std::deque<Swatch> history_;
  unsigned long long next_id_ = 1;
};
