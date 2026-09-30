#pragma once

#include <windows.h>

#include <functional>

// Pipette : fige l'écran et affiche une loupe qui suit la souris.
// Clic (ou Entrée) = choisir la couleur du pixel au centre ; Échap / clic droit = annuler.
// Les flèches déplacent la souris d'un pixel pour viser précisément.
class ColorPicker {
 public:
  using Done = std::function<void(bool ok, COLORREF color)>;

  // Une seule pipette à la fois : retourne false si elle est déjà ouverte.
  static bool Start(HINSTANCE instance, Done done);

 private:
  ColorPicker(HINSTANCE instance, Done done);
  ~ColorPicker();

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT Handle(UINT msg, WPARAM wparam, LPARAM lparam);
  void Paint(HDC target);
  COLORREF PixelAt(POINT p) const;
  void Finish(bool ok);

  static ColorPicker* active_;

  Done done_;
  HWND hwnd_ = nullptr;
  int vx_ = 0, vy_ = 0, vw_ = 0, vh_ = 0;
  HDC shot_dc_ = nullptr;
  HBITMAP shot_bmp_ = nullptr, shot_old_ = nullptr;
  HDC back_dc_ = nullptr;
  HBITMAP back_bmp_ = nullptr, back_old_ = nullptr;
  HFONT font_ = nullptr;
  POINT cur_{};  // position de la souris (coordonnées de la fenêtre)
  bool finished_ = false;
  bool was_active_ = false;  // a eu le focus au moins une fois
  ULONGLONG shown_at_ = 0;   // ignore une perte de focus juste à l'ouverture
};
