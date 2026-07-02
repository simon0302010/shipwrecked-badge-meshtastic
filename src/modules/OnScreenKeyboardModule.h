#pragma once

#include "configuration.h"
#if HAS_SCREEN

#include "graphics/Screen.h" // InputEvent
#include "graphics/VirtualKeyboard.h"
#include <OLEDDisplay.h>
#include <functional>
#include <string>
#include <cstdint>

namespace graphics
{
class OnScreenKeyboardModule
{
  public:
    static OnScreenKeyboardModule &instance();

    void start(const char *header, const char *initialText, uint32_t durationMs,
               std::function<void(const std::string &)> callback);

    void stop(bool callEmptyCallback);

    void handleInput(const InputEvent &event);
    static bool processVirtualKeyboardInput(const InputEvent &event, VirtualKeyboard *keyboard);
    bool draw(OLEDDisplay *display);

    void showPopup(const char *title, const char *content, uint32_t durationMs);
    void clearPopup();
    void drawPopupOverlay(OLEDDisplay *display);

    // --- T9 character selector (Shipwrecked hardware keyboard) ---
    /// Check T9 timeout — call before drawing. Commits if timeout fired.
    void updateT9Timeout();
    /// Handle a raw input event for the T9 popup. Returns true if consumed.
    bool handleT9Event(const InputEvent &event);
    /// Draw the T9 character selector popup on top of the keyboard.
    void drawT9Popup(OLEDDisplay *display);

    /// Cycle the T9 selection forward by `dir` (call +1 for cycle).
    void t9CyclePublic(int dir) { t9Cycle(dir); }
    /// True while the T9 popup is shown.
    bool isT9Active() const { return t9Active; }
    /// Which table index (0-8) is active. -1 = none.
    int getT9ActiveTable() const { return t9ActiveTable; }
    /// Reset the T9 auto-commit timer (call when a same-key press arrives).
    void touchT9Timeout() { t9LastMs = millis(); }

  private:
    OnScreenKeyboardModule() = default;
    ~OnScreenKeyboardModule();
    OnScreenKeyboardModule(const OnScreenKeyboardModule &) = delete;
    OnScreenKeyboardModule &operator=(const OnScreenKeyboardModule &) = delete;

    void onSubmit(const std::string &text);
    void onCancel();

    void drawPopup(OLEDDisplay *display);

    VirtualKeyboard *keyboard = nullptr;
    std::function<void(const std::string &)> callback;

    char popupTitle[64] = {0};
    char popupMessage[256] = {0};
    uint32_t popupUntil = 0;
    bool popupVisible = false;

    // --- T9 popup state ---
    static constexpr int T9_MAX_OPTIONS = 16;
    static constexpr uint32_t T9_TIMEOUT_MS = 1500;
    bool t9Active = false;
    int t9ActiveTable = -1; // which T9 table (0-8) is displayed; -1 = none
    int t9Selection = 0;
    int t9OptionCount = 0;
    char t9Options[T9_MAX_OPTIONS];
    uint32_t t9LastMs = 0;
    int16_t t9PopupX, t9PopupY, t9PopupW, t9PopupH;

    void buildT9Options(int tableIdx);
    void commitT9Selection();
    void t9Cycle(int direction);
};

} // namespace graphics

#endif // HAS_SCREEN
