#pragma once

// Commands asked by Switch Super Manager through the cheats server (see cheats_server.hpp).
// Must be called from the UI thread (it shows dialogs and uses libnx services).
namespace inst::remote {
    // Cheap when nothing is pending. Asks for confirmation on the console before doing anything.
    void processPending();
}
