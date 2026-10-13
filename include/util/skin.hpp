#pragma once

#include <string>
#include <utility>
#include <vector>

// Colour themes of the interface (called skins in the code).
//   "default"   the original dark teal look
//   "dreamcast" light grey and orange, the same look as the Dreamcast Grey theme of Switch Super Manager
//   any folder of sdmc:/switch/Switch-Super-Installer/themes/<id>/ that holds a theme.json (see skin.cpp for the format)
// The skin is read once when the pages are built: a change made in the options takes effect the next time SSI starts.
namespace inst::skin {
    // Scans the skins folder and picks the skin chosen in the config (call once, after the config is read).
    void load();

    // Every skin that can be chosen: {id, label}, the built-in ones first.
    std::vector<std::pair<std::string, std::string>> list();
    std::string currentId();
    std::string currentLabel();

    // True for the Dreamcast Grey family (dark charcoal and orange): the built-in skin and the themes that use it as base.
    bool dreamcast();

    std::string bg();         // page background
    std::string topbar();     // bar with the logo
    std::string bar();        // information bar and bottom bar
    std::string barText();    // text drawn on the bars
    std::string text();       // text drawn on the page background (menus, lists)
    std::string focus();      // highlight of the selected menu line
    std::string scroll();     // scroll bar of the main menu
    std::string scrollSoft(); // scroll bar of the lists
    std::string menuClear();  // transparent background of the menus
    std::string ok();         // "already installed" / success text

    std::string installBg();
    std::string installTop();
    std::string installInfo();
    std::string installBar();
    std::string installBarFill(); // empty = keep the default fill colour

    // Picture behind the pages. A background.png in the SSI folder still wins over it.
    std::string background();
    // Logo of the title bar.
    std::string logo();

    // Icon of the menus. A skin can bring its own files in <skin folder>/icons/ (same names as the built-in ones);
    // otherwise the white ones (or the dark ones when the theme asks for "iconSet": "dark", for a light background).
    std::string icon(const std::string &file);
}
