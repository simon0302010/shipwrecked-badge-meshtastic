#include "configuration.h"
#if HAS_SCREEN

#include "MessageInboxModule.h"
#include "NodeDB.h"
#include "graphics/Screen.h"
#include <algorithm>
#include <cstring>

MessageInboxModule *messageInboxModule;

MessageInboxModule::MessageInboxModule()
    : SinglePortModule("inbox", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
    inputObserver.observe(inputBroker);
}

// ---------------------------------------------------------------------------
// Mesh message handling
// ---------------------------------------------------------------------------

ProcessMessage MessageInboxModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (mp.from == 0 || mp.from == nodeDB->getNodeNum())
        return ProcessMessage::CONTINUE;

    size_t len = mp.decoded.payload.size;
    if (len == 0)
        return ProcessMessage::CONTINUE;

    InboxEntry entry;
    entry.sender = mp.from;
    entry.timestamp = mp.rx_time ? mp.rx_time : (millis() / 1000);
    entry.read = false;

    size_t copyLen = (len >= sizeof(entry.text)) ? sizeof(entry.text) - 1 : len;
    memcpy(entry.text, mp.decoded.payload.bytes, copyLen);
    entry.text[copyLen] = '\0';

    inbox.push_back(entry);
    if (inbox.size() > MAX_INBOX)
        inbox.pop_front();

    unreadCount++;
    const UIFrameEvent ev{UIFrameEvent::REGENERATE_FRAMESET_BACKGROUND};
    notifyObservers(&ev);

    return ProcessMessage::CONTINUE;
}

// ---------------------------------------------------------------------------
// UI frame
// ---------------------------------------------------------------------------

bool MessageInboxModule::wantUIFrame()
{
    return hasMessages() || isOpen();
}

void MessageInboxModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setColor(BLACK);
    display->fillRect(x, y, display->getWidth(), display->getHeight());
    display->setColor(WHITE);
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    if (!hasMessages()) {
        display->drawString(x + 4, y + 4, "(no messages)");
        return;
    }

    // When the inbox is closed, show a compact summary so this frame slot is useful
    if (!isOpen()) {
        rebuildSenderCache();
        char buf[64];
        snprintf(buf, sizeof(buf), "Inbox  %d sender(s)", (int)senderCache.size());
        display->drawString(x + 4, y + 4, buf);
        if (unreadCount > 0 && senderCache.size() > 0) {
            // Show which sender has the most recent unread
            const auto &s = senderCache[0];
            char sub[64];
            snprintf(sub, sizeof(sub), "%s: %d unread", nodeNameFor(s.nodeNum), s.unread);
            display->drawString(x + 4, y + FONT_HEIGHT_SMALL + 6, sub);
        }
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(display->getWidth() / 2, display->getHeight() - FONT_HEIGHT_SMALL - 2,
                            "OK to open  CANCEL dismiss");
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        return;
    }

    if (viewMode == VIEW_SENDER_LIST)
        drawSenderList(display, x, y);
    else
        drawMessageList(display, x, y);
}

