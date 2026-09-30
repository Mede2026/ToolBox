#include "app.h"

#include <dwmapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <string>

#include "modules/app_launcher.h"
#include "modules/color_picker.h"
#include "modules/clipboard_history.h"
#include "modules/snippets.h"
#include "modules/enter_guard.h"
#include "modules/place_launcher.h"
#include "modules/process_manager.h"
#include "modules/screen_ocr.h"
#include "modules/stats.h"
#include "modules/system_monitor.h"
#include "resource_ids.h"
#include "util.h"
#include "version.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using nlohmann::json;

namespace {

constexpr UINT_PTR kTimerTick = 1;          // Tick des modules
constexpr UINT_PTR kTimerUpdate = 2;        // vérification des mises à jour
constexpr UINT_PTR kTimerLive = 3;          // données en direct (fenêtre visible seulement)
constexpr UINT_PTR kTimerOcr = 4;           // délai avant la capture (le temps de cacher ToolBox)
constexpr UINT_PTR kTimerUnload = 5;        // libère l'interface quand ToolBox reste caché
constexpr UINT kUnloadAfterMs = 3 * 60 * 1000;
constexpr UINT kLiveMs = 1000;
constexpr UINT kTickMs = 10 * 1000;
constexpr UINT kUpdateEveryMs = 6 * 60 * 60 * 1000;
constexpr UINT kFirstUpdateCheckMs = 8 * 1000;

constexpr UINT kTrayOpen = 1001;
constexpr UINT kTrayToggle = 1002;
constexpr UINT kTrayQuit = 1003;

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::wstring LoadUiHtml(HINSTANCE instance) {
  HRSRC res = FindResourceW(instance, MAKEINTRESOURCEW(IDR_UI_HTML), RT_RCDATA);
  if (!res) return L"<h1>Interface introuvable</h1>";
  HGLOBAL data = LoadResource(instance, res);
  const char* bytes = static_cast<const char*>(LockResource(data));
  return util::FromUtf8(std::string(bytes, SizeofResource(instance, res)));
}

// Boîte de dialogue Windows « Ouvrir un fichier ».
std::wstring PickFile(HWND owner) {
  std::wstring result;
  ComPtr<IFileOpenDialog> dlg;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return result;
  COMDLG_FILTERSPEC filters[] = {
      {L"Applications et raccourcis", L"*.exe;*.lnk;*.bat;*.cmd;*.url"},
      {L"Tous les fichiers", L"*.*"},
  };
  dlg->SetFileTypes(2, filters);
  dlg->SetTitle(L"Choisir un fichier ou une app à ouvrir");
  DWORD opts = 0;
  dlg->GetOptions(&opts);
  dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_NODEREFERENCELINKS);  // garder le .lnk tel quel
  if (dlg->Show(owner) != S_OK) return result;
  ComPtr<IShellItem> item;
  if (SUCCEEDED(dlg->GetResult(&item))) {
    PWSTR path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      result = path;
      CoTaskMemFree(path);
    }
  }
  return result;
}

}  // namespace

App::App(HINSTANCE instance, bool start_hidden) : instance_(instance), start_hidden_(start_hidden) {
  settings_.Load();
}

void App::CreateModules() {
  // Peut être appelé depuis un autre fil (ex. GPS) : PostMessage est sûr.
  auto push = [hwnd = hwnd_] { PostMessageW(hwnd, WM_APP_PUSH_STATE, 0, 0); };

  auto clip_popup = [hwnd = hwnd_] { PostMessageW(hwnd, WM_APP_CLIP_POPUP, 0, 0); };
  auto snip_popup = [hwnd = hwnd_] { PostMessageW(hwnd, WM_APP_SNIP_POPUP, 0, 0); };
  auto ocr_hotkey = [hwnd = hwnd_](bool shot) { PostMessageW(hwnd, WM_APP_OCR_START, shot ? 1 : 0, 0); };
  auto color_hotkey = [hwnd = hwnd_] { PostMessageW(hwnd, WM_APP_OCR_START, 2, 0); };
  auto notify = [this](const std::string& title, const std::string& text) { Notify(title, text); };

  // ---- Liste des fonctions (ordre = ordre dans le menu) : ajouter les nouveaux modules ici ----
  // Les fonctions toujours actives d'abord.
  modules_.push_back(std::make_unique<SystemMonitor>());
  modules_.push_back(std::make_unique<UiModule>("converter", "Convertisseur", "Unités et devises.", /*always_on=*/true));
  modules_.push_back(std::make_unique<Stats>(push));
  modules_.push_back(std::make_unique<ProcessManager>());
  modules_.push_back(std::make_unique<EnterGuard>(push));
  modules_.push_back(std::make_unique<ClipboardHistory>(hwnd_, push, clip_popup));
  modules_.push_back(std::make_unique<Snippets>(hwnd_, snip_popup));
  modules_.push_back(std::make_unique<ScreenOcr>(hwnd_, instance_, push, ocr_hotkey, notify));
  modules_.push_back(std::make_unique<ColorPickerModule>(hwnd_, instance_, push, color_hotkey, notify));
  modules_.push_back(std::make_unique<AppLauncher>());
  modules_.push_back(std::make_unique<PlaceLauncher>(push));

  for (auto& m : modules_) {
    try {
      m->LoadConfig(settings_.Module(m->Id()));
    } catch (const std::exception& e) {  // réglages illisibles : cette fonction repart à zéro
      util::LogError("réglages de « " + m->Id() + " » illisibles, remis à zéro : " + e.what());
      settings_.Module(m->Id()) = nlohmann::json::object();
      m->LoadConfig(nlohmann::json::object());
    }
  }

  // Les modules signalent leurs actions (texte lu, copie…) aux Statistiques.
  Module::event_sink = [this](const std::string& event) {
    if (auto* stats = static_cast<Stats*>(FindModule("stats")); stats && stats->Running()) stats->Count(event);
  };
}

