#include "util/cheats_server.hpp"

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <mutex>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <switch.h>

#include "util/json.hpp"
#include "util/power.hpp"
#include "util/config.hpp"
#include "netInstall.hpp"
#include <ctime>

namespace {
    constexpr size_t kMaxHeader = 8192;
    constexpr size_t kMaxBody = 2 * 1024 * 1024;

    std::atomic<bool> g_running{false};
    std::thread g_thread;
    int g_listen = -1;
    std::atomic<bool> g_finished{false};
    std::mutex g_uninstallMutex;
    std::vector<inst::cheats::UninstallRequest> g_uninstallQueue;
    bool g_inventoryRequested = false;  // guarded by g_uninstallMutex
    std::string g_inventoryUrl;
    std::vector<inst::cheats::BackupRequest> g_backupQueue;
    std::atomic<bool> g_netInstallRequested{false};
    std::atomic<int> g_remoteTarget{-1};
    std::atomic<long long> g_remoteTargetUntil{0};
    std::atomic<long long> g_remoteTransferUntil{0};
    std::mutex g_coverMutex;
    std::string g_coverPath;
    long long g_coverUntil = 0;

    bool isHex16(const std::string &s) {
        if (s.size() != 16) return false;
        for (char c : s) if (!isxdigit(static_cast<unsigned char>(c))) return false;
        return true;
    }

