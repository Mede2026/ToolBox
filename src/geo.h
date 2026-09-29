#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

// Position via le service de localisation de Windows (GPS si présent, sinon Wi-Fi / IP).
// Les requêtes tournent sur un fil séparé ; `on_update` est appelé à la fin.
class GeoLocator {
 public:
  struct Fix {
    bool valid = false;
    double lat = 0, lon = 0;
    double accuracy_m = 0;
    long long time = 0;       // secondes depuis 1970
    std::string error;        // "denied", "unavailable", …
  };

  explicit GeoLocator(std::function<void()> on_update);
  ~GeoLocator();

  void RequestAsync();
  Fix Last() const;

  // Distance en mètres entre deux points (formule de Haversine).
  static double DistanceMeters(double lat1, double lon1, double lat2, double lon2);

 private:
  std::function<void()> on_update_;
  std::atomic<bool> busy_{false};
  std::atomic<bool> alive_{true};
  mutable std::mutex mu_;
  Fix last_;
};