void MessageInboxModule::drawSenderList(OLEDDisplay *display, int16_t x, int16_t y)
{
    int16_t row = y + 2;
    int16_t rowH = FONT_HEIGHT_SMALL + 2;
    int16_t h = display->getHeight();
    int visible = (h - FONT_HEIGHT_SMALL - 6) / rowH;

    int start = 0;
    if (cursor >= visible)
        start = cursor - visible + 1;
    int shown = (int)senderCache.size() - start;
    if (shown > visible)
        shown = visible;

    for (int i = 0; i < shown; i++) {
        int idx = start + i;
        const auto &s = senderCache[idx];
        bool sel = (idx == cursor);

        char line[64];
        int n = snprintf(line, sizeof(line), "%s", nodeNameFor(s.nodeNum));
        if (s.unread > 0)
            snprintf(line + n, sizeof(line) - n, " (%d)", s.unread);

        display->setColor(sel ? WHITE : BLACK);
        display->fillRect(x + 1, row, display->getWidth() - 2, rowH);
        display->setColor(sel ? BLACK : WHITE);
        display->drawString(x + 6, row, line);
        row += rowH;
    }

    display->setColor(WHITE);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(display->getWidth() - 4, display->getHeight() - FONT_HEIGHT_SMALL - 2,
                        "OK=open  CANCEL=exit");
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void MessageInboxModule::drawMessageList(OLEDDisplay *display, int16_t x, int16_t y)
{
    if (selectedSenderIdx < 0 || selectedSenderIdx >= (int)senderCache.size())
        return;
    uint32_t target = senderCache[selectedSenderIdx].nodeNum;

    // Build index of matching messages (newest first)
    struct Idx { size_t i; };
    std::vector<Idx> msgs;
    for (size_t i = 0; i < inbox.size(); i++) {
        if (inbox[i].sender == target)
            msgs.push_back({i});
    }

    int16_t row = y + 2;
    int16_t rowH = FONT_HEIGHT_SMALL + 2;
    int16_t h = display->getHeight();
    int visible = (h - FONT_HEIGHT_SMALL - 6) / rowH;

    int start = 0;
    if (cursor >= visible)
        start = cursor - visible + 1;
    int shown = (int)msgs.size() - start;
    if (shown > visible)
        shown = visible;

    for (int i = 0; i < shown; i++) {
        int idx = start + i;
        int revIdx = (int)msgs.size() - 1 - idx;
        const auto &e = inbox[msgs[revIdx].i];
        bool sel = (idx == cursor);

        char line[64];
        snprintf(line, sizeof(line), "%c %s", e.read ? ' ' : '*', e.text);

        display->setColor(sel ? WHITE : BLACK);
        display->fillRect(x + 1, row, display->getWidth() - 2, rowH);
        display->setColor(sel ? BLACK : WHITE);
        display->drawString(x + 6, row, line);
        row += rowH;
    }

    display->setColor(WHITE);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(display->getWidth() - 4, display->getHeight() - FONT_HEIGHT_SMALL - 2,
                        "LEFT=back  OK=mark read");
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------

int MessageInboxModule::handleInputEvent(const InputEvent *event)
{
    if (!isOpen())
        return 0;

    lastActivity = millis();
    const UIFrameEvent redraw{UIFrameEvent::REDRAW_ONLY};

    switch (event->inputEvent) {
    case INPUT_BROKER_DOWN: {
        int maxc = (viewMode == VIEW_SENDER_LIST) ? (int)senderCache.size() - 1 : (int)getMessageCount() - 1;
        if (cursor < maxc) {
            cursor++;
            notifyObservers(&redraw);
        }
        return 1;
    }
    case INPUT_BROKER_UP:
        if (cursor > 0) {
            cursor--;
            notifyObservers(&redraw);
        }
        return 1;

    case INPUT_BROKER_SELECT:
        if (viewMode == VIEW_SENDER_LIST && !senderCache.empty()) {
            selectedSenderIdx = cursor;
            cursor = 0;
            viewMode = VIEW_MESSAGE_LIST;
            notifyObservers(&redraw);
        } else if (viewMode == VIEW_MESSAGE_LIST) {
            markRead();
            notifyObservers(&redraw);
        }
        return 1;

    case INPUT_BROKER_LEFT:
    case INPUT_BROKER_BACK:
        if (viewMode == VIEW_MESSAGE_LIST) {
            cursor = selectedSenderIdx;
            selectedSenderIdx = -1;
            viewMode = VIEW_SENDER_LIST;
            notifyObservers(&redraw);
        } else {
            close();
            const UIFrameEvent regen{UIFrameEvent::REGENERATE_FRAMESET_BACKGROUND};
            notifyObservers(&regen);
        }
        return 1;

    case INPUT_BROKER_CANCEL:
        close();
        {
            const UIFrameEvent regen{UIFrameEvent::REGENERATE_FRAMESET_BACKGROUND};
            notifyObservers(&regen);
        }
        return 1;

    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void MessageInboxModule::rebuildSenderCache()
{
    senderCache.clear();
    for (const auto &e : inbox) {
        bool found = false;
        for (auto &s : senderCache) {
            if (s.nodeNum == e.sender) {
                s.msgCount++;
                if (!e.read)
                    s.unread++;
                if (e.timestamp > s.lastTimestamp)
                    s.lastTimestamp = e.timestamp;
                found = true;
                break;
            }
        }
        if (!found) {
            SenderSummary s;
            s.nodeNum = e.sender;
            s.lastTimestamp = e.timestamp;
            s.unread = e.read ? 0 : 1;
            s.msgCount = 1;
            senderCache.push_back(s);
        }
    }
    std::sort(senderCache.begin(), senderCache.end(),
              [](const SenderSummary &a, const SenderSummary &b) { return a.lastTimestamp > b.lastTimestamp; });
}

const char *MessageInboxModule::nodeNameFor(uint32_t nodeNum)
{
    if (nodeNum == NODENUM_BROADCAST)
        return "Broadcast";
    meshtastic_NodeInfoLite *info = nodeDB->getMeshNode(nodeNum);
    if (info && info->has_user) {
        if (strlen(info->user.short_name) > 0)
            return info->user.short_name;
        if (strlen(info->user.long_name) > 0)
            return info->user.long_name;
    }
    static char fallback[12];
    snprintf(fallback, sizeof(fallback), "0x%08x", nodeNum);
    return fallback;
}

void MessageInboxModule::open()
{
    if (viewMode != VIEW_CLOSED)
        return;
    viewMode = VIEW_SENDER_LIST;
    cursor = 0;
    selectedSenderIdx = -1;
    lastActivity = millis();
    rebuildSenderCache();
}

void MessageInboxModule::close()
{
    viewMode = VIEW_CLOSED;
    cursor = 0;
    selectedSenderIdx = -1;
}

void MessageInboxModule::markRead()
{
    if (selectedSenderIdx < 0 || selectedSenderIdx >= (int)senderCache.size())
        return;
    uint32_t target = senderCache[selectedSenderIdx].nodeNum;

    struct Idx { size_t i; };
    std::vector<Idx> msgs;
    for (size_t i = 0; i < inbox.size(); i++) {
        if (inbox[i].sender == target)
            msgs.push_back({i});
    }

    int revIdx = (int)msgs.size() - 1 - cursor;
    if (revIdx >= 0 && revIdx < (int)msgs.size()) {
        size_t idx = msgs[revIdx].i;
        if (!inbox[idx].read) {
            inbox[idx].read = true;
            if (unreadCount > 0)
                unreadCount--;
        }
    }
}

size_t MessageInboxModule::getMessageCount()
{
    if (selectedSenderIdx < 0 || selectedSenderIdx >= (int)senderCache.size())
        return 0;
    uint32_t target = senderCache[selectedSenderIdx].nodeNum;
    size_t count = 0;
    for (const auto &e : inbox) {
        if (e.sender == target)
            count++;
    }
    return count;
}

#endif
