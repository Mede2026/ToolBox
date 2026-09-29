#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "modules/module.h"

// Garde Enter : annule la touche voisine d'Enter frappée par accident.
//
// La touche surveillée est identifiée par son scancode (position physique),
// donc ça marche avec n'importe quelle disposition : « à », « \ », « # », « * »…
// Par défaut : scancode 0x2B, la touche au-dessus / à côté d'Enter.
//
//  - Touche puis Enter trop vite  -> Enter bloqué, Retour arrière, puis Enter renvoyé.
//  - Enter puis touche trop vite  -> la touche est bloquée.
class EnterGuard : public Module {
 public:
  explicit EnterGuard(std::function<void()> on_change);
  ~EnterGuard() override;

  std::string Id() const override { return "enter_guard"; }
  std::string Name() const override { return "Garde Enter"; }
  std::string Description() const override {
    return "Supprime la touche voisine d'Enter frappée par accident.";
  }

  void Start() override;
  void Stop() override;
  bool Running() const override { return hook_ != nullptr; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;

 private:
  static constexpr DWORD kDefaultScan = 0x2B;
  static constexpr DWORD kDefaultThresholdMs = 80;

  static LRESULT CALLBACK HookProc(int code, WPARAM wparam, LPARAM lparam);
  bool OnKey(WPARAM wparam, const KBDLLHOOKSTRUCT& k);  // true = bloquer la touche
  bool Excluded() const;
  void SendBackspaceThenEnter(const KBDLLHOOKSTRUCT& enter);
  std::wstring KeyName() const;

  static EnterGuard* instance_;

  std::function<void()> on_change_;
  HHOOK hook_ = nullptr;

  // Réglages
  DWORD scan_ = kDefaultScan;
  bool extended_ = false;
  DWORD threshold_ms_ = kDefaultThresholdMs;
  std::vector<std::wstring> exclusions_;
  unsigned long long corrections_ = 0;

  // État du clavier
  bool capturing_ = false;
  DWORD swallow_up_scan_ = 0;       // relâchement à bloquer (touche capturée)
  bool enter_down_ = false;
  DWORD last_enter_down_ = 0;
  bool guard_down_ = false;
  DWORD last_guard_down_ = 0;
  bool last_key_was_guard_ = false;
  bool suppress_guard_repeats_ = false;
  bool block_guard_up_ = false;
};
