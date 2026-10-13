#pragma once
#include <pu/Plutonium>

using namespace pu::ui::elm;
namespace inst::ui {
    class MainPage : public pu::ui::Layout
    {
        public:
            MainPage();
            PU_SMART_CTOR(MainPage)
            void installMenuItem_Click();
            void ssmInstallMenuItem_Click();
            void netInstallMenuItem_Click();
            void usbInstallMenuItem_Click();
            void sigPatchesMenuItem_Click();
            void settingsMenuItem_Click();
            void exitMenuItem_Click();
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            Image::Ref titleImage;
            TextBlock::Ref appVersionText;
            // Status line (top right): IP and server state. Segments 0 and 2 are drawn twice, one pixel
            // apart, to look bold (the UI library has a single font weight).
            TextBlock::Ref statusSeg[4];
            TextBlock::Ref statusBold[4];
            void refreshStatus();
        private:
            bool appletFinished;
            bool updateFinished;
            TextBlock::Ref butText;
            Rectangle::Ref topRect;
            Rectangle::Ref botRect;
            pu::ui::elm::Menu::Ref optionMenu;
            pu::ui::elm::MenuItem::Ref installMenuItem;
            pu::ui::elm::MenuItem::Ref ssmMenuItem;
            pu::ui::elm::MenuItem::Ref netInstallMenuItem;
            pu::ui::elm::MenuItem::Ref usbInstallMenuItem;
            pu::ui::elm::MenuItem::Ref sigPatchesMenuItem;
            pu::ui::elm::MenuItem::Ref settingsMenuItem;
            pu::ui::elm::MenuItem::Ref exitMenuItem;
    };
}