    bool sendAll(int fd, const char *data, size_t len) {
        while (len > 0) {
            ssize_t n = send(fd, data, len, 0);
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // the socket buffer is full (big answers such as the album list): wait until it drains
                pollfd out{fd, POLLOUT, 0};
                if (poll(&out, 1, 8000) <= 0) return false;
                continue;
            }
            if (n <= 0) return false;
            data += n;
            len -= static_cast<size_t>(n);
        }
        return true;
    }

    void reply(int fd, int code, const char *text, const std::string &body) {
        std::string head = "HTTP/1.1 " + std::to_string(code) + " " + text +
                           "\r\nContent-Type: text/plain\r\nConnection: close\r\nContent-Length: " +
                           std::to_string(body.size()) + "\r\n\r\n";
        sendAll(fd, head.data(), head.size());
        sendAll(fd, body.data(), body.size());
    }

    bool readSome(int fd, std::string &buf) {
        pollfd p{fd, POLLIN, 0};
        if (poll(&p, 1, 8000) <= 0) return false;
        char tmp[4096];
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) return false;
        buf.append(tmp, static_cast<size_t>(n));
        return true;
    }

    void makeDirs(const std::string &path) {
        // path is a full directory path such as sdmc:/atmosphere/contents/<tid>/cheats
        for (size_t pos = path.find('/', 7); pos != std::string::npos; pos = path.find('/', pos + 1)) {
            mkdir(path.substr(0, pos).c_str(), 0777);
        }
        mkdir(path.c_str(), 0777);
    }

    // ---- album (screenshots and videos taken in games): sdmc:/Nintendo/Album/<year>/<month>/<day>/<file>
    // The album is not always in sdmc:/Nintendo/Album (emuMMC moves the Nintendo folder), so ask the system
    // for the real image directory of the SD card and mount it; fall back to the usual folders.
    std::string g_albumRoot;
    std::string g_albumOverride;  // folder chosen in SSM (relative to the SD root), empty = automatic
    bool g_albumMounted = false;

    bool safeOverride(const std::string &path) {
        if (path.empty() || path.size() > 200 || path.find("..") != std::string::npos) return false;
        for (char c : path) {
            if (!(isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '.' || c == '_' || c == '-' || c == ' ')) return false;
        }
        return true;
    }

    std::string percentDecode(const std::string &text) {
        std::string decoded;
        for (size_t i = 0; i < text.size(); i++) {
            if (text[i] == '%' && i + 2 < text.size() + 0 && isxdigit(static_cast<unsigned char>(text[i + 1])) && isxdigit(static_cast<unsigned char>(text[i + 2]))) {
                decoded += static_cast<char>(strtol(text.substr(i + 1, 2).c_str(), nullptr, 16));
                i += 2;
            } else if (text[i] == '+') decoded += ' ';
            else decoded += text[i];
        }
        return decoded;
    }

    bool dirHasEntries(const std::string &path) {
        DIR *dir = opendir(path.c_str());
        if (!dir) return false;
        bool any = false;
        while (struct dirent *entry = readdir(dir)) {
            std::string name = entry->d_name;
            if (name != "." && name != "..") { any = true; break; }
        }
        closedir(dir);
        return any;
    }

    const std::string &albumRoot() {
        if (!g_albumRoot.empty()) return g_albumRoot;
        if (!g_albumOverride.empty()) {
            std::string rel = g_albumOverride;
            while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) rel.erase(0, 1);
            g_albumRoot = "sdmc:/" + rel;
            while (g_albumRoot.size() > 6 && g_albumRoot.back() == '/') g_albumRoot.pop_back();
            return g_albumRoot;
        }
        FsFileSystem fs;
        if (R_SUCCEEDED(fsOpenImageDirectoryFileSystem(&fs, FsImageDirectoryId_Sd))) {
            if (fsdevMountDevice("album", fs) >= 0) {
                g_albumMounted = true;
                g_albumRoot = "album:";
                return g_albumRoot;
            }
            fsFsClose(&fs);
        }
        if (dirHasEntries("sdmc:/Nintendo/Album")) { g_albumRoot = "sdmc:/Nintendo/Album"; return g_albumRoot; }
        // emuMMC keeps its own Nintendo folder: sdmc:/emuMMC/<RAW1, SD1, any name>/Nintendo/Album
        if (DIR *dir = opendir("sdmc:/emuMMC")) {
            std::vector<std::string> names;
            while (struct dirent *entry = readdir(dir)) {
                std::string name = entry->d_name;
                if (name != "." && name != "..") names.push_back(name);
            }
            closedir(dir);
            for (const auto &name : names) {
                std::string candidate = "sdmc:/emuMMC/" + name + "/Nintendo/Album";
                if (dirHasEntries(candidate)) { g_albumRoot = candidate; return g_albumRoot; }
            }
        }
        g_albumRoot = "sdmc:/Nintendo/Album";
        return g_albumRoot;
    }

    bool albumExtension(const std::string &name) {
        size_t dot = name.rfind('.');
        if (dot == std::string::npos) return false;
        std::string ext = name.substr(dot + 1);
        for (auto &c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "mp4";
    }

    void collectAlbum(const std::string &disk, const std::string &rel, int depth, nlohmann::json &out) {
        DIR *dir = opendir(disk.c_str());
        if (!dir) return;
        std::vector<std::pair<std::string, bool>> entries;
        while (struct dirent *entry = readdir(dir)) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            entries.emplace_back(name, entry->d_type == DT_DIR);
        }
        closedir(dir);
        for (const auto &entry : entries) {
            std::string childDisk = disk + "/" + entry.first;
            std::string childRel = rel.empty() ? entry.first : rel + "/" + entry.first;
            if (entry.second) {
                if (depth < 5) collectAlbum(childDisk, childRel, depth + 1, out);
            } else if (albumExtension(entry.first)) {
                struct stat info;
                if (stat(childDisk.c_str(), &info) == 0) out.push_back({{"p", childRel}, {"s", (long long)info.st_size}});
            }
        }
    }

    // Only plain relative paths made of safe characters are accepted.
    bool safeAlbumPath(const std::string &path) {
        if (path.empty() || path.size() > 200 || path[0] == '/') return false;
        if (path.find("..") != std::string::npos) return false;
        for (char c : path) {
            if (!(isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '.' || c == '_' || c == '-')) return false;
        }
        return albumExtension(path);
    }

    void sendAlbumFile(int fd, const std::string &rel) {
        std::string disk = albumRoot() + "/" + rel;
        struct stat info;
        FILE *in = stat(disk.c_str(), &info) == 0 ? std::fopen(disk.c_str(), "rb") : nullptr;
        if (!in) { reply(fd, 404, "Not Found", "not found" + std::string(1, 10)); return; }
        std::string head = "HTTP/1.1 200 OK" + std::string(1, 13) + std::string(1, 10) +
                           "Content-Type: application/octet-stream" + std::string(1, 13) + std::string(1, 10) +
                           "Connection: close" + std::string(1, 13) + std::string(1, 10) +
                           "Content-Length: " + std::to_string((long long)info.st_size) + std::string(1, 13) + std::string(1, 10) +
                           std::string(1, 13) + std::string(1, 10);
        if (sendAll(fd, head.data(), head.size())) {
            static std::vector<char> buffer(64 * 1024);
            size_t got;
            while ((got = std::fread(buffer.data(), 1, buffer.size(), in)) > 0) {
                if (!sendAll(fd, buffer.data(), got)) break;
            }
        }
        std::fclose(in);
    }

    void handle(int fd) {
        std::string buf;
        size_t headerEnd = std::string::npos;
        while (headerEnd == std::string::npos) {
            if (buf.size() > kMaxHeader || !readSome(fd, buf)) { reply(fd, 400, "Bad Request", "bad request\n"); return; }
            headerEnd = buf.find("\r\n\r\n");
        }
        std::string head = buf.substr(0, headerEnd);
        std::string body = buf.substr(headerEnd + 4);

        size_t eol = head.find("\r\n");
        std::string requestLine = head.substr(0, eol);
        size_t sp1 = requestLine.find(' ');
        size_t sp2 = requestLine.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) { reply(fd, 400, "Bad Request", "bad request\n"); return; }
        std::string method = requestLine.substr(0, sp1);
        std::string target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);

        if (method == "GET" && target == "/status") {
            // lets SSM know that the console is really waiting for the next file (the previous install is over)
            reply(fd, 200, "OK", std::string("{\"waiting\":") + (netInstStuff::isListening() ? "true" : "false") + "}");
            return;
        }
        if (method == "POST" && target == "/session/end") {
            inst::cheats::endRemoteSession();
            reply(fd, 200, "OK", "ok" + std::string(1, 10));
            return;
        }
        if (method == "GET" && target == "/ping") { reply(fd, 200, "OK", "SSI cheats\n"); return; }
        if (method == "GET" && (target == "/album" || target.compare(0, 12, "/album?root=") == 0)) {
            if (target.size() > 6) {
                // /album?root=<folder on the SD card> : manual choice from SSM, empty = automatic
                std::string wanted = percentDecode(target.substr(12));
                if (wanted != g_albumOverride && (wanted.empty() || safeOverride(wanted))) {
                    g_albumOverride = wanted;
                    g_albumRoot.clear();
                }
            }
            nlohmann::json files = nlohmann::json::array();
            collectAlbum(albumRoot(), "", 0, files);
            nlohmann::json out;
            out["files"] = files;
            out["root"] = albumRoot();
            reply(fd, 200, "OK", out.dump());
            return;
        }
        if (method == "GET" && target.compare(0, 12, "/album/file?") == 0) {
            std::string query = target.substr(12);
            if (query.compare(0, 2, "p=") != 0) { reply(fd, 400, "Bad Request", "bad query" + std::string(1, 10)); return; }
            std::string rel = query.substr(2);
            // the path is sent with its slashes escaped as %2F
            std::string decoded;
            for (size_t i = 0; i < rel.size(); i++) {
                if (rel[i] == '%' && i + 2 < rel.size() + 0 && isxdigit(static_cast<unsigned char>(rel[i + 1])) && isxdigit(static_cast<unsigned char>(rel[i + 2]))) {
                    decoded += static_cast<char>(strtol(rel.substr(i + 1, 2).c_str(), nullptr, 16));
                    i += 2;
                } else decoded += rel[i];
            }
            if (!safeAlbumPath(decoded)) { reply(fd, 400, "Bad Request", "bad path" + std::string(1, 10)); return; }
            sendAlbumFile(fd, decoded);
            return;
        }
        if (method == "POST" && target == "/cover") {
            size_t clen = 0;
            size_t ccl = head.find("Content-Length:");
            if (ccl == std::string::npos) ccl = head.find("content-length:");
            if (ccl != std::string::npos) clen = static_cast<size_t>(strtoull(head.c_str() + ccl + 15, nullptr, 10));
            if (clen < 16 || clen > 4 * 1024 * 1024) { reply(fd, 413, "Payload Too Large", "bad size" + std::string(1, 10)); return; }
            while (body.size() < clen) {
                if (!readSome(fd, body)) { reply(fd, 400, "Bad Request", "incomplete body" + std::string(1, 10)); return; }
            }
            body.resize(clen);
            const unsigned char *magic = reinterpret_cast<const unsigned char *>(body.data());
            std::string extension;
            if (magic[0] == 0xFF && magic[1] == 0xD8) extension = ".jpg";
            else if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') extension = ".png";
            else { reply(fd, 415, "Unsupported Media Type", "jpeg or png only" + std::string(1, 10)); return; }
            std::string path = "sdmc:/switch/Switch-Super-Installer/temp_cover" + extension;
            FILE *out = fopen(path.c_str(), "wb");
            if (!out) { reply(fd, 500, "Internal Server Error", "cannot write" + std::string(1, 10)); return; }
            size_t written = fwrite(body.data(), 1, body.size(), out);
            fclose(out);
            if (written != body.size()) { reply(fd, 507, "Insufficient Storage", "write failed" + std::string(1, 10)); return; }
            {
                std::lock_guard<std::mutex> lock(g_coverMutex);
                g_coverPath = path;
                g_coverUntil = static_cast<long long>(time(nullptr)) + 600;
            }
            reply(fd, 200, "OK", "ok" + std::string(1, 10));
            return;
        }
        if (method == "POST" && target == "/options") {
            size_t olen = 0;
            size_t ocl = head.find("Content-Length:");
            if (ocl == std::string::npos) ocl = head.find("content-length:");
            if (ocl != std::string::npos) olen = static_cast<size_t>(strtoull(head.c_str() + ocl + 15, nullptr, 10));
            if (olen == 0 || olen > 4096) { reply(fd, 400, "Bad Request", "bad size" + std::string(1, 10)); return; }
            while (body.size() < olen) {
                if (!readSome(fd, body)) { reply(fd, 400, "Bad Request", "incomplete body" + std::string(1, 10)); return; }
            }
            body.resize(olen);
            try {
                auto json = nlohmann::json::parse(body);
                bool keepAwake = json.value("keep_awake_ac", false);
                inst::power::setKeepAwakeOnAc(keepAwake);
                if (inst::config::keepAwakeAc != keepAwake) {
                    inst::config::keepAwakeAc = keepAwake;  // remembered on the console for the next launches
                    inst::config::setConfig();
                }
            } catch (...) {
                reply(fd, 400, "Bad Request", "bad json" + std::string(1, 10));
                return;
            }
            reply(fd, 200, "OK", "ok" + std::string(1, 10));
            return;
        }
        if (method == "POST" && target.compare(0, 11, "/netinstall") == 0) {
            if (target == "/netinstall?target=sd") g_remoteTarget = 0;
            else if (target == "/netinstall?target=nand") g_remoteTarget = 1;
            else g_remoteTarget = -1;
            g_remoteTargetUntil = static_cast<long long>(time(nullptr)) + 600;
            g_remoteTransferUntil = static_cast<long long>(time(nullptr)) + 600;
            g_netInstallRequested = true;
            reply(fd, 202, "Accepted", "network install requested" + std::string(1, 10));
            return;
        }
        if (method == "POST" && target == "/backup") {
            size_t blen = 0;
            size_t bcl = head.find("Content-Length:");
            if (bcl == std::string::npos) bcl = head.find("content-length:");
            if (bcl == std::string::npos) { reply(fd, 411, "Length Required", "content-length required\n"); return; }
            blen = static_cast<size_t>(strtoull(head.c_str() + bcl + 15, nullptr, 10));
            if (blen == 0 || blen > 65536) { reply(fd, 413, "Payload Too Large", "bad size\n"); return; }
            while (body.size() < blen) {
                if (!readSome(fd, body)) { reply(fd, 400, "Bad Request", "incomplete body\n"); return; }
            }
            body.resize(blen);
            size_t queuedBackups = 0;
            try {
                auto json = nlohmann::json::parse(body);
                std::string ssmUrl = json.value("ssm_url", "");
                std::lock_guard<std::mutex> lock(g_uninstallMutex);
                for (auto &item : json.at("titles")) {
                    std::string idText = item.value("id", "");
                    if (!isHex16(idText) || ssmUrl.empty()) continue;
                    inst::cheats::BackupRequest request;
                    request.id = strtoull(idText.c_str(), nullptr, 16);
                    request.name = item.value("name", "");
                    request.ssmUrl = ssmUrl;
                    if (request.id == 0 || (request.id & 0xFFF) != 0) continue;
                    g_backupQueue.push_back(request);
                    queuedBackups++;
                }
            } catch (...) {
                reply(fd, 400, "Bad Request", "bad json\n");
                return;
            }
            reply(fd, queuedBackups ? 202 : 400, queuedBackups ? "Accepted" : "Bad Request", std::to_string(queuedBackups) + " queued\n");
            return;
        }
        if (method == "POST" && target == "/inventory") {
            size_t ilen = 0;
            size_t icl = head.find("Content-Length:");
            if (icl == std::string::npos) icl = head.find("content-length:");
            if (icl != std::string::npos) ilen = static_cast<size_t>(strtoull(head.c_str() + icl + 15, nullptr, 10));
            if (ilen > 4096) { reply(fd, 413, "Payload Too Large", "bad size\n"); return; }
            while (body.size() < ilen) {
                if (!readSome(fd, body)) { reply(fd, 400, "Bad Request", "incomplete body\n"); return; }
            }
            body.resize(ilen);
            std::string url;
            try {
                if (ilen > 0) url = nlohmann::json::parse(body).value("ssm_url", "");
            } catch (...) {
                reply(fd, 400, "Bad Request", "bad json\n");
                return;
            }
            {
                std::lock_guard<std::mutex> lock(g_uninstallMutex);
                g_inventoryUrl = url;
                g_inventoryRequested = true;
            }
            reply(fd, 202, "Accepted", "queued\n");
            return;
        }
        if (method == "POST" && target == "/uninstall") {
            size_t ulen = 0;
            size_t ucl = head.find("Content-Length:");
            if (ucl == std::string::npos) ucl = head.find("content-length:");
            if (ucl == std::string::npos) { reply(fd, 411, "Length Required", "content-length required\n"); return; }
            ulen = static_cast<size_t>(strtoull(head.c_str() + ucl + 15, nullptr, 10));
            if (ulen == 0 || ulen > 65536) { reply(fd, 413, "Payload Too Large", "bad size\n"); return; }
            while (body.size() < ulen) {
                if (!readSome(fd, body)) { reply(fd, 400, "Bad Request", "incomplete body\n"); return; }
            }
            body.resize(ulen);
            size_t queued = 0;
            try {
                auto json = nlohmann::json::parse(body);
                std::lock_guard<std::mutex> lock(g_uninstallMutex);
                for (auto &item : json.at("titles")) {
                    std::string idText = item.value("id", "");
                    if (!isHex16(idText)) continue;
                    inst::cheats::UninstallRequest request;
                    request.id = strtoull(idText.c_str(), nullptr, 16);
                    request.name = item.value("name", "");
                    request.ssmUrl = json.value("ssm_url", "");
                    if (request.id == 0 || (request.id & 0xFFF) != 0) continue;  // base applications only
                    g_uninstallQueue.push_back(request);
                    queued++;
                }
            } catch (...) {
                reply(fd, 400, "Bad Request", "bad json\n");
                return;
            }
            reply(fd, queued ? 202 : 400, queued ? "Accepted" : "Bad Request", std::to_string(queued) + " queued\n");
            return;
        }
        if (method != "PUT" && method != "POST") { reply(fd, 405, "Method Not Allowed", "use PUT\n"); return; }

        // /cheats/<TID>/<BID>.txt
        const std::string prefix = "/cheats/";
        if (target.compare(0, prefix.size(), prefix) != 0) { reply(fd, 404, "Not Found", "not found\n"); return; }
        std::string rest = target.substr(prefix.size());
        size_t slash = rest.find('/');
        if (slash == std::string::npos) { reply(fd, 400, "Bad Request", "bad path\n"); return; }
        std::string tid = rest.substr(0, slash);
        std::string file = rest.substr(slash + 1);
        if (file.size() < 5 || file.compare(file.size() - 4, 4, ".txt") != 0) { reply(fd, 400, "Bad Request", "bad file\n"); return; }
        std::string bid = file.substr(0, file.size() - 4);
        if (!isHex16(tid) || !isHex16(bid)) { reply(fd, 400, "Bad Request", "ids must be 16 hex digits\n"); return; }
        for (auto &c : tid) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        for (auto &c : bid) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

        size_t length = 0;
        size_t cl = head.find("Content-Length:");
        if (cl == std::string::npos) cl = head.find("content-length:");
        if (cl == std::string::npos) { reply(fd, 411, "Length Required", "content-length required\n"); return; }
        length = static_cast<size_t>(strtoull(head.c_str() + cl + 15, nullptr, 10));
        if (length == 0 || length > kMaxBody) { reply(fd, 413, "Payload Too Large", "bad size\n"); return; }

        while (body.size() < length) {
            if (!readSome(fd, body)) { reply(fd, 400, "Bad Request", "incomplete body\n"); return; }
        }
        body.resize(length);

        std::string dir = "sdmc:/atmosphere/contents/" + tid + "/cheats";
        makeDirs(dir);
        std::string path = dir + "/" + bid + ".txt";
        FILE *out = fopen(path.c_str(), "wb");
        if (!out) { reply(fd, 500, "Internal Server Error", "cannot open " + path + "\n"); return; }
        size_t written = fwrite(body.data(), 1, body.size(), out);
        fclose(out);
        if (written != body.size()) { reply(fd, 507, "Insufficient Storage", "write failed\n"); return; }
        reply(fd, 200, "OK", path + "\n");
    }

    // (Re)opens the listening socket. After the console went to sleep the network stack was restarted and the old
    // socket is dead: the listener is rebuilt from scratch, at once when sleep is detected and again on any error.
    bool openListener() {
        if (g_listen >= 0) {
            close(g_listen);
            g_listen = -1;
        }
        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (fd < 0) return false;
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(inst::cheats::kPort);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 || listen(fd, 3) < 0) {
            close(fd);
            return false;
        }
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        g_listen = fd;
        return true;
    }

    void serve() {
        // Non-blocking accept + short sleeps: the loop notices g_running quickly whatever the socket layer does.
        long long lastTick = static_cast<long long>(time(nullptr));
        while (g_running) {
            long long now = static_cast<long long>(time(nullptr));
            bool slept = (now - lastTick) > 4;  // the loop wakes every 100 ms: a long gap means the console slept
            lastTick = now;
            if (g_listen < 0 || slept) {
                if (!openListener()) {
                    usleep(1000 * 1000);
                    continue;
                }
            }
            int client = accept(g_listen, nullptr, nullptr);
            if (client < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    close(g_listen);
                    g_listen = -1;  // the socket is broken: it is rebuilt on the next turn
                }
                usleep(100 * 1000);
                continue;
            }
            // the listening socket is non-blocking: make sure the connection itself blocks normally
            fcntl(client, F_SETFL, fcntl(client, F_GETFL, 0) & ~O_NONBLOCK);
            inst::power::acquire();
            handle(client);
            inst::power::release();
            close(client);
        }
        g_finished = true;
    }
}

