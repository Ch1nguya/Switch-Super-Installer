#include "util/power.hpp"

#include <atomic>
#include <switch.h>

namespace {
    std::atomic<int> g_holds{0};
    std::atomic<bool> g_keepOnAc{false};
    bool g_applied = false;
    bool g_psmReady = false;

    bool charging() {
        if (!g_psmReady) g_psmReady = R_SUCCEEDED(psmInitialize());
        if (!g_psmReady) return false;
        PsmChargerType type = PsmChargerType_Unconnected;
        return R_SUCCEEDED(psmGetChargerType(&type)) && type != PsmChargerType_Unconnected;
    }

    void apply(bool wanted) {
        if (wanted == g_applied) return;
        appletSetAutoSleepDisabled(wanted);
        appletSetMediaPlaybackState(wanted);
        g_applied = wanted;
    }
}

namespace inst::power {
    void acquire() { g_holds++; }
    void release() { if (g_holds > 0) g_holds--; }
    void setKeepAwakeOnAc(bool enabled) { g_keepOnAc = enabled; }

    void update() {
        bool wanted = g_holds > 0 || (g_keepOnAc && charging());
        apply(wanted);
    }

    void shutdown() {
        apply(false);
        if (g_psmReady) { psmExit(); g_psmReady = false; }
    }
}
