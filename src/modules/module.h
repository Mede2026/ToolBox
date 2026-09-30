#pragma once

#include <windows.h>

#include <nlohmann/json.hpp>

#include <functional>
#include <string>

class Settings;

// Interface commune à toutes les fonctions de ToolBox.
// Pour ajouter une fonction : créer une classe dérivée et l'enregistrer dans App::CreateModules().
class Module {
 public:
  virtual ~Module() = default;

  virtual std::string Id() const = 0;           // ex. "enter_guard"
  virtual std::string Name() const = 0;         // nom affiché
  virtual std::string Description() const = 0;  // phrase courte

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual bool Running() const = 0;

  // Réglages propres au module (lus depuis / écrits dans settings.json).
  virtual void LoadConfig(const nlohmann::json& cfg) = 0;
  virtual nlohmann::json SaveConfig() const = 0;

  // État envoyé à l'interface (réglages + statistiques).
  virtual nlohmann::json State() const = 0;

  // Message venant de l'interface et destiné à ce module.
  virtual void HandleAction(const std::string& action, const nlohmann::json& payload) = 0;

  // Appelé toutes les quelques secondes pendant que le module tourne.
  virtual void Tick() {}

  // Vrai = fonction toujours active, sans interrupteur (ex. Moniteur).
  virtual bool AlwaysOn() const { return false; }

  // Données en direct envoyées chaque seconde quand la fenêtre est visible (null = aucune).
  virtual nlohmann::json Live() { return nullptr; }

  // Messages Windows de la fenêtre principale (ex. WM_CLIPBOARDUPDATE).
  virtual void OnWindowMessage(UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {}

  // Événements comptés par les Statistiques (ex. "ocr", "copy"). Fil principal seulement.
  static inline std::function<void(const std::string&)> event_sink;

 protected:
  static void Emit(const std::string& event) {
    if (event_sink) event_sink(event);
  }
};

// Module sans travail en arrière-plan : toute la logique est dans l'interface (ex. Convertisseur).
class UiModule : public Module {
 public:
  UiModule(std::string id, std::string name, std::string description, bool always_on = false)
      : id_(std::move(id)), name_(std::move(name)), description_(std::move(description)), always_on_(always_on) {}

  std::string Id() const override { return id_; }
  std::string Name() const override { return name_; }
  std::string Description() const override { return description_; }
  void Start() override { running_ = true; }
  void Stop() override { running_ = false; }
  bool Running() const override { return running_; }
  bool AlwaysOn() const override { return always_on_; }
  void LoadConfig(const nlohmann::json& cfg) override { config_ = cfg; config_.erase("enabled"); }
  nlohmann::json SaveConfig() const override { return config_; }
  nlohmann::json State() const override { return config_; }
  // L'interface peut garder de petites préférences : { "key": ..., "value": ... }.
  void HandleAction(const std::string& action, const nlohmann::json& payload) override {
    if (action == "setPref" && payload.contains("key") && payload["key"].is_string()) {
      config_[payload["key"].get<std::string>()] = payload.value("value", nlohmann::json());
    }
  }

 private:
  std::string id_, name_, description_;
  bool always_on_ = false;
  bool running_ = false;
  nlohmann::json config_ = nlohmann::json::object();
};
