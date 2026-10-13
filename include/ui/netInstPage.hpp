#pragma once
#include <pu/Plutonium>
#include <atomic>

using namespace pu::ui::elm;
namespace inst::ui {
    class netInstPage : public pu::ui::Layout
    {
        public:
            netInstPage();
            PU_SMART_CTOR(netInstPage)
            void startInstall(bool urlMode);
            void startNetwork();
            bool startNetworkOnce();
            void startSSM();
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            TextBlock::Ref pageInfoText;
            Image::Ref titleImage;
            TextBlock::Ref appVersionText;
        private:
            std::vector<std::string> ourUrls;
            std::vector<std::string> imageUrls;
            std::vector<std::string> coverUrls;
            std::vector<std::string> selectedUrls;
            std::vector<std::string> alternativeNames;
            TextBlock::Ref butText;
            Rectangle::Ref topRect;
            Rectangle::Ref infoRect;
            Rectangle::Ref botRect;
            pu::ui::elm::Menu::Ref menu;
            Image::Ref infoImage;
            ProgressBar::Ref progressBar;
            std::atomic<bool> isSsmMode = false;
            std::atomic<bool> isSsmLoading = false;
            void drawMenuItems(bool clearItems);
            void selectTitle(int selectedIndex);
            void ssmAsyncTick();
            void updateAutoInstallWaitLabel();
    };
}
