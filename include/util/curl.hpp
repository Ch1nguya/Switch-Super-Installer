#pragma once
#include <string>

namespace inst::curl {
    bool downloadFile(const std::string ourUrl, const char *pagefilename, long timeout = 5000, bool writeProgress = false);
    std::string downloadToBuffer (const std::string ourUrl, int firstRange = -1, int secondRange = -1, long timeout = 5000);
    // POST a JSON body; returns true on HTTP 2xx. The response body is written to outResponse.
    // POST the content of a file (streamed, any size). Returns true on a 2xx answer.
    bool postFile(const std::string &url, const std::string &filePath, std::string &outResponse, long timeoutSeconds = 600);
    bool postJson(const std::string &url, const std::string &jsonBody, std::string &outResponse, long timeout = 5000);
}