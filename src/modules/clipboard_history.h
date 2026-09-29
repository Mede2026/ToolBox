#pragma once

#include <windows.h>

#include <deque>
#include <functional>
#include <string>

#include "modules/module.h"

// Historique du presse-papiers (texte seulement).
// Les textes marqués « à ne pas mémoriser » par les gestionnaires de mots de passe sont ignorés.
class ClipboardHistory : public Module {
 public:
  ClipboardHistory(HWND hwnd, std::function<void()> on_change);
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

  HWND hwnd_;
  std::function<void()> on_change_;
  bool running_ = false;

  std::deque<Entry> items_;   // plus récent en premier
  unsigned long long next_id_ = 1;
  int max_items_ = 50;
  bool keep_after_restart_ = false;
};