App::~App() {
  for (auto& m : modules_) m->Stop();
  RemoveTrayIcon();
}

int App::Run() {
  CreateMainWindow();
  CreateModules();
  updater_ = std::make_unique<Updater>(hwnd_, WM_APP_UPDATE);
  AddTrayIcon();
  ApplyModules();
  ApplyStartWithWindows();
  InitWebView();

  SetTimer(hwnd_, kTimerTick, kTickMs, nullptr);
  SetTimer(hwnd_, kTimerUpdate, kFirstUpdateCheckMs, nullptr);

  if (!start_hidden_) ShowMainWindow();

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}

// ---------------------------------------------------------------- Fenêtre

void App::CreateMainWindow() {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance_;
  wc.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP));
  wc.hIconSm = wc.hIcon;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = CreateSolidBrush(RGB(32, 32, 32));
  wc.lpszClassName = kWindowClass;
  RegisterClassExW(&wc);

  const UINT dpi = GetDpiForSystem();
  const int w = MulDiv(1100, dpi, 96), h = MulDiv(740, dpi, 96);
  RECT work{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  const int x = work.left + ((work.right - work.left) - w) / 2;
  const int y = work.top + ((work.bottom - work.top) - h) / 2;

  hwnd_ = CreateWindowExW(0, kWindowClass, L"ToolBox", WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr,
                          instance_, this);

  BOOL dark = TRUE;  // barre de titre sombre (Windows 10 20H1+ / 11)
  DwmSetWindowAttribute(hwnd_, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
  const COLORREF caption = RGB(32, 32, 32);
  DwmSetWindowAttribute(hwnd_, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));

  RECT min{0, 0, MulDiv(760, dpi, 96), MulDiv(520, dpi, 96)};
  SetPropW(hwnd_, L"minW", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(min.right)));
  SetPropW(hwnd_, L"minH", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(min.bottom)));
}

void App::ShowMainWindow() {
  if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  KillTimer(hwnd_, kTimerUnload);
  if (controller_) controller_->put_IsVisible(TRUE);
  else CreateMainWebView();  // interface libérée pendant que ToolBox était caché
  SetTimer(hwnd_, kTimerLive, kLiveMs, nullptr);
  PushState();
}

void App::HideMainWindow() {
  ShowWindow(hwnd_, SW_HIDE);
  KillTimer(hwnd_, kTimerLive);
  if (controller_) controller_->put_IsVisible(FALSE);  // WebView2 consomme moins caché
  if (settings_.Get(settings_.Root(), "lowMemory", true)) SetTimer(hwnd_, kTimerUnload, kUnloadAfterMs, nullptr);
}

