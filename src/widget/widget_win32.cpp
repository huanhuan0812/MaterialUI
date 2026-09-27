// src/widget/widget_win32.cpp
#include "widget.h"

#if defined(_WIN32)
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM

#include <string>
#include <memory>
#include <cmath>
#include <vector>
#include <cstdint>

namespace ui {
namespace {

// ============================================================
// Win32Canvas
// ============================================================
class Win32Canvas : public Canvas {
public:
    explicit Win32Canvas(HDC hdc) : hdc_(hdc) {}
    ~Win32Canvas() override {
        if (font_) DeleteObject(font_);
    }

    // ---------- 基础图元 ----------
    void fillRect(const Rect& r, Color c) override {
        RECT rc{ r.x, r.y, r.x + r.w, r.y + r.h };
        HBRUSH brush = CreateSolidBrush(toCOLORREF(c));
        FillRect(hdc_, &rc, brush);
        DeleteObject(brush);
    }

    void drawRect(const Rect& r, Color c, int lineWidth) override {
        HPEN pen = CreatePen(PS_SOLID, lineWidth, toCOLORREF(c));
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
        Rectangle(hdc_, r.x, r.y, r.x + r.w, r.y + r.h);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    void drawLine(int x0, int y0, int x1, int y1, Color c) override {
        HPEN pen = CreatePen(PS_SOLID, 1, toCOLORREF(c));
        HGDIOBJ oldPen = SelectObject(hdc_, pen);
        MoveToEx(hdc_, x0, y0, nullptr);
        LineTo(hdc_, x1, y1);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    void drawText(const std::string& text, int x, int y, Color c) override {
        applyFont();
        SetTextColor(hdc_, toCOLORREF(c));
        SetBkMode(hdc_, TRANSPARENT);
        std::wstring w = toWide(text);
        TextOutW(hdc_, x, y, w.c_str(), static_cast<int>(w.size()));
    }

    // ---------- 圆角 / 圆 ----------
    void fillRoundRect(const Rect& r, int radius, Color c) override {
        HBRUSH brush = CreateSolidBrush(toCOLORREF(c));
        HPEN   pen   = CreatePen(PS_SOLID, 1, toCOLORREF(c));
        HGDIOBJ oldBrush = SelectObject(hdc_, brush);
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        RoundRect(hdc_, r.x, r.y, r.x + r.w, r.y + r.h, radius * 2, radius * 2);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void drawRoundRect(const Rect& r, int radius, Color c, int lw) override {
        HPEN   pen   = CreatePen(PS_SOLID, lw, toCOLORREF(c));
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
        RoundRect(hdc_, r.x, r.y, r.x + r.w, r.y + r.h, radius * 2, radius * 2);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    void fillCircle(Point c, int radius, Color color) override {
        HBRUSH brush = CreateSolidBrush(toCOLORREF(color));
        HPEN   pen   = CreatePen(PS_SOLID, 1, toCOLORREF(color));
        HGDIOBJ oldBrush = SelectObject(hdc_, brush);
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        Ellipse(hdc_, c.x - radius, c.y - radius, c.x + radius, c.y + radius);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void drawCircle(Point c, int radius, Color color, int lw) override {
        HPEN   pen   = CreatePen(PS_SOLID, lw, toCOLORREF(color));
        HGDIOBJ oldPen   = SelectObject(hdc_, pen);
        HGDIOBJ oldBrush = SelectObject(hdc_, GetStockObject(NULL_BRUSH));
        Ellipse(hdc_, c.x - radius, c.y - radius, c.x + radius, c.y + radius);
        SelectObject(hdc_, oldBrush);
        SelectObject(hdc_, oldPen);
        DeleteObject(pen);
    }

    // ---------- 阴影 ----------
    // GDI 无 alpha 混合，用 AlphaBlend（msimg32）画柔化矩形
    void drawShadow(const Rect& r, int radius, int elevation,
                    Color shadow) override {
        if (elevation <= 0) return;

        // 逐层半透明叠加，模拟阴影扩散
        BYTE baseA = alphaOf(shadow);
        for (int i = elevation; i >= 1; --i) {
            BYTE a = static_cast<BYTE>(baseA / (i + 1));
            if (a == 0) continue;

            Rect rr{ r.x - i, r.y - i + elevation / 2,
                     r.w + i * 2, r.h + i * 2 };

            // 用 AlphaBlend 画一层柔化圆角矩形
            HDC mem = CreateCompatibleDC(hdc_);
            int w = rr.w, h = rr.h;
            HBITMAP bmp = CreateCompatibleBitmap(hdc_, w, h);
            HGDIOBJ oldBmp = SelectObject(mem, bmp);

            // 画一个实心圆角矩形（颜色 rgb 部分来自 shadow）
            HBRUSH brush = CreateSolidBrush(RGB(redOf(shadow),
                                                greenOf(shadow),
                                                blueOf(shadow)));
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(redOf(shadow),
                                                   greenOf(shadow),
                                                   blueOf(shadow)));
            HGDIOBJ ob = SelectObject(mem, brush);
            HGDIOBJ op = SelectObject(mem, pen);
            RoundRect(mem, 0, 0, w, h, (radius + i) * 2, (radius + i) * 2);
            SelectObject(mem, ob);
            SelectObject(mem, op);
            DeleteObject(brush);
            DeleteObject(pen);

            // 整块用 alpha 混合贴到目标 DC
            BLENDFUNCTION bf{ AC_SRC_OVER, 0, a, 0 };
            AlphaBlend(hdc_, rr.x, rr.y, w, h,
                       mem, 0, 0, w, h, bf);

            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
        }
    }

    void drawCard(const Rect& r, int radius, Color fill,
                  int elevation) override {
        if (elevation > 0) drawShadow(r, radius, elevation);
        fillRoundRect(r, radius, fill);
    }

    // ---------- 渐变 ----------
    void fillLinearGradient(const Rect& r, Color c0, Color c1,
                            bool vertical) override {
        TRIVERTEX v[2] = {
            { r.x, r.y,
              static_cast<COLOR16>(redOf(c0)   << 8),
              static_cast<COLOR16>(greenOf(c0) << 8),
              static_cast<COLOR16>(blueOf(c0)  << 8),
              static_cast<COLOR16>(alphaOf(c0) << 8) },
            { r.x + r.w, r.y + r.h,
              static_cast<COLOR16>(redOf(c1)   << 8),
              static_cast<COLOR16>(greenOf(c1) << 8),
              static_cast<COLOR16>(blueOf(c1)  << 8),
              static_cast<COLOR16>(alphaOf(c1) << 8) }
        };
        GRADIENT_RECT g{ 0, 1 };
        ULONG mode = vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H;
        GradientFill(hdc_, v, 2, &g, 1, mode);
    }

    // ---------- 文本扩展 ----------
    void setFontSize(int px) override {
        if (fontSize_ != px) { fontSize_ = px; fontDirty_ = true; }
    }
    void setFontBold(bool bold) override {
        if (bold_ != bold) { bold_ = bold; fontDirty_ = true; }
    }

    void drawTextAligned(const std::string& text, const Rect& box,
                         Color c, TextAlign align, bool vcenter) override {
        applyFont();
        SetTextColor(hdc_, toCOLORREF(c));
        SetBkMode(hdc_, TRANSPARENT);

        std::wstring w = toWide(text);

        UINT fmt = DT_SINGLELINE | DT_NOPREFIX;
        switch (align) {
            case TextAlign::Left:   fmt |= DT_LEFT;   break;
            case TextAlign::Center: fmt |= DT_CENTER; break;
            case TextAlign::Right:  fmt |= DT_RIGHT;  break;
        }
        fmt |= vcenter ? DT_VCENTER : DT_TOP;

        RECT rc{ box.x, box.y, box.x + box.w, box.y + box.h };
        DrawTextW(hdc_, w.c_str(), static_cast<int>(w.size()), &rc, fmt);
    }

    // ---------- 状态栈 ----------
    void save() override {
        // SaveDC 会保存字体、裁剪、viewport 等全部状态
        saved_.push_back(SaveDC(hdc_));
        // 注意：SaveDC 后当前选中的 font 仍在，但恢复时会还原
    }
    void restore() override {
        if (!saved_.empty()) {
            RestoreDC(hdc_, saved_.back());
            saved_.pop_back();
            // DC 状态被还原，之前选入的 font 可能已失效
            fontSelected_ = false;
        }
    }
    void translate(int dx, int dy) override {
        POINT p{ 0, 0 };
        GetViewportOrgEx(hdc_, &p);
        SetViewportOrgEx(hdc_, p.x + dx, p.y + dy, nullptr);
    }
    void clipRect(const Rect& r) override {
        // 直接传逻辑坐标，GDI 会按当前 viewport 做映射
        // 用 CombineRgn 与现有裁剪区求交更符合直觉
        HRGN rgn = CreateRectRgn(r.x, r.y, r.x + r.w, r.y + r.h);
        // ExtSelectClipRgn(hdc_, rgn, RGN_AND); // 若要与现有求交
        SelectClipRgn(hdc_, rgn);  // RGN_COPY: 替换，配合 save/restore
        DeleteObject(rgn);
    }

    // ---------- 后端标识 ----------
    Backend backend() const override { return Backend::GDI; }

private:
    void applyFont() {
        if (font_ && !fontDirty_ && fontSelected_) return;
        if (font_) DeleteObject(font_);

        LOGFONTW lf{};
        lf.lfHeight  = -fontSize_;
        lf.lfWeight  = bold_ ? FW_BOLD : FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");

        font_ = CreateFontIndirectW(&lf);
        if (font_) {
            // 关键修复：必须 SelectObject 才能生效
            SelectObject(hdc_, font_);
            fontSelected_ = true;
        }
        fontDirty_ = false;
    }

    static COLORREF toCOLORREF(Color c) {
        return RGB(redOf(c), greenOf(c), blueOf(c));
    }
    static std::wstring toWide(const std::string& s) {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(n - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
        return w;
    }

    HDC   hdc_;
    HFONT font_ = nullptr;
    int   fontSize_ = 13;
    bool  bold_ = false;
    bool  fontDirty_ = true;
    bool  fontSelected_ = false;

    std::vector<int> saved_;
};

// ---------- 窗口类名 ----------
const wchar_t* kClassName = L"UiWidgetClass";

// ============================================================
// Win32Widget
// ============================================================
class Win32Widget : public Widget {
public:
    ~Win32Widget() override {
        if (hwnd_) DestroyWindow(hwnd_);
    }

    bool create(const std::string& title, int w, int h) override {
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = &Win32Widget::WndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc);

        hwnd_ = CreateWindowExW(
            0, kClassName, toWide(title).c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, w, h,
            nullptr, nullptr, hInst, this);

        return hwnd_ != nullptr;
    }

    void show() override { ShowWindow(hwnd_, SW_SHOW); }
    void hide() override { ShowWindow(hwnd_, SW_HIDE); }

    void close() override {
        shouldQuit_ = true;
        if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }

    bool pumpEvents(int timeoutMs) override {
        DWORD wait = (timeoutMs < 0) ? INFINITE : static_cast<DWORD>(timeoutMs);
        DWORD r = MsgWaitForMultipleObjects(0, nullptr, FALSE, wait, QS_ALLINPUT);

        if (r == WAIT_OBJECT_0) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) return false;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        return !shouldQuit_;
    }

    void setTitle(const std::string& t) override {
        SetWindowTextW(hwnd_, toWide(t).c_str());
    }
    void setSize(int w, int h) override {
        SetWindowPos(hwnd_, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
    }
    void getSize(int& w, int& h) const override {
        RECT r; GetClientRect(hwnd_, &r);
        w = r.right - r.left;
        h = r.bottom - r.top;
    }

    void invalidate(const Rect& r) override {
        RECT rc{ r.x, r.y, r.x + r.w, r.y + r.h };
        InvalidateRect(hwnd_, &rc, FALSE);
    }
    void invalidateAll() override {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    void repaintNow() override { UpdateWindow(hwnd_); }

    void setPaintCallback(PaintCallback cb) override { paintCb_ = std::move(cb); }
    void setEventCallback(EventCallback cb) override { eventCb_ = std::move(cb); }

private:
    void emit(Event e) {
        if (eventCb_) eventCb_(e);
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp) {
        Win32Widget* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<Win32Widget*>(cs->lpCreateParams);
            self->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<Win32Widget*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                if (self->paintCb_) {
                    Win32Canvas canvas(hdc);
                    const RECT& rc = ps.rcPaint;
                    Rect dirty{ rc.left, rc.top,
                                rc.right - rc.left, rc.bottom - rc.top };
                    if (!dirty.empty()) self->paintCb_(canvas, dirty);
                }
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_ERASEBKGND:
                return 1;

            case WM_SIZE: {
                Event e;
                e.type = EventType::Resize;
                e.width  = static_cast<int>(LOWORD(lp));
                e.height = static_cast<int>(HIWORD(lp));
                self->emit(e);
                return 0;
            }

            case WM_LBUTTONDOWN: {
                Event e;
                e.type = EventType::MouseDown;
                e.x = GET_X_LPARAM(lp);
                e.y = GET_Y_LPARAM(lp);
                self->emit(e);
                SetFocus(hwnd);
                return 0;
            }

            case WM_LBUTTONUP: {
                Event e;
                e.type = EventType::MouseUp;
                e.x = GET_X_LPARAM(lp);
                e.y = GET_Y_LPARAM(lp);
                self->emit(e);
                return 0;
            }

            case WM_MOUSEMOVE: {
                // 首次进入：开启 TRACKMOUSEEVENT，触发 MouseEnter
                if (!self->trackingMouse_) {
                    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                    TrackMouseEvent(&tme);
                    self->trackingMouse_ = true;

                    Event enter;
                    enter.type = EventType::MouseEnter;
                    enter.x = GET_X_LPARAM(lp);
                    enter.y = GET_Y_LPARAM(lp);
                    self->emit(enter);
                }

                Event e;
                e.type = EventType::MouseMove;
                e.x = GET_X_LPARAM(lp);
                e.y = GET_Y_LPARAM(lp);
                self->emit(e);
                return 0;
            }

            case WM_MOUSELEAVE: {
                self->trackingMouse_ = false;
                Event e;
                e.type = EventType::MouseExit;
                self->emit(e);
                return 0;
            }

            case WM_MOUSEWHEEL: {
                int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
                POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                Event e;
                e.type = EventType::Wheel;
                e.x = pt.x;
                e.y = pt.y;
                e.wheelDelta = delta;
                self->emit(e);
                return 0;
            }

            case WM_KEYDOWN: {
                Event e;
                e.type = EventType::KeyDown;
                e.key = static_cast<int>(wp);
                e.shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
                e.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                e.alt   = (GetKeyState(VK_MENU)    & 0x8000) != 0;
                e.meta  = (GetKeyState(VK_LWIN) & 0x8000) ||
                          (GetKeyState(VK_RWIN) & 0x8000);
                self->emit(e);
                return 0;
            }

            case WM_CHAR: {
                Event e;
                e.type = EventType::Char;
                e.codepoint = static_cast<uint32_t>(wp);
                self->emit(e);
                return 0;
            }

            case WM_KEYUP: {
                Event e;
                e.type = EventType::KeyUp;
                e.key = static_cast<int>(wp);
                self->emit(e);
                return 0;
            }

            case WM_CLOSE:
                self->shouldQuit_ = true;
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    static std::wstring toWide(const std::string& s) {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(n - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
        return w;
    }

    HWND hwnd_ = nullptr;
    bool shouldQuit_ = false;
    bool trackingMouse_ = false;
    PaintCallback paintCb_;
    EventCallback eventCb_;
};

} // namespace

std::unique_ptr<Widget> createWin32Widget() {
    return std::make_unique<Win32Widget>();
}

} // namespace ui
#endif // _WIN32