#pragma once

// GameTools.h
// Lightweight Win32 window and input abstraction.
// Define GAMETOOLS_ENABLE_IMGUI=1 to enable Dear ImGui Win32 message handling.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef GAMETOOLS_ENABLE_IMGUI
#define GAMETOOLS_ENABLE_IMGUI 0
#endif

#if GAMETOOLS_ENABLE_IMGUI
#if !__has_include(<imgui.h>)
#error "GAMETOOLS_ENABLE_IMGUI=1 requires <imgui.h>."
#endif
#if !__has_include(<imgui_impl_win32.h>)
#error "GAMETOOLS_ENABLE_IMGUI=1 requires <imgui_impl_win32.h>."
#endif
#include <imgui.h>
#include <imgui_impl_win32.h>
#endif

namespace Vultaik
{
    /// Win32 keyboard and mouse state tracker.
    class GameInput final
    {
    public:
        enum class KeyState : uint8_t
        {
            Up,
            Down,
            Pressed,
            Released
        };

        enum class KeyCode : uint16_t
        {
            A = 'A', B, C, D, E, F, G, H, I, J, K, L, M,
            N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

            Num0 = '0', Num1, Num2, Num3, Num4,
            Num5, Num6, Num7, Num8, Num9,

            Space = VK_SPACE,
            Enter = VK_RETURN,
            Escape = VK_ESCAPE,
            Shift = VK_SHIFT,
            Control = VK_CONTROL,
            Alt = VK_MENU,
            Tab = VK_TAB,
            Backspace = VK_BACK,

            Left = VK_LEFT,
            Right = VK_RIGHT,
            Up = VK_UP,
            Down = VK_DOWN,

            F1 = VK_F1, F2, F3, F4, F5, F6,
            F7, F8, F9, F10, F11, F12
        };

        enum class MouseButton : uint8_t
        {
            Left,
            Right,
            Middle,
            Button4,
            Button5
        };

        // Absolute uses client-space cursor coordinates; Relative uses Raw Input deltas.
        enum class MouseMode : uint8_t
        {
            Absolute,
            Relative
        };

        using KeyCallback = std::function<void(KeyCode, KeyState)>;
        using MouseCallback = std::function<void(int, int, MouseButton, KeyState)>;
        using MouseMoveCallback = std::function<void(int, int)>;
        using ScrollCallback = std::function<void(float)>;
        using CallbackId = uint64_t;

        GameInput() = delete;

        static void Initialize(HWND window = nullptr)
        {
            if (m_initialized)
                return;

            m_window = window;

            m_currentKeys.fill(false);
            m_previousKeys.fill(false);
            m_currentMouseButtons.fill(false);
            m_previousMouseButtons.fill(false);

            m_mouseX = 0;
            m_mouseY = 0;
            m_mouseDeltaX = 0;
            m_mouseDeltaY = 0;
            m_scrollDelta = 0.0f;

            m_hasMousePosition = false;
            m_hasFocus = window ? GetFocus() == window : true;

            if (m_window)
                RegisterRawMouse();

            m_initialized = true;
            ApplyMouseModeForFocus();
        }

        static void Shutdown()
        {
            if (!m_initialized)
                return;

            ReleaseCursorClip();
            SetCursorHidden(false);

            m_keyCallbacks.clear();
            m_mouseCallbacks.clear();
            m_mouseMoveCallbacks.clear();
            m_scrollCallbacks.clear();

            m_currentKeys.fill(false);
            m_previousKeys.fill(false);
            m_currentMouseButtons.fill(false);
            m_previousMouseButtons.fill(false);

            m_mouseDeltaX = 0;
            m_mouseDeltaY = 0;
            m_scrollDelta = 0.0f;

            m_window = nullptr;
            m_initialized = false;
            m_hasFocus = false;
            m_hasMousePosition = false;
        }

        // Advances transient input state once per frame.
        static void BeginFrame()
        {
            if (!m_initialized)
                return;

            m_previousKeys = m_currentKeys;
            m_previousMouseButtons = m_currentMouseButtons;
            m_mouseDeltaX = 0;
            m_mouseDeltaY = 0;
            m_scrollDelta = 0.0f;
        }