void App::Quit() {
  quitting_ = true;
  DestroyWindow(hwnd_);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  App* self = nullptr;
  if (msg == WM_NCCREATE) {
    self = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) return DefWindowProcW(hwnd, msg, wparam, lparam);
  try {
    return self->HandleMessage(msg, wparam, lparam);
  } catch (const std::exception& e) {  // une erreur ne doit pas fermer toute l'app
    util::LogError(std::string("message ") + std::to_string(msg) + " : " + e.what());
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
}

LRESULT App::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  static const UINT taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
  if (msg == taskbar_created) {  // l'Explorateur a redémarré : remettre l'icône
    AddTrayIcon();
    return 0;
  }

  switch (msg) {
    case WM_SIZE:
      ResizeWebView();
      return 0;

    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
      info->ptMinTrackSize.x = static_cast<LONG>(reinterpret_cast<INT_PTR>(GetPropW(hwnd_, L"minW")));
      info->ptMinTrackSize.y = static_cast<LONG>(reinterpret_cast<INT_PTR>(GetPropW(hwnd_, L"minH")));
      return 0;
    }

    case WM_CLOSE:
      if (!quitting_ && settings_.Get(settings_.Root(), "minimizeToTray", true)) {
        HideMainWindow();
        return 0;
      }
      Quit();
      return 0;

    case WM_DESTROY:
      KillTimer(hwnd_, kTimerTick);
      KillTimer(hwnd_, kTimerUpdate);
      KillTimer(hwnd_, kTimerLive);
      for (auto& m : modules_) m->Stop();
      if (popup_controller_) popup_controller_->Close();
      if (popup_hwnd_) DestroyWindow(popup_hwnd_);
      if (controller_) controller_->Close();
      PostQuitMessage(0);
      return 0;

    case WM_TIMER:
      if (wparam == kTimerTick) {
        for (auto& m : modules_) {
          if (m->Running()) m->Tick();
        }
      } else if (wparam == kTimerUnload) {
        KillTimer(hwnd_, kTimerUnload);
        ReleaseMainWebView();
      } else if (wparam == kTimerOcr) {
        KillTimer(hwnd_, kTimerOcr);
        BeginOcrNow();
      } else if (wparam == kTimerLive) {
        PushLive();
      } else if (wparam == kTimerUpdate) {
        SetTimer(hwnd_, kTimerUpdate, kUpdateEveryMs, nullptr);
        if (settings_.Get(settings_.Root(), "autoUpdate", true)) updater_->CheckAsync();
      }
      return 0;

    case WM_APP_TRAY:
      if (LOWORD(lparam) == WM_LBUTTONUP || LOWORD(lparam) == NIN_SELECT) ShowMainWindow();
      if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) ShowTrayMenu();
      return 0;

    case WM_CLIPBOARDUPDATE:
    case WM_HOTKEY:
    case WM_TOOLBOX_OCR_DONE:
      for (auto& m : modules_) m->OnWindowMessage(msg, wparam, lparam);
      return 0;

    case WM_APP_CLIP_POPUP:
      ShowPopup(false);
      return 0;

    case WM_APP_SNIP_POPUP:
      ShowPopup(true);
      return 0;

    case WM_APP_OCR_START:
      StartOcr(false, static_cast<int>(wparam));
      return 0;

    case WM_APP_SHOW:
      ShowMainWindow();
      return 0;

    case WM_APP_PUSH_STATE:
      for (auto& m : modules_) SaveModule(*m);
      settings_.Save();
      PushState();
      return 0;

    case WM_APP_UPDATE:
      MaybeAutoUpdate();
      PostToUi({{"type", "update"}, {"update", updater_->State()}});
      return 0;

    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case kTrayOpen:
          ShowMainWindow();
          break;
        case kTrayToggle:
          settings_.Root()["enabled"] = !settings_.Get(settings_.Root(), "enabled", true);
          settings_.Save();
          ApplyModules();
          PushState();
          break;
        case kTrayQuit:
          Quit();
          break;
      }
      return 0;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

// ---------------------------------------------------------------- WebView2

void App::InitWebView() {
  const std::wstring data_dir = (util::DataDir() / L"WebView2").wstring();
  const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, data_dir.c_str(), nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result) || !env) return result;
            env_ = env;
            if (IsWindowVisible(hwnd_) || !start_hidden_) CreateMainWebView();
            if (settings_.Get(settings_.Root(), "instantPopup", true)) PreparePopup();
            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    const int choice = MessageBoxW(
        hwnd_,
        L"ToolBox a besoin de Microsoft Edge WebView2 (déjà inclus dans Windows 10/11 à jour).\n\n"
        L"Ouvrir la page de téléchargement ?",
        L"ToolBox", MB_YESNO | MB_ICONWARNING);
    if (choice == IDYES) {
      ShellExecuteW(nullptr, L"open", L"https://developer.microsoft.com/microsoft-edge/webview2/", nullptr, nullptr,
                    SW_SHOWNORMAL);
    }
  }
}

