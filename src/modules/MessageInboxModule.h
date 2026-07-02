#pragma once
#include "configuration.h"
#if HAS_SCREEN

#include "Observer.h"
#include "concurrency/OSThread.h"
#include "graphics/Screen.h"
#include "mesh/SinglePortModule.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>
#include <cstdint>
#include <deque>
#include <vector>

class MessageInboxModule : public SinglePortModule, public Observable<const UIFrameEvent *>
{
    CallbackObserver<MessageInboxModule, const InputEvent *> inputObserver =
        CallbackObserver<MessageInboxModule, const InputEvent *>(this, &MessageInboxModule::handleInputEvent);

  public:
    MessageInboxModule();

    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

    bool wantUIFrame() override;
    Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    bool interceptingKeyboardInput() override { return isOpen(); }

  protected:
    int handleInputEvent(const InputEvent *event);

  private:
    struct InboxEntry {
        uint32_t sender;
        uint32_t timestamp;
        char text[256];
        bool read;
    };

    struct SenderSummary {
        uint32_t nodeNum;
        uint32_t lastTimestamp;
        int unread;
        int msgCount;
    };

    enum ViewMode { VIEW_CLOSED, VIEW_SENDER_LIST, VIEW_MESSAGE_LIST };

    ViewMode viewMode = VIEW_CLOSED;
    std::deque<InboxEntry> inbox;
    int cursor = 0;
    int selectedSenderIdx = -1;
    std::vector<SenderSummary> senderCache;

    static constexpr size_t MAX_INBOX = 20;
    uint32_t lastActivity = 0;
    uint32_t unreadCount = 0;

    bool isOpen() const { return viewMode != VIEW_CLOSED; }
    bool hasMessages() const { return !inbox.empty(); }
    size_t getMessageCount();

    const char *nodeNameFor(uint32_t nodeNum);
    void rebuildSenderCache();
    void open();
    void close();
    void markRead();

    void drawSenderList(OLEDDisplay *display, int16_t x, int16_t y);
    void drawMessageList(OLEDDisplay *display, int16_t x, int16_t y);
};

extern MessageInboxModule *messageInboxModule;

#endif
