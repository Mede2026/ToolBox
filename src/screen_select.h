#pragma once

#include <windows.h>

#include <cstdint>
#include <functional>
#include <vector>

// Fige l'écran et laisse l'utilisateur sélectionner une zone à la souris.
// À la fin, `done` reçoit les pixels de la zone (BGRA, lignes de haut en bas),
// ou ok = false si l'utilisateur a annulé (Échap / clic droit).
class ScreenSelector {
 public:
  using Done = std::function<void(bool ok, std::vector<uint8_t> bgra, int width, int height)>;

  // Une seule sélection à la fois : retourne false si une sélection est déjà ouverte.
  static bool Start(HINSTANCE instance, Done done);

 private:
  ScreenSelector(HINSTANCE instance, Done done);
  ~ScreenSelector();

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT Handle(UINT msg, WPARAM wparam, LPARAM lparam);
  void Paint(HDC target);
  RECT Selection() const;
  void Finish(bool ok);

  static ScreenSelector* active_;

  Done done_;
  HWND hwnd_ = nullptr;
  int vx_ = 0, vy_ = 0, vw_ = 0, vh_ = 0;  // écran virtuel (tous les moniteurs)
  HDC shot_dc_ = nullptr;                 // capture d'écran
  HBITMAP shot_bmp_ = nullptr, shot_old_ = nullptr;
  HDC dim_dc_ = nullptr;                  // capture assombrie
  HBITMAP dim_bmp_ = nullptr, dim_old_ = nullptr;
  HDC back_dc_ = nullptr;                // tampon pour dessiner sans clignoter
  HBITMAP back_bmp_ = nullptr, back_old_ = nullptr;
  HFONT font_ = nullptr;
  bool dragging_ = false;
  POINT start_{}, cur_{};
  bool finished_ = false;
};
