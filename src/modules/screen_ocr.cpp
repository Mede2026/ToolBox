#include "modules/screen_ocr.h"

#include <shellapi.h>
#include <shlobj.h>
#include <tesseract/baseapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>

#include "http.h"
#include "screen_select.h"
#include "util.h"

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

constexpr ULONGLONG kUnloadAfterMs = 60 * 1000;  // libère la mémoire après 1 min sans lecture
constexpr size_t kMaxHistory = 30;

// Modèles « rapides » de Tesseract, figés à un commit précis et vérifiés par SHA-256.
constexpr char kTessdataBase[] =
    "https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/87416418657359cb625c412a48b6e1d6d41c29bd/";

struct LangInfo {
  const char* code;
  const char* name;
  size_t size;
  const char* sha256;
};

constexpr LangInfo kLangs[] = {
    {"fra", "Français", 1130365, "ced037562e8c80c13122dece28dd477d399af80911a28791a66a63ac1e3445ca"},
    {"eng", "Anglais", 4113088, "7d4322bd2a7749724879683fc3912cb542f19906c83bcc1a52132556427170b2"},
    {"spa", "Espagnol", 2294433, "6f2e04d02774a18f01bed44b1111f2cd7f3ba7ac9dc4373cd3f898a40ea6b464"},
    {"deu", "Allemand", 1525436, "19d219bbb6672c869d20a9636c6816a81eb9a71796cb93ebe0cb1530e2cdb22d"},
    {"ita", "Italien", 2701314, "b8f89e1e785118dac4d51ae042c029a64edb5c3ee42ef73027a6d412748d8827"},
    {"por", "Portugais", 1982756, "c4932b937207a9514b7514d518b931a99938c02a28a5a5a553f8599ed58b7deb"},
};

const LangInfo* FindLang(const std::string& code) {
  for (const auto& l : kLangs) {
    if (code == l.code) return &l;
  }
  return nullptr;
}

fs::path TessdataDir() {
  fs::path dir = util::DataDir() / L"tessdata";
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir;
}

fs::path LangFile(const std::string& code) { return TessdataDir() / util::FromUtf8(code + ".traineddata"); }