// L'interface principale. Détruite quand ToolBox reste caché (économie de mémoire),
// recréée à la réouverture de la fenêtre.
void App::CreateMainWebView() {
  if (!env_ || controller_ || creating_webview_) return;
  creating_webview_ = true;
  env_->CreateCoreWebView2Controller(
      hwnd_, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                 [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                   creating_webview_ = false;
                   if (FAILED(result) || !controller) return result;
                   controller_ = controller;
                   controller_->get_CoreWebView2(&webview_);

                   ComPtr<ICoreWebView2Controller2> c2;
                   if (SUCCEEDED(controller_.As(&c2))) {
                     c2->put_DefaultBackgroundColor({255, 32, 32, 32});  // pas de flash blanc
                   }

                   ComPtr<ICoreWebView2Settings> s;
                   webview_->get_Settings(&s);
                   s->put_IsStatusBarEnabled(FALSE);
                   s->put_IsZoomControlEnabled(FALSE);
#ifdef NDEBUG
                   s->put_AreDevToolsEnabled(FALSE);
                   s->put_AreDefaultContextMenusEnabled(FALSE);
#endif
                   ComPtr<ICoreWebView2Settings3> s3;
                   if (SUCCEEDED(s.As(&s3))) s3->put_AreBrowserAcceleratorKeysEnabled(FALSE);

                   webview_->add_WebMessageReceived(
                       Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                           [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
                             LPWSTR raw = nullptr;
                             if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
                               json msg = json::parse(util::ToUtf8(raw), nullptr, false);
                               CoTaskMemFree(raw);
                               if (msg.is_object()) {
                       try {
                         OnWebMessage(msg);
                       } catch (const std::exception& e) {
                         util::LogError(std::string("interface : ") + e.what());
                       }
                     }
                             }
                             return S_OK;
                           })
                           .Get(),
                       nullptr);

                   // Les liens externes s'ouvrent dans le navigateur, pas dans l'app.
                   webview_->add_NewWindowRequested(
                       Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                           [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) {
                             LPWSTR uri = nullptr;
                             args->get_Uri(&uri);
                             if (uri && wcsncmp(uri, L"https://", 8) == 0) {
                               ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL);
                             }
                             CoTaskMemFree(uri);
                             args->put_Handled(TRUE);
                             return S_OK;
                           })
                           .Get(),
                       nullptr);

                   ResizeWebView();
                   controller_->put_IsVisible(IsWindowVisible(hwnd_));
                   webview_->NavigateToString(LoadUiHtml(instance_).c_str());
                   return S_OK;
                 })
                 .Get());
}

void App::ReleaseMainWebView() {
  if (!controller_ || IsWindowVisible(hwnd_)) return;
  controller_->Close();
  controller_.Reset();
  webview_.Reset();
}

void App::ResizeWebView() {
  if (!controller_) return;
  RECT bounds;
  GetClientRect(hwnd_, &bounds);
  controller_->put_Bounds(bounds);
}

void App::PostToUi(const json& msg) {
  const std::wstring text = util::FromUtf8(msg.dump());
  if (webview_) webview_->PostWebMessageAsJson(text.c_str());
  if (popup_webview_ && msg.value("type", "") != "live") popup_webview_->PostWebMessageAsJson(text.c_str());
}

void App::PushState() {
  json modules = json::array();
  for (auto& m : modules_) {
    modules.push_back({
        {"id", m->Id()},
        {"name", m->Name()},
        {"description", m->Description()},
        {"enabled", ModuleEnabled(m->Id())},
        {"alwaysOn", m->AlwaysOn()},
        {"running", m->Running()},
        {"state", m->State()},
    });
  }
  const auto& r = settings_.Root();
  PostToUi({
      {"type", "state"},
      {"app",
       {{"version", util::ToUtf8(TOOLBOX_VERSION_STR)},
        {"enabled", settings_.Get(r, "enabled", true)},
        {"startWithWindows", settings_.Get(r, "startWithWindows", false)},
        {"minimizeToTray", settings_.Get(r, "minimizeToTray", true)},
        {"autoUpdate", settings_.Get(r, "autoUpdate", true)},
        {"lowMemory", settings_.Get(r, "lowMemory", true)},
        {"instantPopup", settings_.Get(r, "instantPopup", true)},
        {"dataDir", util::ToUtf8(util::DataDir().wstring())},
        {"userName", util::ToUtf8(util::UserFirstName())}}},
      {"modules", modules},
      {"update", updater_->State()},
  });
}

void App::PushLive() {
  if (!webview_ || IsIconic(hwnd_)) return;
  json live = json::object();
  for (auto& m : modules_) {
    if (!m->Running()) continue;
    json data = m->Live();
    if (!data.is_null()) live[m->Id()] = std::move(data);
  }
  if (!live.empty()) PostToUi({{"type", "live"}, {"live", live}});
}

