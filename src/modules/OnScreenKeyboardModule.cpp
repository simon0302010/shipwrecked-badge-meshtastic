#include "configuration.h"
#if HAS_SCREEN

#include "graphics/SharedUIDisplay.h"
#include "graphics/draw/NotificationRenderer.h"
#include "input/RotaryEncoderInterruptImpl1.h"
#include "input/UpDownInterruptImpl1.h"
#include "modules/OnScreenKeyboardModule.h"
#include <Arduino.h>
#include <algorithm>

namespace graphics
{

// --- T9 tables for hardware keyboard ---
static const char *t9_tables[9] = {
    ".,?!'\"@:;-()&%$", // 0: key 1  (SW15)
    "abc2",             // 1: key 2  (SW8)
    "def3",             // 2: key 3  (SW16)
    "ghi4",             // 3: key 4  (SW9)
    "jkl5",             // 4: key 5  (SW7)
    "mno6",             // 5: key 6  (SW18)
    "pqrs7",            // 6: key 7  (SW13)
    "tuv8",             // 7: key 8  (SW10)
    "wxyz9",            // 8: key 9  (SW6)
};

OnScreenKeyboardModule &OnScreenKeyboardModule::instance()
{
    static OnScreenKeyboardModule inst;
    return inst;
}

OnScreenKeyboardModule::~OnScreenKeyboardModule()
{
    if (keyboard) {
        delete keyboard;
        keyboard = nullptr;
    }
}

void OnScreenKeyboardModule::start(const char *header, const char *initialText, uint32_t durationMs,
                                   std::function<void(const std::string &)> cb)
{
    if (keyboard) {
        delete keyboard;
        keyboard = nullptr;
    }
    keyboard = new VirtualKeyboard();
    callback = cb;
    if (header)
        keyboard->setHeader(header);
    if (initialText)
        keyboard->setInputText(initialText);

    keyboard->setCallback([this](const std::string &text) {
        if (text.empty()) {
            this->onCancel();
        } else {
            this->onSubmit(text);
        }
    });

    NotificationRenderer::virtualKeyboard = keyboard;
    NotificationRenderer::textInputCallback = callback;
}

void OnScreenKeyboardModule::stop(bool callEmptyCallback)
{
    auto cb = callback;
    callback = nullptr;
    if (keyboard) {
        delete keyboard;
        keyboard = nullptr;
    }
    NotificationRenderer::virtualKeyboard = nullptr;
    NotificationRenderer::textInputCallback = nullptr;
    clearPopup();
    t9Active = false;
    t9ActiveTable = -1;

    if (callEmptyCallback && cb)
        cb("");
}

void OnScreenKeyboardModule::handleInput(const InputEvent &event)
{
    if (!keyboard)
        return;

    if (handleT9Event(event))
        return;

    if (processVirtualKeyboardInput(event, keyboard))
        return;

    if (event.inputEvent == INPUT_BROKER_ANYKEY && event.kbchar != 0) {
        std::string text = keyboard->getInputText();
        text += static_cast<char>(event.kbchar);
        keyboard->setInputText(text);
        return;
    }

    if (event.inputEvent == INPUT_BROKER_BACK) {
        std::string text = keyboard->getInputText();
        if (!text.empty()) {
            text.pop_back();
            keyboard->setInputText(text);
        }
        return;
    }

    if (event.inputEvent == INPUT_BROKER_CANCEL)
        onCancel();
}

bool OnScreenKeyboardModule::processVirtualKeyboardInput(const InputEvent &event, VirtualKeyboard *targetKeyboard)
{
    if (!targetKeyboard)
        return false;

    switch (event.inputEvent) {
    case INPUT_BROKER_UP:
    case INPUT_BROKER_UP_LONG:
        targetKeyboard->moveCursorUp();
        return true;
    case INPUT_BROKER_DOWN:
    case INPUT_BROKER_DOWN_LONG:
        targetKeyboard->moveCursorDown();
        return true;
    case INPUT_BROKER_LEFT:
    case INPUT_BROKER_ALT_PRESS:
        targetKeyboard->moveCursorLeft();
        return true;
    case INPUT_BROKER_RIGHT:
    case INPUT_BROKER_USER_PRESS:
        targetKeyboard->moveCursorRight();
        return true;
    case INPUT_BROKER_SELECT:
        targetKeyboard->handlePress();
        return true;
    case INPUT_BROKER_SELECT_LONG:
        targetKeyboard->handleLongPress();
        return true;
    default:
        return false;
    }
}

bool OnScreenKeyboardModule::draw(OLEDDisplay *display)
{
    if (!keyboard)
        return false;

    if (keyboard->isTimedOut()) {
        onCancel();
        return false;
    }

    display->setColor(BLACK);
    display->fillRect(0, 0, display->getWidth(), display->getHeight());
    display->setColor(WHITE);
    keyboard->draw(display, 0, 0);

    drawPopup(display);
    drawT9Popup(display);
    return true;
}

// --- T9 popup implementation ---

void OnScreenKeyboardModule::buildT9Options(int tableIdx)
{
    t9OptionCount = 0;
    t9Selection = 0;
    t9ActiveTable = tableIdx;
    if (tableIdx < 0 || tableIdx >= 9)
        return;
    const char *tbl = t9_tables[tableIdx];
    for (int i = 0; tbl[i] && t9OptionCount < T9_MAX_OPTIONS - 1; i++)
        t9Options[t9OptionCount++] = tbl[i];
    t9Options[t9OptionCount++] = '\0'; // blank option
    t9Active = true;
    t9LastMs = millis();
}

void OnScreenKeyboardModule::commitT9Selection()
{
    if (!t9Active || !keyboard || t9Selection >= t9OptionCount)
        return;
    char ch = t9Options[t9Selection];
    if (ch != '\0') {
        std::string text = keyboard->getInputText();
        text += ch;
        keyboard->setInputText(text);
    }
    t9Active = false;
    t9ActiveTable = -1;
}

void OnScreenKeyboardModule::t9Cycle(int direction)
{
    if (!t9Active || t9OptionCount == 0)
        return;
    t9Selection = (t9Selection + direction) % t9OptionCount;
    if (t9Selection < 0)
        t9Selection += t9OptionCount;
    t9LastMs = millis();
}

void OnScreenKeyboardModule::updateT9Timeout()
{
    if (t9Active && millis() - t9LastMs > T9_TIMEOUT_MS)
        commitT9Selection();
}

bool OnScreenKeyboardModule::handleT9Event(const InputEvent &event)
{
    if (!keyboard)
        return false;

    // MATRIXKEY: same key → cycle, different key → commit + switch
    if (event.inputEvent == INPUT_BROKER_MATRIXKEY) {
        if (t9Active && event.kbchar == t9ActiveTable) {
            t9Cycle(1); // press same key again → next option (classic multi-tap)
        } else {
            commitT9Selection();
            buildT9Options(event.kbchar);
        }
        return true;
    }

    if (!t9Active)
        return false;

    // While popup is open, intercept nav keys
    if (event.inputEvent == INPUT_BROKER_LEFT || event.inputEvent == INPUT_BROKER_UP) {
        t9Cycle(-1);
        return true;
    }
    if (event.inputEvent == INPUT_BROKER_RIGHT || event.inputEvent == INPUT_BROKER_DOWN) {
        t9Cycle(1);
        return true;
    }
    if (event.inputEvent == INPUT_BROKER_SELECT) {
        commitT9Selection();
        return true;
    }
    if (event.inputEvent == INPUT_BROKER_BACK) {
        t9Active = false;
        t9ActiveTable = -1; // close popup, don't commit, don't backspace
        return true;
    }
    // SPACE while popup open: commit current selection, then let space insert
    if (event.inputEvent == INPUT_BROKER_ANYKEY && event.kbchar == ' ') {
        commitT9Selection();
        return false; // fall through so NotificationRenderer inserts the space
    }

    return false;
}

void OnScreenKeyboardModule::drawT9Popup(OLEDDisplay *display)
{
    if (!t9Active || t9OptionCount == 0)
        return;

    int n = t9OptionCount;
    int cellW = 18;
    int gap = 2;
    int pad = 6;
    int totalW = n * (cellW + gap) - gap + pad * 2;
    int maxW = display->getWidth() - 16;

    int start = 0;
    int shown = n;
    if (totalW > maxW) {
        int maxCells = (maxW - pad * 2 + gap) / (cellW + gap);
        if (maxCells < 1)
            maxCells = 1;
        shown = maxCells;
        start = t9Selection - shown / 2;
        if (start < 0)
            start = 0;
        if (start + shown > n)
            start = n - shown;
        totalW = shown * (cellW + gap) - gap + pad * 2;
    }

    int popupX = (display->getWidth() - totalW) / 2;
    int popupY = 72;
    int popupH = 26;

    display->setColor(BLACK);
    display->fillRect(popupX, popupY, totalW, popupH);
    display->setColor(WHITE);
    display->drawRect(popupX, popupY, totalW, popupH);

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    for (int i = 0; i < shown; i++) {
        int idx = start + i;
        int cx = popupX + pad + i * (cellW + gap);
        int cy = popupY + (popupH - FONT_HEIGHT_SMALL) / 2;

        if (idx == t9Selection) {
            display->setColor(WHITE);
            display->fillRect(cx - 1, popupY + 2, cellW + 2, popupH - 4);
            display->setColor(BLACK);
        } else {
            display->setColor(WHITE);
        }

        char ch = t9Options[idx];
        if (ch == '\0') {
            int dashW = 8;
            int dashX = cx + (cellW - dashW) / 2;
            int dashY = popupY + popupH / 2;
            display->drawHorizontalLine(dashX, dashY, dashW);
        } else {
            char str[2] = {ch, '\0'};
            int strW = display->getStringWidth(str);
            display->drawString(cx + (cellW - strW) / 2, cy, str);
        }
    }

    t9PopupX = popupX;
    t9PopupY = popupY;
    t9PopupW = totalW;
    t9PopupH = popupH;
}

// --- Notification-style popup ---

void OnScreenKeyboardModule::onSubmit(const std::string &text)
{
    auto cb = callback;
    stop(false);
    if (cb)
        cb(text);
}

void OnScreenKeyboardModule::onCancel()
{
    stop(true);
}

void OnScreenKeyboardModule::showPopup(const char *title, const char *content, uint32_t durationMs)
{
    if (!title || !content)
        return;
    strncpy(popupTitle, title, sizeof(popupTitle) - 1);
    popupTitle[sizeof(popupTitle) - 1] = '\0';
    strncpy(popupMessage, content, sizeof(popupMessage) - 1);
    popupMessage[sizeof(popupMessage) - 1] = '\0';
    popupUntil = millis() + durationMs;
    popupVisible = true;
}

void OnScreenKeyboardModule::clearPopup()
{
    popupTitle[0] = '\0';
    popupMessage[0] = '\0';
    popupUntil = 0;
    popupVisible = false;
}

void OnScreenKeyboardModule::drawPopupOverlay(OLEDDisplay *display)
{
    drawPopup(display);
    drawT9Popup(display);
}

void OnScreenKeyboardModule::drawPopup(OLEDDisplay *display)
{
    if (!popupVisible)
        return;
    if (millis() > popupUntil || popupMessage[0] == '\0') {
        popupVisible = false;
        return;
    }

    constexpr uint16_t maxContentLines = 3;
    const bool hasTitle = popupTitle[0] != '\0';

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const uint16_t maxWrapWidth = display->width() - 40;

    auto wrapText = [&](const char *text, uint16_t availableWidth) -> std::vector<std::string> {
        std::vector<std::string> wrapped;
        std::string current;
        std::string word;
        const char *p = text;
        while (*p && wrapped.size() < maxContentLines) {
            while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
                if (*p == '\n') {
                    if (!current.empty()) {
                        wrapped.push_back(current);
                        current.clear();
                        if (wrapped.size() >= maxContentLines)
                            break;
                    }
                }
                ++p;
            }
            if (!*p || wrapped.size() >= maxContentLines)
                break;
            word.clear();
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
                word += *p++;
            if (word.empty())
                continue;
            std::string test = current.empty() ? word : (current + " " + word);
            uint16_t w = display->getStringWidth(test.c_str(), test.length(), true);
            if (w <= availableWidth)
                current = test;
            else {
                if (!current.empty()) {
                    wrapped.push_back(current);
                    current = word;
                    if (wrapped.size() >= maxContentLines)
                        break;
                } else {
                    current = word;
                    while (current.size() > 1 &&
                           display->getStringWidth(current.c_str(), current.length(), true) > availableWidth)
                        current.pop_back();
                }
            }
        }
        if (!current.empty() && wrapped.size() < maxContentLines)
            wrapped.push_back(current);
        return wrapped;
    };

    std::vector<std::string> allLines;
    if (hasTitle)
        allLines.emplace_back(popupTitle);

    char buf[sizeof(popupMessage)];
    strncpy(buf, popupMessage, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *paragraph = strtok(buf, "\n");
    while (paragraph && allLines.size() < maxContentLines + (hasTitle ? 1 : 0)) {
        auto wrapped = wrapText(paragraph, maxWrapWidth);
        for (const auto &ln : wrapped) {
            if (allLines.size() >= maxContentLines + (hasTitle ? 1 : 0))
                break;
            allLines.push_back(ln);
        }
        paragraph = strtok(nullptr, "\n");
    }

    std::vector<const char *> ptrs;
    for (const auto &ln : allLines)
        ptrs.push_back(ln.c_str());
    ptrs.push_back(nullptr);

    NotificationRenderer::drawNotificationBox(display, nullptr, ptrs.data(), allLines.size(), 0, 0);
}

} // namespace graphics

#endif // HAS_SCREEN
