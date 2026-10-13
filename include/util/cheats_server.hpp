#pragma once

// Tiny HTTP server that receives cheat files from Switch Super Manager.
//   PUT /cheats/<TitleID 16 hex>/<BuildID 16 hex>.txt   (body = the cheat file)
//   POST /uninstall  {"titles":[{"id":"<16 hex>","name":"..."}]}   (queued, confirmed on the console)
//   GET /ping
// Files are written to sdmc:/atmosphere/contents/<TitleID>/cheats/<BuildID>.txt
#include <cstdint>
#include <string>
#include <vector>

namespace inst::cheats {
    struct UninstallRequest {
        uint64_t id;
        std::string name;
        std::string ssmUrl;
    };
    // Uninstall requests received from SSM (POST /uninstall). They are only queued here: the UI thread
    // asks for confirmation on the console and does the work (see util/remote_commands).
    struct BackupRequest {
        uint64_t id;
        std::string name;
        std::string ssmUrl;
    };
    // Save backups asked by SSM (POST /backup): only queued here, run by the UI thread.
    bool hasBackupRequests();
    std::vector<BackupRequest> takeBackupRequests();

    // SSM asks SSI to open its network install mode (POST /netinstall[?target=sd|nand]).
    bool takeNetInstallRequest();
    // Install location chosen in SSM for the next network transfer: 0 = SD card, 1 = internal storage,
    // -1 = none (ask on the console). Valid for the whole session opened by SSM (ten minutes after its last request).
    int remoteInstallTarget();
    // True when the transfer that just arrived was announced by SSM (POST /netinstall in the last ten minutes: a session that SSM keeps
    // alive for a whole batch of files, so that the second file is handled like the first). The files are then all selected and only the install location is asked.
    bool remoteTransferActive();
    // SSM is done with its batch: forget the install location and the "announced by SSM" state.
    void endRemoteSession();
    // Cover picture received from SSM (POST /cover) for the transfer that is starting; empty when there is none.
    std::string takeRemoteCover();

    // SSM asks for a fresh inventory right now (POST /inventory {"ssm_url": "..."}): the UI thread builds and sends it.
    bool takeInventoryRequest(std::string &ssmUrl);

    bool hasUninstallRequests();
    std::vector<UninstallRequest> takeUninstallRequests();

    constexpr int kPort = 2003;

    // Start the background listener (call once, after socketInitialize). Safe to call twice.
    void start();
    // True while the listener is accepting connections.
    bool isRunning();
    // Ask the listener to stop and wait for it.
    void stop();
}
