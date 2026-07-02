#include "configuration.h"
#if HAS_SCREEN

#include "ProximityAlertModule.h"
#include "NodeDB.h"
#include "NodeStatus.h"
#include "graphics/draw/NotificationRenderer.h"
#include "modules/ExternalNotificationModule.h"

ProximityAlertModule *proximityAlertModule;

ProximityAlertModule::ProximityAlertModule()
    : concurrency::OSThread("ProximityAlert"), lastTotal(0), buzzUntil(0), buzzActive(false)
{
    if (nodeStatus) {
        statusObserver.observe(&nodeStatus->onNewStatus);
        lastTotal = nodeStatus->getNumTotal();
    }
}

int32_t ProximityAlertModule::runOnce()
{
    if (buzzActive && millis() > buzzUntil) {
        buzzOff();
    }
    if (!buzzActive)
        return INT32_MAX;
    return 50;
}

int ProximityAlertModule::onNodeStatusChanged(const meshtastic::Status *status)
{
    const meshtastic::NodeStatus *ns = static_cast<const meshtastic::NodeStatus *>(status);
    uint16_t newTotal = ns->getNumTotal();
    if (newTotal <= lastTotal) {
        lastTotal = newTotal;
        return 0;
    }

    // Scan for the newly added node — the node at the last index is the most recently added.
    for (int i = nodeDB->numMeshNodes - 1; i >= 0; i--) {
        meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
        if (!node || node->num == nodeDB->getNodeNum())
            continue;

        bool directNeighbor = (node->hops_away == 0) || (node->snr > 0.0f);
        if (!directNeighbor)
            continue;

        buzzOn();

        const char *name = (node->has_user && strlen(node->user.long_name) > 0)
                               ? node->user.long_name
                               : node->user.short_name;
        if (!name || strlen(name) == 0)
            name = "Unknown";

        snprintf(graphics::NotificationRenderer::proximityAlertMessage,
                 sizeof(graphics::NotificationRenderer::proximityAlertMessage), "Node: %s  SNR:%.1f", name,
                 (double)node->snr);
        graphics::NotificationRenderer::proximityAlertUntil = millis() + 4000;
        break;
    }

    lastTotal = newTotal;
    return 0;
}

void ProximityAlertModule::buzzOn()
{
    if (buzzActive)
        return;
    buzzActive = true;
    buzzUntil = millis() + 200;
    if (externalNotificationModule)
        externalNotificationModule->setExternalState(2, true);
}

void ProximityAlertModule::buzzOff()
{
    if (!buzzActive)
        return;
    buzzActive = false;
    if (externalNotificationModule)
        externalNotificationModule->setExternalState(2, false);
}

#endif
