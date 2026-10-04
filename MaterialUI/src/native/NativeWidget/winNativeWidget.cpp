// src/native/NativeWidget/winNativeWidget.cpp
#include "NativeWidget.h"

#if defined(_WIN32)
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM

#include <string>
#include <memory>
#include <cmath>
#include <vector>
#include <cstdint>

#include "../../core/types.h"
#include "../../canvas/win32View.hpp"

namespace ui {
namespace {

// ---------- 窗口类名 ----------
const wchar_t* kClassName = L"UiWidgetClass";

// ============================================================
// Win32Widget
// ============================================================
class Win32Widget : public NativeWidget {
public:
    ~Win32Widget() override {
        if (hwnd_) DestroyWindow(hwnd_);
    }

    bool shouldQuit() const override { return shouldQuit_; }

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
    void setCloseCallback(CloseCallback cb) override { closeCb_ = std::move(cb); }

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
    CloseCallback closeCb_;
};

} // namespace

std::unique_ptr<NativeWidget> createWin32Widget() {
    return std::make_unique<Win32Widget>();
}

} // namespace ui
#endif // _WIN32