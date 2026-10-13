#include "util/console_inventory.hpp"

#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <switch.h>

#include "util/config.hpp"
#include "util/curl.hpp"
#include "util/json.hpp"
#include "util/util.hpp"

namespace {
    std::mutex g_baseUrlMutex;
    std::string g_baseUrl;
    std::atomic<bool> g_sending{false};

    struct Volume {
        s64 total = 0;
        s64 free = 0;
    };

    // Step log, written line by line so that it survives a console crash: after a crash the last
    // line tells which step was running. Create sdmc:/switch/Switch-Super-Installer/inventory_disabled
    // to turn the inventory off completely.
    const char *kLogPath = "sdmc:/switch/Switch-Super-Installer/inventory.log";
    const char *kDisablePath = "sdmc:/switch/Switch-Super-Installer/inventory_disabled";

    void logStep(const char *format, ...) {
        FILE *file = std::fopen(kLogPath, "a");
        if (!file) return;
        va_list args;
        va_start(args, format);
        std::vfprintf(file, format, args);
        va_end(args);
        std::fputs("\n", file);
        std::fflush(file);
        std::fclose(file);
    }

    bool inventoryDisabled() {
        FILE *file = std::fopen(kDisablePath, "rb");
        if (!file) return false;
        std::fclose(file);
        return true;
    }

    std::string hex16(u64 value) {
        char buffer[17];
        std::snprintf(buffer, sizeof(buffer), "%016llX", (unsigned long long)value);
        return buffer;
    }

    // Capacity and free space of a storage: ns first, statvfs on the mounted device as a fallback.
    Volume readVolume(NcmStorageId storage, const char *fallbackPath, bool nsAvailable) {
        Volume volume;
        s64 total = 0;
        s64 free = 0;
        if (nsAvailable && R_SUCCEEDED(nsGetTotalSpaceSize(storage, &total)) && R_SUCCEEDED(nsGetFreeSpaceSize(storage, &free)) && total > 0) {
            volume.total = total;
            volume.free = free < 0 ? 0 : (free > total ? total : free);
            return volume;
        }
        if (fallbackPath) {
            const double fsTotal = inst::util::totalsize(fallbackPath);
            const double fsFree = inst::util::GetAvailableSpace(fallbackPath);
            if (fsTotal > 0 && fsFree >= 0) {
                volume.total = (s64)fsTotal;
                volume.free = (s64)fsFree;
            }
        }
        return volume;
    }

    // Bytes used by every content of one content meta key.
    u64 contentsSize(NcmContentMetaDatabase *db, const NcmContentMetaKey *key) {
        u64 sum = 0;
        NcmContentInfo infos[32];
        s32 offset = 0;
        for (;;) {
            s32 written = 0;
            if (R_FAILED(ncmContentMetaDatabaseListContentInfo(db, &written, infos, 32, key, offset)) || written <= 0) break;
            for (s32 i = 0; i < written; i++) {
                u64 size = 0;
                ncmContentInfoSizeToU64(&infos[i], &size);
                sum += size;
            }
            offset += written;
            if (written < 32) break;
        }
        return sum;
    }

    void collectStorage(NcmStorageId storage, const char *storageName, nlohmann::json &titles) {
        NcmContentMetaDatabase db;
        if (R_FAILED(ncmOpenContentMetaDatabase(&db, storage))) return;

        struct Kind {
            NcmContentMetaType metaType;
            const char *name;
        };
        const Kind kinds[] = {
            {NcmContentMetaType_Application, "base"},
            {NcmContentMetaType_Patch, "update"},
            {NcmContentMetaType_AddOnContent, "dlc"},
        };

        for (const auto &kind : kinds) {
            std::vector<NcmContentMetaKey> keys(1024);
            s32 total = 0;
            s32 written = 0;
            Result rc = ncmContentMetaDatabaseList(&db, &total, &written, keys.data(), (s32)keys.size(), kind.metaType,
                                                   0, 0, UINT64_MAX, NcmContentInstallType_Full);
            if (R_SUCCEEDED(rc) && total > (s32)keys.size()) {
                keys.resize((size_t)total);
                rc = ncmContentMetaDatabaseList(&db, &total, &written, keys.data(), (s32)keys.size(), kind.metaType,
                                                0, 0, UINT64_MAX, NcmContentInstallType_Full);
            }
            if (R_FAILED(rc)) continue;

            for (s32 i = 0; i < written; i++) {
                const NcmContentMetaKey &key = keys[(size_t)i];
                nlohmann::json title = nlohmann::json::object();
                title["id"] = hex16(key.id);
                title["type"] = kind.name;
                title["storage"] = storageName;
                title["version"] = key.version;
                title["size"] = contentsSize(&db, &key);
                titles.push_back(title);
            }
        }
        ncmContentMetaDatabaseClose(&db);
    }

