#include <filesystem>
#include <switch.h>
#include "ui/MainApplication.hpp"
#include "ui/mainPage.hpp"
#include "ui/instPage.hpp"
#include "ui/optionsPage.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/skin.hpp"
#include "util/power.hpp"
#include "util/curl.hpp"
#include "util/unzip.hpp"
#include "util/lang.hpp"
#include "util/theme.hpp"
#include "ui/instPage.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;

    std::vector<std::string> languageStrings = {"English", "日本語", "Français", "Deutsch", "Italiano", "Español", "Português", "한국어", "Українська", "簡体中文","繁體中文"};

    optionsPage::optionsPage() : Layout::Layout() {
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
        this->pageInfoText = TextBlock::New(10, 109, "options.title"_lang, 30);
        this->pageInfoText->SetColor(COLOR(inst::skin::barText()));
        this->butText = TextBlock::New(10, 678, "options.buttons"_lang, 24);
        this->butText->SetColor(COLOR(inst::skin::barText()));
        this->menu = pu::ui::elm::Menu::New(0, 156, 1280, COLOR(inst::skin::menuClear()), 84, (506 / 84));
        this->menu->SetOnFocusColor(COLOR(inst::skin::focus()));
        this->menu->SetScrollbarColor(COLOR(inst::skin::scrollSoft()));
        this->Add(this->topRect);
        this->Add(this->infoRect);
        this->Add(this->botRect);
        this->Add(this->titleImage);
        this->Add(this->appVersionText);
        this->Add(this->butText);
        this->Add(this->pageInfoText);
        this->setMenuText();
        this->Add(this->menu);
    }

    void optionsPage::askToUpdate(std::vector<std::string> updateInfo) {
            if (!mainApp->CreateShowDialog("options.update.title"_lang, "options.update.desc0"_lang + updateInfo[0] + "options.update.desc1"_lang, {"options.update.opt0"_lang, "common.cancel"_lang}, false)) {
                inst::ui::instPage::loadInstallScreen();
                inst::ui::instPage::setTopInstInfoText("options.update.top_info"_lang + updateInfo[0]);
                inst::ui::instPage::setInstBarPerc(0);
                inst::ui::instPage::setInstInfoText("options.update.bot_info"_lang + updateInfo[0]);
                try {
                    std::string downloadName = inst::config::appDir + "/temp_download.zip";
                    inst::curl::downloadFile(updateInfo[1], downloadName.c_str(), 0, true);
                    romfsExit();
                    inst::ui::instPage::setInstInfoText("options.update.bot_info2"_lang + updateInfo[0]);
                    inst::zip::extractFile(downloadName, "sdmc:/");
                    std::filesystem::remove(downloadName);
                    mainApp->CreateShowDialog("options.update.complete"_lang, "options.update.end_desc"_lang, {"common.ok"_lang}, false);
                } catch (...) {
                    mainApp->CreateShowDialog("options.update.failed"_lang, "options.update.end_desc"_lang, {"common.ok"_lang}, false);
                }
                mainApp->FadeOut();
                mainApp->Close();
            }
        return;
    }

    std::string optionsPage::getMenuOptionIcon(bool ourBool) {
        if(ourBool) {
            if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json") && std::filesystem::exists(inst::config::appDir + "icons_others.checkbox-checked"_theme))
                return inst::config::appDir + "icons_others.checkbox-checked"_theme;
            return inst::skin::icon("check-box-outline.png");
        }
        else {
            if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json") && std::filesystem::exists(inst::config::appDir + "icons_others.checkbox-empty"_theme))
                return inst::config::appDir + "icons_others.checkbox-empty"_theme;
            return inst::skin::icon("checkbox-blank-outline.png");
        }
    }

    std::string optionsPage::getMenuLanguage(int ourLangCode) {
        switch (ourLangCode) {
            case 1:
            case 12:
                return languageStrings[0];
            case 0:
                return languageStrings[1];
            case 2:
            case 13:
                return languageStrings[2];
            case 3:
                return languageStrings[3];
            case 4:
                return languageStrings[4];
            case 5:
            case 14:
                return languageStrings[5];
            case 9:
                return languageStrings[6];
            case 7:
                return languageStrings[7];
            case 10:
                return languageStrings[8];
            case 6:
                return languageStrings[9];
            case 11:
                return languageStrings[10];
            default:
                return "options.language.system_language"_lang;
        }
    }

    void optionsPage::setMenuText() {
        this->menu->ClearItems();
        auto ignoreFirmOption = pu::ui::elm::MenuItem::New("options.menu_items.ignore_firm"_lang);
        ignoreFirmOption->SetColor(COLOR(inst::skin::text()));
        ignoreFirmOption->SetIcon(this->getMenuOptionIcon(inst::config::ignoreReqVers));
        this->menu->AddItem(ignoreFirmOption);
        auto validateOption = pu::ui::elm::MenuItem::New("options.menu_items.nca_verify"_lang);
        validateOption->SetColor(COLOR(inst::skin::text()));
        validateOption->SetIcon(this->getMenuOptionIcon(inst::config::validateNCAs));
        this->menu->AddItem(validateOption);
        auto overclockOption = pu::ui::elm::MenuItem::New("options.menu_items.boost_mode"_lang);
        overclockOption->SetColor(COLOR(inst::skin::text()));
        overclockOption->SetIcon(this->getMenuOptionIcon(inst::config::overClock));
        this->menu->AddItem(overclockOption);
        auto deletePromptOption = pu::ui::elm::MenuItem::New("options.menu_items.ask_delete"_lang);
        deletePromptOption->SetColor(COLOR(inst::skin::text()));
        deletePromptOption->SetIcon(this->getMenuOptionIcon(inst::config::deletePrompt));
        this->menu->AddItem(deletePromptOption);
        auto autoUpdateOption = pu::ui::elm::MenuItem::New("options.menu_items.auto_update"_lang);
        autoUpdateOption->SetColor(COLOR(inst::skin::text()));
        autoUpdateOption->SetIcon(this->getMenuOptionIcon(inst::config::autoUpdate));
        this->menu->AddItem(autoUpdateOption);
        auto remoteOption = pu::ui::elm::MenuItem::New("extra.remote_label"_lang);
        remoteOption->SetColor(COLOR(inst::skin::text()));
        remoteOption->SetIcon(this->getMenuOptionIcon(inst::config::remoteManagement));
        this->menu->AddItem(remoteOption);
        auto keepAwakeOption = pu::ui::elm::MenuItem::New("extra.keep_awake"_lang);
        keepAwakeOption->SetColor(COLOR(inst::skin::text()));
        keepAwakeOption->SetIcon(this->getMenuOptionIcon(inst::config::keepAwakeAc));
        this->menu->AddItem(keepAwakeOption);
        auto skinOption = pu::ui::elm::MenuItem::New("theme.label"_lang + inst::skin::currentLabel());
        skinOption->SetColor(COLOR(inst::skin::text()));
        this->menu->AddItem(skinOption);

        auto sigPatchesUrlOption = pu::ui::elm::MenuItem::New("options.menu_items.sig_url"_lang + inst::util::shortenString(inst::config::sigPatchesUrl, 42, false));
        sigPatchesUrlOption->SetColor(COLOR(inst::skin::text()));
        this->menu->AddItem(sigPatchesUrlOption);
        auto languageOption = pu::ui::elm::MenuItem::New("options.menu_items.language"_lang + this->getMenuLanguage(inst::config::languageSetting));
        languageOption->SetColor(COLOR(inst::skin::text()));
        this->menu->AddItem(languageOption);
        auto updateOption = pu::ui::elm::MenuItem::New("options.menu_items.check_update"_lang);
        updateOption->SetColor(COLOR(inst::skin::text()));
        this->menu->AddItem(updateOption);
        auto creditsOption = pu::ui::elm::MenuItem::New("options.menu_items.credits"_lang);
        creditsOption->SetColor(COLOR(inst::skin::text()));
        this->menu->AddItem(creditsOption);
    }

    void optionsPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        if (Down & HidNpadButton_B) {
            mainApp->LoadLayout(mainApp->mainPage);
        }
        if ((Down & HidNpadButton_A) || (Up & TouchPseudoKey)) {
            std::string keyboardResult;
            int rc;
            std::vector<std::string> downloadUrl;
            std::vector<std::string> languageList;
            switch (this->menu->GetSelectedIndex()) {
                case 0:
                    inst::config::ignoreReqVers = !inst::config::ignoreReqVers;
                    inst::config::setConfig();
                    this->setMenuText();
                    break;
                case 1:
                    if (inst::config::validateNCAs) {
                        if (inst::ui::mainApp->CreateShowDialog("options.nca_warn.title"_lang, "options.nca_warn.desc"_lang, {"common.cancel"_lang, "options.nca_warn.opt1"_lang}, false) == 1) inst::config::validateNCAs = false;
                    } else inst::config::validateNCAs = true;
                    inst::config::setConfig();
                    this->setMenuText();
                    break;
                case 2:
                    inst::config::overClock = !inst::config::overClock;
                    inst::config::setConfig();
                    this->setMenuText();
                    break;
                case 3:
                    inst::config::deletePrompt = !inst::config::deletePrompt;
                    inst::config::setConfig();
                    this->setMenuText();
                    break;
                case 4:
                    inst::config::autoUpdate = !inst::config::autoUpdate;
                    inst::config::setConfig();
                    this->setMenuText();
                    break;
                case 5:
                    if (!inst::config::remoteManagement) {
                        // turning it on lets SSM install, uninstall games and read saves without asking: confirm once
                        if (inst::ui::mainApp->CreateShowDialog("extra.remote_title"_lang, "extra.remote_desc"_lang, {"common.cancel"_lang, "extra.remote_enable"_lang}, false) != 1) break;
                    }
                    inst::config::remoteManagement = !inst::config::remoteManagement;
                    inst::config::setConfig();
                    this->setMenuText();
                    break;
                case 6:
                    inst::config::keepAwakeAc = !inst::config::keepAwakeAc;
                    inst::config::setConfig();
                    inst::power::setKeepAwakeOnAc(inst::config::keepAwakeAc);
                    this->setMenuText();
                    break;
                case 7: {
                    std::vector<std::pair<std::string, std::string>> skins = inst::skin::list();
                    std::vector<std::string> labels;
                    for (const auto &item : skins) labels.push_back(item.second + (item.first == inst::skin::currentId() ? "  *" : ""));
                    int pickedSkin = inst::ui::mainApp->CreateShowDialog("theme.dialog_title"_lang, "theme.dialog_desc"_lang, labels, false);
                    if (pickedSkin < 0 || pickedSkin >= (int)skins.size()) break;
                    if (skins[pickedSkin].first == inst::skin::currentId()) break;
                    inst::config::skin = skins[pickedSkin].first;
                    inst::config::setConfig();
                    inst::ui::mainApp->CreateShowDialog("theme.dialog_title"_lang, "theme.restart"_lang, {"common.ok"_lang}, true);
                    break;
                }
                case 8:
                    keyboardResult = inst::util::softwareKeyboard("options.sig_hint"_lang, inst::config::sigPatchesUrl.c_str(), 500);
                    if (keyboardResult.size() > 0) {
                        inst::config::sigPatchesUrl = keyboardResult;
                        inst::config::setConfig();
                        this->setMenuText();
                    }
                    break;
                case 9:
                    languageList = languageStrings;
                    languageList.push_back("options.language.system_language"_lang);
                    rc = inst::ui::mainApp->CreateShowDialog("options.language.title"_lang, "options.language.desc"_lang, languageList, false);
                    if (rc == -1) break;
                    switch(rc) {
                        case 0:
                            inst::config::languageSetting = 1;
                            break;
                        case 1:
                            inst::config::languageSetting = 0;
                            break;
                        case 2:
                            inst::config::languageSetting = 2;
                            break;
                        case 3:
                            inst::config::languageSetting = 3;
                            break;
                        case 4:
                            inst::config::languageSetting = 4;
                            break;
                        case 5:
                            inst::config::languageSetting = 14;
                            break;
                        case 6:
                            inst::config::languageSetting = 9;
                            break;
                        case 7:
                            inst::config::languageSetting = 7;
                            break;
                        case 8:
                            inst::config::languageSetting = 10;
                            break;
                        case 9:
                            inst::config::languageSetting = 6;
                            break;
                        case 10:
                            inst::config::languageSetting = 11;
                            break;
                        default:
                            inst::config::languageSetting = 99;
                    }
                    inst::config::setConfig();
                    mainApp->FadeOut();
                    mainApp->Close();
                    break;
                case 10:
                    if (inst::util::getIPAddress() == "1.0.0.127") {
                        inst::ui::mainApp->CreateShowDialog("main.net.title"_lang, "main.net.desc"_lang, {"common.ok"_lang}, true);
                        break;
                    }
                    downloadUrl = inst::util::checkForAppUpdate();
                    if (!downloadUrl.size()) {
                        mainApp->CreateShowDialog("options.update.title_check_fail"_lang, "options.update.desc_check_fail"_lang, {"common.ok"_lang}, false);
                        break;
                    }
                    this->askToUpdate(downloadUrl);
                    break;
                case 11:
                    inst::ui::mainApp->CreateShowDialog("options.credits.title"_lang, "options.credits.desc"_lang, {"common.close"_lang}, true);
                    break;
                default:
                    break;
            }
        }
    }
}
