#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "hotkey.h"
#include "modules/module.h"

namespace tesseract {
class TessBaseAPI;
}

// Envoyé à la fenêtre principale quand une lecture est terminée (lparam = OcrResult*).
inline constexpr UINT WM_TOOLBOX_OCR_DONE = WM_APP + 50;

// Texte à l'écran (OCR) : un raccourci fige l'écran, on sélectionne une zone,
// Tesseract lit le texte et il est copié dans le presse-papiers.
class ScreenOcr : public Module {
 public:
  using Notify = std::function<void(const std::string& title, const std::string& text)>;

  ScreenOcr(HWND hwnd, HINSTANCE instance, std::function<void()> on_change, std::function<void()> on_hotkey,
            Notify notify);
  ~ScreenOcr() override;

  std::string Id() const override { return "ocr"; }
  std::string Name() const override { return "Texte à l'écran"; }
  std::string Description() const override {
    return "Sélectionne une zone de l'écran : le texte est lu (Tesseract) et copié.";
  }

  void Start() override;
  void Stop() override;
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;
  void OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;
  void Tick() override;

  // Ouvre la sélection d'écran. `after_select` est appelé quand elle se ferme (lue ou annulée).
  void BeginCapture(std::function<void()> after_select);

 private:
  struct Entry {
    unsigned long long id;
    std::string text;
    long long time;
    int ms;  // durée de lecture
  };

  void RecognizeAsync(std::vector<uint8_t> bgra, int w, int h);
  void DownloadAsync();
  // Télécharge les langues manquantes (fil de travail). Retourne false en cas d'erreur.
  bool EnsureLanguages(const std::vector<std::string>& langs, std::string& err);
  std::string Recognize(const std::vector<uint8_t>& bgra, int w, int h, const std::vector<std::string>& langs,
                        bool single_line, std::string& err);
  void ReleaseEngine();
  void SetStatus(const std::string& status, const std::string& detail = {});
  static std::string LangString(const std::vector<std::string>& langs);

  HWND hwnd_;
  HINSTANCE instance_;
  std::function<void()> on_change_;
  std::function<void()> on_hotkey_;
  Notify notify_;
  bool running_ = false;
  GlobalHotkey hotkey_;  // par défaut Ctrl + Alt + T

  // Réglages
  std::vector<std::string> langs_ = {"fra", "eng"};
  bool single_line_ = false;
  bool notify_enabled_ = true;

  // Historique (fil principal)
  std::deque<Entry> history_;
  unsigned long long next_id_ = 1;

  // Moteur (fil de travail)
  std::mutex engine_mu_;
  std::unique_ptr<tesseract::TessBaseAPI> api_;
  std::string loaded_langs_;
  std::atomic<ULONGLONG> last_use_{0};
  std::atomic<bool> busy_{false};

  mutable std::mutex status_mu_;
  std::string status_ = "idle";  // idle, downloading, reading, error
  std::string detail_;
};
