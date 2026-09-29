#pragma once

#include <windows.h>

#include "modules/module.h"

// Moniteur système : processeur, mémoire, disque, batterie, réseau.
// Les mesures ne sont prises que lorsque la fenêtre est visible (via Live()).
class SystemMonitor : public Module {
 public:
  std::string Id() const override { return "monitor"; }
  std::string Name() const override { return "Moniteur"; }
  std::string Description() const override { return "Processeur, mémoire, disque et batterie en direct."; }

  void Start() override { running_ = true; }
  void Stop() override { running_ = false; }
  bool Running() const override { return running_; }
  bool AlwaysOn() const override { return true; }

  void LoadConfig(const nlohmann::json&) override {}
  nlohmann::json SaveConfig() const override { return nlohmann::json::object(); }
  nlohmann::json State() const override { return nlohmann::json::object(); }
  void HandleAction(const std::string&, const nlohmann::json&) override {}
  nlohmann::json Live() override;

 private:
  bool running_ = false;
  ULONGLONG prev_idle_ = 0, prev_total_ = 0;
  ULONGLONG prev_net_bytes_in_ = 0, prev_net_bytes_out_ = 0, prev_net_tick_ = 0;
};