long long NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Lecteur de fichiers pour Tesseract : chemins UTF-8 (le dossier peut contenir « Médéric »).
bool ReadFileUtf8(const char* filename, std::vector<char>* data) {
  FILE* f = _wfopen(util::FromUtf8(filename).c_str(), L"rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  const long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  data->resize(size > 0 ? size : 0);
  const bool ok = size > 0 && fread(data->data(), 1, data->size(), f) == data->size();
  fclose(f);
  return ok;
}

bool SetClipboardText(HWND hwnd, const std::wstring& text) {
  for (int i = 0; i < 10 && !OpenClipboard(hwnd); ++i) Sleep(20);
  EmptyClipboard();
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  bool ok = false;
  if (mem) {
    memcpy(GlobalLock(mem), text.c_str(), bytes);
    GlobalUnlock(mem);
    ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
    if (!ok) GlobalFree(mem);
  }
  CloseClipboard();
  return ok;
}

// Prépare l'image pour Tesseract : niveaux de gris, texte foncé sur fond clair,
// agrandie (le texte d'écran est petit), avec une marge blanche.
std::vector<uint8_t> Preprocess(const std::vector<uint8_t>& bgra, int w, int h, int& out_w, int& out_h) {
  std::vector<uint8_t> gray(size_t(w) * h);
  unsigned long long sum = 0;
  for (size_t i = 0; i < gray.size(); ++i) {
    const uint8_t b = bgra[i * 4], g = bgra[i * 4 + 1], r = bgra[i * 4 + 2];
    gray[i] = static_cast<uint8_t>((r * 299 + g * 587 + b * 114) / 1000);
    sum += gray[i];
  }
  if (sum / gray.size() < 120) {  // thème sombre : on inverse
    for (auto& v : gray) v = static_cast<uint8_t>(255 - v);
  }

  int scale = h < 40 ? 4 : h < 150 ? 3 : 2;
  while (scale > 1 && double(w) * h * scale * scale > 40e6) --scale;
  const int pad = 12 * scale;
  out_w = w * scale + pad * 2;
  out_h = h * scale + pad * 2;
  std::vector<uint8_t> out(size_t(out_w) * out_h, 255);
  for (int y = 0; y < h * scale; ++y) {
    const double sy = std::clamp((y + 0.5) / scale - 0.5, 0.0, double(h - 1));
    const int y0 = static_cast<int>(sy), y1 = std::min(h - 1, y0 + 1);
    const double fy = sy - y0;
    uint8_t* row = &out[size_t(y + pad) * out_w + pad];
    for (int x = 0; x < w * scale; ++x) {
      const double sx = std::clamp((x + 0.5) / scale - 0.5, 0.0, double(w - 1));
      const int x0 = static_cast<int>(sx), x1 = std::min(w - 1, x0 + 1);
      const double fx = sx - x0;
      const double top = gray[size_t(y0) * w + x0] * (1 - fx) + gray[size_t(y0) * w + x1] * fx;
      const double bot = gray[size_t(y1) * w + x0] * (1 - fx) + gray[size_t(y1) * w + x1] * fx;
      row[x] = static_cast<uint8_t>(top * (1 - fy) + bot * fy + 0.5);
    }
  }
  return out;
}

std::string CleanText(std::string text, bool single_line) {
  // Tesseract termine souvent par des retours à la ligne ou un saut de page.
  text.erase(std::remove(text.begin(), text.end(), '\f'), text.end());
  text = std::regex_replace(text, std::regex("[ \\t]+\\n"), "\n");
  text = std::regex_replace(text, std::regex("\\n{3,}"), "\n\n");
  if (single_line) {
    text = std::regex_replace(text, std::regex("\\s*\\n+\\s*"), " ");
    text = std::regex_replace(text, std::regex(" {2,}"), " ");
  }
  const auto first = text.find_first_not_of(" \n\t");
  const auto last = text.find_last_not_of(" \n\t");
  return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

struct OcrResult {
  bool ok;
  std::string text;
  std::string error;
  int ms;
  bool shot = false;  // capture d'écran (pas de lecture de texte)
  std::string path;
  int w = 0, h = 0;
};

constexpr size_t kMaxShots = 30;

// Images\Captures ToolBox (créé au besoin).
fs::path ShotsDir() {
  PWSTR raw = nullptr;
  fs::path dir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &raw))) dir = fs::path(raw) / L"Captures ToolBox";
  else dir = util::DataDir() / L"Captures";
  CoTaskMemFree(raw);
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir;
}

// Copie une image (BGRA, lignes de haut en bas) dans le presse-papiers (format CF_DIB).
bool SetClipboardImage(HWND hwnd, const std::vector<uint8_t>& bgra, int w, int h) {
  const size_t stride = size_t(w) * 4, bytes = stride * h;
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + bytes);
  if (!mem) return false;
  auto* p = static_cast<uint8_t*>(GlobalLock(mem));
  BITMAPINFOHEADER bih{};
  bih.biSize = sizeof(bih);
  bih.biWidth = w;
  bih.biHeight = h;  // DIB classique : lignes de bas en haut
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = static_cast<DWORD>(bytes);
  memcpy(p, &bih, sizeof(bih));
  for (int y = 0; y < h; ++y) memcpy(p + sizeof(bih) + size_t(h - 1 - y) * stride, &bgra[size_t(y) * stride], stride);
  GlobalUnlock(mem);
  for (int i = 0; i < 10 && !OpenClipboard(hwnd); ++i) Sleep(20);
  EmptyClipboard();
  const bool ok = SetClipboardData(CF_DIB, mem) != nullptr;
  if (!ok) GlobalFree(mem);
  CloseClipboard();
  return ok;
}

// Enregistre en PNG avec WIC (Windows Imaging Component). Appelé sur un fil de travail.
bool SavePng(const fs::path& path, const std::vector<uint8_t>& bgra, int w, int h) {
  using Microsoft::WRL::ComPtr;
  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICStream> stream;
  ComPtr<IWICBitmapEncoder> encoder;
  ComPtr<IWICBitmapFrameEncode> frame;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
      FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) ||
      FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
      FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
      FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)) ||
      FAILED(frame->SetSize(w, h))) {
    return false;
  }
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  if (FAILED(frame->SetPixelFormat(&format)) || format != GUID_WICPixelFormat32bppBGRA) return false;
  return SUCCEEDED(frame->WritePixels(h, w * 4, static_cast<UINT>(bgra.size()), const_cast<BYTE*>(bgra.data()))) &&
         SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}

