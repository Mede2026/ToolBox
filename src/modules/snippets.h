#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "hotkey.h"
#include "modules/module.h"

// Textes rapides : des textes préparés (courriel, adresse, signature…).
// Un raccourci clavier ouvre une petite fenêtre ; un clic colle le texte là où on écrivait.
class Snippets : public Module {
 public:
  Snippets(HWND hwnd, std::function<void()> on_hotkey);
  ~Snippets() override;

  std::string Id() const override { return "snippets"; }
  std::string Name() const override { return "Textes rapides"; }
  std::string Description() const override { return "Colle ton courriel, ton adresse… en un clic."; }

  void Start() override;
  void Stop() override;
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;
  void OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

  // Texte de l'élément `id` (vide s'il n'existe pas). Compté dans les statistiques.
  std::wstring Use(const std::string& id);
  bool AutoPaste() const { return auto_paste_; }

 private:
  struct Item {
    std::string id;
    std::string label;  // ce qu'on voit dans la fenêtre (ex. « Mon courriel »)
    std::string text;   // ce qui est collé
  };

  HWND hwnd_;
  std::function<void()> on_hotkey_;
  bool running_ = false;
  GlobalHotkey hotkey_;  // par défaut Ctrl + Alt + Q
  bool auto_paste_ = true;
  std::vector<Item> items_;
};
