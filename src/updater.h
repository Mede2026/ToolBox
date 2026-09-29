#pragma once

#include <windows.h>

#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

// Mises à jour automatiques via les Releases GitHub.
//
// La Release doit contenir ToolBox.exe et ToolBox.exe.sha256 (créés par la GitHub Action).
// Le travail réseau se fait sur un fil séparé ; chaque changement d'état envoie
// `notify_msg` à la fenêtre `notify_hwnd`.
class Updater {
 public:
  Updater(HWND notify_hwnd, UINT notify_msg);

  void CheckAsync();
  void InstallAsync();

  // Vrai quand le nouvel exe est en place : il reste à relancer l'app.
  bool ReadyToRestart() const { return ready_; }

  nlohmann::json State() const;

  // Supprime ToolBox.old.exe laissé par une mise à jour précédente.
  static void CleanupOldExe();

 private:
  void SetState(const std::string& status, const std::string& error = {});
  void DoCheck();
  void DoInstall();

  HWND hwnd_;
  UINT msg_;
  std::atomic<bool> busy_{false};
  std::atomic<bool> ready_{false};

  mutable std::mutex mu_;
  std::string status_ = "idle";  // idle, checking, upToDate, available, downloading, ready, error
  std::string error_;
  std::string latest_;
  std::string notes_;
  std::string exe_url_;
  std::string sha_url_;
  int progress_ = 0;
  long long last_check_ = 0;  // secondes depuis 1970
};
