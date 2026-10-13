#pragma once
#include <string>

// Reports what is installed on the console (titles, sizes, SD/NAND usage, MAC) to Switch Super Manager.
// Format and endpoint: see CONSOLE_INVENTORY.md in the Switch-Super-Manager repository.
namespace inst::inventory {
    // Remember the SSM address (http://host:port/) used by the last successful list load.
    void rememberBaseUrl(const std::string &baseUrl);

    // Collect the inventory (libnx services: call from the UI thread) and POST it to
    // <baseUrl>/api/console/inventory from a background thread. An empty baseUrl uses the remembered one.
    void sendFromUiThread(const std::string &baseUrl = "");

    // Short report for SSM (install done or failed, uninstall result...): POST <baseUrl>/api/console/event.
    void postEvent(const std::string &kind, bool ok, const std::string &name, const std::string &message);
}
