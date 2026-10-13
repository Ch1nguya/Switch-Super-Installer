#include <filesystem>
#include <switch.h>
#include <sys/statvfs.h>
#include "ui/MainApplication.hpp"
#include "ui/instPage.hpp"
#include "util/config.hpp"
#include "util/skin.hpp"
#include "util/lang.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

// TinWoo helpers
FsFileSystem* fs;
FsFileSystem devices[4];

double GetSpace(const char* path)
{
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) {
        return -1;
    }
    return (double)stat.f_bsize * stat.f_bavail;
}

namespace inst::ui {
    extern MainApplication *mainApp;

    // TinWoo helpers impl
    Result sdfreespace() {
        devices[0] = *fsdevGetDeviceFileSystem("sdmc");
        fs = &devices[0];
        Result rc = fsOpenSdCardFileSystem(fs);
        double mb = 0;
        if (R_FAILED(rc)) {
            return 0;
        }
        else {
            mb = (GetSpace("sdmc:/") / 1024) / 1024; //megabytes
        }
        return mb;
    }

    Result sysfreespace() {
        FsFileSystem nandFS;
        Result rc = fsOpenBisFileSystem(&nandFS, FsBisPartitionId_User, "");
        fsdevMountDevice("user", nandFS);
        double mb = 0;
        if (R_FAILED(rc)) {
            return 0;
        }
        else {
            mb = (GetSpace("user:/") / 1024) / 1024; //megabytes
        }
        fsdevUnmountDevice("user");
        return mb;
    }

    instPage::instPage() : Layout::Layout() {
        this->SetBackgroundColor(COLOR(inst::skin::installBg()));
        if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else if (!inst::skin::background().empty()) this->SetBackgroundImage(inst::skin::background());
        
        this->topRect = Rectangle::New(0, 0, 1280, 94, COLOR(inst::skin::installTop()));
        this->infoRect = Rectangle::New(0, 95, 1280, 60, COLOR(inst::skin::installInfo()));
        
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
        
        // TinWoo additions positioned roughly
        this->installInfoText = TextBlock::New(15, 640, "", 22); // Moved down
        this->installInfoText->SetColor(COLOR(inst::skin::text()));
        
        this->sdInfoText = TextBlock::New(15, 600, "", 22);
        this->sdInfoText->SetColor(COLOR(inst::skin::text()));

        this->nandInfoText = TextBlock::New(15, 560, "", 22);
        this->nandInfoText->SetColor(COLOR(inst::skin::text()));

        this->countText = TextBlock::New(15, 520, "", 22);
        this->countText->SetColor(COLOR(inst::skin::text()));

        this->installBar = pu::ui::elm::ProgressBar::New(10, 675, 1260, 35, 100.0f); // Resized like TinWoo
        this->installBar->SetColor(COLOR(inst::skin::installBar()));
        if (!inst::skin::installBarFill().empty()) this->installBar->SetProgressColor(COLOR(inst::skin::installBarFill()));
        
        if (std::filesystem::exists(inst::config::appDir + "/awoo_inst.png")) this->awooImage = Image::New(410, 190, inst::config::appDir + "/awoo_inst.png");
        else this->awooImage = Image::New(510, 166, "romfs:/images/awoos/7d8a05cddfef6da4901b20d2698d5a71.png");
        
        this->Add(this->topRect);
        this->Add(this->infoRect);
        this->Add(this->titleImage);
        this->Add(this->appVersionText);
        this->Add(this->pageInfoText);
        this->Add(this->installInfoText);
        this->Add(this->sdInfoText);   // New
        this->Add(this->nandInfoText); // New
        this->Add(this->countText);    // New
        this->Add(this->installBar);
        this->Add(this->awooImage);
        
        if (inst::config::gayMode) this->awooImage->SetVisible(false);
    }

    void instPage::setCoverImage(const std::string &path) {
        if (path.empty() || !std::filesystem::exists(path)) return;
        mainApp->instpage->awooImage->SetImage(path);
        mainApp->instpage->awooImage->SetX(460);
        mainApp->instpage->awooImage->SetY(158);
        mainApp->instpage->awooImage->SetWidth(360);
        mainApp->instpage->awooImage->SetHeight(360);
        mainApp->instpage->awooImage->SetVisible(true);
        mainApp->CallForRender();
    }

    void instPage::setTopInstInfoText(std::string ourText){
        mainApp->instpage->pageInfoText->SetText(ourText);
        mainApp->CallForRender();
    }

    void instPage::filecount(std::string ourText) {
        mainApp->instpage->countText->SetText(ourText);
        mainApp->CallForRender();
    }

    void instPage::setInstInfoText(std::string ourText){
        mainApp->instpage->installInfoText->SetText(ourText);
        // TinWoo Update logic
        std::string info = std::to_string(sdfreespace());
        std::string message = ("inst.net.sd"_lang + info + " MB");
        mainApp->instpage->sdInfoText->SetText(message);
        
        info = std::to_string(sysfreespace());
        message = ("inst.net.nand"_lang + info + " MB");
        mainApp->instpage->nandInfoText->SetText(message);
        
        mainApp->CallForRender();
    }

    void instPage::setInstBarPerc(double ourPercent){
        mainApp->instpage->installBar->SetVisible(true);
        mainApp->instpage->installBar->SetProgress(ourPercent);
        mainApp->CallForRender();
    }

    void instPage::loadMainMenu(){
        mainApp->LoadLayout(mainApp->mainPage);
    }

    void instPage::loadInstallScreen(){
        mainApp->instpage->pageInfoText->SetText("");
        mainApp->instpage->installInfoText->SetText("");
        mainApp->instpage->sdInfoText->SetText("");
        mainApp->instpage->nandInfoText->SetText("");
        mainApp->instpage->countText->SetText("");
        mainApp->instpage->installBar->SetProgress(0);
        mainApp->instpage->installBar->SetVisible(false);
        mainApp->instpage->awooImage->SetVisible(!inst::config::gayMode);
        mainApp->LoadLayout(mainApp->instpage);
        mainApp->CallForRender();
    }

    void instPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
    }
}
