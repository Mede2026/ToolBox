#include "screen_select.h"

#include <windowsx.h>

#include <algorithm>
#include <string>

namespace {
constexpr wchar_t kClass[] = L"ToolBoxScreenSelector";
constexpr COLORREF kAccent = RGB(124, 188, 255);
}  // namespace

ScreenSelector* ScreenSelector::active_ = nullptr;

bool ScreenSelector::Start(HINSTANCE instance, Done done) {
  if (active_) return false;
  new ScreenSelector(instance, std::move(done));  // se détruit à la fermeture de sa fenêtre
  return true;
}

ScreenSelector::ScreenSelector(HINSTANCE instance, Done done) : done_(std::move(done)) {
  active_ = this;
  vx_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
  vy_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
  vw_ = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  vh_ = GetSystemMetrics(SM_CYVIRTUALSCREEN);

  // 1. Photo de tout l'écran (avant d'afficher notre fenêtre).
  HDC screen = GetDC(nullptr);
  shot_dc_ = CreateCompatibleDC(screen);
  shot_bmp_ = CreateCompatibleBitmap(screen, vw_, vh_);
  shot_old_ = static_cast<HBITMAP>(SelectObject(shot_dc_, shot_bmp_));
  BitBlt(shot_dc_, 0, 0, vw_, vh_, screen, vx_, vy_, SRCCOPY | CAPTUREBLT);

  // 2. Version assombrie (le fond hors sélection).
  dim_dc_ = CreateCompatibleDC(screen);
  dim_bmp_ = CreateCompatibleBitmap(screen, vw_, vh_);
  dim_old_ = static_cast<HBITMAP>(SelectObject(dim_dc_, dim_bmp_));
  BitBlt(dim_dc_, 0, 0, vw_, vh_, shot_dc_, 0, 0, SRCCOPY);
  {
    HDC black_dc = CreateCompatibleDC(screen);
    HBITMAP black = CreateCompatibleBitmap(screen, 1, 1);
    HGDIOBJ old = SelectObject(black_dc, black);
    SetPixel(black_dc, 0, 0, RGB(0, 0, 0));
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 130, 0};
    AlphaBlend(dim_dc_, 0, 0, vw_, vh_, black_dc, 0, 0, 1, 1, bf);
    SelectObject(black_dc, old);
    DeleteObject(black);
    DeleteDC(black_dc);
  }

  back_dc_ = CreateCompatibleDC(screen);
  back_bmp_ = CreateCompatibleBitmap(screen, vw_, vh_);
  back_old_ = static_cast<HBITMAP>(SelectObject(back_dc_, back_bmp_));
  ReleaseDC(nullptr, screen);

  const UINT dpi = GetDpiForSystem();
  font_ = CreateFontW(-MulDiv(15, dpi, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.lpszClassName = kClass;
  RegisterClassExW(&wc);  // échoue sans conséquence si déjà enregistrée

  hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClass, L"Sélection OCR", WS_POPUP, vx_, vy_, vw_, vh_,
                          nullptr, nullptr, instance, this);
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  SetFocus(hwnd_);
}

ScreenSelector::~ScreenSelector() {
  SelectObject(shot_dc_, shot_old_);
  DeleteObject(shot_bmp_);
  DeleteDC(shot_dc_);
  SelectObject(dim_dc_, dim_old_);
  DeleteObject(dim_bmp_);
  DeleteDC(dim_dc_);
  SelectObject(back_dc_, back_old_);
  DeleteObject(back_bmp_);
  DeleteDC(back_dc_);
  DeleteObject(font_);
  if (active_ == this) active_ = nullptr;
}

RECT ScreenSelector::Selection() const {
  return RECT{std::min(start_.x, cur_.x), std::min(start_.y, cur_.y), std::max(start_.x, cur_.x),
              std::max(start_.y, cur_.y)};
}

