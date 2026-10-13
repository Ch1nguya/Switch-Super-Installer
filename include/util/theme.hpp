#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

namespace Theme {
	void Load();
	std::string ThemeEntry(std::string key);
	inline json GetRelativeJson(json j, std::string key) {
		std::istringstream ss(key);
		std::string token;

		while (std::getline(ss, token, '.') && j != nullptr) {
			j = j[token];
		}

		return j;
	}
}

inline std::string operator ""_theme(const char* key, size_t size) {
	return Theme::ThemeEntry(std::string(key, size));
}