        // Backward-compatible alias.
        static void Update()
        {
            BeginFrame();
        }

        static LRESULT ProcessMessage(HWND, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if (!m_initialized)
                return 0;

            switch (message)
            {
            case WM_SETFOCUS:
                m_hasFocus = true;
                m_hasMousePosition = false;
                ApplyMouseModeForFocus();
                break;

            case WM_KILLFOCUS:
                m_hasFocus = false;
                ClearHeldInput();
                ReleaseCursorClip();
                SetCursorHidden(false);
                break;

            case WM_ACTIVATEAPP:
                if (wParam)
                {
                    m_hasFocus = true;
                    m_hasMousePosition = false;
                    ApplyMouseModeForFocus();
                }
                else
                {
                    m_hasFocus = false;
                    ClearHeldInput();
                    ReleaseCursorClip();
                    SetCursorHidden(false);
                }
                break;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                if (wParam < KeyCount)
                {
                    const KeyCode key = static_cast<KeyCode>(wParam);
                    const size_t index = KeyIndex(key);
                    const bool wasDown = m_currentKeys[index];
                    m_currentKeys[index] = true;

                    // Emit Pressed only on the up-to-down transition.
                    if (!wasDown)
                    {
                        for (const auto& entry : m_keyCallbacks)
                        {
                            if (entry.callback)
                                entry.callback(key, KeyState::Pressed);
                        }
                    }
                }
                break;
            }

            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                if (wParam < KeyCount)
                {
                    const KeyCode key = static_cast<KeyCode>(wParam);
                    const size_t index = KeyIndex(key);
                    m_currentKeys[index] = false;

                    for (const auto& entry : m_keyCallbacks)
                    {
                        if (entry.callback)
                            entry.callback(key, KeyState::Released);
                    }
                }
                break;
            }

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            {
                const MouseButton button = MessageToMouseButton(message, wParam);
                SetMouseButton(button, true, lParam);

                if (m_window)
                    SetCapture(m_window);

                return message == WM_XBUTTONDOWN ? TRUE : 0;
            }

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
            {
                const MouseButton button = MessageToMouseButton(message, wParam);
                SetMouseButton(button, false, lParam);

                if (!AnyMouseButtonDown() && GetCapture() == m_window)
                    ReleaseCapture();

                return message == WM_XBUTTONUP ? TRUE : 0;
            }

            case WM_MOUSEMOVE:
            {
                const int newX = GET_X_LPARAM(lParam);
                const int newY = GET_Y_LPARAM(lParam);

                if (m_mouseMode == MouseMode::Absolute)
                {
                    if (m_hasMousePosition)
                    {
                        m_mouseDeltaX += newX - m_mouseX;
                        m_mouseDeltaY += newY - m_mouseY;
                    }

                    m_mouseX = newX;
                    m_mouseY = newY;
                    m_hasMousePosition = true;
                }
                else
                {
                    // Relative motion is accumulated from WM_INPUT.
                    m_mouseX = newX;
                    m_mouseY = newY;
                }

                for (const auto& entry : m_mouseMoveCallbacks)
                {
                    if (entry.callback)
                        entry.callback(m_mouseX, m_mouseY);
                }
                break;
            }

            case WM_INPUT:
                if (m_mouseMode == MouseMode::Relative && m_hasFocus)
                    ProcessRawMouse(lParam);
                break;

            case WM_MOUSEWHEEL:
            {
                const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);

                m_scrollDelta += delta;

                for (const auto& entry : m_scrollCallbacks)
                {
                    if (entry.callback)
                        entry.callback(delta);
                }
                break;
            }

