#include <iostream>
#include <switch.h>
#include <cstring>
#include <sstream>
#include <filesystem>
#include "util/theme.hpp"
#include "util/config.hpp"

namespace Theme {
	json theme;

	void Load() {
		std::ifstream ifs2;
		std::string ThemePath = inst::config::appDir + "/theme/theme.json";
		if (std::filesystem::exists(ThemePath)) {
			ifs2 = std::ifstream(ThemePath);
			theme = json::parse(ifs2);
			ifs2.close();
		}
		else {
			std::cout << "[FAILED TO LOAD Theme FILE]" << std::endl;
			return;
		}
	}

	std::string ThemeEntry(std::string key) {
		json j = GetRelativeJson(theme, key);
		if (j == nullptr) {
			return "Json object: " + key + " does not exist!";
		}
		return j.get<std::string>();
	}
}