namespace inst::cheats {
    std::string takeRemoteCover() {
        std::lock_guard<std::mutex> lock(g_coverMutex);
        if (g_coverPath.empty() || static_cast<long long>(time(nullptr)) > g_coverUntil) return "";
        std::string path = g_coverPath;
        g_coverPath.clear();
        return path;
    }

    bool remoteTransferActive() {
        long long until = g_remoteTransferUntil.load();
        return until > 0 && static_cast<long long>(time(nullptr)) <= until;
    }

    int remoteInstallTarget() {
        if (static_cast<long long>(time(nullptr)) > g_remoteTargetUntil) return -1;
        return g_remoteTarget.load();
    }

    void endRemoteSession() {
        g_remoteTransferUntil = 0;
        g_remoteTargetUntil = 0;
        g_remoteTarget = -1;
    }

    bool takeNetInstallRequest() {
        return g_netInstallRequested.exchange(false);
    }

    bool hasBackupRequests() {
        std::lock_guard<std::mutex> lock(g_uninstallMutex);
        return !g_backupQueue.empty();
    }

    std::vector<BackupRequest> takeBackupRequests() {
        std::lock_guard<std::mutex> lock(g_uninstallMutex);
        std::vector<BackupRequest> out;
        out.swap(g_backupQueue);
        return out;
    }

