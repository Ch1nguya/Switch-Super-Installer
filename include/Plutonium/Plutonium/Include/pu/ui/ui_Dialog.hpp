
/*

    Plutonium library

    @file ui_Dialog.hpp
    @brief A Dialog is an easy way to ask the user to choose between several options.
    @author XorTroll

    @copyright Plutonium project - an easy-to-use UI framework for Nintendo Switch homebrew

*/

#pragma once
#include <pu/ui/render/render_Renderer.hpp>
#include <vector>

namespace pu::ui
{
    class Dialog
    {
        public:
            Dialog(const std::string& Title, const std::string& Content);
            PU_SMART_CTOR(Dialog)
            ~Dialog();

            void AddOption(const std::string& Name);
            void SetCancelOption(const std::string& Name = "Cancel");
            void RemoveCancelOption();
            bool HasCancelOption();
            void SetIcon(const std::string& Icon);
            bool Hasicon();
            s32 Show(render::Renderer::Ref &Drawer, void *App);
            bool UserCancelled();
            bool IsOk();
        private:
            bool hcancel;
            std::string scancel;
            render::NativeFont titleFont;
            render::NativeFont contentFont;
            render::NativeFont optionsFont;
            render::NativeFont titleFontEx;
            render::NativeFont contentFontEx;
            render::NativeFont optionsFontEx;
            std::string stitle;
            std::string scnt;
            render::NativeTexture title = nullptr;
            render::NativeTexture cnt = nullptr;
            std::vector<std::string> sopts;
            std::vector<render::NativeTexture> opts;
            s32 osel;
            bool cancel;
            bool hicon;
            render::NativeTexture icon = nullptr;
            s32 prevosel;
            s32 pselfact;
            s32 selfact;
    };
}