            case WM_SIZE:
                if (m_mouseMode == MouseMode::Relative && m_hasFocus)
                    UpdateCursorClip();
                break;
            }

            return 0;
        }

        static bool IsKeyDown(KeyCode key)
        {
            return m_currentKeys[KeyIndex(key)];
        }

        static bool IsKeyUp(KeyCode key)
        {
            return !IsKeyDown(key);
        }

        static bool IsKeyPressed(KeyCode key)
        {
            const size_t index = KeyIndex(key);
            return m_currentKeys[index] && !m_previousKeys[index];
        }

        static bool IsKeyReleased(KeyCode key)
        {
            const size_t index = KeyIndex(key);
            return !m_currentKeys[index] && m_previousKeys[index];
        }

        static bool IsMouseButtonDown(MouseButton button)
        {
            return m_currentMouseButtons[MouseIndex(button)];
        }

        static bool IsMouseButtonUp(MouseButton button)
        {
            return !IsMouseButtonDown(button);
        }

        static bool IsMouseButtonPressed(MouseButton button)
        {
            const size_t index = MouseIndex(button);
            return m_currentMouseButtons[index] && !m_previousMouseButtons[index];
        }

        static bool IsMouseButtonReleased(MouseButton button)
        {
            const size_t index = MouseIndex(button);
            return !m_currentMouseButtons[index] && m_previousMouseButtons[index];
        }

        static void GetMousePosition(int& x, int& y)
        {
            x = m_mouseX;
            y = m_mouseY;
        }

        static int GetMouseX() { return m_mouseX; }
        static int GetMouseY() { return m_mouseY; }

        static void GetMouseDelta(int& deltaX, int& deltaY)
        {
            deltaX = m_mouseDeltaX;
            deltaY = m_mouseDeltaY;
        }

        static int GetMouseDeltaX() { return m_mouseDeltaX; }
        static int GetMouseDeltaY() { return m_mouseDeltaY; }
        static float GetScrollDelta() { return m_scrollDelta; }

        static void SetMouseMode(MouseMode mode)
        {
            if (m_mouseMode == mode)
                return;

            m_mouseMode = mode;
            m_mouseDeltaX = 0;
            m_mouseDeltaY = 0;
            m_hasMousePosition = false;
            ApplyMouseModeForFocus();
        }

        static MouseMode GetMouseMode() { return m_mouseMode; }
        static bool IsFocused() { return m_hasFocus; }

        static CallbackId AddKeyCallback(const KeyCallback& callback)
        {
            const CallbackId id = NextCallbackId();
            m_keyCallbacks.push_back({ id, callback });
            return id;
        }

        static CallbackId AddMouseCallback(const MouseCallback& callback)
        {
            const CallbackId id = NextCallbackId();
            m_mouseCallbacks.push_back({ id, callback });
            return id;
        }

        static CallbackId AddMouseMoveCallback(const MouseMoveCallback& callback)
        {
            const CallbackId id = NextCallbackId();
            m_mouseMoveCallbacks.push_back({ id, callback });
            return id;
        }

        static CallbackId AddScrollCallback(const ScrollCallback& callback)
        {
            const CallbackId id = NextCallbackId();
            m_scrollCallbacks.push_back({ id, callback });
            return id;
        }

        static void RemoveCallback(CallbackId id)
        {
            EraseCallback(m_keyCallbacks, id);
            EraseCallback(m_mouseCallbacks, id);
            EraseCallback(m_mouseMoveCallbacks, id);
            EraseCallback(m_scrollCallbacks, id);
        }

    private:
        static constexpr size_t KeyCount = 256;
        static constexpr size_t MouseButtonCount = 5;

        template<typename CallbackT>
        struct CallbackEntry
        {
            CallbackId id = 0;
            CallbackT callback;
        };

        static size_t KeyIndex(KeyCode key)
        {
            return static_cast<size_t>(key);
        }

        static size_t MouseIndex(MouseButton button)
        {
            return static_cast<size_t>(button);
        }

        static MouseButton MessageToMouseButton(UINT message, WPARAM wParam)
        {
            switch (message)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return MouseButton::Left;

            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return MouseButton::Right;

            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return MouseButton::Middle;

            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MouseButton::Button4 : MouseButton::Button5;

            default:
                return MouseButton::Left;
            }
        }

        static void SetMouseButton(MouseButton button, bool down, LPARAM lParam)
        {
            const size_t index = MouseIndex(button);
            const bool wasDown = m_currentMouseButtons[index];
            m_currentMouseButtons[index] = down;

            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);

            m_mouseX = x;
            m_mouseY = y;
            m_hasMousePosition = true;

            if (down && wasDown)
                return;

            if (!down && !wasDown)
                return;

            const KeyState state = down ? KeyState::Pressed : KeyState::Released;

            for (const auto& entry : m_mouseCallbacks)
            {
                if (entry.callback)
                    entry.callback(x, y, button, state);
            }
        }

        static bool AnyMouseButtonDown()
        {
            for (bool down : m_currentMouseButtons)
            {
                if (down)
                    return true;
            }
            return false;
        }

        static void ClearHeldInput()
        {
            m_currentKeys.fill(false);
            m_currentMouseButtons.fill(false);
            m_mouseDeltaX = 0;
            m_mouseDeltaY = 0;
            m_scrollDelta = 0.0f;
            m_hasMousePosition = false;

            if (GetCapture() == m_window)
                ReleaseCapture();
        }

        static void RegisterRawMouse()
        {
            RAWINPUTDEVICE device{};
            device.usUsagePage = 0x01; // Generic Desktop Controls
            device.usUsage = 0x02;     // Mouse
            device.dwFlags = 0;
            device.hwndTarget = m_window;

            if (!RegisterRawInputDevices(&device, 1, sizeof(device)))
                throw std::runtime_error("Failed to register Raw Input mouse.");
        }

        static void ProcessRawMouse(LPARAM lParam)
        {
            RAWINPUT raw{};
            UINT size = sizeof(raw);
            const UINT result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

            if (result == static_cast<UINT>(-1))
                return;

            if (raw.header.dwType != RIM_TYPEMOUSE)
                return;

            // Ignore absolute Raw Input devices.
            if ((raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
            {
                m_mouseDeltaX += raw.data.mouse.lLastX;
                m_mouseDeltaY += raw.data.mouse.lLastY;
            }
        }

        static void ApplyMouseModeForFocus()
        {
            if (!m_initialized)
                return;

            if (m_mouseMode == MouseMode::Relative && m_hasFocus)
            {
                SetCursorHidden(true);
                UpdateCursorClip();
            }
            else
            {
                ReleaseCursorClip();
                SetCursorHidden(false);
            }
        }

        static void SetCursorHidden(bool hidden)
        {
            if (hidden == m_cursorHidden)
                return;

            if (hidden)
            {
                while (ShowCursor(FALSE) >= 0) {}
                m_cursorHidden = true;
            }
            else
            {
                while (ShowCursor(TRUE) < 0) {}
                m_cursorHidden = false;
            }
        }

        static void UpdateCursorClip()
        {
            if (!m_window || !IsWindow(m_window))
                return;

            RECT client{};
            if (!GetClientRect(m_window, &client))
                return;

            POINT topLeft{ client.left, client.top };
            POINT bottomRight{ client.right, client.bottom };

            ClientToScreen(m_window, &topLeft);
            ClientToScreen(m_window, &bottomRight);

            RECT screenRect{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };

            ClipCursor(&screenRect);
        }

        static void ReleaseCursorClip()
        {
            ClipCursor(nullptr);
        }

        static CallbackId NextCallbackId()
        {
            return m_nextCallbackId++;
        }

        template<typename CallbackT>
        static void EraseCallback(std::vector<CallbackEntry<CallbackT>>& entries, CallbackId id)
        {
            entries.erase(std::remove_if(entries.begin(), entries.end(), [id](const CallbackEntry<CallbackT>& entry) { return entry.id == id; }), entries.end());
        }

        inline static std::array<bool, KeyCount> m_currentKeys{};
        inline static std::array<bool, KeyCount> m_previousKeys{};
        inline static std::array<bool, MouseButtonCount> m_currentMouseButtons{};
        inline static std::array<bool, MouseButtonCount> m_previousMouseButtons{};

        inline static int m_mouseX = 0;
        inline static int m_mouseY = 0;
        inline static int m_mouseDeltaX = 0;
        inline static int m_mouseDeltaY = 0;
        inline static float m_scrollDelta = 0.0f;

        inline static MouseMode m_mouseMode = MouseMode::Absolute;
        inline static HWND m_window = nullptr;

        inline static bool m_initialized = false;
        inline static bool m_hasFocus = false;
        inline static bool m_hasMousePosition = false;
        inline static bool m_cursorHidden = false;

        inline static CallbackId m_nextCallbackId = 1;

        inline static std::vector<CallbackEntry<KeyCallback>> m_keyCallbacks;
        inline static std::vector<CallbackEntry<MouseCallback>> m_mouseCallbacks;
        inline static std::vector<CallbackEntry<MouseMoveCallback>> m_mouseMoveCallbacks;
        inline static std::vector<CallbackEntry<ScrollCallback>> m_scrollCallbacks;
    };

    /// DPI-aware Win32 window wrapper.
    class GameWindow final
    {
    public:
        struct Config
        {
            std::wstring title = L"Roll-a-Ball";

            // Requested client-area dimensions.
            uint32_t width = 1280;
            uint32_t height = 720;

            bool resizable = true;
        };

        GameWindow() = default;

        ~GameWindow()
        {
            Shutdown();
        }

        GameWindow(const GameWindow&) = delete;
        GameWindow& operator=(const GameWindow&) = delete;

        void Initialize()
        {
            Initialize(Config{});
        }

        void Initialize(const Config& config)
        {
            if (m_handle)
                return;

            EnableDpiAwareness();

            m_instance = GetModuleHandleW(nullptr);

            WNDCLASSW windowClass{};
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            windowClass.lpfnWndProc = WindowProc;
            windowClass.hInstance = m_instance;
            windowClass.lpszClassName = WindowClassName;
            windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);

            const ATOM atom = RegisterClassW(&windowClass);

            if (!atom)
            {
                if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                    throw std::runtime_error("Failed to register the game window class.");

                m_classRegistered = false;
            }
            else
            {
                m_classRegistered = true;
            }

            DWORD style = WS_OVERLAPPEDWINDOW;

            if (!config.resizable)
            {
                style &= ~WS_THICKFRAME;
                style &= ~WS_MAXIMIZEBOX;
            }

            constexpr DWORD exStyle = 0;
            RECT windowRect{ 0, 0, static_cast<LONG>(config.width), static_cast<LONG>(config.height) };
            const UINT dpi = GetInitialDpi();

            if (!AdjustWindowRectExForDpi(&windowRect, style, FALSE, exStyle, dpi))
                throw std::runtime_error("Failed to calculate the game window size.");

            const int windowWidth = windowRect.right - windowRect.left;
            const int windowHeight = windowRect.bottom - windowRect.top;
            m_handle = CreateWindowExW(exStyle, WindowClassName, config.title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight, nullptr, nullptr, m_instance, this);

            if (!m_handle)
                throw std::runtime_error("Failed to create the game window.");

            ShowWindow(m_handle, SW_SHOW);
            UpdateWindow(m_handle);

            UpdateClientSize();
            GameInput::Initialize(m_handle);

            m_focused = GetFocus() == m_handle;
            m_running = true;
        }

        void Shutdown()
        {
            GameInput::Shutdown();

            if (m_handle && IsWindow(m_handle))
                DestroyWindow(m_handle);

            m_handle = nullptr;
            m_running = false;
            m_focused = false;
            m_minimized = false;

            if (m_classRegistered)
            {
                UnregisterClassW(WindowClassName, m_instance);
                m_classRegistered = false;
            }

            m_instance = nullptr;
        }

        void PumpMessages()
        {
            GameInput::BeginFrame();

            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);

                if (message.message == WM_QUIT)
                    m_running = false;
            }
        }

        bool IsRunning() const { return m_running; }
        bool IsFocused() const { return m_focused; }
        bool IsMinimized() const { return m_minimized; }

        HWND Handle() const { return m_handle; }
        uint32_t ClientWidth() const { return m_clientWidth; }
        uint32_t ClientHeight() const { return m_clientHeight; }

        bool WasResized() const { return m_resized; }
        void ClearResizeFlag() { m_resized = false; }

        void SetTitle(const std::wstring& title)
        {
            if (m_handle)
                SetWindowTextW(m_handle, title.c_str());
        }

        // Compatibility alias for SetClientSize().
        void SetWindowSize(uint32_t width, uint32_t height)
        {
            SetClientSize(width, height);
        }

        void SetClientSize(uint32_t width, uint32_t height)
        {
            if (!m_handle || !IsWindow(m_handle))
                return;

            const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(m_handle, GWL_STYLE));
            const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(m_handle, GWL_EXSTYLE));
            RECT rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
            const UINT dpi = GetDpiForWindow(m_handle);

            if (!AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, dpi))
                return;

            const int outerWidth = rect.right - rect.left;
            const int outerHeight = rect.bottom - rect.top;
            SetWindowPos(m_handle, nullptr, 0, 0, outerWidth, outerHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

            UpdateClientSize();
        }

    private:
        inline static constexpr wchar_t WindowClassName[] = L"Engine.GameWindow";

        static void EnableDpiAwareness()
        {
            // Preserve an existing process DPI configuration when already established.
            SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }

        static UINT GetInitialDpi()
        {
            const UINT dpi = GetDpiForSystem();
            return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
        }

        LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
        {
            // Device state is tracked independently of UI event consumption.
            const LRESULT inputResult = GameInput::ProcessMessage(m_handle, message, wParam, lParam);

#if GAMETOOLS_ENABLE_IMGUI
            const bool imguiHandled = ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler(m_handle, message, wParam, lParam);
#else
            const bool imguiHandled = false;
#endif

            switch (message)
            {
            case WM_SETFOCUS:
                m_focused = true;
                break;

            case WM_KILLFOCUS:
                m_focused = false;
                break;

            case WM_SIZE:
            {
                if (wParam == SIZE_MINIMIZED)
                {
                    m_minimized = true;
                    break;
                }

                m_minimized = false;

                const uint32_t newWidth = static_cast<uint32_t>(LOWORD(lParam));
                const uint32_t newHeight = static_cast<uint32_t>(HIWORD(lParam));

                // Suppress zero-sized resize notifications while minimized.
                if (newWidth > 0 && newHeight > 0)
                {
                    if (newWidth != m_clientWidth || newHeight != m_clientHeight)
                    {
                        m_clientWidth = newWidth;
                        m_clientHeight = newHeight;
                        m_resized = true;
                    }
                }
                break;
            }

            case WM_DPICHANGED:
            {
                const RECT* suggested = reinterpret_cast<const RECT*>(lParam);

                if (suggested)
                {
                    SetWindowPos(m_handle, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
                }

                UpdateClientSize();
                break;
            }

            case WM_CLOSE:
                DestroyWindow(m_handle);
                return 0;

            case WM_DESTROY:
                m_running = false;
                PostQuitMessage(0);
                return 0;

            case WM_ERASEBKGND:
                return 1;
            }

            if (imguiHandled)
                return 1;

            if (inputResult != 0)
                return inputResult;

            return DefWindowProcW(m_handle, message, wParam, lParam);
        }

        static LRESULT CALLBACK WindowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
        {
            GameWindow* window = reinterpret_cast<GameWindow*>(GetWindowLongPtrW(handle, GWLP_USERDATA));

            if (message == WM_NCCREATE)
            {
                auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);

                window = static_cast<GameWindow*>(create->lpCreateParams);

                window->m_handle = handle;

                SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            }

            return window ? window->HandleMessage(message, wParam, lParam) : DefWindowProcW(handle, message, wParam, lParam);
        }

        void UpdateClientSize()
        {
            if (!m_handle || !IsWindow(m_handle))
                return;

            RECT clientRect{};
            if (!GetClientRect(m_handle, &clientRect))
                return;

            const uint32_t width = static_cast<uint32_t>(clientRect.right - clientRect.left);
            const uint32_t height = static_cast<uint32_t>(clientRect.bottom - clientRect.top);

            if (width == 0 || height == 0)
                return;

            if (width != m_clientWidth || height != m_clientHeight)
            {
                m_clientWidth = width;
                m_clientHeight = height;
                m_resized = true;
            }
        }

        HINSTANCE m_instance = nullptr;
        HWND m_handle = nullptr;

        bool m_running = false;
        bool m_classRegistered = false;
        bool m_resized = false;
        bool m_minimized = false;
        bool m_focused = false;

        uint32_t m_clientWidth = 0;
        uint32_t m_clientHeight = 0;
    };

}