// Messages envoyés par l'interface (window.chrome.webview.postMessage).
void App::OnWebMessage(const json& msg) {
  const std::string type = msg.value("type", "");

  if (type == "ready") {
    // rien : l'état est renvoyé plus bas
  } else if (type == "setGlobal") {
    settings_.Root()["enabled"] = msg.value("enabled", true);
    ApplyModules();
  } else if (type == "setSetting") {
    const std::string key = msg.value("key", "");
    if (key == "startWithWindows" || key == "minimizeToTray" || key == "autoUpdate" || key == "lowMemory" ||
        key == "instantPopup") {
      settings_.Root()[key] = msg.value("value", false);
      if (key == "startWithWindows") ApplyStartWithWindows();
    }
  } else if (type == "setModule") {
    const std::string id = msg.value("id", "");
    if (Module* m = FindModule(id); m && !m->AlwaysOn()) {
      settings_.Module(id)["enabled"] = msg.value("enabled", true);
      ApplyModules();
    }
  } else if (type == "moduleAction") {
    if (Module* m = FindModule(msg.value("id", ""))) {
      m->HandleAction(msg.value("action", ""), msg.value("payload", json::object()));
      SaveModule(*m);
    }
  } else if (type == "pickFile") {
    const std::wstring path = PickFile(hwnd_);
    PostToUi({{"type", "filePicked"}, {"requestId", msg.value("requestId", "")}, {"path", util::ToUtf8(path)}});
    return;
  } else if (type == "openSettingsPage") {
    const std::string page = msg.value("page", "");
    if (page == "location") {
      ShellExecuteW(nullptr, L"open", L"ms-settings:privacy-location", nullptr, nullptr, SW_SHOWNORMAL);
    }
    return;
  } else if (type == "ocrCapture") {
    StartOcr(true, msg.value("shot", false) ? 1 : 0);
    return;
  } else if (type == "colorPick") {
    StartOcr(true, 2);
    return;
  } else if (type == "openDataDir") {
    ShellExecuteW(nullptr, L"open", util::DataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return;
  } else if (type == "checkUpdate") {
    updater_->CheckAsync();
    return;
  } else if (type == "installUpdate") {
    updater_->InstallAsync();
    return;
  } else if (type == "restart") {
    if (updater_->ReadyToRestart()) RestartForUpdate();
    return;
  } else {
    return;
  }

  settings_.Save();
  PushState();
}

// ---------------------------------------------------------------- Modules

bool App::ModuleEnabled(const std::string& id) {
  if (Module* m = FindModule(id); m && m->AlwaysOn()) return true;
  return settings_.Get(settings_.Module(id), "enabled", true);
}

Module* App::FindModule(const std::string& id) {
  for (auto& m : modules_) {
    if (m->Id() == id) return m.get();
  }
  return nullptr;
}

void App::SaveModule(Module& m) {
  auto& section = settings_.Module(m.Id());
  const json cfg = m.SaveConfig();
  for (auto it = cfg.begin(); it != cfg.end(); ++it) section[it.key()] = it.value();
}

void App::ApplyModules() {
  const bool global = settings_.Get(settings_.Root(), "enabled", true);
  for (auto& m : modules_) {
    const bool want = m->AlwaysOn() || (global && ModuleEnabled(m->Id()));
    if (want && !m->Running()) m->Start();
    if (!want && m->Running()) m->Stop();
  }
  if (tray_.cbSize) {
    wcscpy_s(tray_.szTip, global ? L"ToolBox" : L"ToolBox (en pause)");
    Shell_NotifyIconW(NIM_MODIFY, &tray_);
  }
}

// ---------------------------------------------------------------- Barre des tâches

void App::AddTrayIcon() {
  tray_ = {};
  tray_.cbSize = sizeof(tray_);
  tray_.hWnd = hwnd_;
  tray_.uID = 1;
  tray_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  tray_.uCallbackMessage = WM_APP_TRAY;
  tray_.hIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
  wcscpy_s(tray_.szTip, L"ToolBox");
  Shell_NotifyIconW(NIM_ADD, &tray_);
  tray_.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIconW(NIM_SETVERSION, &tray_);
}

void App::RemoveTrayIcon() {
  if (tray_.cbSize) Shell_NotifyIconW(NIM_DELETE, &tray_);
  tray_.cbSize = 0;
}

void App::ShowTrayMenu() {
  HMENU menu = CreatePopupMenu();
  const bool enabled = settings_.Get(settings_.Root(), "enabled", true);
  AppendMenuW(menu, MF_STRING, kTrayOpen, L"Ouvrir ToolBox");
  AppendMenuW(menu, MF_STRING | (enabled ? MF_CHECKED : 0), kTrayToggle, L"Activé");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayQuit, L"Quitter");
  SetMenuDefaultItem(menu, kTrayOpen, FALSE);

  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(hwnd_);  // sinon le menu ne se ferme pas en cliquant ailleurs
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
  PostMessageW(hwnd_, WM_NULL, 0, 0);
  DestroyMenu(menu);
}