    bool takeInventoryRequest(std::string &ssmUrl) {
        std::lock_guard<std::mutex> lock(g_uninstallMutex);
        if (!g_inventoryRequested) return false;
        g_inventoryRequested = false;
        ssmUrl = g_inventoryUrl;
        return true;
    }

    bool hasUninstallRequests() {
        std::lock_guard<std::mutex> lock(g_uninstallMutex);
        return !g_uninstallQueue.empty();
    }

    std::vector<UninstallRequest> takeUninstallRequests() {
        std::lock_guard<std::mutex> lock(g_uninstallMutex);
        std::vector<UninstallRequest> out;
        out.swap(g_uninstallQueue);
        return out;
    }

    bool isRunning() {
        return g_running && g_listen >= 0;
    }

    void start() {
        if (g_running) return;
        if (!openListener()) return;
        g_finished = false;
        g_running = true;
        g_thread = std::thread(serve);
    }

    void stop() {
        if (!g_running) return;
        g_running = false;
        // Never hang the exit of the app: wait for the thread a little, then let it go.
        for (int i = 0; i < 30 && !g_finished; i++) usleep(100 * 1000);
        if (g_thread.joinable()) {
            if (g_finished) g_thread.join();
            else g_thread.detach();
        }
        if (g_listen >= 0) close(g_listen);
        g_listen = -1;
        if (g_albumMounted && g_finished) {
            fsdevUnmountDevice("album");
            g_albumMounted = false;
        }
    }
}
