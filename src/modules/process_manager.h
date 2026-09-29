#pragma once

#include <windows.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "modules/module.h"

// Programmes : voir ce qui tourne, arrêter les programmes inutiles pour libérer
// des ressources, et les relancer ensuite.
//
// Les processus de Windows (dossier C:\Windows + liste de sécurité) sont protégés.
class ProcessManager : public Module {
 public:
  std::string Id() const override { return "processes"; }
  std::string Name() const override { return "Programmes"; }
  std::string Description() const override {
    return "Arrête les programmes inutiles pour libérer de la mémoire, puis relance-les d'un clic.";
  }

  void Start() override { running_ = true; }
  void Stop() override { running_ = false; }
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;
  nlohmann::json Live() override;

 private:
  struct Stopped {
    std::string path;
    std::string name;
    long long time;
  };
  struct CpuSample {
    ULONGLONG cpu_time;  // 100 ns
    ULONGLONG wall;      // 100 ns
  };

  nlohmann::json Snapshot();
  int StopProgram(const std::wstring& path_lower, bool force);  // nombre de processus visés
  bool IsProtected(const std::wstring& path_lower, const std::wstring& exe_lower) const;
  std::string FriendlyName(const std::wstring& path);
  void RememberStopped(const std::wstring& path);

  bool running_ = false;
  ULONGLONG watch_until_ = 0;   // l'interface regarde la page : on mesure
  ULONGLONG last_snapshot_ = 0;

  std::set<std::string> useless_;           // chemins (minuscules) marqués « inutiles »
  std::vector<Stopped> stopped_;            // arrêtés récemment (pour relancer)
  std::map<DWORD, CpuSample> cpu_prev_;
  std::map<std::wstring, std::string> names_cache_;
  std::string last_event_;
};
