#include "geo.h"

#include <winrt/Windows.Devices.Geolocation.h>
#include <winrt/Windows.Foundation.h>

#include <chrono>
#include <cmath>
#include <thread>

using namespace winrt::Windows::Devices::Geolocation;

GeoLocator::GeoLocator(std::function<void()> on_update) : on_update_(std::move(on_update)) {}

GeoLocator::~GeoLocator() { alive_ = false; }

GeoLocator::Fix GeoLocator::Last() const {
  std::lock_guard lock(mu_);
  return last_;
}

void GeoLocator::RequestAsync() {
  if (busy_.exchange(true)) return;
  std::thread([this] {
    Fix fix;
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
      const auto access = Geolocator::RequestAccessAsync().get();
      if (access != GeolocationAccessStatus::Allowed) {
        fix.error = "denied";
      } else {
        Geolocator locator;
        locator.DesiredAccuracy(PositionAccuracy::High);  // utilise le GPS s'il existe
        const auto pos = locator
                             .GetGeopositionAsync(std::chrono::minutes(1),   // position récente acceptée
                                                  std::chrono::seconds(20))  // délai max
                             .get();
        const auto c = pos.Coordinate();
        const auto p = c.Point().Position();
        fix.valid = true;
        fix.lat = p.Latitude;
        fix.lon = p.Longitude;
        fix.accuracy_m = c.Accuracy();
      }
    } catch (const winrt::hresult_error& e) {
      fix.error = e.code() == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) ? "denied" : "unavailable";
    } catch (...) {
      fix.error = "unavailable";
    }
    fix.time = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count();
    winrt::uninit_apartment();

    if (!alive_) return;
    {
      std::lock_guard lock(mu_);
      if (fix.valid || !last_.valid) {
        last_ = fix;
      } else {
        last_.error = fix.error;  // garder la dernière bonne position
      }
    }
    busy_ = false;
    if (on_update_) on_update_();
  }).detach();
}

double GeoLocator::DistanceMeters(double lat1, double lon1, double lat2, double lon2) {
  constexpr double kEarth = 6371000.0;
  constexpr double kRad = 3.14159265358979323846 / 180.0;
  const double dlat = (lat2 - lat1) * kRad;
  const double dlon = (lon2 - lon1) * kRad;
  const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(lat1 * kRad) * std::cos(lat2 * kRad) * std::sin(dlon / 2) * std::sin(dlon / 2);
  return 2 * kEarth * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}
