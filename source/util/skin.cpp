#include <algorithm>
#include <filesystem>
#include <fstream>
#include "util/skin.hpp"
#include "util/config.hpp"
#include "util/json.hpp"

/*
 * themes/<id>/theme.json
 * {
 *   "name": "My theme",                      label shown in the options (default: the folder name)
 *   "base": "default" | "dreamcast",         what is used for everything the file does not set
 *   "background": "background.png",          optional picture in the theme folder
 *   "logo": "logo.png",                      optional picture in the theme folder (480x93 like the original)
 *   "iconSet": "light" | "dark",             icons of the menus: white (default) or dark, for themes with a light background
 *   "colors": {                              optional, "#RRGGBB" or "#RRGGBBAA"
 *     "bg", "topbar", "bar", "barText", "text", "focus", "scroll", "scrollSoft", "menuClear", "ok",
 *     "installBg", "installTop", "installInfo", "installBar", "installBarFill"
 *   }
 * }
 * Own icons: put files with the names of romfs/images/icons in themes/<id>/icons/.
 */
namespace inst::skin {
    namespace {
        struct UserSkin {
            std::string id;
            std::string label;
            std::string base;
            std::string dir;
            std::string background;
            std::string logo;
            bool darkIcons = false;
            nlohmann::json colors;
        };
        std::vector<UserSkin> g_skins;
        int g_active = -1;

        std::string themesDir() {
            return inst::config::appDir + "/themes";
        }

        std::string validColor(const std::string &hex) {
            if (hex.size() != 7 && hex.size() != 9) return "";
            if (hex[0] != '#') return "";
            for (size_t i = 1; i < hex.size(); i++) {
                if (!isxdigit(static_cast<unsigned char>(hex[i]))) return "";
            }
            return hex.size() == 7 ? hex + "FF" : hex;
        }

        std::string pick(const char *key, const char *standard, const char *dc) {
            if (g_active >= 0) {
                const nlohmann::json &colors = g_skins[g_active].colors;
                if (colors.is_object() && colors.contains(key) && colors[key].is_string()) {
                    std::string custom = validColor(colors[key].get<std::string>());
                    if (!custom.empty()) return custom;
                }
            }
            return dreamcast() ? dc : standard;
        }

        std::string ownFile(const std::string &name) {
            if (g_active < 0 || name.empty()) return "";
            std::string path = g_skins[g_active].dir + "/" + name;
            std::error_code ec;
            return std::filesystem::exists(path, ec) ? path : "";
        }
    }

    void load() {
        g_skins.clear();
        g_active = -1;
        std::error_code ec;
        if (std::filesystem::is_directory(themesDir(), ec)) {
            for (const auto &entry : std::filesystem::directory_iterator(themesDir(), ec)) {
                if (!entry.is_directory()) continue;
                std::string file = entry.path().string() + "/theme.json";
                if (!std::filesystem::exists(file, ec)) continue;
                try {
                    std::ifstream stream(file);
                    nlohmann::json j = nlohmann::json::parse(stream);
                    UserSkin skin;
                    skin.id = entry.path().filename().string();
                    skin.label = j.value("name", skin.id);
                    skin.base = j.value("base", std::string("default")) == "dreamcast" ? "dreamcast" : "default";
                    skin.dir = entry.path().string();
                    skin.background = j.value("background", std::string());
                    skin.logo = j.value("logo", std::string());
                    skin.darkIcons = j.value("iconSet", std::string("light")) == "dark";
                    if (j.contains("colors") && j["colors"].is_object()) skin.colors = j["colors"];
                    if (skin.id == "default" || skin.id == "dreamcast") continue;  // the built-in names stay theirs
                    g_skins.push_back(skin);
                } catch (...) {
                    // an unreadable theme is simply not offered
                }
            }
        }
        std::sort(g_skins.begin(), g_skins.end(), [](const UserSkin &a, const UserSkin &b) { return a.label < b.label; });
        for (size_t i = 0; i < g_skins.size(); i++) {
            if (g_skins[i].id == inst::config::skin) g_active = static_cast<int>(i);
        }
    }

    std::vector<std::pair<std::string, std::string>> list() {
        std::vector<std::pair<std::string, std::string>> result;
        result.push_back({"default", "Default"});
        result.push_back({"dreamcast", "Dreamcast Grey"});
        for (const auto &skin : g_skins) result.push_back({skin.id, skin.label});
        return result;
    }

    std::string currentId() {
        if (g_active >= 0) return g_skins[g_active].id;
        return inst::config::skin == "dreamcast" ? "dreamcast" : "default";
    }

    std::string currentLabel() {
        for (const auto &item : list()) {
            if (item.first == currentId()) return item.second;
        }
        return "Default";
    }

    // Dreamcast Grey: dark charcoal with orange, the same family of colours as the grey and orange theme of SSM.
    bool dreamcast() {
        if (g_active >= 0) return g_skins[g_active].base == "dreamcast";
        return inst::config::skin == "dreamcast";
    }

    std::string bg() { return pick("bg", "#002833FF", "#2C2D31FF"); }
    std::string topbar() { return pick("topbar", "#001015FF", "#1B1C1FFF"); }
    std::string bar() { return pick("bar", "#00101580", "#1B1C1F99"); }
    std::string barText() { return pick("barText", "#FFFFFFFF", "#FFFFFFFF"); }
    std::string text() { return pick("text", "#FFFFFFFF", "#FFFFFFFF"); }
    std::string focus() { return pick("focus", "#00000033", "#E8590C55"); }
    std::string scroll() { return pick("scroll", "#001015FF", "#E8590CFF"); }
    std::string scrollSoft() { return pick("scrollSoft", "#00101580", "#E8590CCC"); }
    std::string menuClear() { return pick("menuClear", "#FFFFFF00", "#2C2D3100"); }
    std::string ok() { return pick("ok", "#00FF00FF", "#7BD88FFF"); }

    std::string installBg() { return pick("installBg", "#670000FF", "#2C2D31FF"); }
    std::string installTop() { return pick("installTop", "#170909FF", "#1B1C1FFF"); }
    std::string installInfo() { return pick("installInfo", "#17090980", "#1B1C1F99"); }
    std::string installBar() { return pick("installBar", "#222222FF", "#1B1C1FFF"); }
    std::string installBarFill() { return pick("installBarFill", "", "#E8590CFF"); }

    std::string background() {
        if (g_active >= 0) {
            std::string own = ownFile(g_skins[g_active].background);
            if (!own.empty()) return own;
        }
        return dreamcast() ? "romfs:/images/background_dcg.jpg" : "romfs:/images/background.jpg";
    }

    std::string logo() {
        if (g_active >= 0) {
            std::string own = ownFile(g_skins[g_active].logo);
            if (!own.empty()) return own;
        }
        return dreamcast() ? "romfs:/images/logo_dcg.png" : "romfs:/images/logo.png";
    }

    std::string icon(const std::string &file) {
        if (g_active >= 0) {
            std::string own = ownFile("icons/" + file);
            if (!own.empty()) return own;
            if (g_skins[g_active].darkIcons) return "romfs:/images/icons_dc/" + file;
        }
        return "romfs:/images/icons/" + file;
    }
}