std::string ShotFileName() {
  SYSTEMTIME t;
  GetLocalTime(&t);
  char name[64];
  snprintf(name, sizeof(name), "Capture %04d-%02d-%02d %02dh%02d %02d.png", t.wYear, t.wMonth, t.wDay, t.wHour,
           t.wMinute, t.wSecond);
  return name;
}

}  // namespace

ScreenOcr::ScreenOcr(HWND hwnd, HINSTANCE instance, std::function<void()> on_change,
                     std::function<void(bool)> on_hotkey, Notify notify)
    : hwnd_(hwnd),
      instance_(instance),
      on_change_(std::move(on_change)),
      on_hotkey_(std::move(on_hotkey)),
      notify_(std::move(notify)),
      hotkey_(hwnd, 0x4202, MOD_CONTROL | MOD_ALT, 'T', "Ctrl + Alt + T"),
      shot_hotkey_(hwnd, 0x4203, MOD_CONTROL | MOD_ALT, 'S', "Ctrl + Alt + S", "shot") {}

ScreenOcr::~ScreenOcr() { Stop(); }

void ScreenOcr::Start() {
  if (running_) return;
  running_ = true;
  hotkey_.Register();
  shot_hotkey_.Register();
}

void ScreenOcr::Stop() {
  hotkey_.Unregister();
  shot_hotkey_.Unregister();
  running_ = false;
  if (!busy_) {
    std::lock_guard lock(engine_mu_);
    ReleaseEngine();
  }
}

void ScreenOcr::LoadConfig(const json& cfg) {
  hotkey_.Load(cfg);
  shot_hotkey_.Load(cfg);
  shot_save_ = cfg.value("shotSave", true);
  shots_.clear();
  for (const auto& e : cfg.value("shots", json::array())) {
    if (e.is_object()) {
      shots_.push_back({next_id_++, e.value("path", ""), e.value("time", 0LL), e.value("w", 0), e.value("h", 0)});
    }
  }
  single_line_ = cfg.value("singleLine", false);
  notify_enabled_ = cfg.value("notify", true);
  if (auto it = cfg.find("langs"); it != cfg.end() && it->is_array()) {
    langs_.clear();
    for (const auto& l : *it) {
      if (l.is_string() && FindLang(l.get<std::string>())) langs_.push_back(l.get<std::string>());
    }
    if (langs_.empty()) langs_ = {"fra", "eng"};
  }
  history_.clear();
  for (const auto& e : cfg.value("history", json::array())) {
    if (e.is_object()) history_.push_back({next_id_++, e.value("text", ""), e.value("time", 0LL), e.value("ms", 0)});
  }
}

json ScreenOcr::SaveConfig() const {
  json history = json::array();
  for (const auto& e : history_) history.push_back({{"text", e.text}, {"time", e.time}, {"ms", e.ms}});
  json shots = json::array();
  for (const auto& e : shots_) shots.push_back({{"path", e.path}, {"time", e.time}, {"w", e.w}, {"h", e.h}});
  json cfg{{"singleLine", single_line_}, {"notify", notify_enabled_}, {"langs", langs_}, {"history", history},
           {"shotSave", shot_save_}, {"shots", shots}};
  hotkey_.Save(cfg);
  shot_hotkey_.Save(cfg);
  return cfg;
}

json ScreenOcr::State() const {
  json langs = json::array();
  for (const auto& l : kLangs) {
    std::error_code ec;
    langs.push_back({{"code", l.code},
                     {"name", l.name},
                     {"size", l.size},
                     {"installed", fs::exists(LangFile(l.code), ec)},
                     {"selected", std::find(langs_.begin(), langs_.end(), l.code) != langs_.end()}});
  }
  json history = json::array();
  for (const auto& e : history_) history.push_back({{"id", e.id}, {"text", e.text}, {"time", e.time}, {"ms", e.ms}});
  json s{{"singleLine", single_line_}, {"notify", notify_enabled_}, {"langs", langs}, {"history", history}};
  {
    std::lock_guard lock(status_mu_);
    s["status"] = status_;
    s["detail"] = detail_;
  }
  s["engineLoaded"] = last_use_ != 0;
  json shots = json::array();
  for (const auto& e : shots_) {
    std::error_code ec;
    shots.push_back({{"id", e.id}, {"path", e.path}, {"time", e.time}, {"w", e.w}, {"h", e.h},
                     {"exists", !e.path.empty() && fs::exists(util::FromUtf8(e.path), ec)}});
  }
  s["shots"] = shots;
  s["shotSave"] = shot_save_;
  s["shotFolder"] = util::ToUtf8(ShotsDir().wstring());
  hotkey_.AddState(s);
  shot_hotkey_.AddState(s);
  return s;
}

