#include "ui/usbInstPage.hpp"
#include "ui/MainApplication.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/skin.hpp"
#include "util/lang.hpp"
#include "util/usb_util.hpp"
#include "usbInstall.hpp"


#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;

    namespace {
        std::string buildUsbAutoStateText() {
            std::string state = "inst.usb.auto_state_off"_lang;
            if (inst::config::autoInstallMode) {
                state = (inst::config::autoInstallTarget == 1) ? "inst.usb.auto_state_nand"_lang : "inst.usb.auto_state_sd"_lang;
            }

            std::string text = "inst.usb.auto_state_prefix"_lang + state + " " + "inst.usb.auto_wait_suffix"_lang;
            if (inst::config::autoInstallMode) text += "    " + ("inst.usb.auto_hint_toggle"_lang);
            else text += "    " + ("inst.usb.auto_hint_lr"_lang);
            return text;
        }
    }

    usbInstPage::usbInstPage() : Layout::Layout() {
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
        this->pageInfoText = TextBlock::New(10, 109, "", 24);
        this->pageInfoText->SetColor(COLOR(inst::skin::barText()));
        this->butText = TextBlock::New(10, 678, "", 24);
        this->butText->SetColor(COLOR(inst::skin::barText()));
        this->menu = pu::ui::elm::Menu::New(0, 156, 1280, COLOR(inst::skin::menuClear()), 84, (506 / 84));
        this->menu->SetOnFocusColor(COLOR(inst::skin::focus()));
        this->menu->SetScrollbarColor(COLOR(inst::skin::scrollSoft()));
        this->infoImage = Image::New(460, 332, inst::skin::icon("usb-connection-waiting.png"));
        this->Add(this->topRect);
        this->Add(this->infoRect);
        this->Add(this->botRect);
        this->Add(this->titleImage);
        this->Add(this->appVersionText);
        this->Add(this->butText);
        this->Add(this->pageInfoText);
        this->Add(this->menu);
        this->Add(this->infoImage);
    }

    void usbInstPage::drawMenuItems(bool clearItems) {
        if (clearItems) this->selectedTitles = {};
        this->menu->ClearItems();
        for (auto& url: this->ourTitles) {
            std::string itm = inst::util::shortenString(inst::util::formatUrlString(url), 56, true);
            auto ourEntry = pu::ui::elm::MenuItem::New(itm);
            ourEntry->SetColor(COLOR(inst::skin::text()));
            ourEntry->SetIcon(inst::skin::icon("checkbox-blank-outline.png"));
            for (long unsigned int i = 0; i < this->selectedTitles.size(); i++) {
                if (this->selectedTitles[i] == url) {
                    ourEntry->SetIcon(inst::skin::icon("check-box-outline.png"));
                }
            }
            this->menu->AddItem(ourEntry);
        }
    }

    void usbInstPage::selectTitle(int selectedIndex) {
        std::string targetTitle = this->ourTitles[selectedIndex];
        bool alreadySelected = false;
        size_t selectedIdx = 0;

        for (size_t i = 0; i < this->selectedTitles.size(); i++) {
            if (this->selectedTitles[i] == targetTitle) {
                alreadySelected = true;
                selectedIdx = i;
                break;
            }
        }

        if (alreadySelected) {
            this->selectedTitles.erase(this->selectedTitles.begin() + selectedIdx);
        } else {
            this->selectedTitles.push_back(targetTitle);
        }

        this->drawMenuItems(false);
    }

    void usbInstPage::startUsb() {
        this->pageInfoText->SetText("inst.usb.top_info"_lang + "\n" + ("inst.usb.auto_hint_lr"_lang));
        this->butText->SetText("inst.usb.buttons"_lang + "inst.usb.auto_buttons_suffix"_lang);
        this->menu->SetVisible(false);
        this->menu->ClearItems();
        this->infoImage->SetVisible(true);
        mainApp->LoadLayout(mainApp->usbinstPage);

        if (inst::config::autoInstallMode) {
            this->pageInfoText->SetText(buildUsbAutoStateText());
        }

        mainApp->CallForRender();
        this->ourTitles = usbInstStuff::OnSelected();
        if (!this->ourTitles.size()) {
            inst::config::autoInstallMode = false; // Reset
            mainApp->LoadLayout(mainApp->mainPage);
            return;
        } 
        
        if (inst::config::autoInstallMode) {
            usbInstStuff::installTitleUsb(this->ourTitles, inst::config::autoInstallTarget);
            this->startUsb();
            return;
        }

        else {
            mainApp->CallForRender(); // If we re-render a few times during this process the main screen won't flicker
            this->pageInfoText->SetText("inst.usb.top_info2"_lang);
            this->butText->SetText("inst.usb.buttons2"_lang);
            this->drawMenuItems(true);
            this->menu->SetSelectedIndex(0);
            mainApp->CallForRender();
            this->infoImage->SetVisible(false);
            this->menu->SetVisible(true);
        }
        return;
    }

    void usbInstPage::startInstall() {
        int dialogResult = -1;
        if (this->selectedTitles.size() == 1) dialogResult = mainApp->CreateShowDialog("inst.target.desc0"_lang + inst::util::shortenString(inst::util::formatUrlString(this->selectedTitles[0]), 32, true) + "inst.target.desc1"_lang, "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
        else dialogResult = mainApp->CreateShowDialog("inst.target.desc00"_lang + std::to_string(this->selectedTitles.size()) + "inst.target.desc01"_lang, "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
        if (dialogResult == -1) return;
        usbInstStuff::installTitleUsb(this->selectedTitles, dialogResult);
        return;
    }

    void usbInstPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        if (Down & HidNpadButton_B) {
            tin::util::USBCmdManager::SendExitCmd();
            mainApp->LoadLayout(mainApp->mainPage);
        }
        if ((Down & HidNpadButton_A) || (Up & TouchPseudoKey)) {
            this->selectTitle(this->menu->GetSelectedIndex());
        }
        if ((Down & HidNpadButton_Y)) {
            if (this->selectedTitles.size() == this->menu->GetItems().size()) this->drawMenuItems(true);
            else {
                for (long unsigned int i = 0; i < this->menu->GetItems().size(); i++) {
                    if (this->menu->GetItems()[i]->GetIcon() == inst::skin::icon("check-box-outline.png")) continue;
                    else this->selectTitle(i);
                }
                this->drawMenuItems(false);
            }
        }
        if (Down & HidNpadButton_Plus) {
            if (this->selectedTitles.size() == 0) {
                this->selectTitle(this->menu->GetSelectedIndex());
                this->startInstall();
                return;
            }
            this->startInstall();
        }
    }
}
