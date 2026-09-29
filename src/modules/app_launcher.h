#pragma once

#include <string>
#include <vector>

#include "modules/module.h"

// Raccourcis d'apps : des groupes d'apps / fichiers / sites à lancer d'un seul clic.
class AppLauncher : public Module {
 public:
  std::string Id() const override { return "app_launcher"; }
  std::string Name() const override { return "Raccourcis d'apps"; }
  std::string Description() const override { return "Lance plusieurs apps d'un seul clic."; }

  void Start() override { running_ = true; }
  void Stop() override { running_ = false; }
  bool Running() const override { return running_; }

  void LoadConfig(const nlohmann::json& cfg) override;
  nlohmann::json SaveConfig() const override;
  nlohmann::json State() const override;
  void HandleAction(const std::string& action, const nlohmann::json& payload) override;

 private:
  struct Group {
    std::string id;
    std::string name;
    std::string emoji;
    std::vector<std::string> items;  // chemins de fichiers ou adresses https://
  };

  static bool Open(const std::string& target);

  bool running_ = false;
  std::vector<Group> groups_;
  std::string last_event_;
};