void ScreenSelector::Paint(HDC target) {
  BitBlt(back_dc_, 0, 0, vw_, vh_, dim_dc_, 0, 0, SRCCOPY);

  if (dragging_) {
    const RECT r = Selection();
    const int w = r.right - r.left, h = r.bottom - r.top;
    BitBlt(back_dc_, r.left, r.top, w, h, shot_dc_, r.left, r.top, SRCCOPY);  // zone choisie en clair
    HPEN pen = CreatePen(PS_SOLID, 2, kAccent);
    HGDIOBJ old_pen = SelectObject(back_dc_, pen);
    HGDIOBJ old_brush = SelectObject(back_dc_, GetStockObject(NULL_BRUSH));
    Rectangle(back_dc_, r.left - 1, r.top - 1, r.right + 1, r.bottom + 1);
    SelectObject(back_dc_, old_brush);
    SelectObject(back_dc_, old_pen);
    DeleteObject(pen);

    // Taille de la sélection
    const std::wstring size = std::to_wstring(w) + L" × " + std::to_wstring(h);
    SelectObject(back_dc_, font_);
    SetBkMode(back_dc_, TRANSPARENT);
    SetTextColor(back_dc_, kAccent);
    RECT label{r.left, r.bottom + 6, r.left + 300, r.bottom + 40};
    DrawTextW(back_dc_, size.c_str(), -1, &label, DT_LEFT | DT_TOP | DT_SINGLELINE);
  }

  // Consigne en haut du moniteur où se trouve la souris
  POINT pt;
  GetCursorPos(&pt);
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
  const std::wstring hint = L"Sélectionne le texte à lire  ·  Échap pour annuler";
  SelectObject(back_dc_, font_);
  RECT measure{0, 0, 0, 0};
  DrawTextW(back_dc_, hint.c_str(), -1, &measure, DT_CALCRECT | DT_SINGLELINE);
  const int pad = 18, bw = measure.right + pad * 2, bh = measure.bottom + pad;
  const int bx = (mi.rcMonitor.left + mi.rcMonitor.right) / 2 - bw / 2 - vx_;
  const int by = mi.rcMonitor.top - vy_ + 28;
  HBRUSH bg = CreateSolidBrush(RGB(32, 32, 32));
  HPEN border = CreatePen(PS_SOLID, 1, RGB(70, 70, 70));
  HGDIOBJ ob = SelectObject(back_dc_, bg), op = SelectObject(back_dc_, border);
  RoundRect(back_dc_, bx, by, bx + bw, by + bh, 14, 14);
  SelectObject(back_dc_, ob);
  SelectObject(back_dc_, op);
  DeleteObject(bg);
  DeleteObject(border);
  SetBkMode(back_dc_, TRANSPARENT);
  SetTextColor(back_dc_, RGB(255, 255, 255));
  RECT text{bx, by, bx + bw, by + bh};
  DrawTextW(back_dc_, hint.c_str(), -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

  BitBlt(target, 0, 0, vw_, vh_, back_dc_, 0, 0, SRCCOPY);
}

void ScreenSelector::Finish(bool ok) {
  if (finished_) return;
  finished_ = true;
  std::vector<uint8_t> pixels;
  int w = 0, h = 0;
  const RECT r = Selection();
  if (ok) {
    w = r.right - r.left;
    h = r.bottom - r.top;
    ok = w >= 4 && h >= 4;  // un simple clic = annuler
  }
  if (ok) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // lignes de haut en bas
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(shot_dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
      HDC dc = CreateCompatibleDC(shot_dc_);
      HGDIOBJ old = SelectObject(dc, dib);
      BitBlt(dc, 0, 0, w, h, shot_dc_, r.left, r.top, SRCCOPY);
      GdiFlush();
      pixels.assign(static_cast<uint8_t*>(bits), static_cast<uint8_t*>(bits) + size_t(w) * h * 4);
      SelectObject(dc, old);
      DeleteDC(dc);
    }
    if (dib) DeleteObject(dib);
    ok = !pixels.empty();
  }
  Done done = std::move(done_);
  DestroyWindow(hwnd_);  // WM_NCDESTROY supprime l'objet
  if (done) done(ok, std::move(pixels), w, h);
}

LRESULT CALLBACK ScreenSelector::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  ScreenSelector* self = nullptr;
  if (msg == WM_NCCREATE) {
    self = static_cast<ScreenSelector*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<ScreenSelector*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) return DefWindowProcW(hwnd, msg, wparam, lparam);
  if (msg == WM_NCDESTROY) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    delete self;
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
  return self->Handle(msg, wparam, lparam);
}

LRESULT ScreenSelector::Handle(UINT msg, WPARAM wparam, LPARAM lparam) {
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
    case WM_LBUTTONDOWN:
      dragging_ = true;
      start_ = cur_ = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      SetCapture(hwnd_);
      InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    case WM_MOUSEMOVE:
      cur_ = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    case WM_LBUTTONUP:
      if (dragging_) {
        cur_ = POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ReleaseCapture();
        Finish(true);
      }
      return 0;
    case WM_RBUTTONUP:
      Finish(false);
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) Finish(false);
      return 0;
    case WM_ACTIVATE:
      if (LOWORD(wparam) == WA_INACTIVE && !dragging_) Finish(false);  // Alt+Tab ailleurs = annuler
      return 0;
    case WM_CLOSE:
      Finish(false);
      return 0;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}
