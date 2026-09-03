#ifndef JKPLATFORM_H
#define JKPLATFORM_H

#include <stdint.h>

struct SDL_Window;

namespace jk {

// Opaque platform window handle. The actual type (HWND on Windows,
// WindowRef on macOS, X11 Window on Linux) is hidden inside the PAL.
struct PlatformWindow;

// Per-display information reported in physical pixels.
struct DisplayInfo {
    int index = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int workX = 0;
    int workY = 0;
    int workWidth = 0;
    int workHeight = 0;
    float dpi = 96.0f;   // physical DPI along the primary axis
    float scale = 1.0f;  // dpi / 96.0f
    bool primary = false;
};

// Platform abstraction layer. All Win32/imm32/X11/Cocoa specifics live in
// OS-specific .cpp files. Headers stay clean of windows.h macros.
class JKPlatform {
public:
    // ---------------------------------------------------------------------
    // Process-level initialization
    // ---------------------------------------------------------------------
    // Set the process-wide DPI awareness mode before any SDL/window calls.
    // On Windows this requests per-monitor V2 awareness. Non-Windows: no-op.
    static void InitializeProcessDpiAwareness();

    // ---------------------------------------------------------------------
    // Display enumeration / multi-monitor
    // ---------------------------------------------------------------------
    static int GetDisplayCount();
    static bool GetDisplayInfo(int displayIndex, DisplayInfo& out);
    static int GetWindowDisplayIndex(SDL_Window* window);

    // ---------------------------------------------------------------------
    // Window handle / placement / DPI
    // ---------------------------------------------------------------------
    // Return the native window handle wrapped in an opaque type.
    static PlatformWindow* GetNativeWindow(SDL_Window* window);

    struct FrameMetrics {
        int frameX = 0;      // screen (physical px) frame left
        int frameY = 0;      // screen (physical px) frame top
        int frameW = 0;      // physical px frame width
        int frameH = 0;      // physical px frame height
        int clientX = 0;     // screen (physical px) client left
        int clientY = 0;     // screen (physical px) client top
        int clientW = 0;     // physical px client width
        int clientH = 0;     // physical px client height
        int borderLeft = 0;  // physical px left decoration
        int borderTop = 0;   // physical px top decoration (title bar)
        float dpiScale = 1.0f;
    };

    // Fill FrameMetrics for the given SDL window. Returns false if the OS
    // does not expose native frame metrics (non-Windows path falls back).
    static bool GetWindowFrameMetrics(SDL_Window* window, FrameMetrics& out);

    struct Placement {
        int clientPtX = 0;  // logical-point client origin to pass to SDL_SetWindowPosition
        int clientPtY = 0;
        int targetClientPxX = 0;  // physical px where the client should end up
        int targetClientPxY = 0;
    };

    // Compute a window placement that centers the frame (decorations included)
    // inside the monitor work area. The returned clientPt* values are in SDL
    // logical points and can be passed directly to SDL_SetWindowPosition.
    static bool ComputeCenteredPlacement(SDL_Window* window,
                                         int clientPtW, int clientPtH,
                                         Placement& out);

    // Check whether the window is currently using per-monitor V2 DPI awareness.
    // Mainly useful for diagnostic logging on Windows.
    static bool IsPerMonitorDpiAware(SDL_Window* window);

    // Force the OS to activate the window and give it keyboard focus.
    // On Windows this calls SetForegroundWindow; non-Windows: no-op.
    static void ActivateWindow(SDL_Window* window);

    // ---------------------------------------------------------------------
    // Mouse input
    // ---------------------------------------------------------------------
    // Return the mouse cursor position relative to the SDL window client area
    // in physical pixels. Falls back to SDL_GetGlobalMouseState on non-Windows.
    static bool GetPhysicalMousePos(SDL_Window* window, int& outX, int& outY);

    // Return the mouse cursor position relative to the SDL window client area
    // in SDL logical points. On Windows this uses Win32 ScreenToClient with the
    // monitor DPI scale factor; non-Windows falls back to SDL event coords.
    static bool GetLogicalMousePos(SDL_Window* window, int& outX, int& outY);

    // ---------------------------------------------------------------------
    // Synthetic input (testing)
    // ---------------------------------------------------------------------
    // Send a synthetic key press/release pair. Scan codes follow Win32/USB HID
    // conventions; the PAL translates to the native API.
    static bool SendSyntheticKey(uint32_t vkOrScancode, bool extended, bool keyUp);

    // Send a synthetic Unicode character through the OS input path.
    // On Windows this uses SendInput with KEYEVENTF_UNICODE. Non-Windows: no-op.
    static bool SendSyntheticChar(wchar_t ch);

    // ---------------------------------------------------------------------
    // IME (moved from the former JKPlatformIme PAL)
    // ---------------------------------------------------------------------
    enum class ImeMode {
        Ascii,
        Hangul,
        Unknown
    };

    // Query the OS IME conversion mode for the given SDL window.
    // Non-Windows platforms always return ImeMode::Unknown.
    static ImeMode GetCurrentConversionMode(SDL_Window* window);

    // Set the OS IME conversion mode (Windows only).
    static void SetConversionMode(SDL_Window* window, ImeMode mode);

    // Force the OS IME to complete/flush the active composition string.
    // Useful on focus loss so that the composed text is committed before the
    // control disappears.
    static void CompleteComposition(SDL_Window* window);
};

} // namespace jk

#endif // JKPLATFORM_H