// ---------------------------------------------------------------- Fenêtre « presse-papiers »

bool App::PopupWanted() {
  for (const char* id : {"clipboard", "snippets"}) {
    if (Module* m = FindModule(id); m && m->Running()) return true;
  }
  return false;
}

void App::ShowPopup(bool snippets) {
  if (!env_) return;
  Module* owner = FindModule(snippets ? "snippets" : "clipboard");
  if (!owner || !owner->Running()) return;
  // Même raccourci une 2e fois : on ferme.
  if (popup_hwnd_ && IsWindowVisible(popup_hwnd_) && popup_snippets_ == snippets) {
    HidePopup();
    if (prev_foreground_) SetForegroundWindow(prev_foreground_);
    return;
  }
  popup_snippets_ = snippets;

  const HWND fg = GetForegroundWindow();
  if (fg != popup_hwnd_) prev_foreground_ = fg;

  PreparePopup();
  if (!popup_hwnd_) return;

  // Près de la souris, sans dépasser de l'écran.
  const UINT dpi = GetDpiForSystem();
  int w = MulDiv(400, dpi, 96), h = MulDiv(500, dpi, 96);
  if (snippets) {
    // Estimation (l'interface envoie ensuite la hauteur exacte) : en-tête, recherche si > 6, lignes, pied.
    const int n = static_cast<int>(static_cast<Snippets*>(owner)->Count());
    const int dip = 42 + (n > 6 ? 46 : 0) + std::max(n, 1) * 50 + 8 + 34;
    w = MulDiv(300, dpi, 96);
    h = MulDiv(std::min(dip, 460), dpi, 96);
  }
  POINT pt;
  GetCursorPos(&pt);
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
  const RECT& wa = mi.rcWork;
  const int x = std::max<int>(wa.left, std::min<int>(pt.x - w / 2, wa.right - w));
  const int y = std::max<int>(wa.top, std::min<int>(pt.y - MulDiv(40, dpi, 96), wa.bottom - h));
  SetWindowPos(popup_hwnd_, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
  util::ForceForeground(popup_hwnd_);
  if (popup_controller_) {
    popup_controller_->put_IsVisible(TRUE);
    popup_controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
  }
  PushState();
  if (popup_webview_) {
    popup_webview_->PostWebMessageAsJson(snippets ? L"{\"type\":\"popupShown\",\"mode\":\"snippets\"}"
                                                  : L"{\"type\":\"popupShown\",\"mode\":\"clipboard\"}");
  }
}

void App::PreparePopup() {
  if (popup_hwnd_ || !env_) return;
  // Au démarrage, seulement si l'option « fenêtre rapide instantanée » est active.
  if (!PopupWanted()) return;

  const UINT dpi = GetDpiForSystem();
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = PopupProc;
  wc.hInstance = instance_;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = CreateSolidBrush(RGB(32, 32, 32));
  wc.lpszClassName = kPopupClass;
  RegisterClassExW(&wc);
  popup_hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kPopupClass, L"ToolBox", WS_POPUP | WS_BORDER,
                                0, 0, MulDiv(400, dpi, 96), MulDiv(500, dpi, 96), nullptr, nullptr, instance_, this);
  const int round = 2;  // DWMWCP_ROUND : coins arrondis sous Windows 11
  DwmSetWindowAttribute(popup_hwnd_, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &round, sizeof(round));
  BOOL dark = TRUE;
  DwmSetWindowAttribute(popup_hwnd_, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
  CreatePopupWebView();
}

void App::HidePopup() {
  if (!popup_hwnd_ || !IsWindowVisible(popup_hwnd_)) return;
  ShowWindow(popup_hwnd_, SW_HIDE);
}

void App::CreatePopupWebView() {
  env_->CreateCoreWebView2Controller(
      popup_hwnd_,
      Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
            if (FAILED(result) || !controller) return result;
            popup_controller_ = controller;
            popup_controller_->get_CoreWebView2(&popup_webview_);
            ComPtr<ICoreWebView2Controller2> c2;
            if (SUCCEEDED(popup_controller_.As(&c2))) c2->put_DefaultBackgroundColor({255, 32, 32, 32});
            ComPtr<ICoreWebView2Settings> s;
            popup_webview_->get_Settings(&s);
            s->put_IsStatusBarEnabled(FALSE);
            s->put_IsZoomControlEnabled(FALSE);
#ifdef NDEBUG
            s->put_AreDevToolsEnabled(FALSE);
            s->put_AreDefaultContextMenusEnabled(FALSE);
#endif
            popup_webview_->AddScriptToExecuteOnDocumentCreated(L"window.TOOLBOX_POPUP = true;", nullptr);
            popup_webview_->add_WebMessageReceived(
                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                    [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
                      LPWSTR raw = nullptr;
                      if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
                        json msg = json::parse(util::ToUtf8(raw), nullptr, false);
                        CoTaskMemFree(raw);
                        if (msg.is_object()) {
                        try {
                          OnPopupMessage(msg);
                        } catch (const std::exception& e) {
                          util::LogError(std::string("fenêtre rapide : ") + e.what());
                        }
                      }
                      }
                      return S_OK;
                    })
                    .Get(),
                nullptr);
            RECT bounds;
            GetClientRect(popup_hwnd_, &bounds);
            popup_controller_->put_Bounds(bounds);
            popup_controller_->put_IsVisible(IsWindowVisible(popup_hwnd_));
            popup_controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            popup_webview_->NavigateToString(LoadUiHtml(instance_).c_str());
            return S_OK;
          })
          .Get());
}

