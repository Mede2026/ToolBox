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

// Capture d'écran : un raccourci fige l'écran et on sélectionne une zone, puis
//  - capture : l'image est copiée et enregistrée en PNG (Images\Captures ToolBox) ;
//  - texte (OCR) : Tesseract lit le texte et il est copié.
// L'id reste « ocr » pour garder les réglages de la 0.3.
class ScreenOcr : public Module {
 public:
  using Notify = std::function<void(const std::string& title, const std::string& text)>;

  // on_hotkey(screenshot) : un des deux raccourcis a été pressé.
  ScreenOcr(HWND hwnd, HINSTANCE instance, std::function<void()> on_change, std::function<void(bool)> on_hotkey,
            Notify notify);
  ~ScreenOcr() override;

  std::string Id() const override { return "ocr"; }
  std::string Name() const override { return "Capture d'écran"; }
  std::string Description() const override {
    return "Capture une zone de l'écran en image, ou lis le texte qu'elle contient (OCR).";
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

  // Ouvre la sélection d'écran. `after_select` est appelé quand elle se ferme (validée ou annulée).
  void BeginCapture(bool screenshot, std::function<void()> after_select);

 private:
  struct Entry {
    unsigned long long id;
    std::string text;
    long long time;
    int ms;  // durée de lecture
  };

  struct Shot {
    unsigned long long id;
    std::string path;  // vide si non enregistrée
    long long time;
    int w, h;
  };

  void RecognizeAsync(std::vector<uint8_t> bgra, int w, int h);
  void SaveScreenshot(std::vector<uint8_t> bgra, int w, int h);
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
  std::function<void(bool)> on_hotkey_;
  Notify notify_;
  bool running_ = false;
  GlobalHotkey hotkey_;       // texte : Ctrl + Alt + T
  GlobalHotkey shot_hotkey_;  // image : Ctrl + Alt + S

  // Captures d'écran
  bool shot_save_ = true;
  std::deque<Shot> shots_;

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
