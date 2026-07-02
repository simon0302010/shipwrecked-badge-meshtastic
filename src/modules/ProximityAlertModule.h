#pragma once
#include "configuration.h"
#if HAS_SCREEN

#include "Observer.h"
#include "NodeStatus.h"
#include "concurrency/OSThread.h"

class ProximityAlertModule : private concurrency::OSThread
{
    CallbackObserver<ProximityAlertModule, const meshtastic::Status *> statusObserver =
        CallbackObserver<ProximityAlertModule, const meshtastic::Status *>(this,
                                                                           &ProximityAlertModule::onNodeStatusChanged);

    uint16_t lastTotal;
    uint32_t buzzUntil;
    bool buzzActive;

  public:
    ProximityAlertModule();

  protected:
    int32_t runOnce() override;
    int onNodeStatusChanged(const meshtastic::Status *status);
    void buzzOn();
    void buzzOff();
};

extern ProximityAlertModule *proximityAlertModule;

#endif