void ScreenOcr::SetStatus(const std::string& status, const std::string& detail) {
  {
    std::lock_guard lock(status_mu_);
    status_ = status;
    detail_ = detail;
  }
  if (on_change_) on_change_();  // PostMessage : sûr depuis un autre fil
}

std::string ScreenOcr::LangString(const std::vector<std::string>& langs) {
  std::string s;
  for (const auto& l : langs) s += (s.empty() ? "" : "+") + l;
  return s;
}

void ScreenOcr::HandleAction(const std::string& action, const json& payload) {
  if (hotkey_.HandleAction(action, payload, running_)) return;
  if (shot_hotkey_.HandleAction(action, payload, running_)) return;

  const auto find_shot = [&]() -> const Shot* {
    const auto id = payload.value("id", 0ULL);
    for (const auto& e : shots_) {
      if (e.id == id) return &e;
    }
    return nullptr;
  };
  if (action == "setShotSave") {
    shot_save_ = payload.value("value", true);
    return;
  } else if (action == "openShotFolder") {
    ShellExecuteW(nullptr, L"open", ShotsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return;
  } else if (action == "openShot") {
    if (const Shot* e = find_shot(); e && !e->path.empty()) {
      ShellExecuteW(nullptr, L"open", util::FromUtf8(e->path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    return;
  } else if (action == "showShot") {  // ouvre l'Explorateur avec le fichier sélectionné
    if (const Shot* e = find_shot(); e && !e->path.empty()) {
      const std::wstring args = L"/select,\"" + util::FromUtf8(e->path) + L"\"";
      ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
    return;
  } else if (action == "deleteShot") {  // retire de la liste (le fichier reste)
    const auto id = payload.value("id", 0ULL);
    std::erase_if(shots_, [&](const Shot& e) { return e.id == id; });
    return;
  } else if (action == "clearShots") {
    shots_.clear();
    return;
  }

  if (action == "setLangs") {
    std::vector<std::string> langs;
    for (const auto& l : payload.value("list", json::array())) {
      if (l.is_string() && FindLang(l.get<std::string>())) langs.push_back(l.get<std::string>());
    }
    if (!langs.empty()) langs_ = std::move(langs);
  } else if (action == "downloadLangs") {
    DownloadAsync();
  } else if (action == "deleteLang") {
    const std::string code = payload.value("code", "");
    if (FindLang(code) && !busy_) {
      std::lock_guard lock(engine_mu_);
      ReleaseEngine();
      std::error_code ec;
      fs::remove(LangFile(code), ec);
    }
  } else if (action == "setSingleLine") {
    single_line_ = payload.value("value", false);
  } else if (action == "setNotify") {
    notify_enabled_ = payload.value("value", true);
  } else if (action == "copy") {
    const auto id = payload.value("id", 0ULL);
    for (const auto& e : history_) {
      if (e.id == id) SetClipboardText(hwnd_, util::FromUtf8(e.text));
    }
  } else if (action == "delete") {
    const auto id = payload.value("id", 0ULL);
    std::erase_if(history_, [&](const Entry& e) { return e.id == id; });
  } else if (action == "clear") {
    history_.clear();
  }
}

void ScreenOcr::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_TOOLBOX_OCR_DONE) {
    std::unique_ptr<OcrResult> r(reinterpret_cast<OcrResult*>(lparam));
    if (r->shot) {
      shots_.push_front({next_id_++, r->path, NowSeconds(), r->w, r->h});
      while (shots_.size() > kMaxShots) shots_.pop_back();
      Emit("screenshot");
      if (notify_ && notify_enabled_) {
        notify_(r->ok ? "Capture copiée" : "Capture copiée (non enregistrée)",
                r->ok ? (r->path.empty() ? "Colle-la avec Ctrl + V." : "Enregistrée dans Images\\Captures ToolBox.")
                      : r->error);
      }
      if (on_change_) on_change_();
      return;
    }
    if (!r->ok) {
      SetStatus("error", r->error);
      if (notify_ && notify_enabled_) notify_("Texte à l'écran", r->error);
      return;
    }
    Emit("ocr");
    SetClipboardText(hwnd_, util::FromUtf8(r->text));
    history_.push_front({next_id_++, r->text, NowSeconds(), r->ms});
    while (history_.size() > kMaxHistory) history_.pop_back();
    SetStatus("idle");
    if (notify_ && notify_enabled_) {
      std::wstring preview = util::FromUtf8(r->text);
      if (preview.size() > 180) preview = preview.substr(0, 180) + L"…";
      notify_("Texte copié", util::ToUtf8(preview));
    }
    return;
  }
  if (running_ && hotkey_.Matches(msg, wparam) && on_hotkey_) on_hotkey_(false);
  if (running_ && shot_hotkey_.Matches(msg, wparam) && on_hotkey_) on_hotkey_(true);
}

void ScreenOcr::Tick() {
  // Libère Tesseract (~50 Mo) quand on ne s'en sert plus.
  const ULONGLONG last = last_use_;
  if (last && !busy_ && GetTickCount64() - last > kUnloadAfterMs) {
    std::lock_guard lock(engine_mu_);
    ReleaseEngine();
    if (on_change_) on_change_();
  }
}

void ScreenOcr::ReleaseEngine() {
  if (api_) {
    api_->End();
    api_.reset();
  }
  loaded_langs_.clear();
  last_use_ = 0;
}

void ScreenOcr::BeginCapture(bool screenshot, std::function<void()> after_select) {
  if (!running_ || (!screenshot && busy_)) {
    if (after_select) after_select();
    return;
  }
  const std::wstring hint = screenshot ? L"Sélectionne la zone à capturer" : L"Sélectionne le texte à lire";
  const bool started = ScreenSelector::Start(
      instance_, hint, [this, after_select, screenshot](bool ok, std::vector<uint8_t> px, int w, int h) {
        if (after_select) after_select();
        if (!ok) return;
        if (screenshot) SaveScreenshot(std::move(px), w, h);
        else RecognizeAsync(std::move(px), w, h);
      });
  if (!started && after_select) after_select();
}

void ScreenOcr::SaveScreenshot(std::vector<uint8_t> bgra, int w, int h) {
  for (size_t i = 3; i < bgra.size(); i += 4) bgra[i] = 255;  // la capture GDI n'a pas de transparence
  SetClipboardImage(hwnd_, bgra, w, h);
  if (!shot_save_) {
    PostMessageW(hwnd_, WM_TOOLBOX_OCR_DONE, 0,
                 reinterpret_cast<LPARAM>(new OcrResult{true, {}, {}, 0, true, {}, w, h}));
    return;
  }
  std::thread([this, bgra = std::move(bgra), w, h] {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const fs::path file = ShotsDir() / util::FromUtf8(ShotFileName());
    const bool ok = SavePng(file, bgra, w, h);
    CoUninitialize();
    auto* r = new OcrResult{ok, {}, ok ? std::string() : "Impossible d'enregistrer le fichier PNG.", 0, true,
                            ok ? util::ToUtf8(file.wstring()) : std::string(), w, h};
    PostMessageW(hwnd_, WM_TOOLBOX_OCR_DONE, 0, reinterpret_cast<LPARAM>(r));
  }).detach();
}

void ScreenOcr::RecognizeAsync(std::vector<uint8_t> bgra, int w, int h) {
  if (busy_.exchange(true)) return;
  SetStatus("reading");
  std::thread([this, bgra = std::move(bgra), w, h, langs = langs_, single_line = single_line_] {
    auto* result = new OcrResult{false, {}, {}, 0};
    const auto t0 = std::chrono::steady_clock::now();
    std::string err;
    if (EnsureLanguages(langs, err)) {
      SetStatus("reading");
      result->text = Recognize(bgra, w, h, langs, single_line, err);
      result->ok = err.empty();
      if (result->ok && result->text.empty()) {
        result->ok = false;
        err = "Aucun texte trouvé dans la zone choisie.";
      }
    }
    result->error = err;
    result->ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
    busy_ = false;
    PostMessageW(hwnd_, WM_TOOLBOX_OCR_DONE, 0, reinterpret_cast<LPARAM>(result));
  }).detach();
}

void ScreenOcr::DownloadAsync() {
  if (busy_.exchange(true)) return;
  std::thread([this, langs = langs_] {
    std::string err;
    const bool ok = EnsureLanguages(langs, err);
    busy_ = false;
    SetStatus(ok ? "idle" : "error", err);
  }).detach();
}

bool ScreenOcr::EnsureLanguages(const std::vector<std::string>& langs, std::string& err) {
  for (const auto& code : langs) {
    const LangInfo* info = FindLang(code);
    const fs::path file = LangFile(code);
    std::error_code ec;
    if (!info || fs::exists(file, ec)) continue;

    SetStatus("downloading", std::string(info->name) + " · 0 %");
    std::string data;
    int last = -1;
    const bool ok = net::HttpGet(std::string(kTessdataBase) + code + ".traineddata", data, err,
                                 [&](size_t done, size_t total) {
                                   const int pct = total ? int(done * 100 / total) : 0;
                                   if (pct / 5 != last) {
                                     last = pct / 5;
                                     SetStatus("downloading", std::string(info->name) + " · " +
                                                                  std::to_string(pct) + " %");
                                   }
                                 });
    if (!ok) {
      err = "Téléchargement de la langue « " + std::string(info->name) + " » impossible : " + err;
      return false;
    }
    if (data.size() != info->size || net::Sha256Hex(data) != info->sha256) {
      err = "Fichier de langue « " + std::string(info->name) + " » corrompu. Réessaie.";
      return false;
    }
    fs::path tmp = file;
    tmp += L".tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out.write(data.data(), static_cast<std::streamsize>(data.size()));
      if (!out) {
        err = "Impossible d'enregistrer la langue « " + std::string(info->name) + " ».";
        return false;
      }
    }
    fs::rename(tmp, file, ec);
    if (ec) {
      err = "Impossible d'enregistrer la langue « " + std::string(info->name) + " ».";
      return false;
    }
  }
  return true;
}

std::string ScreenOcr::Recognize(const std::vector<uint8_t>& bgra, int w, int h,
                                 const std::vector<std::string>& lang_list, bool single_line, std::string& err) {
  std::lock_guard lock(engine_mu_);
  const std::string langs = LangString(lang_list);
  if (!api_ || loaded_langs_ != langs) {
    ReleaseEngine();
    api_ = std::make_unique<tesseract::TessBaseAPI>();
    const std::string datapath = util::ToUtf8(TessdataDir().wstring());
    // Init « avec lecteur » : Tesseract lit les fichiers via ReadFileUtf8 (chemins Unicode).
    if (api_->Init(datapath.c_str(), 0, langs.c_str(), tesseract::OEM_LSTM_ONLY, nullptr, 0, nullptr, nullptr, false,
                   &ReadFileUtf8) != 0) {
      api_.reset();
      err = "Impossible de démarrer Tesseract (" + langs + ").";
      return {};
    }
    api_->SetVariable("preserve_interword_spaces", "1");
    loaded_langs_ = langs;
  }
  last_use_ = GetTickCount64();

  int pw = 0, ph = 0;
  const std::vector<uint8_t> img = Preprocess(bgra, w, h, pw, ph);

  auto run = [&](tesseract::PageSegMode mode) {
    api_->SetPageSegMode(mode);
    api_->SetImage(img.data(), pw, ph, 1, pw);
    api_->SetSourceResolution(300);
    std::unique_ptr<char[]> text(api_->GetUTF8Text());
    return CleanText(text ? std::string(text.get()) : std::string(), single_line);
  };
  // Petite zone (une ligne) : mode « une ligne », sinon détection automatique des blocs.
  std::string text = run(h < 45 ? tesseract::PSM_SINGLE_LINE : tesseract::PSM_AUTO);
  if (text.empty()) text = run(tesseract::PSM_SINGLE_BLOCK);
  api_->Clear();
  last_use_ = GetTickCount64();
  return text;
}
