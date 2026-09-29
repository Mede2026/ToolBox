#pragma once

#include <windows.h>
#include <shellapi.h>
#include <wrl.h>

#include <WebView2.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

#include "modules/module.h"
#include "settings.h"
#include "updater.h"

inline constexpr wchar_t kWindowClass[] = L"ToolBoxMainWindow";

// Messages internes de la fenêtre principale.
enum : UINT {
  WM_APP_TRAY = WM_APP + 1,       // clic sur l'icône de la barre des tâches
  WM_APP_PUSH_STATE,              // un module a changé -> renvoyer l'état à l'interface
  WM_APP_UPDATE,                  // l'updater a changé d'état
  WM_APP_SHOW,                    // une 2e instance demande d'afficher la fenêtre
};

class App {
 public:
  App(HINSTANCE instance, bool start_hidden);
  ~App();

  int Run();

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void CreateMainWindow();
  void InitWebView();
  void ResizeWebView();
  void ShowMainWindow();
  void HideMainWindow();
  void Quit();

  void AddTrayIcon();
  void RemoveTrayIcon();
  void ShowTrayMenu();

  void OnWebMessage(const nlohmann::json& msg);
  void PushState();
  void PushLive();
  void CreateModules();
  void PostToUi(const nlohmann::json& msg);

  void ApplyModules();       // démarre/arrête chaque module selon les réglages
  void SaveModule(Module& m);
  Module* FindModule(const std::string& id);
  bool ModuleEnabled(const std::string& id);

  void ApplyStartWithWindows();
  void MaybeAutoUpdate();
  void RestartForUpdate();

  HINSTANCE instance_;
  HWND hwnd_ = nullptr;
  bool start_hidden_;
  bool quitting_ = false;
  NOTIFYICONDATAW tray_{};

  Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
  Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

  Settings settings_;
  std::vector<std::unique_ptr<Module>> modules_;
  std::unique_ptr<Updater> updater_;
};
