#pragma once

#include <vector>

namespace inst::config {
    static const std::string appDir = "sdmc:/switch/Switch-Super-Installer";
    static const std::string configPath = appDir + "/config.json";
    static const std::string appVersion = std::string(APP_VERSION);

    extern std::string gAuthKey;
    extern std::string sigPatchesUrl;
    extern std::string lastNetUrl;
    extern std::string lastSSMIp;
    extern std::vector<std::string> updateInfo;
    extern int languageSetting;
    extern bool ignoreReqVers;
    extern bool validateNCAs;
    extern bool fixticket;
    extern bool overClock;
    extern bool deletePrompt;
    extern bool autoUpdate;
    extern bool gayMode;
    extern bool usbAck;
    extern bool validateNCAs;
    extern bool useTheme;
    extern bool autoInstallMode;
    extern int autoInstallTarget;
    // Full remote management from SSM: backups, uninstalls and installs run without any question on the console.
    extern bool remoteManagement;
    // Do not let the console sleep while it is on a charger (set here or from SSM, kept between launches).
    extern bool keepAwakeAc;
    // Colour skin of the interface: "default", "dreamcast" or the folder name of a theme in themes/ (see util/skin.hpp).
    extern std::string skin;

    void setConfig();
    void parseConfig();
}