void App::OnPopupMessage(const json& msg) {
  const std::string type = msg.value("type", "");
  if (type == "popupClose") {
    HidePopup();
    if (prev_foreground_) SetForegroundWindow(prev_foreground_);
  } else if (type == "clipPaste") {
    auto* clip = static_cast<ClipboardHistory*>(FindModule("clipboard"));
    if (!clip || !clip->CopyItem(msg.value("id", 0ULL))) return;
    popup_pasting_ = true;
    HidePopup();
    popup_pasting_ = false;
    if (prev_foreground_) SetForegroundWindow(prev_foreground_);
    // Petit délai : la fenêtre d'origine doit reprendre le focus avant le Ctrl + V.
    if (clip->AutoPaste()) SetTimer(popup_hwnd_, 1, 80, nullptr);
  } else if (type == "snipPaste") {
    auto* snip = static_cast<Snippets*>(FindModule("snippets"));
    if (!snip) return;
    const std::wstring text = snip->Use(msg.value("id", ""));
    if (text.empty()) return;
    // On garde le presse-papiers actuel pour le remettre après avoir collé.
    clip_to_restore_ = snip->AutoPaste() ? util::GetClipboardText(hwnd_) : std::wstring();
    if (!util::SetClipboardText(hwnd_, text, /*hidden=*/snip->AutoPaste())) return;
    clip_restore_seq_ = GetClipboardSequenceNumber();
    popup_pasting_ = true;
    HidePopup();
    popup_pasting_ = false;
    if (prev_foreground_) SetForegroundWindow(prev_foreground_);
    if (snip->AutoPaste()) SetTimer(popup_hwnd_, 1, 80, nullptr);
  } else if (type == "popupSize") {
    // Hauteur exacte des textes rapides (px CSS), sans dépasser l'écran.
    if (!popup_snippets_ || !popup_hwnd_) return;
    const UINT dpi = GetDpiForWindow(popup_hwnd_);
    RECT r;
    GetWindowRect(popup_hwnd_, &r);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(popup_hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
    const int h = std::min<int>(MulDiv(std::clamp(msg.value("h", 300), 120, 460), dpi, 96) + 2,
                                mi.rcWork.bottom - mi.rcWork.top);
    const int y = std::min<int>(r.top, mi.rcWork.bottom - h);
    SetWindowPos(popup_hwnd_, nullptr, r.left, y, r.right - r.left, h, SWP_NOZORDER | SWP_NOACTIVATE);
  } else if (type == "openMain") {
    HidePopup();
    ShowMainWindow();
  } else {
    OnWebMessage(msg);  // "ready", "moduleAction" (épingler, supprimer)…
    // Fenêtre créée au moment du raccourci : elle doit savoir quoi afficher.
    if (type == "ready" && popup_webview_ && IsWindowVisible(popup_hwnd_)) {
      popup_webview_->PostWebMessageAsJson(popup_snippets_ ? L"{\"type\":\"popupShown\",\"mode\":\"snippets\"}"
                                                           : L"{\"type\":\"popupShown\",\"mode\":\"clipboard\"}");
    }
  }
}

void App::PasteIntoPreviousWindow() {
  INPUT in[4] = {};
  for (auto& i : in) i.type = INPUT_KEYBOARD;
  in[0].ki.wVk = VK_CONTROL;
  in[1].ki.wVk = 'V';
  in[2].ki.wVk = 'V';
  in[2].ki.dwFlags = KEYEVENTF_KEYUP;
  in[3].ki.wVk = VK_CONTROL;
  in[3].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(4, in, sizeof(INPUT));
}

LRESULT CALLBACK App::PopupProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  App* self = nullptr;
  if (msg == WM_NCCREATE) {
    self = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self) {
    switch (msg) {
      case WM_ACTIVATE:
        // Clic ailleurs : on ferme, comme le menu Win + V.
        if (LOWORD(wparam) == WA_INACTIVE && !self->popup_pasting_) ShowWindow(hwnd, SW_HIDE);
        return 0;
      case WM_SIZE:
        if (self->popup_controller_) {
          RECT r;
          GetClientRect(hwnd, &r);
          self->popup_controller_->put_Bounds(r);
        }
        return 0;
      case WM_TIMER:
        KillTimer(hwnd, wparam);
        if (wparam == 1) {
          self->PasteIntoPreviousWindow();
          if (!self->clip_to_restore_.empty()) SetTimer(hwnd, 2, 700, nullptr);
        } else if (wparam == 2) {
          // Remet ce qui était copié avant, si rien d'autre n'a été copié entre-temps.
          if (GetClipboardSequenceNumber() == self->clip_restore_seq_) {
            util::SetClipboardText(self->hwnd_, self->clip_to_restore_, /*hidden=*/true);
          }
          self->clip_to_restore_.clear();
        }
        return 0;
      case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// ---------------------------------------------------------------- Texte à l'écran (OCR)

void App::StartOcr(bool from_ui, int mode) {
  Module* m = FindModule(mode == 2 ? "color" : "ocr");
  if (!m || !m->Running()) return;
  capture_mode_ = mode;
  if (from_ui && IsWindowVisible(hwnd_)) {
    // Bouton dans ToolBox : on cache la fenêtre pour qu'elle ne soit pas sur la photo.
    ocr_restore_main_ = true;
    HideMainWindow();
    SetTimer(hwnd_, kTimerOcr, 250, nullptr);
    return;
  }
  BeginOcrNow();
}

void App::BeginOcrNow() {
  auto after = [this] {
    if (ocr_restore_main_) {
      ocr_restore_main_ = false;
      ShowMainWindow();
    }
  };
  if (capture_mode_ == 2) {
    if (auto* color = static_cast<ColorPickerModule*>(FindModule("color"))) color->BeginPick(after);
    return;
  }
  if (auto* ocr = static_cast<ScreenOcr*>(FindModule("ocr"))) ocr->BeginCapture(capture_mode_ == 1, after);
}

void App::Notify(const std::string& title, const std::string& text) {
  if (!tray_.cbSize) return;
  NOTIFYICONDATAW n = tray_;
  n.uFlags = NIF_INFO;
  n.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
  wcsncpy_s(n.szInfoTitle, util::FromUtf8(title).c_str(), _TRUNCATE);
  wcsncpy_s(n.szInfo, util::FromUtf8(text).c_str(), _TRUNCATE);
  Shell_NotifyIconW(NIM_MODIFY, &n);
}

// ---------------------------------------------------------------- Démarrage / mises à jour

void App::ApplyStartWithWindows() {
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
  if (settings_.Get(settings_.Root(), "startWithWindows", false)) {
    const std::wstring cmd = L"\"" + util::ExePath().wstring() + L"\" --tray";
    RegSetValueExW(key, L"ToolBox", 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                   static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(key, L"ToolBox");
  }
  RegCloseKey(key);
}

void App::MaybeAutoUpdate() {
  if (!settings_.Get(settings_.Root(), "autoUpdate", true)) return;
  const std::string status = updater_->State().value("status", "");
  if (status == "available") {
    updater_->InstallAsync();
  } else if (status == "ready" && !IsWindowVisible(hwnd_)) {
    RestartForUpdate();  // fenêtre cachée : on redémarre en silence
  }
}

void App::RestartForUpdate() {
  std::wstring cmd = L"\"" + util::ExePath().wstring() + L"\" --updated";
  if (!IsWindowVisible(hwnd_)) cmd += L" --tray";
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  ReleaseSingleInstance();  // la nouvelle version démarre sans attendre notre fermeture
  if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Quit();
  }
}
