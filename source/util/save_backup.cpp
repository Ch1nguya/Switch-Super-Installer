#include "util/save_backup.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <switch.h>

namespace {
    constexpr size_t kBlock = 512;
    const char *kMount = "ssisave";

    struct Writer {
        FILE *file = nullptr;
        int files = 0;
        int skipped = 0;
        bool failed = false;
    };

    void putOctal(char *field, size_t width, unsigned long long value) {
        // width includes the trailing NUL
        std::snprintf(field, width, "%0*llo", (int)(width - 1), value);
    }

    // Writes a ustar header for a regular file. Returns false when the name does not fit.
    bool writeHeader(Writer &w, const std::string &path, unsigned long long size) {
        std::string name = path;
        std::string prefix;
        if (name.size() > 100) {
            size_t cut = name.rfind('/', 155);
            while (cut != std::string::npos && name.size() - cut - 1 > 100) cut = (cut == 0) ? std::string::npos : name.rfind('/', cut - 1);
            if (cut == std::string::npos || cut > 155) return false;
            prefix = name.substr(0, cut);
            name = name.substr(cut + 1);
            if (name.size() > 100) return false;
        }
        char header[kBlock];
        std::memset(header, 0, sizeof(header));
        std::memcpy(header, name.data(), name.size());
        putOctal(header + 100, 8, 0644);
        putOctal(header + 108, 8, 0);
        putOctal(header + 116, 8, 0);
        putOctal(header + 124, 12, size);
        putOctal(header + 136, 12, 0);
        std::memset(header + 148, ' ', 8);
        header[156] = '0';
        std::memcpy(header + 257, "ustar", 5);
        std::memcpy(header + 263, "00", 2);
        std::memcpy(header + 345, prefix.data(), prefix.size());
        unsigned sum = 0;
        for (size_t i = 0; i < kBlock; i++) sum += (unsigned char)header[i];
        std::snprintf(header + 148, 7, "%06o", sum);
        header[154] = '\0';
        header[155] = ' ';
        return std::fwrite(header, 1, kBlock, w.file) == kBlock;
    }

    bool writePadding(Writer &w, unsigned long long size) {
        size_t rest = (size_t)(size % kBlock);
        if (rest == 0) return true;
        char zeros[kBlock] = {0};
        return std::fwrite(zeros, 1, kBlock - rest, w.file) == kBlock - rest;
    }

    bool addMemoryFile(Writer &w, const std::string &path, const std::string &content) {
        if (!writeHeader(w, path, content.size())) return false;
        if (std::fwrite(content.data(), 1, content.size(), w.file) != content.size()) return false;
        return writePadding(w, content.size());
    }

    void addFile(Writer &w, const std::string &diskPath, const std::string &tarPath) {
        struct stat info;
        if (stat(diskPath.c_str(), &info) != 0) { w.skipped++; return; }
        FILE *in = std::fopen(diskPath.c_str(), "rb");
        if (!in) { w.skipped++; return; }
        unsigned long long size = (unsigned long long)info.st_size;
        if (!writeHeader(w, tarPath, size)) { std::fclose(in); w.skipped++; return; }
        static std::vector<char> buffer(256 * 1024);
        unsigned long long left = size;
        while (left > 0) {
            size_t want = left < buffer.size() ? (size_t)left : buffer.size();
            size_t got = std::fread(buffer.data(), 1, want, in);
            if (got == 0) { w.failed = true; break; }
            if (std::fwrite(buffer.data(), 1, got, w.file) != got) { w.failed = true; break; }
            left -= got;
        }
        std::fclose(in);
        if (w.failed) return;
        if (!writePadding(w, size)) { w.failed = true; return; }
        w.files++;
    }

    void addTree(Writer &w, const std::string &diskDir, const std::string &tarDir) {
        DIR *dir = opendir(diskDir.c_str());
        if (!dir) return;
        std::vector<std::pair<std::string, bool>> entries;
        while (struct dirent *entry = readdir(dir)) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            entries.emplace_back(name, entry->d_type == DT_DIR);
        }
        closedir(dir);
        for (const auto &entry : entries) {
            if (w.failed) return;
            std::string disk = diskDir + (diskDir.back() == '/' ? "" : "/") + entry.first;
            std::string tar = tarDir + "/" + entry.first;
            if (entry.second) addTree(w, disk, tar);
            else addFile(w, disk, tar);
        }
    }

    std::string uidText(const AccountUid &uid) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%016llx%016llx", (unsigned long long)uid.uid[1], (unsigned long long)uid.uid[0]);
        return buffer;
    }

    std::string nicknameOf(const AccountUid &uid) {
        std::string nickname;
        AccountProfile profile;
        if (R_SUCCEEDED(accountGetProfile(&profile, uid))) {
            AccountProfileBase base;
            if (R_SUCCEEDED(accountProfileGet(&profile, nullptr, &base))) nickname = base.nickname;
            accountProfileClose(&profile);
        }
        return nickname;
    }
}

namespace inst::saves {
    bool backupToTar(uint64_t titleId, const std::string &tarPath, std::string &error, int &fileCount) {
        fileCount = 0;
        Writer w;
        w.file = std::fopen(tarPath.c_str(), "wb");
        if (!w.file) { error = "cannot create the temporary archive"; return false; }

        std::string meta = "title=" + std::to_string(titleId) + "\n";
        bool accountReady = R_SUCCEEDED(accountInitialize(AccountServiceType_Application));
        if (accountReady) {
            AccountUid uids[ACC_USER_LIST_SIZE];
            s32 total = 0;
            if (R_SUCCEEDED(accountListAllUsers(uids, ACC_USER_LIST_SIZE, &total))) {
                for (s32 i = 0; i < total; i++) {
                    if (R_FAILED(fsdevMountSaveDataReadOnly(kMount, titleId, uids[i]))) continue;
                    std::string folder = "user_" + uidText(uids[i]);
                    meta += folder + "=" + nicknameOf(uids[i]) + "\n";
                    addTree(w, std::string(kMount) + ":/", folder);
                    fsdevUnmountDevice(kMount);
                }
            }
            accountExit();
        }

        // Device save (shared by all accounts), when the game has one.
        FsSaveDataAttribute attr;
        std::memset(&attr, 0, sizeof(attr));
        attr.application_id = titleId;
        attr.save_data_type = FsSaveDataType_Device;
        FsFileSystem fs;
        if (R_SUCCEEDED(fsOpenSaveDataFileSystem(&fs, FsSaveDataSpaceId_User, &attr))) {
            if (fsdevMountDevice(kMount, fs) >= 0) {
                addTree(w, std::string(kMount) + ":/", "device");
                fsdevUnmountDevice(kMount);
            } else {
                fsFsClose(&fs);
            }
        }

        addMemoryFile(w, "ssi_meta.txt", meta);
        char zeros[kBlock * 2] = {0};
        std::fwrite(zeros, 1, sizeof(zeros), w.file);
        std::fclose(w.file);
        fileCount = w.files;

        if (w.failed) { error = "write error while reading the save data"; return false; }
        if (w.files == 0) { error = "no_save"; return false; }
        return true;
    }
}
