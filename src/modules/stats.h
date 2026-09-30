#pragma once

#include <windows.h>

#include <deque>
#include <functional>
#include <map>
#include <string>

#include "modules/module.h"

// Statistiques : temps d'écran par app (seulement quand on se sert du PC),
// processeur et mémoire heure par heure, et compteurs des fonctions ToolBox.
// Tout reste sur le PC (settings.json).
class Stats : public Module {
 public:
  explicit Stats(std::function<void()> on_change);

  std::string Id() const override { return "stats"; }
  std::string Name() const override { return "Statistiques"; }
  std::string Description() const override {
    return "Temps d'écran par app, utilisation du PC et tes chiffres ToolBox (tout reste sur ton PC).";
  }

  void Start() override;
  void Stop() override { running_ = false; }
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;
  void Tick() override;

  // Un événement ToolBox (ex. "ocr", "copy", "enterFix").
  void Count(const std::string& event);

 private:
  struct Day {
    std::string date;                          // AAAA-MM-JJ (heure locale)
    long long active = 0;                      // secondes d'utilisation
    std::map<std::string, long long> apps;     // app -> secondes au premier plan
    std::map<std::string, long long> events;   // événement -> nombre
  };
  struct Hour {
    long long hour;  // heures depuis 1970 (UTC)
    double cpu = 0, ram = 0;
    int n = 0;
  };

  Day& Today();
  std::string AppName(DWORD pid);

  std::function<void()> on_change_;
  bool running_ = false;

  std::deque<Day> days_;    // plus récent en premier (30 jours max)
  std::deque<Hour> hours_;  // plus récente en dernier (7 jours max)
  std::map<std::string, long long> totals_;
  long long since_ = 0;

  ULONGLONG last_tick_ = 0;
  ULONGLONG prev_idle_ = 0, prev_total_ = 0;
  int ticks_ = 0;
  std::map<std::wstring, std::string> names_;  // chemin -> nom lisible
};
