#include <filesystem>
#include <switch.h>
#include <sys/statvfs.h>
#include <sstream>
#include <iomanip>
#include "ui/MainApplication.hpp"
#include "util/remote_commands.hpp"
#include "ui/mainPage.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/skin.hpp"
#include "util/lang.hpp"
#include "sigInstall.hpp"
#include "data/buffered_placeholder_writer.hpp"
#include "util/theme.hpp"
#include "util/cheats_server.hpp"
#include "util/power.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;
    bool appletFinished = false;
    bool updateFinished = false;

    void showSpaceInfo() {
        double sdFree = (inst::util::GetAvailableSpace("sdmc:/") / 1024) / 1024 / 1024; // GB
        double sdUsed = inst::util::amountOfDiskSpaceUsed("sdmc:/");
        double sdTotal = (inst::util::totalsize("sdmc:/") / 1024) / 1024 / 1024; // GB

        std::stringstream streamSdFree;
        streamSdFree << std::fixed << std::setprecision(2) << sdFree;
        
        std::stringstream streamSdTotal;
        streamSdTotal << std::fixed << std::setprecision(2) << sdTotal;

        std::stringstream streamSdUsed;
        streamSdUsed << std::fixed << std::setprecision(2) << sdUsed;

        // Mount NAND
        FsFileSystem nandFS;
        if (R_SUCCEEDED(fsOpenBisFileSystem(&nandFS, FsBisPartitionId_User, ""))) {
            fsdevMountDevice("user", nandFS);
        }

        double nandFree = (inst::util::GetAvailableSpace("user:/") / 1024) / 1024 / 1024; // GB
        double nandUsed = inst::util::amountOfDiskSpaceUsed("user:/");
        double nandTotal = (inst::util::totalsize("user:/") / 1024) / 1024 / 1024; // GB

        // Unmount NAND
        fsdevUnmountDevice("user");

        std::stringstream streamNandFree;
        streamNandFree << std::fixed << std::setprecision(2) << nandFree;
        
        std::stringstream streamNandTotal;
        streamNandTotal << std::fixed << std::setprecision(2) << nandTotal;

        std::stringstream streamNandUsed;
        streamNandUsed << std::fixed << std::setprecision(2) << nandUsed;

        std::string info = "usage.system_size"_lang + streamNandTotal.str() + "usage.gb"_lang + 
                        "usage.freespace"_lang + streamNandFree.str() + "usage.gb"_lang + 
                        "usage.percent_used"_lang + streamNandUsed.str() + "usage.percent"_lang + 
                        "usage.sd_size"_lang + streamSdTotal.str() + "usage.gb"_lang + 
                        "usage.sd_space"_lang + streamSdFree.str() + "usage.gb"_lang + 
                        "usage.sd_used"_lang + streamSdUsed.str() + "usage.percent_symbol"_lang;

        mainApp->CreateShowDialog("usage.space_info"_lang, info, {"common.ok"_lang}, true);
    }

    void mainMenuThread() {
        static int frame = 0;
        if (frame++ % 30 == 0 && mainApp->mainPage != nullptr) {
            mainApp->mainPage->refreshStatus();
            inst::power::update();
        }
        bool menuLoaded = mainApp->IsShown();
        if (!appletFinished && appletGetAppletType() == AppletType_LibraryApplet) {
            tin::data::NUM_BUFFER_SEGMENTS = 2;
            if (menuLoaded) {
                inst::ui::appletFinished = true;
                mainApp->CreateShowDialog("main.applet.title"_lang, "main.applet.desc"_lang, {"common.ok"_lang}, true);
            } 
        } else if (!appletFinished) {
            inst::ui::appletFinished = true;
            tin::data::NUM_BUFFER_SEGMENTS = 128;
        }
        if (!updateFinished && (!inst::config::autoUpdate || inst::util::getIPAddress() == "1.0.0.127")) updateFinished = true;
        if (!updateFinished && menuLoaded && inst::config::updateInfo.size()) {
            updateFinished = true;
            optionsPage::askToUpdate(inst::config::updateInfo);
        }
    }

    void MainPage::refreshStatus() {
        if (!this->statusSeg[0]) return;
        std::string ip = inst::util::getIPAddress();
        bool online = ip != "1.0.0.127" && !ip.empty();
        bool serving = inst::cheats::isRunning();
        std::string parts[4];
        if (!online) {
            parts[0] = "extra.no_network"_lang;
        } else {
            parts[0] = ip;
            parts[1] = "extra.ssm_link"_lang;
            parts[2] = serving ? "extra.on"_lang : "extra.off"_lang;
            parts[3] = "extra.port"_lang + std::to_string(inst::cheats::kPort) + ")";
        }
        int widths[4];
        int total = 0;
        for (int i = 0; i < 4; i++) {
            this->statusSeg[i]->SetText(parts[i].empty() ? " " : parts[i]);
            if (this->statusBold[i]) this->statusBold[i]->SetText(parts[i].empty() ? " " : parts[i]);
            widths[i] = parts[i].empty() ? 0 : this->statusSeg[i]->GetWidth();
            total += widths[i];
        }
        int x = 1280 - 24 - total;
        for (int i = 0; i < 4; i++) {
            this->statusSeg[i]->SetX(x);
            if (this->statusBold[i]) this->statusBold[i]->SetX(x + 1);
            x += widths[i];
        }
    }

    MainPage::MainPage() : Layout::Layout() {
        std::string bg_colour = "colour.background"_theme;
        std::string tbar_colour = "colour.topbar"_theme;
        std::string bbar_colour = "colour.bottombar"_theme;
        std::string bottombar_text = "colour.bottombar_text"_theme;
        std::string text_colour = "colour.main_text"_theme;
        
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->SetBackgroundColor(COLOR(bg_colour));
        else this->SetBackgroundColor(COLOR(inst::skin::bg()));

        if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else if (!inst::skin::background().empty()) this->SetBackgroundImage(inst::skin::background());
        
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->topRect = Rectangle::New(0, 0, 1280, 94, COLOR(tbar_colour));
        else this->topRect = Rectangle::New(0, 0, 1280, 94, COLOR(inst::skin::topbar()));
        
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->botRect = Rectangle::New(0, 659, 1280, 61, COLOR(bbar_colour));
        else this->botRect = Rectangle::New(0, 659, 1280, 61, COLOR(inst::skin::bar()));

        if (inst::config::gayMode) {
            this->titleImage = Image::New(-113, 0, inst::skin::logo());
            this->appVersionText = TextBlock::New(367, 49, "v" + inst::config::appVersion, 22);
        }
        else {
            this->titleImage = Image::New(0, 0, inst::skin::logo());
            this->appVersionText = TextBlock::New(480, 49, "v" + inst::config::appVersion, 22);
        }
        this->appVersionText->SetColor(COLOR(inst::skin::barText()));
        for (int i = 0; i < 4; i++) {
            this->statusSeg[i] = TextBlock::New(700, 34, " ", 22);
            this->statusSeg[i]->SetColor(COLOR(inst::skin::barText()));
            if (i == 0 || i == 2) {
                this->statusBold[i] = TextBlock::New(701, 34, " ", 22);
                this->statusBold[i]->SetColor(COLOR(inst::skin::barText()));
            }
        }
        this->butText = TextBlock::New(10, 678, "main.buttons"_lang, 24);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->butText->SetColor(COLOR(bottombar_text));
        else this->butText->SetColor(COLOR(inst::skin::barText()));

        // Resize menu for 7 items: Height 80px instead of 94px
        this->optionMenu = pu::ui::elm::Menu::New(0, 95, 1280, COLOR(inst::skin::menuClear()), 80, 7);
        this->optionMenu->SetOnFocusColor(COLOR(inst::skin::focus()));
        this->optionMenu->SetScrollbarColor(COLOR(inst::skin::scroll()));
        
        this->installMenuItem = pu::ui::elm::MenuItem::New("main.menu.sd"_lang);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->installMenuItem->SetColor(COLOR(text_colour));
        else this->installMenuItem->SetColor(COLOR(inst::skin::text()));
        this->installMenuItem->SetIcon(inst::skin::icon("micro-sd.png"));

        // New SSM Menu Item
        this->ssmMenuItem = pu::ui::elm::MenuItem::New("extra.menu_ssm"_lang);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->ssmMenuItem->SetColor(COLOR(text_colour));
        else this->ssmMenuItem->SetColor(COLOR(inst::skin::text()));
        this->ssmMenuItem->SetIcon(inst::skin::icon("ssm.png"));

        this->netInstallMenuItem = pu::ui::elm::MenuItem::New("main.menu.net"_lang);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->netInstallMenuItem->SetColor(COLOR(text_colour));
        else this->netInstallMenuItem->SetColor(COLOR(inst::skin::text()));
        this->netInstallMenuItem->SetIcon(inst::skin::icon("cloud-download.png"));

        this->usbInstallMenuItem = pu::ui::elm::MenuItem::New("main.menu.usb"_lang);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->usbInstallMenuItem->SetColor(COLOR(text_colour));
        else this->usbInstallMenuItem->SetColor(COLOR(inst::skin::text()));
        this->usbInstallMenuItem->SetIcon(inst::skin::icon("usb-port.png"));

        this->sigPatchesMenuItem = pu::ui::elm::MenuItem::New("main.menu.sig"_lang);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->sigPatchesMenuItem->SetColor(COLOR(text_colour));
        else this->sigPatchesMenuItem->SetColor(COLOR(inst::skin::text()));
        this->sigPatchesMenuItem->SetIcon(inst::skin::icon("wrench.png"));

        this->settingsMenuItem = pu::ui::elm::MenuItem::New("main.menu.set"_lang);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->settingsMenuItem->SetColor(COLOR(text_colour));
        else this->settingsMenuItem->SetColor(COLOR(inst::skin::text()));
        this->settingsMenuItem->SetIcon(inst::skin::icon("settings.png"));

        this->exitMenuItem = pu::ui::elm::MenuItem::New("main.menu.exit"_lang);
        if (inst::config::useTheme && std::filesystem::exists(inst::config::appDir + "/theme/theme.json")) this->exitMenuItem->SetColor(COLOR(text_colour));
        else this->exitMenuItem->SetColor(COLOR(inst::skin::text()));
        this->exitMenuItem->SetIcon(inst::skin::icon("exit-run.png"));

        this->Add(this->topRect);
        this->Add(this->botRect);
        this->Add(this->titleImage);
        this->Add(this->appVersionText);
        this->Add(this->butText);
        for (int i = 0; i < 4; i++) {
            this->Add(this->statusSeg[i]);
            if (this->statusBold[i]) this->Add(this->statusBold[i]);
        }
        this->optionMenu->AddItem(this->installMenuItem);
        this->optionMenu->AddItem(this->ssmMenuItem);
        this->optionMenu->AddItem(this->netInstallMenuItem);
        this->optionMenu->AddItem(this->usbInstallMenuItem);
        this->optionMenu->AddItem(this->sigPatchesMenuItem);
        this->optionMenu->AddItem(this->settingsMenuItem);
        this->optionMenu->AddItem(this->exitMenuItem);
        this->Add(this->optionMenu);
        this->AddThread(mainMenuThread);
        this->refreshStatus();
    }

    void MainPage::installMenuItem_Click() {
        mainApp->sdinstPage->drawMenuItems(true, "sdmc:/");
        mainApp->sdinstPage->menu->SetSelectedIndex(0);
        mainApp->LoadLayout(mainApp->sdinstPage);
    }

    void MainPage::ssmInstallMenuItem_Click() {
        if (inst::util::getIPAddress() == "1.0.0.127") {
            inst::ui::mainApp->CreateShowDialog("main.net.title"_lang, "main.net.desc"_lang, {"common.ok"_lang}, true);
            return;
        }
        mainApp->netinstPage->startSSM();
    }

    void MainPage::netInstallMenuItem_Click() {
        if (inst::util::getIPAddress() == "1.0.0.127") {
            inst::ui::mainApp->CreateShowDialog("main.net.title"_lang, "main.net.desc"_lang, {"common.ok"_lang}, true);
            return;
        }
        mainApp->netinstPage->startNetwork();
    }

    void MainPage::usbInstallMenuItem_Click() {
        if (!inst::config::usbAck) {
            if (mainApp->CreateShowDialog("main.usb.warn.title"_lang, "main.usb.warn.desc"_lang, {"common.ok"_lang, "main.usb.warn.opt1"_lang}, false) == 1) {
                inst::config::usbAck = true;
                inst::config::setConfig();
            }
        }
        if (inst::util::usbIsConnected()) mainApp->usbinstPage->startUsb();
        else mainApp->CreateShowDialog("main.usb.error.title"_lang, "main.usb.error.desc"_lang, {"common.ok"_lang}, false);
    }

    void MainPage::sigPatchesMenuItem_Click() {
        sig::installSigPatches();
    }

    void MainPage::exitMenuItem_Click() {
        mainApp->FadeOut();
        mainApp->Close();
    }

    void MainPage::settingsMenuItem_Click() {
        mainApp->LoadLayout(mainApp->optionspage);
    }

    void MainPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        inst::remote::processPending();
        if (((Down & HidNpadButton_Plus) || (Down & HidNpadButton_Minus) || (Down & HidNpadButton_B)) && mainApp->IsShown()) {
            mainApp->FadeOut();
            mainApp->Close();
        }
        if ((Down & HidNpadButton_A) || (Up & TouchPseudoKey)) {
            switch (this->optionMenu->GetSelectedIndex()) {
                case 0:
                    this->installMenuItem_Click();
                    break;
                case 1:
                    this->ssmInstallMenuItem_Click();
                    break;
                case 2:
                    this->netInstallMenuItem_Click();
                    break;
                case 3:
                    MainPage::usbInstallMenuItem_Click();
                    break;
                case 4:
                    MainPage::sigPatchesMenuItem_Click();
                    break;
                case 5:
                    MainPage::settingsMenuItem_Click();
                    break;
                case 6:
                    MainPage::exitMenuItem_Click();
                    break;
                default:
                    break;
            }
        }
        if (Down & HidNpadButton_Y) {
            showSpaceInfo();
        }
    }
}

