#include <filesystem>
#include <mutex>
#include <thread>
#include <cctype>
#include <atomic>
#include <algorithm>
#include <switch.h>
#include "ui/MainApplication.hpp"
#include "util/remote_commands.hpp"
#include "ui/mainPage.hpp"
#include "ui/netInstPage.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/skin.hpp"
#include "util/curl.hpp"
#include "util/console_inventory.hpp"
#include "util/cheats_server.hpp"
#include "util/lang.hpp"
#include "netInstall.hpp"
#include "util/json.hpp"
#include "util/network_util.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;

    std::string lastFileID = "";
    std::string sourceString = "";

    namespace {
        inline bool startsWith(const std::string &value, const std::string &prefix) {
            return value.rfind(prefix, 0) == 0;
        }

        inline bool endsWith(const std::string &value, const std::string &suffix) {
            if (suffix.size() > value.size()) return false;
            return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
        }

        std::string trimCopy(std::string s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
            size_t i = 0;
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (i > 0) s.erase(0, i);
            return s;
        }

        bool normalizeSsmListUrl(const std::string &input, std::string &outBaseUrl, std::string &outListUrl) {
            outBaseUrl.clear();
            outListUrl.clear();

            std::string raw = trimCopy(input);
            if (raw.empty()) return false;

            if (!startsWith(raw, "http://") && !startsWith(raw, "https://")) raw = "http://" + raw;

            const size_t schemePos = raw.find("://");
            if (schemePos == std::string::npos) return false;
            const size_t hostStart = schemePos + 3;
            const size_t slashPos = raw.find('/', hostStart);
            std::string hostPort = (slashPos == std::string::npos) ? raw.substr(hostStart) : raw.substr(hostStart, slashPos - hostStart);

            // Add default SSM web port if user entered host without explicit port.
            if (!hostPort.empty()) {
                if (hostPort.front() == '[') {
                    const size_t rb = hostPort.find(']');
                    if (rb != std::string::npos) {
                        const bool hasPort = (rb + 1 < hostPort.size() && hostPort[rb + 1] == ':');
                        if (!hasPort) hostPort += ":2002";
                    }
                } else if (hostPort.find(':') == std::string::npos) {
                    hostPort += ":2002";
                }
            }

            outBaseUrl = raw.substr(0, hostStart) + hostPort;
            if (slashPos == std::string::npos) {
                outBaseUrl += "/";
                outListUrl = outBaseUrl + "list.json";
                return true;
            }

            std::string path = raw.substr(slashPos);
            if (path.empty() || path == "/") {
                outBaseUrl += "/";
                outListUrl = outBaseUrl + "list.json";
                return true;
            }

            if (endsWith(path, "/list.json") || endsWith(path, "/list") || endsWith(path, "/api/list") || endsWith(path, "/api/list.json")) {
                outListUrl = outBaseUrl + path;
                outBaseUrl += "/";
                return true;
            }

            if (path.back() != '/') path += "/";
            outBaseUrl += path;
            outListUrl = outBaseUrl + "list.json";
            return true;
        }

        struct SsmEndpointCandidate {
            std::string baseUrl;
            std::string listUrl;
        };

        void pushUniqueCandidate(std::vector<SsmEndpointCandidate> &out, const std::string &baseUrl, const std::string &listUrl) {
            for (const auto &it : out) {
                if (it.listUrl == listUrl) return;
            }
            out.push_back({baseUrl, listUrl});
        }

        std::vector<SsmEndpointCandidate> buildSsmCandidates(const std::string &input) {
            std::vector<SsmEndpointCandidate> out;
            std::string raw = trimCopy(input);
            if (raw.empty()) return out;

            if (!startsWith(raw, "http://") && !startsWith(raw, "https://")) raw = "http://" + raw;

            const size_t schemePos = raw.find("://");
            if (schemePos == std::string::npos) return out;
            const std::string scheme = raw.substr(0, schemePos + 3); // keep "http://"
            const size_t hostStart = schemePos + 3;
            const size_t slashPos = raw.find('/', hostStart);
            const std::string hostPort = (slashPos == std::string::npos) ? raw.substr(hostStart) : raw.substr(hostStart, slashPos - hostStart);
            std::string path = (slashPos == std::string::npos) ? "" : raw.substr(slashPos);

            if (hostPort.empty()) return out;

            std::vector<std::string> hosts;
            // Keep candidate count tight for responsiveness:
            // - If no port is provided, assume SSM default :2002 only.
            // - If a port is provided, honor it as-is.
            if (hostPort.find(':') == std::string::npos) hosts.push_back(hostPort + ":2002");
            else hosts.push_back(hostPort);

            for (const auto &h : hosts) {
                const std::string base = scheme + h + "/";
                if (!path.empty() && path != "/") {
                    if (endsWith(path, "/list.json") || endsWith(path, "/api/list.json") || endsWith(path, "/list") || endsWith(path, "/api/list")) {
                        pushUniqueCandidate(out, base, scheme + h + path);
                    } else {
                        std::string p = path;
                        if (!p.empty() && p.back() != '/') p += "/";
                        pushUniqueCandidate(out, base, scheme + h + p + "list.json");
                        pushUniqueCandidate(out, base, scheme + h + p + "api/list.json");
                    }
                }
                pushUniqueCandidate(out, base, base + "list.json");
                pushUniqueCandidate(out, base, base + "api/list.json");
            }

            return out;
        }

        struct SsmLoadPayload {
            std::vector<std::string> urls;
            std::vector<std::string> names;
            std::vector<std::string> imageUrls;
            std::vector<std::string> coverUrls;
        };

        struct SsmLoadContext {
            std::mutex mtx;
            std::thread worker;
            bool running = false;
            bool done = false;
            bool failed = false;
            bool canceled = false;
            bool canceledResult = false;
            float progress = 0.0f;
            std::string status;
            std::string error;
            SsmLoadPayload payload;
        };

        bool tryLoadSsmListImmediate(const std::string &input, SsmLoadPayload &outPayload, std::string &outErr) {
            outPayload = {};
            outErr.clear();

            std::string baseUrl;
            std::string jsonStr;
            auto candidates = buildSsmCandidates(input);
            for (const auto &c : candidates) {
                jsonStr = inst::curl::downloadToBuffer(c.listUrl, -1, -1, 5000);
                if (!jsonStr.empty()) {
                    baseUrl = c.baseUrl;
                    break;
                }
            }
            if (jsonStr.empty()) return false;
            inst::inventory::rememberBaseUrl(baseUrl);

            try {
                auto j = nlohmann::json::parse(jsonStr);
                if (!j.contains("files") || !j["files"].is_array()) {
                    outErr = "Invalid list.json format";
                    return false;
                }

                for (const auto &file : j["files"]) {
                    std::string fileUrl;
                    std::string displayName;
                    std::string iconUrl;
                    std::string coverUrl;

                    if (file.is_object()) {
                        if (file.contains("url")) fileUrl = file["url"];
                        if (file.contains("name")) displayName = file["name"];
                        if (file.contains("region")) displayName += " [" + std::string(file["region"]) + "]";
                        if (file.contains("size")) displayName += " (" + std::string(file["size"]) + ")";
                        if (file.contains("image_url")) iconUrl = file["image_url"];
                        if (file.contains("image_512_url")) coverUrl = file["image_512_url"];
                        else if (file.contains("cover_url")) coverUrl = file["cover_url"];
                    } else if (file.is_string()) {
                        fileUrl = file;
                    }

                    if (fileUrl.empty()) continue;
                    if (displayName.empty()) displayName = inst::util::shortenString(inst::util::formatUrlString(fileUrl), 56, true);
                    if (!iconUrl.empty() && iconUrl.find("http") == std::string::npos) iconUrl = baseUrl + iconUrl;
                    if (!coverUrl.empty() && coverUrl.find("http") == std::string::npos) coverUrl = baseUrl + coverUrl;
                    if (fileUrl.find("http") == std::string::npos) fileUrl = baseUrl + fileUrl;

                    outPayload.urls.push_back(fileUrl);
                    outPayload.names.push_back(displayName);
                    outPayload.imageUrls.push_back(iconUrl);
                    outPayload.coverUrls.push_back(coverUrl);
                }
            } catch (std::exception &e) {
                outErr = e.what();
                return false;
            } catch (...) {
                outErr = "JSON parse error";
                return false;
            }

            return !outPayload.urls.empty();
        }

        SsmLoadContext g_ssmLoad;

        void ssmSetStatus(float progress, const std::string& statusText) {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            g_ssmLoad.progress = progress;
            g_ssmLoad.status = statusText;
        }

        bool ssmIsCanceled() {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            return g_ssmLoad.canceled;
        }

        void ssmFinishSuccess(SsmLoadPayload&& payload) {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            g_ssmLoad.payload = std::move(payload);
            g_ssmLoad.running = false;
            g_ssmLoad.done = true;
            g_ssmLoad.failed = false;
            g_ssmLoad.canceledResult = false;
            g_ssmLoad.progress = 100.0f;
        }

        void ssmFinishError(const std::string& err) {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            g_ssmLoad.running = false;
            g_ssmLoad.done = true;
            g_ssmLoad.failed = true;
            g_ssmLoad.canceledResult = false;
            g_ssmLoad.error = err;
        }

        void ssmFinishCanceled() {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            g_ssmLoad.running = false;
            g_ssmLoad.done = true;
            g_ssmLoad.failed = false;
            g_ssmLoad.canceledResult = true;
        }

        void ssmRequestCancel() {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            g_ssmLoad.canceled = true;
        }

        void ssmPrepareRun() {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            g_ssmLoad.running = true;
            g_ssmLoad.done = false;
            g_ssmLoad.failed = false;
            g_ssmLoad.canceled = false;
            g_ssmLoad.canceledResult = false;
            g_ssmLoad.progress = 0.0f;
            g_ssmLoad.status = "inst.net.ssm.loading_list"_lang;
            g_ssmLoad.error.clear();
            g_ssmLoad.payload = {};
        }

        std::string buildAutoStateLabel() {
            std::string state = "inst.net.auto_state_off"_lang;
            if (inst::config::autoInstallMode) {
                state = (inst::config::autoInstallTarget == 1) ? "inst.net.auto_state_nand"_lang : "inst.net.auto_state_sd"_lang;
            }
            return "inst.net.auto_state_prefix"_lang + state;
        }

        std::string getIconCacheExtensionFromUrl(const std::string &url) {
            std::string clean = url;
            size_t qpos = clean.find('?');
            if (qpos != std::string::npos) clean = clean.substr(0, qpos);
            size_t hpos = clean.find('#');
            if (hpos != std::string::npos) clean = clean.substr(0, hpos);

            size_t dotPos = clean.find_last_of('.');
            if (dotPos == std::string::npos) return ".png";
            std::string ext = clean.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp") return ext;
            return ".png";
        }

        std::string getPreferredIconCachePath(const std::string &iconUrl) {
            std::hash<std::string> hasher;
            size_t hash = hasher(iconUrl);
            return inst::config::appDir + "/cache/" + std::to_string(hash) + getIconCacheExtensionFromUrl(iconUrl);
        }

        bool isLikelyValidImageFile(const std::string &path) {
            try {
                if (!std::filesystem::exists(path)) return false;
                if (std::filesystem::file_size(path) < 16) return false;
            } catch (...) {
                return false;
            }

            std::ifstream f(path, std::ios::binary);
            if (!f.is_open()) return false;
            unsigned char h[16] = {0};
            f.read(reinterpret_cast<char*>(h), sizeof(h));
            const std::streamsize n = f.gcount();
            if (n < 12) return false;

            // PNG
            if (n >= 8 &&
                h[0] == 0x89 && h[1] == 0x50 && h[2] == 0x4E && h[3] == 0x47 &&
                h[4] == 0x0D && h[5] == 0x0A && h[6] == 0x1A && h[7] == 0x0A) return true;
            // JPEG
            if (n >= 3 && h[0] == 0xFF && h[1] == 0xD8 && h[2] == 0xFF) return true;
            // BMP
            if (h[0] == 'B' && h[1] == 'M') return true;
            // WEBP: RIFF....WEBP
            if (n >= 12 &&
                h[0] == 'R' && h[1] == 'I' && h[2] == 'F' && h[3] == 'F' &&
                h[8] == 'W' && h[9] == 'E' && h[10] == 'B' && h[11] == 'P') return true;
            return false;
        }

        std::string resolveExistingIconCachePath(const std::string &iconUrl) {
            std::hash<std::string> hasher;
            size_t hash = hasher(iconUrl);
            const std::string prefix = inst::config::appDir + "/cache/" + std::to_string(hash);
            const std::vector<std::string> exts = {".png", ".jpg", ".jpeg", ".bmp", ".webp"};

            std::string preferred = getPreferredIconCachePath(iconUrl);
            if (std::filesystem::exists(preferred)) {
                try {
                    if (std::filesystem::file_size(preferred) > 0 && isLikelyValidImageFile(preferred)) return preferred;
                    std::filesystem::remove(preferred);
                } catch (...) {}
            }
            for (const auto &ext : exts) {
                const std::string candidate = prefix + ext;
                if (std::filesystem::exists(candidate)) {
                    try {
                        if (std::filesystem::file_size(candidate) > 0 && isLikelyValidImageFile(candidate)) return candidate;
                        std::filesystem::remove(candidate);
                    } catch (...) {}
                }
            }
            return "";
        }
    }

    static std::string deriveCover512Url(const std::string& imageUrl128) {
        if (imageUrl128.empty()) return "";
        std::string out = imageUrl128;
        size_t p = out.find("_128.");
        if (p != std::string::npos) {
            out.replace(p, 5, "_512.");
            return out;
        }
        p = out.find("_128");
        if (p != std::string::npos) {
            out.replace(p, 4, "_512");
            return out;
        }
        return out;
    }

    netInstPage::netInstPage() : Layout::Layout() {
        this->SetBackgroundColor(COLOR(inst::skin::bg()));
        if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else if (!inst::skin::background().empty()) this->SetBackgroundImage(inst::skin::background());
        this->topRect = Rectangle::New(0, 0, 1280, 94, COLOR(inst::skin::topbar()));
        this->infoRect = Rectangle::New(0, 95, 1280, 60, COLOR(inst::skin::bar()));
        this->botRect = Rectangle::New(0, 660, 1280, 60, COLOR(inst::skin::bar()));

        if (inst::config::gayMode) {
            this->titleImage = Image::New(-113, 0, inst::skin::logo());
            this->appVersionText = TextBlock::New(367, 49, "v" + inst::config::appVersion, 22);
        }
        else {
            this->titleImage = Image::New(0, 0, inst::skin::logo());
            this->appVersionText = TextBlock::New(480, 49, "v" + inst::config::appVersion, 22);
        }
        this->appVersionText->SetColor(COLOR(inst::skin::barText()));
        // Keep top info text aligned with the bottom button hints size.
        this->pageInfoText = TextBlock::New(10, 109, "", 24);
        this->pageInfoText->SetColor(COLOR(inst::skin::barText()));
        this->butText = TextBlock::New(10, 678, "", 24);
        this->butText->SetColor(COLOR(inst::skin::barText()));
        this->menu = pu::ui::elm::Menu::New(0, 156, 1280, COLOR(inst::skin::menuClear()), 84, (506 / 84));
        this->menu->SetOnFocusColor(COLOR(inst::skin::focus()));
        this->menu->SetScrollbarColor(COLOR(inst::skin::scrollSoft()));
        this->infoImage = Image::New(453, 292, inst::skin::icon("lan-connection-waiting.png"));
        this->progressBar = ProgressBar::New(10, 500, 1260, 35, 100.0f);
        this->progressBar->SetVisible(false);
        this->Add(this->topRect);
        this->Add(this->infoRect);
        this->Add(this->botRect);
        this->Add(this->titleImage);
        this->Add(this->appVersionText);
        this->Add(this->butText);
        this->Add(this->pageInfoText);
        this->Add(this->menu);
        this->Add(this->infoImage);
        this->Add(this->progressBar);
        this->AddThread(std::bind(&netInstPage::ssmAsyncTick, this));
    }

    void netInstPage::drawMenuItems(bool clearItems) {
        if (clearItems) this->selectedUrls = {};
        this->menu->ClearItems();
        for (long unsigned int i = 0; i < this->ourUrls.size(); i++) {
            std::string itm;
            if (this->alternativeNames.size() == this->ourUrls.size()) {
                itm = this->alternativeNames[i];
            } else {
                itm = inst::util::shortenString(inst::util::formatUrlString(this->ourUrls[i]), 56, true);
            }
            
            auto ourEntry = pu::ui::elm::MenuItem::New(itm);
            ourEntry->SetColor(COLOR(inst::skin::text()));
            
            std::string iconPath = inst::skin::icon("checkbox-blank-outline.png");
            
            // Utilisation de l'icône en cache si elle existe
            if (!this->imageUrls.empty() && i < this->imageUrls.size() && !this->imageUrls[i].empty()) {
                const std::string cacheFile = resolveExistingIconCachePath(this->imageUrls[i]);
                if (!cacheFile.empty()) iconPath = cacheFile;
            }

            ourEntry->SetIcon(iconPath);

            for (long unsigned int k = 0; k < this->selectedUrls.size(); k++) {
                if (this->selectedUrls[k] == this->ourUrls[i]) {
                    // Si on a une image personnalisée, on met le texte en VERT pour la sélection
                    if (iconPath.find("cache") != std::string::npos) {
                        ourEntry->SetColor(COLOR(inst::skin::ok()));
                    } else {
                        // Sinon on utilise la checkbox classique
                        ourEntry->SetIcon(inst::skin::icon("check-box-outline.png"));
                    }
                }
            }
            this->menu->AddItem(ourEntry);
        }
    }

    void netInstPage::selectTitle(int selectedIndex) {
        std::string targetUrl = this->ourUrls[selectedIndex];
        bool alreadySelected = false;
        size_t selectedIdx = 0;

        for (size_t i = 0; i < this->selectedUrls.size(); i++) {
            if (this->selectedUrls[i] == targetUrl) {
                alreadySelected = true;
                selectedIdx = i;
                break;
            }
        }

        if (alreadySelected) {
            this->selectedUrls.erase(this->selectedUrls.begin() + selectedIdx);
        } else {
            this->selectedUrls.push_back(targetUrl);
        }

        this->drawMenuItems(false);
    }

    void netInstPage::updateAutoInstallWaitLabel() {
        std::string line = buildAutoStateLabel();
        if (inst::config::autoInstallMode) {
            line += "    " + ("inst.net.auto_hint_toggle"_lang);
        } else {
            line += "    " + ("inst.net.auto_hint_lr"_lang);
        }
        this->pageInfoText->SetText(line);
    }

    void netInstPage::ssmAsyncTick() {
        if (!this->isSsmLoading) {
            return;
        }

        bool running = false;
        bool done = false;
        bool failed = false;
        bool canceledResult = false;
        float progress = 0.0f;
        std::string status;
        std::string error;
        SsmLoadPayload payload;

        {
            std::lock_guard<std::mutex> lock(g_ssmLoad.mtx);
            running = g_ssmLoad.running;
            done = g_ssmLoad.done;
            failed = g_ssmLoad.failed;
            canceledResult = g_ssmLoad.canceledResult;
            progress = g_ssmLoad.progress;
            status = g_ssmLoad.status;
            error = g_ssmLoad.error;
            if (done && !failed && !canceledResult) {
                payload = g_ssmLoad.payload;
            }
        }

        if (running) {
            this->progressBar->SetVisible(true);
            this->progressBar->SetProgress(progress);
            if (!status.empty()) this->pageInfoText->SetText(status);
            return;
        }

        if (!done) {
            return;
        }

        if (g_ssmLoad.worker.joinable()) {
            g_ssmLoad.worker.join();
        }

        this->isSsmLoading = false;
        this->progressBar->SetVisible(false);

        if (canceledResult) {
            mainApp->LoadLayout(mainApp->mainPage);
            return;
        }

        if (failed) {
            std::string message = "inst.net.ssm.generic_error"_lang;
            if (!error.empty()) message += "\n\n" + error;
            mainApp->CreateShowDialog("inst.net.failed"_lang, message, {"common.ok"_lang}, false);
            mainApp->LoadLayout(mainApp->mainPage);
            return;
        }

        this->ourUrls = std::move(payload.urls);
        this->alternativeNames = std::move(payload.names);
        this->imageUrls = std::move(payload.imageUrls);
        this->coverUrls = std::move(payload.coverUrls);

        sourceString = "__ssm__";
        this->pageInfoText->SetText("inst.net.top_info"_lang);
        this->butText->SetText("inst.net.buttons1"_lang);
        this->drawMenuItems(true);
        this->menu->SetSelectedIndex(0);
        this->infoImage->SetVisible(false);
        this->menu->SetVisible(true);
        mainApp->CallForRender();
        inst::inventory::sendFromUiThread();  // list shown: now report the console inventory to SSM
    }

    void netInstPage::startSSM() {
        this->isSsmMode = true;
        this->isSsmLoading = false;
        this->butText->SetText("inst.net.buttons"_lang + "inst.net.auto_buttons_suffix"_lang);
        this->menu->SetVisible(false);
        this->menu->ClearItems();
        this->infoImage->SetVisible(true);
        this->progressBar->SetVisible(false);
        mainApp->LoadLayout(mainApp->netinstPage);

        std::string keyboardResult = inst::util::softwareKeyboard("inst.net.ssm.ip_prompt"_lang, inst::config::lastSSMIp, 30);
        if (keyboardResult.size() > 0) {
            keyboardResult = trimCopy(keyboardResult);
            inst::config::lastSSMIp = keyboardResult;
            inst::config::setConfig();

            auto endpointCandidates = buildSsmCandidates(keyboardResult);
            if (endpointCandidates.empty()) {
                mainApp->CreateShowDialog("inst.net.failed"_lang, "inst.net.ssm.fetch_failed"_lang, {"common.ok"_lang}, false);
                this->isSsmMode = false;
                mainApp->LoadLayout(mainApp->mainPage);
                return;
            }
            if (g_ssmLoad.worker.joinable()) {
                g_ssmLoad.worker.join();
            }

            ssmPrepareRun();
            this->isSsmLoading = true;
            this->progressBar->SetVisible(true);
            this->progressBar->SetProgress(0.0f);
            this->butText->SetText("inst.net.ssm.buttons_loading"_lang);
            this->pageInfoText->SetText("inst.net.ssm.loading_list"_lang);
            mainApp->CallForRender();

            g_ssmLoad.worker = std::thread([endpointCandidates]() {
                try {
                    SsmLoadPayload payload;
                    std::string cachePath = inst::config::appDir + "/list_cache.json";
                    std::string etagPath = inst::config::appDir + "/list_cache.etag";
                    std::string baseUrl;
                    std::string targetUrl;
                    std::string jsonStr;
                    bool usedCachedList = false;

                    if (ssmIsCanceled()) {
                        ssmFinishCanceled();
                        return;
                    }

                    ssmSetStatus(5.0f, "inst.net.ssm.loading_list"_lang);

                    int idx = 0;
                    for (const auto &cand : endpointCandidates) {
                        if (ssmIsCanceled()) {
                            ssmFinishCanceled();
                            return;
                        }
                        idx++;
                        ssmSetStatus(
                            5.0f,
                            "inst.net.ssm.loading_list"_lang + " (" +
                            std::to_string(idx) + "/" + std::to_string(endpointCandidates.size()) + ")"
                        );
                        // SSM list generation can take >2s on large libraries.
                        // Keep a safer timeout to avoid false failures.
                        std::string fetched = inst::curl::downloadToBuffer(cand.listUrl, -1, -1, 5000);
                        if (!fetched.empty()) {
                            jsonStr = std::move(fetched);
                            baseUrl = cand.baseUrl;
                            targetUrl = cand.listUrl;
                            break;
                        }
                    }

                    const bool allowOfflineListCache = false;
                    if (allowOfflineListCache && jsonStr.empty() && std::filesystem::exists(cachePath)) {
                        std::ifstream f(cachePath);
                        if (f.is_open()) {
                            std::stringstream buffer;
                            buffer << f.rdbuf();
                            jsonStr = buffer.str();
                            f.close();
                            usedCachedList = true;
                            // Keep predictable base for relative paths when using cache.
                            baseUrl = endpointCandidates[0].baseUrl;
                            ssmSetStatus(12.0f, "inst.net.ssm.loading_list"_lang + " (cache offline)");
                        }
                    }

                    if (!jsonStr.empty() && !baseUrl.empty()) inst::inventory::rememberBaseUrl(baseUrl);

                    if (jsonStr.empty()) {
                        std::string msg = "inst.net.ssm.fetch_failed"_lang;
                        msg += "\n";
                        for (const auto &cand : endpointCandidates) {
                            msg += cand.listUrl + "\n";
                        }
                        ssmFinishError(msg);
                        return;
                    }

                    if (!targetUrl.empty()) {
                        std::ofstream cacheFile(cachePath);
                        cacheFile << jsonStr;
                        cacheFile.close();
                        std::ofstream etagFile(etagPath);
                        etagFile << targetUrl;
                        etagFile.close();
                    }

                    if (ssmIsCanceled()) {
                        ssmFinishCanceled();
                        return;
                    }

                    auto j = nlohmann::json::parse(jsonStr);
                    if (!j.contains("files") || !j["files"].is_array()) {
                        ssmFinishError("inst.net.ssm.invalid_json"_lang);
                        return;
                    }

                    for (const auto& file : j["files"]) {
                        std::string fileUrl;
                        std::string displayName;
                        std::string iconUrl;
                        std::string coverUrl;
                        if (file.is_object()) {
                            if (file.contains("url")) fileUrl = file["url"];
                            if (file.contains("name")) displayName = file["name"];
                            if (file.contains("region")) displayName += " [" + std::string(file["region"]) + "]";
                            if (file.contains("size")) displayName += " (" + std::string(file["size"]) + ")";

                            if (file.contains("image_url")) {
                                iconUrl = file["image_url"];
                                if (iconUrl.find("http") == std::string::npos) iconUrl = baseUrl + iconUrl;
                            }
                            if (file.contains("image_512_url")) {
                                coverUrl = file["image_512_url"];
                            } else if (file.contains("cover_url")) {
                                coverUrl = file["cover_url"];
                            }
                            if (coverUrl.empty() && !iconUrl.empty()) coverUrl = deriveCover512Url(iconUrl);
                            if (!coverUrl.empty() && coverUrl.find("http") == std::string::npos) coverUrl = baseUrl + coverUrl;
                            payload.imageUrls.push_back(iconUrl);
                            payload.coverUrls.push_back(coverUrl);
                        } else if (file.is_string()) {
                            fileUrl = file;
                            payload.imageUrls.push_back("");
                            payload.coverUrls.push_back("");
                        }
                        if (displayName.empty()) displayName = inst::util::shortenString(inst::util::formatUrlString(fileUrl), 56, true);
                        if (fileUrl.find("http") == std::string::npos) fileUrl = baseUrl + fileUrl;
                        payload.urls.push_back(fileUrl);
                        payload.names.push_back(displayName);
                    }

                    if (ssmIsCanceled()) {
                        ssmFinishCanceled();
                        return;
                    }

                    if (!usedCachedList && !payload.imageUrls.empty()) {
                        const size_t totalIcons = payload.imageUrls.size();
                        for (size_t i = 0; i < totalIcons; i++) {
                            if (ssmIsCanceled()) {
                                ssmFinishCanceled();
                                return;
                            }
                            const std::string &iconUrl = payload.imageUrls[i];
                            if (iconUrl.empty()) continue;

                            const std::string existing = resolveExistingIconCachePath(iconUrl);
                            if (!existing.empty()) continue;
                            const std::string filename = getPreferredIconCachePath(iconUrl);

                            const double ratio = (double)(i + 1) / (double)totalIcons;
                            ssmSetStatus((float)(20.0 + (ratio * 80.0)),
                                "inst.net.ssm.loading_icons"_lang + " (" + std::to_string(i + 1) + "/" + std::to_string(totalIcons) + ")");
                            bool ok = inst::curl::downloadFile(iconUrl, filename.c_str(), 2500, false);
                            if (!ok || !isLikelyValidImageFile(filename)) {
                                try { std::filesystem::remove(filename); } catch (...) {}
                                ok = inst::curl::downloadFile(iconUrl, filename.c_str(), 6000, false);
                                if (!ok || !isLikelyValidImageFile(filename)) {
                                    try { std::filesystem::remove(filename); } catch (...) {}
                                }
                            }
                        }
                    }

                    ssmFinishSuccess(std::move(payload));
                } catch (std::exception& e) {
                    ssmFinishError((std::string)e.what());
                } catch (...) {
                    ssmFinishError("inst.net.ssm.generic_error"_lang);
                }
            });
            return;
        } else {
            this->isSsmMode = false;
            mainApp->LoadLayout(mainApp->mainPage);
            return;
        }
    }

    // One round of "wait for a transfer". Returns true when the console should wait for another one
    // (after an automatic install); a loop in startNetwork() replaces the old recursive call, so a long
    // session with many transfers no longer grows the stack.
    void netInstPage::startNetwork() {
        while (this->startNetworkOnce()) {
        }
    }

    bool netInstPage::startNetworkOnce() {
        this->isSsmMode = false;
        this->isSsmLoading = false;
        // Explicitly mark classic network mode.
        sourceString = "inst.net.source_string"_lang;
        this->butText->SetText("inst.net.buttons"_lang + "inst.net.auto_buttons_suffix"_lang);
        this->menu->SetVisible(false);
        this->menu->ClearItems();
        this->infoImage->SetVisible(true);
        mainApp->LoadLayout(mainApp->netinstPage);
        
        if (inst::config::autoInstallMode) {
            this->updateAutoInstallWaitLabel();
        } else {
            this->updateAutoInstallWaitLabel();
        }

        this->ourUrls = netInstStuff::OnSelected();
        if (!this->ourUrls.size()) {
            inst::config::autoInstallMode = false;
            mainApp->LoadLayout(mainApp->mainPage);
            return false;
        } else if (this->ourUrls[0] == "supplyUrl") {
            std::string keyboardResult;
            switch (mainApp->CreateShowDialog("inst.net.src.title"_lang, "common.cancel_desc"_lang, {"inst.net.src.opt0"_lang, "inst.net.src.opt1"_lang}, false)) {
                case 0:
                    keyboardResult = inst::util::softwareKeyboard("inst.net.url.hint"_lang, inst::config::lastNetUrl, 500);
                    if (keyboardResult.size() > 0) {
                        SsmLoadPayload quickPayload;
                        std::string quickErr;
                        if (tryLoadSsmListImmediate(keyboardResult, quickPayload, quickErr)) {
                            this->isSsmMode = true;
                            inst::config::lastSSMIp = keyboardResult;
                            inst::config::setConfig();
                            sourceString = "__ssm__";
                            this->ourUrls = std::move(quickPayload.urls);
                            this->alternativeNames = std::move(quickPayload.names);
                            this->imageUrls = std::move(quickPayload.imageUrls);
                            this->coverUrls = std::move(quickPayload.coverUrls);
                            this->selectedUrls = {};
                            this->pageInfoText->SetText("inst.net.top_info"_lang);
                            this->butText->SetText("inst.net.buttons1"_lang);
                            this->drawMenuItems(true);
                            this->menu->SetSelectedIndex(0);
                            this->infoImage->SetVisible(false);
                            this->menu->SetVisible(true);
                            mainApp->CallForRender();
                            inst::inventory::sendFromUiThread();
                            return false;
                        }
                        if (inst::util::formatUrlString(keyboardResult) == "" || keyboardResult == "https://" || keyboardResult == "http://") {
                            mainApp->CreateShowDialog("inst.net.url.invalid"_lang, "", {"common.ok"_lang}, false);
                            break;
                        }
                        inst::config::lastNetUrl = keyboardResult;
                        inst::config::setConfig();
                        sourceString = "inst.net.url.source_string"_lang;
                        this->selectedUrls = {keyboardResult};
                        this->startInstall(true);
                        return false;
                    }
                    break;
                case 1:
                    keyboardResult = inst::util::softwareKeyboard("inst.net.gdrive.hint"_lang, lastFileID, 50);
                    if (keyboardResult.size() > 0) {
                        lastFileID = keyboardResult;
                        std::string fileName = inst::util::getDriveFileName(keyboardResult);
                        if (fileName.size() > 0) this->alternativeNames = {fileName};
                        else this->alternativeNames = {"inst.net.gdrive.alt_name"_lang};
                        sourceString = "inst.net.gdrive.source_string"_lang;
                        this->selectedUrls = {"https://www.googleapis.com/drive/v3/files/" + keyboardResult + "?key=" + inst::config::gAuthKey + "&alt=media"};
                        this->startInstall(true);
                        return false;
                    }
                    break;
            }
            return true;
        } 
        
        int remoteTarget = inst::cheats::remoteInstallTarget();
        bool remoteTransfer = inst::cheats::remoteTransferActive();
        if (remoteTransfer && inst::config::remoteManagement) {
            // full remote management: location chosen in SSM (SD card when it did not say), no question, no dialog
            netInstStuff::silentInstall = true;
            netInstStuff::installTitleNet(this->ourUrls, remoteTarget >= 0 ? remoteTarget : 0, this->alternativeNames, sourceString, inst::cheats::takeRemoteCover());
            netInstStuff::silentInstall = false;
            return true;
        }
        if (remoteTransfer) {
            // transfer announced by SSM without remote management: no file list, only the location is asked
            int choice = mainApp->CreateShowDialog(
                "inst.target.desc00"_lang + std::to_string(this->ourUrls.size()) + "inst.target.desc01"_lang,
                "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
            std::string remoteCover = inst::cheats::takeRemoteCover();
            if (choice >= 0) netInstStuff::installTitleNet(this->ourUrls, choice, this->alternativeNames, sourceString, remoteCover);
            return true;
        }
        if (remoteTarget >= 0) {
            netInstStuff::installTitleNet(this->ourUrls, remoteTarget, this->alternativeNames, sourceString, inst::cheats::takeRemoteCover());
            return true;
        }
        if (inst::config::autoInstallMode) {
            netInstStuff::installTitleNet(this->ourUrls, inst::config::autoInstallTarget, this->alternativeNames, sourceString);
            return true;
        } else {
            sourceString = "inst.net.source_string"_lang;
            this->pageInfoText->SetText("inst.net.top_info"_lang);
            this->butText->SetText("inst.net.buttons1"_lang);
            this->drawMenuItems(true);
            this->menu->SetSelectedIndex(0);
            mainApp->CallForRender();
            this->infoImage->SetVisible(false);
            this->menu->SetVisible(true);
        }
        return false;
    }

    void netInstPage::startInstall(bool urlMode) {
        if (sourceString == "__ssm__") {
            std::string ourIP = inst::util::getIPAddress();
            
            // Open port 2000 BEFORE sending the trigger
            netInstStuff::InitializeServerSocket();

            for (const auto& baseUrl : this->selectedUrls) {
                // Find corresponding image URL
                std::string coverPath = "";
                int idx = -1;
                for (size_t i = 0; i < this->ourUrls.size(); i++) {
                    if (this->ourUrls[i] == baseUrl) {
                        idx = i;
                        break;
                    }
                }

                if (idx != -1) {
                    std::string imgUrl = "";
                    if (idx < (int)this->coverUrls.size() && !this->coverUrls[idx].empty()) imgUrl = this->coverUrls[idx];
                    else if (idx < (int)this->imageUrls.size() && !this->imageUrls[idx].empty()) imgUrl = this->imageUrls[idx];
                    if (imgUrl.empty()) {
                        coverPath = "";
                    } else {
                    std::string ext = ".jpg";
                    size_t qpos = imgUrl.find('?');
                    std::string cleanUrl = (qpos == std::string::npos) ? imgUrl : imgUrl.substr(0, qpos);
                    size_t dpos = cleanUrl.find_last_of('.');
                    if (dpos != std::string::npos) {
                        std::string e = cleanUrl.substr(dpos);
                        if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" || e == ".webp") ext = e;
                    }
                    coverPath = inst::config::appDir + "/temp_cover" + ext;
                    // Download cover just before install start.
                    if (std::filesystem::exists(coverPath)) std::filesystem::remove(coverPath);
                    inst::curl::downloadFile(imgUrl, coverPath.c_str(), 12000, false);
                    }
                }

                std::string triggerUrl = baseUrl;
                if (triggerUrl.find('?') == std::string::npos) triggerUrl += "?";
                else triggerUrl += "&";
                triggerUrl += "switch_ip=" + ourIP + "&switch_port=2000";

                // 1. Send Trigger Request (Short timeout to switch quickly to accept)
                std::string res = inst::curl::downloadToBuffer(triggerUrl, -1, -1, 2000);
                
                if (res.empty()) {
                    mainApp->CreateShowDialog("main.usb.warn.title"_lang, "inst.net.ssm.request_failed"_lang, {"common.ok"_lang}, true);
                    if (!coverPath.empty() && std::filesystem::exists(coverPath)) std::filesystem::remove(coverPath);
                    continue;
                }

                // Parse SSM API response: if install is rejected (e.g. missing files),
                // show the explicit server reason and skip connection wait.
                bool apiAccepted = true;
                std::string apiMessage;
                try {
                    auto jres = nlohmann::json::parse(res);
                    if (jres.is_object()) {
                        if (jres.contains("accepted")) apiAccepted = (bool)jres["accepted"];
                        if (jres.contains("message") && jres["message"].is_string()) apiMessage = (std::string)jres["message"];
                    }
                } catch (...) {
                    // Non-JSON response: keep legacy behavior and continue.
                }
                if (!apiAccepted) {
                    std::string dialogMsg = apiMessage.empty() ? "inst.net.ssm.request_failed"_lang : apiMessage;
                    mainApp->CreateShowDialog("inst.net.failed"_lang, dialogMsg, {"common.ok"_lang}, true);
                    if (!coverPath.empty() && std::filesystem::exists(coverPath)) std::filesystem::remove(coverPath);
                    continue;
                }

                // 2. Listen for connection
                std::vector<std::string> bundleUrls = netInstStuff::OnSelected();
                if (bundleUrls.empty()) {
                    if (!coverPath.empty() && std::filesystem::exists(coverPath)) std::filesystem::remove(coverPath);
                    continue;
                }
                int dialogResult = -1;
                int remoteTargetSsm = inst::cheats::remoteInstallTarget();
                if (remoteTargetSsm >= 0) dialogResult = remoteTargetSsm;
                else if (inst::config::autoInstallMode) dialogResult = inst::config::autoInstallTarget;
                else dialogResult = mainApp->CreateShowDialog("inst.target.desc00"_lang + std::to_string(bundleUrls.size()) + "inst.target.desc01"_lang, "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
                if (dialogResult == -1) {
                    if (!coverPath.empty() && std::filesystem::exists(coverPath)) std::filesystem::remove(coverPath);
                    continue;
                }
                // In SSM remote mode, signature validation can stop after probe reads
                // on some dumps/keys setups. We disable it only for this install flow.
                bool prevValidateNCAs = inst::config::validateNCAs;
                inst::config::validateNCAs = false;
                netInstStuff::installTitleNet(bundleUrls, dialogResult, {}, "inst.net.ssm.source_string"_lang, coverPath);
                inst::config::validateNCAs = prevValidateNCAs;
            }
            return;
        }

        int dialogResult = -1;
        if (this->selectedUrls.size() == 1) {
            std::string ourUrlString;
            if (this->alternativeNames.size() > 0) ourUrlString = inst::util::shortenString(this->alternativeNames[0], 32, true);
            else ourUrlString = inst::util::shortenString(inst::util::formatUrlString(this->selectedUrls[0]), 32, true);
            dialogResult = mainApp->CreateShowDialog("inst.target.desc0"_lang + ourUrlString + "inst.target.desc1"_lang, "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
        } else dialogResult = mainApp->CreateShowDialog("inst.target.desc00"_lang + std::to_string(this->selectedUrls.size()) + "inst.target.desc01"_lang, "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
        if (dialogResult == -1 && !urlMode) return;
        else if (dialogResult == -1 && urlMode) {
            this->startNetwork();
            return;
        }
        netInstStuff::installTitleNet(this->selectedUrls, dialogResult, this->alternativeNames, sourceString);
        return;
    }

    void netInstPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        inst::remote::processPending();
        if (this->isSsmLoading) {
            if (Down & HidNpadButton_B) {
                ssmRequestCancel();
                this->pageInfoText->SetText("inst.net.ssm.canceling"_lang);
                this->progressBar->SetVisible(true);
                mainApp->CallForRender();
            }
            return;
        }

        if (Down & HidNpadButton_B) {
            if (this->menu->GetItems().size() > 0){
                if (this->selectedUrls.size() == 0) this->selectTitle(this->menu->GetSelectedIndex());
                netInstStuff::sendExitCommands(inst::util::formatUrlLink(this->selectedUrls[0]));
            }
            this->isSsmMode = false;
            this->isSsmLoading = false;
            netInstStuff::OnUnwound();
            mainApp->LoadLayout(mainApp->mainPage);
        }
        // Quick auto-install shortcuts (network classic only, not SSM mode):
        // L => NAND, R => SD
        if (!this->isSsmMode) {
            if (Down & HidNpadButton_L) {
                if (inst::config::autoInstallMode && inst::config::autoInstallTarget == 1) inst::config::autoInstallMode = false;
                else {
                    inst::config::autoInstallMode = true;
                    inst::config::autoInstallTarget = 1; // NAND
                }
                inst::config::setConfig();
                this->updateAutoInstallWaitLabel();
                mainApp->CallForRender();
                this->startNetwork();
                return;
            }
            if (Down & HidNpadButton_R) {
                if (inst::config::autoInstallMode && inst::config::autoInstallTarget == 0) inst::config::autoInstallMode = false;
                else {
                    inst::config::autoInstallMode = true;
                    inst::config::autoInstallTarget = 0; // SD
                }
                inst::config::setConfig();
                this->updateAutoInstallWaitLabel();
                mainApp->CallForRender();
                this->startNetwork();
                return;
            }
        }

        // Keep fork behavior for SSM mode: page navigation on L/R.
        // Classic mode keeps page jump on ZL/ZR.
        if (this->isSsmMode) {
            if (Down & HidNpadButton_L) this->menu->SetSelectedIndex(std::max(0, this->menu->GetSelectedIndex() - 6));
            if (Down & HidNpadButton_R) this->menu->SetSelectedIndex(std::min((int)this->menu->GetItems().size() - 1, this->menu->GetSelectedIndex() + 6));
        } else {
            if (Down & HidNpadButton_ZL) this->menu->SetSelectedIndex(std::max(0, this->menu->GetSelectedIndex() - 6));
            if (Down & HidNpadButton_ZR) this->menu->SetSelectedIndex(std::min((int)this->menu->GetItems().size() - 1, this->menu->GetSelectedIndex() + 6));
        }

        if ((Down & HidNpadButton_A) || (Up & TouchPseudoKey)) {
            this->selectTitle(this->menu->GetSelectedIndex());
        }
        if ((Down & HidNpadButton_Y)) {
            if (this->selectedUrls.size() == this->menu->GetItems().size()) this->drawMenuItems(true);
            else {
                for (long unsigned int i = 0; i < this->menu->GetItems().size(); i++) {
                    if (this->menu->GetItems()[i]->GetIcon() == inst::skin::icon("check-box-outline.png")) continue;
                    else this->selectTitle(i);
                }
                this->drawMenuItems(false);
            }
        }
        if (Down & HidNpadButton_Plus) {
            if (this->selectedUrls.size() == 0) this->selectTitle(this->menu->GetSelectedIndex());
            this->startInstall(false);
        }
    }
}
