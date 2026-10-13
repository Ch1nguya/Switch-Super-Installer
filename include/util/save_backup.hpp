#pragma once
#include <cstdint>
#include <string>

// Reads the save data of a game (every account, plus the device save) and writes it to a tar file.
// Layout: user_<account id hex>/..., device/..., ssi_meta.txt. Must run on the UI thread (account service).
namespace inst::saves {
    bool backupToTar(uint64_t titleId, const std::string &tarPath, std::string &error, int &fileCount);
}
