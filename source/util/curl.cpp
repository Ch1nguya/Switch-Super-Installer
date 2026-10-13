#include <curl/curl.h>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>
#include "util/curl.hpp"
#include "util/config.hpp"
#include "util/error.hpp"
#include "ui/instPage.hpp"

namespace {
    std::once_flag g_curlInitOnce;

    inline void ensureCurlGlobalInit() {
        std::call_once(g_curlInitOnce, []() {
            curl_global_init(CURL_GLOBAL_ALL);
        });
    }
}

static size_t writeDataFile(void *ptr, size_t size, size_t nmemb, void *stream) {
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

size_t writeDataBuffer(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::ostringstream *stream = (std::ostringstream*)userdata;
    size_t count = size * nmemb;
    stream->write(ptr, count);
    return count;
}

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (ultotal) {
        int uploadProgress = (int)(((double)ulnow / (double)ultotal) * 100.0);
        inst::ui::instPage::setInstBarPerc(uploadProgress);
    } else if (dltotal) {
        int downloadProgress = (int)(((double)dlnow / (double)dltotal) * 100.0);
        inst::ui::instPage::setInstBarPerc(downloadProgress);
    }
    return 0;
}

namespace inst::curl {
    bool downloadFile (const std::string ourUrl, const char *pagefilename, long timeout, bool writeProgress) {
        CURL *curl_handle;
        CURLcode result;
        FILE *pagefile;
        
        ensureCurlGlobalInit();
        curl_handle = curl_easy_init();
        if (!curl_handle) return false;
        
        curl_easy_setopt(curl_handle, CURLOPT_URL, ourUrl.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Awoo-Installer");
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, timeout);
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS, timeout);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataFile);
        curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_TCP_NODELAY, 1L);
        if (writeProgress) curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
        
        pagefile = fopen(pagefilename, "wb");
        if (!pagefile) {
            curl_easy_cleanup(curl_handle);
            return false;
        }
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
        result = curl_easy_perform(curl_handle);
        
        curl_easy_cleanup(curl_handle);
        fclose(pagefile);

        if (result == CURLE_OK) return true;
        else {
            LOG_DEBUG(curl_easy_strerror(result));
            return false;
        }
    }

    std::string downloadToBuffer (const std::string ourUrl, int firstRange, int secondRange, long timeout) {
        CURL *curl_handle;
        CURLcode result;
        std::ostringstream stream;
        
        ensureCurlGlobalInit();
        curl_handle = curl_easy_init();
        if (!curl_handle) return "";
        
        curl_easy_setopt(curl_handle, CURLOPT_URL, ourUrl.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Awoo-Installer");
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, timeout);
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS, timeout);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataBuffer);
        curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_TCP_NODELAY, 1L);
        if (firstRange >= 0 && secondRange >= 0 && secondRange >= firstRange) {
            std::string ourRange = std::to_string(firstRange) + "-" + std::to_string(secondRange);
            curl_easy_setopt(curl_handle, CURLOPT_RANGE, ourRange.c_str());
        }
        
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &stream);
        result = curl_easy_perform(curl_handle);
        
        curl_easy_cleanup(curl_handle);

        if (result == CURLE_OK) return stream.str();
        else {
            LOG_DEBUG(curl_easy_strerror(result));
            return "";
        }
    }

    bool postFile(const std::string &url, const std::string &filePath, std::string &outResponse, long timeoutSeconds) {
        ensureCurlGlobalInit();
        FILE *file = std::fopen(filePath.c_str(), "rb");
        if (!file) return false;
        std::fseek(file, 0, SEEK_END);
        long long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) { std::fclose(file); return false; }
        std::ostringstream stream;
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

        curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, fread);
        curl_easy_setopt(curl_handle, CURLOPT_READDATA, file);
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)size);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Awoo-Installer");
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, timeoutSeconds);
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataBuffer);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &stream);

        CURLcode result = curl_easy_perform(curl_handle);
        long status = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl_handle);
        std::fclose(file);

        outResponse = stream.str();
        if (result != CURLE_OK) {
            LOG_DEBUG(curl_easy_strerror(result));
            return false;
        }
        return status >= 200 && status < 300;
    }

    bool postJson(const std::string &url, const std::string &jsonBody, std::string &outResponse, long timeout) {
        ensureCurlGlobalInit();
        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) return false;

        std::ostringstream stream;
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, jsonBody.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)jsonBody.size());
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Awoo-Installer");
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, timeout);
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS, timeout);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataBuffer);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &stream);

        CURLcode result = curl_easy_perform(curl_handle);
        long status = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl_handle);

        outResponse = stream.str();
        if (result != CURLE_OK) {
            LOG_DEBUG(curl_easy_strerror(result));
            return false;
        }
        return status >= 200 && status < 300;
    }
}
