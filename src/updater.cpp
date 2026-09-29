#include "updater.h"

#include <array>
#include <cstring>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>

#include "http.h"
#include "util.h"
#include "version.h"

using nlohmann::json;
namespace fs = std::filesystem;

namespace {


std::array<int, 3> ParseVersion(std::string v) {
  if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(0, 1);
  std::array<int, 3> out{0, 0, 0};
  size_t i = 0;
  for (int part = 0; part < 3 && i < v.size(); ++part) {
    int n = 0;
    while (i < v.size() && std::isdigit(static_cast<unsigned char>(v[i]))) n = n * 10 + (v[i++] - '0');
    out[part] = n;
    if (i < v.size() && v[i] == '.') ++i; else break;
  }
  return out;
}


fs::path OldExePath() { return util::ExePath().parent_path() / L"ToolBox.old.exe"; }

}  // namespace

Updater::Updater(HWND notify_hwnd, UINT notify_msg) : hwnd_(notify_hwnd), msg_(notify_msg) {}

void Updater::CleanupOldExe() {
  std::error_code ec;
  for (int i = 0; i < 20 && fs::exists(OldExePath(), ec); ++i) {
    if (fs::remove(OldExePath(), ec)) break;
    Sleep(250);  // l'ancienne version est peut-être encore en train de se fermer
  }
}

json Updater::State() const {
  std::lock_guard lock(mu_);
  return json{
      {"current", util::ToUtf8(TOOLBOX_VERSION_STR)},
      {"status", status_},
      {"error", error_},
      {"latest", latest_},
      {"notes", notes_},
      {"progress", progress_},
      {"lastCheck", last_check_},
  };
}

void Updater::SetState(const std::string& status, const std::string& error) {
  {
    std::lock_guard lock(mu_);
    status_ = status;
    error_ = error;
  }
  PostMessageW(hwnd_, msg_, 0, 0);
}

void Updater::CheckAsync() {
  if (busy_.exchange(true)) return;
  SetState("checking");
  std::thread([this] {
    DoCheck();
    busy_ = false;
  }).detach();
}

void Updater::InstallAsync() {
  {
    std::lock_guard lock(mu_);
    if (exe_url_.empty()) return;
  }
  if (busy_.exchange(true)) return;
  SetState("downloading");
  std::thread([this] {
    DoInstall();
    busy_ = false;
  }).detach();
}

void Updater::DoCheck() {
  std::string body, err;
  const std::string api = "https://api.github.com/repos/" + util::ToUtf8(TOOLBOX_REPO) + "/releases/latest";
  const bool ok = net::HttpGet(api, body, err);
  {
    std::lock_guard lock(mu_);
    last_check_ = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
  }
  if (!ok) {
    SetState("error", err == "Réponse HTTP 404" ? "Aucune version publiée pour l'instant" : err);
    return;
  }

  json rel = json::parse(body, nullptr, false);
  if (rel.is_discarded() || !rel.is_object()) {
    SetState("error", "Réponse GitHub illisible");
    return;
  }
  const std::string tag = rel.value("tag_name", "");
  std::string exe_url, sha_url;
  for (const auto& a : rel.value("assets", json::array())) {
    const std::string name = a.value("name", "");
    if (name == "ToolBox.exe") exe_url = a.value("browser_download_url", "");
    if (name == "ToolBox.exe.sha256") sha_url = a.value("browser_download_url", "");
  }

  const auto current = ParseVersion(util::ToUtf8(TOOLBOX_VERSION_STR));
  const auto latest = ParseVersion(tag);
  const bool newer = latest > current && !exe_url.empty() && !sha_url.empty();
  {
    std::lock_guard lock(mu_);
    latest_ = tag;
    notes_ = rel.value("body", "");
    exe_url_ = newer ? exe_url : "";
    sha_url_ = newer ? sha_url : "";
  }
  SetState(newer ? "available" : "upToDate");
}

void Updater::DoInstall() {
  std::string exe_url, sha_url;
  {
    std::lock_guard lock(mu_);
    exe_url = exe_url_;
    sha_url = sha_url_;
    progress_ = 0;
  }

  std::string sha_file, exe, err;
  if (!net::HttpGet(sha_url, sha_file, err)) {
    SetState("error", "Empreinte SHA-256 : " + err);
    return;
  }
  const std::string expected = sha_file.substr(0, 64);

  if (!net::HttpGet(exe_url, exe, err, [this](size_t done, size_t total) {
        if (total == 0) return;
        const int pct = static_cast<int>(done * 100 / total);
        {
          std::lock_guard lock(mu_);
          if (pct == progress_) return;
          progress_ = pct;
        }
        PostMessageW(hwnd_, msg_, 0, 0);
      })) {
    SetState("error", err);
    return;
  }

  if (_stricmp(net::Sha256Hex(exe).c_str(), expected.c_str()) != 0) {
    SetState("error", "Fichier corrompu (SHA-256 différent) : mise à jour annulée");
    return;
  }

  // Windows permet de renommer un .exe en cours d'exécution, mais pas de l'écraser.
  const fs::path current = util::ExePath();
  const fs::path fresh = current.parent_path() / L"ToolBox.new.exe";
  {
    std::ofstream out(fresh, std::ios::binary | std::ios::trunc);
    out.write(exe.data(), static_cast<std::streamsize>(exe.size()));
    if (!out) {
      SetState("error", "Impossible d'écrire le nouveau fichier");
      return;
    }
  }
  std::error_code ec;
  fs::remove(OldExePath(), ec);
  if (!MoveFileExW(current.c_str(), OldExePath().c_str(), MOVEFILE_REPLACE_EXISTING)) {
    fs::remove(fresh, ec);
    SetState("error", "Impossible de remplacer ToolBox.exe");
    return;
  }
  if (!MoveFileExW(fresh.c_str(), current.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    MoveFileExW(OldExePath().c_str(), current.c_str(), MOVEFILE_REPLACE_EXISTING);  // retour en arrière
    SetState("error", "Impossible d'installer la nouvelle version");
    return;
  }
  ready_ = true;
  SetState("ready");
}