    std::string readMacAddress() {
        std::string mac;
        if (R_SUCCEEDED(setcalInitialize())) {
            SetCalMacAddress address;
            if (R_SUCCEEDED(setcalGetWirelessLanMacAddress(&address))) {
                char buffer[18];
                std::snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                              address.addr[0], address.addr[1], address.addr[2],
                              address.addr[3], address.addr[4], address.addr[5]);
                mac = buffer;
            }
            setcalExit();
        }
        return mac;
    }

    std::string readFirmware() {
        std::string firmware;
        if (R_SUCCEEDED(setsysInitialize())) {
            SetSysFirmwareVersion version;
            if (R_SUCCEEDED(setsysGetFirmwareVersion(&version))) firmware = version.display_version;
            setsysExit();
        }
        return firmware;
    }

    std::string readNickname() {
        std::string nickname;
        if (R_SUCCEEDED(setsysInitialize())) {
            SetSysDeviceNickName nick;
            if (R_SUCCEEDED(setsysGetDeviceNickname(&nick))) nickname = nick.nickname;
            setsysExit();
        }
        return nickname;
    }

    nlohmann::json buildInventory() {
        nlohmann::json titles = nlohmann::json::array();
        logStep("ncmInitialize...");
        const Result ncmRc = ncmInitialize();
        logStep("ncmInitialize rc=0x%x", (unsigned)ncmRc);
        const bool ncmOk = R_SUCCEEDED(ncmRc);
        logStep("nsInitialize...");
        const Result nsRc = nsInitialize();
        logStep("nsInitialize rc=0x%x", (unsigned)nsRc);
        const bool nsOk = R_SUCCEEDED(nsRc);

        Volume sd = readVolume(NcmStorageId_SdCard, "sdmc:/", nsOk);
        logStep("sd volume total=%lld free=%lld", (long long)sd.total, (long long)sd.free);
        Volume nand = readVolume(NcmStorageId_BuiltInUser, nullptr, nsOk);
        logStep("nand volume total=%lld free=%lld", (long long)nand.total, (long long)nand.free);
        if (ncmOk) {
            logStep("collect sd titles...");
            collectStorage(NcmStorageId_SdCard, "sd", titles);
            logStep("collect nand titles (so far %d)...", (int)titles.size());
            collectStorage(NcmStorageId_BuiltInUser, "nand", titles);
            logStep("collected %d titles", (int)titles.size());
        }
        if (nsOk) nsExit();
        if (ncmOk) ncmExit();
        logStep("services closed");

        nlohmann::json console = nlohmann::json::object();
        logStep("read firmware...");
        console["firmware"] = readFirmware();
        console["ssi_version"] = inst::config::appVersion;
        logStep("read nickname...");
        const std::string nickname = readNickname();
        if (!nickname.empty()) console["name"] = nickname;
        logStep("read MAC...");
        const std::string mac = readMacAddress();
        if (!mac.empty()) console["mac"] = mac;
        logStep("MAC %s", mac.empty() ? "unavailable" : "read");

        nlohmann::json inventory = nlohmann::json::object();
        inventory["schema"] = 1;
        inventory["console"] = console;
        inventory["storage"]["sd"]["total"] = sd.total;
        inventory["storage"]["sd"]["free"] = sd.free;
        inventory["storage"]["nand"]["total"] = nand.total;
        inventory["storage"]["nand"]["free"] = nand.free;
        inventory["titles"] = titles;
        return inventory;
    }

    std::string inventoryUrl(const std::string &baseUrl) {
        std::string url = baseUrl;
        if (url.empty()) return "";
        if (url.back() != '/') url += '/';
        return url + "api/console/inventory";
    }
}

namespace inst::inventory {
    void postEvent(const std::string &kind, bool ok, const std::string &name, const std::string &message) {
        std::string base;
        {
            std::lock_guard<std::mutex> lock(g_baseUrlMutex);
            base = g_baseUrl;
        }
        if (base.empty()) return;
        if (base.back() != '/') base += '/';
        nlohmann::json body;
        body["kind"] = kind;
        body["ok"] = ok;
        body["name"] = name;
        body["message"] = message;
        std::string response;
        inst::curl::postJson(base + "api/console/event", body.dump(), response, 3000);
    }

    void rememberBaseUrl(const std::string &baseUrl) {
        if (baseUrl.empty()) return;
        std::lock_guard<std::mutex> lock(g_baseUrlMutex);
        g_baseUrl = baseUrl;
    }

    void sendFromUiThread(const std::string &baseUrl) {
        std::string target = baseUrl;
        if (target.empty()) {
            std::lock_guard<std::mutex> lock(g_baseUrlMutex);
            target = g_baseUrl;
        } else {
            rememberBaseUrl(target);
        }
        const std::string url = inventoryUrl(target);
        if (url.empty() || inventoryDisabled()) return;

        bool expected = false;
        if (!g_sending.compare_exchange_strong(expected, true)) return;  // one send at a time

        // Collecting uses libnx services (ncm, ns, set:cal): do it on the calling (UI) thread, where
        // SSI already initializes the same services for installations. Only the HTTP POST is
        // handed to a background thread.
        std::string body;
        try {
            logStep("--- inventory start -> %s", url.c_str());
            body = buildInventory().dump();
            logStep("JSON built (%d bytes)", (int)body.size());
        } catch (const std::exception &e) {
            logStep("exception: %s", e.what());
        } catch (...) {
            logStep("unknown exception");
        }
        if (body.empty()) {
            g_sending = false;
            return;
        }

        // No extra thread: creating one right after the service calls crashed the console (the log
        // stopped between "JSON built" and the POST). A short blocking POST once per list load is fine.
        logStep("posting %d bytes...", (int)body.size());
        std::string response;
        bool ok = false;
        try {
            ok = inst::curl::postJson(url, body, response, 3000);
        } catch (const std::exception &e) {
            logStep("POST exception: %s", e.what());
        } catch (...) {
            logStep("POST unknown exception");
        }
        logStep(ok ? "POST ok" : "POST failed");
        g_sending = false;
    }
}
