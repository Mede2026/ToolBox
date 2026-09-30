#include "color_pick.h"

#include <windowsx.h>

#include <algorithm>
#include <cstdio>

namespace {
constexpr wchar_t kClass[] = L"ToolBoxColorPicker";
constexpr int kZoomPixels = 11;  // pixels agrandis autour du curseur (impair)
}  // namespace

ColorPicker* ColorPicker::active_ = nullptr;

bool ColorPicker::Start(HINSTANCE instance, Done done) {
  if (active_) return false;
  new ColorPicker(instance, std::move(done));  // se détruit à la fermeture de sa fenêtre
  return true;
}

ColorPicker::ColorPicker(HINSTANCE instance, Done done) : done_(std::move(done)) {
  active_ = this;
  vx_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
  vy_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
  vw_ = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  vh_ = GetSystemMetrics(SM_CYVIRTUALSCREEN);

  HDC screen = GetDC(nullptr);
  shot_dc_ = CreateCompatibleDC(screen);
  shot_bmp_ = CreateCompatibleBitmap(screen, vw_, vh_);
  shot_old_ = static_cast<HBITMAP>(SelectObject(shot_dc_, shot_bmp_));
  BitBlt(shot_dc_, 0, 0, vw_, vh_, screen, vx_, vy_, SRCCOPY | CAPTUREBLT);
  back_dc_ = CreateCompatibleDC(screen);
  back_bmp_ = CreateCompatibleBitmap(screen, vw_, vh_);
  back_old_ = static_cast<HBITMAP>(SelectObject(back_dc_, back_bmp_));
  ReleaseDC(nullptr, screen);

  font_ = CreateFontW(-MulDiv(14, GetDpiForSystem(), 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

  POINT pt;
  GetCursorPos(&pt);
  cur_ = POINT{pt.x - vx_, pt.y - vy_};

  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.lpszClassName = kClass;
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClass, L"Pipette", WS_POPUP, vx_, vy_, vw_, vh_, nullptr,
                          nullptr, instance, this);
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  SetFocus(hwnd_);
}

ColorPicker::~ColorPicker() {
  SelectObject(shot_dc_, shot_old_);
  DeleteObject(shot_bmp_);
  DeleteDC(shot_dc_);
  SelectObject(back_dc_, back_old_);
  DeleteObject(back_bmp_);
  DeleteDC(back_dc_);
  DeleteObject(font_);
  if (active_ == this) active_ = nullptr;
}

COLORREF ColorPicker::PixelAt(POINT p) const {
  p.x = std::clamp<LONG>(p.x, 0, vw_ - 1);
  p.y = std::clamp<LONG>(p.y, 0, vh_ - 1);
  return GetPixel(shot_dc_, p.x, p.y);
}

void ColorPicker::Paint(HDC target) {
  BitBlt(back_dc_, 0, 0, vw_, vh_, shot_dc_, 0, 0, SRCCOPY);

  // Loupe : kZoomPixels × kZoomPixels pixels agrandis, placée à côté du curseur.
  const UINT dpi = GetDpiForSystem();
  const int cell = MulDiv(12, dpi, 96);
  const int size = cell * kZoomPixels;
  const int label_h = MulDiv(30, dpi, 96);
  const int gap = MulDiv(22, dpi, 96);
  int lx = cur_.x + gap, ly = cur_.y + gap;
  if (lx + size > vw_) lx = cur_.x - gap - size;  // bord droit : on passe à gauche
  if (ly + size + label_h > vh_) ly = cur_.y - gap - size - label_h;

  const int half = kZoomPixels / 2;
  SetStretchBltMode(back_dc_, COLORONCOLOR);
  StretchBlt(back_dc_, lx, ly, size, size, shot_dc_, cur_.x - half, cur_.y - half, kZoomPixels, kZoomPixels,
             SRCCOPY);

  // Grille légère + pixel central encadré
  HPEN grid = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
  HGDIOBJ old_pen = SelectObject(back_dc_, grid);
  for (int i = 1; i < kZoomPixels; ++i) {
    MoveToEx(back_dc_, lx + i * cell, ly, nullptr);
    LineTo(back_dc_, lx + i * cell, ly + size);
    MoveToEx(back_dc_, lx, ly + i * cell, nullptr);
    LineTo(back_dc_, lx + size, ly + i * cell);
  }
  HPEN white = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
  SelectObject(back_dc_, white);
  HGDIOBJ old_brush = SelectObject(back_dc_, GetStockObject(NULL_BRUSH));
  Rectangle(back_dc_, lx + half * cell, ly + half * cell, lx + (half + 1) * cell + 1, ly + (half + 1) * cell + 1);
  HPEN border = CreatePen(PS_SOLID, 2, RGB(124, 188, 255));
  SelectObject(back_dc_, border);
  Rectangle(back_dc_, lx - 1, ly - 1, lx + size + 1, ly + size + label_h + 1);
  SelectObject(back_dc_, old_brush);
  SelectObject(back_dc_, old_pen);
  DeleteObject(grid);
  DeleteObject(white);
  DeleteObject(border);

  // Étiquette : aperçu de la couleur + code HEX
  const COLORREF c = PixelAt(cur_);
  RECT label{lx, ly + size, lx + size, ly + size + label_h};
  HBRUSH bg = CreateSolidBrush(RGB(32, 32, 32));
  FillRect(back_dc_, &label, bg);
  DeleteObject(bg);
  const int sw = label_h - MulDiv(10, dpi, 96);
  RECT swatch{lx + MulDiv(6, dpi, 96), label.top + (label_h - sw) / 2, 0, 0};
  swatch.right = swatch.left + sw;
  swatch.bottom = swatch.top + sw;
  HBRUSH sb = CreateSolidBrush(c);
  FillRect(back_dc_, &swatch, sb);
  DeleteObject(sb);
  wchar_t hex[16];
  swprintf_s(hex, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
  SelectObject(back_dc_, font_);
  SetBkMode(back_dc_, TRANSPARENT);
  SetTextColor(back_dc_, RGB(255, 255, 255));
  RECT text{swatch.right + MulDiv(8, dpi, 96), label.top, label.right, label.bottom};
  DrawTextW(back_dc_, hex, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

  BitBlt(target, 0, 0, vw_, vh_, back_dc_, 0, 0, SRCCOPY);
}

void ColorPicker::Finish(bool ok) {
  if (finished_) return;
  finished_ = true;
  const COLORREF color = PixelAt(cur_);
  Done done = std::move(done_);
  DestroyWindow(hwnd_);  // WM_NCDESTROY supprime l'objet
  if (done) done(ok, color);
}

LRESULT CALLBACK ColorPicker::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  ColorPicker* self = nullptr;
  if (msg == WM_NCCREATE) {
    self = static_cast<ColorPicker*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<ColorPicker*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) return DefWindowProcW(hwnd, msg, wparam, lparam);
  if (msg == WM_NCDESTROY) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    delete self;
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return self->Handle(msg, wparam, lparam);
}

LRESULT ColorPicker::Handle(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd_, &ps);
      Paint(dc);
      EndPaint(hwnd_, &ps);
      return 0;
    }
    case WM_MOUSEMOVE:
      cur_ = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    case WM_LBUTTONUP:
      cur_ = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      Finish(true);
      return 0;
    case WM_RBUTTONUP:
      Finish(false);
      return 0;
    case WM_KEYDOWN: {
      if (wparam == VK_ESCAPE) {
        Finish(false);
        return 0;
      }
      if (wparam == VK_RETURN || wparam == VK_SPACE) {
        Finish(true);
        return 0;
      }
      int dx = 0, dy = 0;
      if (wparam == VK_LEFT) dx = -1;
      if (wparam == VK_RIGHT) dx = 1;
      if (wparam == VK_UP) dy = -1;
      if (wparam == VK_DOWN) dy = 1;
      if (dx || dy) {  // viser au pixel près
        POINT pt;
        GetCursorPos(&pt);
        SetCursorPos(pt.x + dx, pt.y + dy);
      }
      return 0;
    }
    case WM_ACTIVATE:
      if (LOWORD(wparam) == WA_INACTIVE) Finish(false);
      return 0;
    case WM_CLOSE:
      Finish(false);
      return 0;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}
