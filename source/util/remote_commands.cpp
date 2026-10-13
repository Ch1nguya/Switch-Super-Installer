#include "util/remote_commands.hpp"

#include <string>
#include <vector>
#include <switch.h>

#include "ui/MainApplication.hpp"
#include "ui/mainPage.hpp"
#include "util/cheats_server.hpp"
#include "util/config.hpp"
#include "util/console_inventory.hpp"
#include "util/curl.hpp"
#include "util/json.hpp"
#include "util/lang.hpp"
#include "util/save_backup.hpp"
#include "util/power.hpp"
#include "netInstall.hpp"
#include "ui/mainPage.hpp"

#include <cctype>

namespace inst::ui {
    extern MainApplication *mainApp;
}

namespace inst::remote {
    namespace {
        bool g_busy = false;

        std::string idText(uint64_t id) {
            char buffer[17];
            std::snprintf(buffer, sizeof(buffer), "%016llX", (unsigned long long)id);
            return buffer;
        }
    }

    namespace {
        std::string urlEncode(const std::string &text) {
            std::string out;
            char buffer[4];
            for (unsigned char c : text) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.') out += (char)c;
                else { std::snprintf(buffer, sizeof(buffer), "%%%02X", c); out += buffer; }
            }
            return out;
        }

        // Tell SSM the request reached SSI (it warns the user when nothing answers).
        void sendAck(const std::string &ssmUrl, const std::string &kind, const std::vector<uint64_t> &ids) {
            if (ssmUrl.empty()) return;
            std::string base = ssmUrl;
            if (base.back() != '/') base += '/';
            nlohmann::json body;
            body["kind"] = kind;
            body["titles"] = nlohmann::json::array();
            for (uint64_t id : ids) body["titles"].push_back(idText(id));
            std::string response;
            inst::curl::postJson(base + "api/console/ack", body.dump(), response, 3000);
        }

        // Tell SSM why a backup did not happen (it shows the reason on the PC).
        void reportSaveResult(const inst::cheats::BackupRequest &request, bool ok, const std::string &error) {
            std::string base = request.ssmUrl;
            if (base.empty() || base.back() != '/') base += '/';
            nlohmann::json body;
            body["title"] = idText(request.id);
            body["name"] = request.name;
            body["ok"] = ok;
            body["error"] = error;
            std::string response;
            inst::curl::postJson(base + "api/console/save-result", body.dump(), response, 4000);
        }

        void processBackups() {
            inst::power::Hold keepAwake;
            std::vector<inst::cheats::BackupRequest> requests = inst::cheats::takeBackupRequests();
            {
                std::vector<uint64_t> ids;
                for (const auto &request : requests) ids.push_back(request.id);
                if (!requests.empty()) sendAck(requests[0].ssmUrl, "backup", ids);
            }
            std::string list;
            for (size_t i = 0; i < requests.size() && i < 8; i++) {
                list += "\n- " + (requests[i].name.empty() ? idText(requests[i].id) : requests[i].name);
            }
            int answer = inst::config::remoteManagement ? 0 : inst::ui::mainApp->CreateShowDialog(
                "extra.save_title"_lang,
                "extra.save_desc0"_lang + std::to_string(requests.size()) + "extra.games_colon"_lang + list + "extra.save_desc1"_lang,
                {"extra.save_go"_lang, "common.cancel"_lang}, true);
            if (answer != 0) return;

            int done = 0;
            std::string problems;
            const std::string tarPath = "sdmc:/switch/Switch-Super-Installer/save_backup.tar";
            for (const auto &request : requests) {
                std::string error;
                int files = 0;
                if (!inst::saves::backupToTar(request.id, tarPath, error, files)) {
                    reportSaveResult(request, false, error);
                    if (error == "no_save") error = "extra.save_nodata"_lang;
                    problems += "\n- " + (request.name.empty() ? idText(request.id) : request.name) + ": " + error;
                    continue;
                }
                std::string base = request.ssmUrl;
                if (base.empty() || base.back() != '/') base += '/';
                std::string url = base + "api/console/save?title=" + idText(request.id) + "&name=" + urlEncode(request.name);
                std::string response;
                if (inst::curl::postFile(url, tarPath, response)) done++;
                else {
                    reportSaveResult(request, false, "send_failed");
                    problems += "\n- " + (request.name.empty() ? idText(request.id) : request.name) + ": " + "extra.save_sendfail"_lang;
                }
            }
            std::remove(tarPath.c_str());
            if (!inst::config::remoteManagement) inst::ui::mainApp->CreateShowDialog(
                "extra.save_done_title"_lang,
                std::to_string(done) + "extra.of"_lang + std::to_string(requests.size()) + "extra.save_sent"_lang + problems,
                {"common.ok"_lang}, true);
        }
    }

    void processPending() {
        if (g_busy) return;
        if (inst::cheats::takeNetInstallRequest()) {
            // SSM wants to send a file over the network: open the listening mode if it is not open yet.
            if (!netInstStuff::isListening()) {
                // not g_busy here: the network mode runs its own loop, which must still serve SSM commands
                inst::ui::mainApp->mainPage->netInstallMenuItem_Click();
                return;
            }
        }
        {
            std::string inventoryUrl;
            if (inst::cheats::takeInventoryRequest(inventoryUrl)) {
                g_busy = true;
                inst::inventory::sendFromUiThread(inventoryUrl);
                g_busy = false;
                return;
            }
        }
        if (inst::cheats::hasBackupRequests()) {
            g_busy = true;
            processBackups();
            g_busy = false;
            return;
        }
        if (!inst::cheats::hasUninstallRequests()) return;
        g_busy = true;
        std::vector<inst::cheats::UninstallRequest> requests = inst::cheats::takeUninstallRequests();
        {
            std::vector<uint64_t> ids;
            for (const auto &request : requests) ids.push_back(request.id);
            if (!requests.empty()) sendAck(requests[0].ssmUrl, "uninstall", ids);
        }

        std::string list;
        for (size_t i = 0; i < requests.size() && i < 8; i++) {
            list += "\n- " + (requests[i].name.empty() ? idText(requests[i].id) : requests[i].name + " (" + idText(requests[i].id) + ")");
        }
        if (requests.size() > 8) list += "\n... +" + std::to_string(requests.size() - 8);

        int answer = inst::config::remoteManagement ? 0 : inst::ui::mainApp->CreateShowDialog(
            "extra.uninstall_title"_lang,
            "extra.uninstall_desc0"_lang + std::to_string(requests.size()) + "extra.games_colon"_lang + list + "extra.uninstall_desc1"_lang,
            {"extra.uninstall_go"_lang, "common.cancel"_lang}, true);
        if (answer != 0) {
            g_busy = false;
            return;
        }

        int done = 0;
        int failed = 0;
        if (R_SUCCEEDED(nsInitialize())) {
            for (const auto &request : requests) {
                if (R_SUCCEEDED(nsDeleteApplicationCompletely(request.id))) done++;
                else failed++;
            }
            nsExit();
        } else {
            failed = (int)requests.size();
        }
        if (inst::config::remoteManagement) inst::inventory::postEvent("uninstall", failed == 0, std::to_string(requests.size()) + " game(s)", std::to_string(done) + " uninstalled, " + std::to_string(failed) + " failed");
        else inst::ui::mainApp->CreateShowDialog(
            "extra.uninstall_done_title"_lang,
            std::to_string(done) + "extra.uninstall_done0"_lang + std::to_string(failed) + "extra.uninstall_done1"_lang,
            {"common.ok"_lang}, true);
        inst::inventory::sendFromUiThread();
        g_busy = false;
    }
}
