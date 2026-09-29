#pragma once

#include <windows.h>

#include <deque>
#include <functional>
#include <string>

#include "modules/module.h"

// Historique du presse-papiers (texte seulement).
// Les textes marqués « à ne pas mémoriser » par les gestionnaires de mots de passe sont ignorés.
// Un raccourci clavier global (choisi par l'utilisateur) ouvre une petite fenêtre avec l'historique.
class ClipboardHistory : public Module {
 public:
  ClipboardHistory(HWND hwnd, std::function<void()> on_change, std::function<void()> on_hotkey);
  ~ClipboardHistory() override;

  std::string Id() const override { return "clipboard"; }
  std::string Name() const override { return "Presse-papiers"; }
  std::string Description() const override { return "Retrouve tout ce que tu as copié."; }

  void Start() override;
  void Stop() override;
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;
  void OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

  // Copie l'élément `id` dans le presse-papiers. Retourne false s'il n'existe pas.
  bool CopyItem(unsigned long long id);
  bool AutoPaste() const { return auto_paste_; }

 private:
  struct Entry {
    unsigned long long id;
    std::wstring text;
    long long time;
    bool pinned = false;
  };

  void Capture();
  bool CopyToClipboard(const std::wstring& text);
  void Trim();
  void RegisterShortcut();
  void UnregisterShortcut();

  static constexpr int kHotkeyId = 0x4201;

  HWND hwnd_;
  std::function<void()> on_change_;
  std::function<void()> on_hotkey_;
  bool running_ = false;

  // Raccourci : par défaut Ctrl + Alt + V
  bool hotkey_enabled_ = true;
  UINT hotkey_mods_ = MOD_CONTROL | MOD_ALT;
  UINT hotkey_vk_ = 'V';
  std::string hotkey_label_ = "Ctrl + Alt + V";
  bool hotkey_registered_ = false;
  std::string hotkey_error_;
  bool auto_paste_ = true;

  std::deque<Entry> items_;   // plus récent en premier
  unsigned long long next_id_ = 1;
  int max_items_ = 50;
  bool keep_after_restart_ = false;
};
