#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "geo.h"
#include "modules/module.h"

// Lancement par lieu : ouvre un fichier ou une app quand on arrive à un endroit.
//
// Un lieu se reconnaît par :
//  - un réseau Wi-Fi (nom / SSID), et/ou
//  - une position + un rayon (GPS du PC s'il en a un, sinon position estimée par Windows).
// Si l'un des deux correspond, on est « sur place ».
class PlaceLauncher : public Module {
 public:
  explicit PlaceLauncher(std::function<void()> on_change);
  ~PlaceLauncher() override;

  std::string Id() const override { return "place_launcher"; }
  std::string Name() const override { return "Lancement par lieu"; }
  std::string Description() const override {
    return "Ouvre un fichier ou une app quand tu arrives à un endroit (Wi-Fi ou GPS).";
  }

  void Start() override;
  void Stop() override;
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;
  void Tick() override;

 private:
  struct Rule {
    std::string id;
    std::string place;       // nom affiché, ex. « Maison »
    std::string ssid;        // réseau Wi-Fi (vide = pas utilisé)
    bool use_gps = false;
    double lat = 0, lon = 0;
    int radius_m = 150;
    std::string path;        // fichier ou app à ouvrir
    bool enabled = true;
    long long last_run = 0;
    // État (non sauvegardé)
    bool known = false;      // présence déjà déterminée depuis le démarrage
    bool inside = false;
    bool gps_inside = false;
  };

  bool QueryConnectedSsids(std::set<std::string>& out);
  bool NeedsGps() const;
  void Launch(Rule& rule);

  std::function<void()> on_change_;
  bool running_ = false;
  HANDLE wlan_ = nullptr;
  std::unique_ptr<GeoLocator> geo_;
  long long last_geo_request_ = 0;

  std::vector<Rule> rules_;
  std::set<std::string> connected_;
  std::string wifi_error_;
  std::string last_event_;
};
