
/*

    Plutonium library

    @file Button.hpp
    @brief A Button is an Element for option selecting.
    @author XorTroll

    @copyright Plutonium project - an easy-to-use UI framework for Nintendo Switch homebrew

*/

#pragma once
#include <functional>
#include <pu/ui/elm/elm_Element.hpp>

namespace pu::ui::elm
{
    class Button : public Element
    {
        public:
            Button(s32 X, s32 Y, s32 Width, s32 Height, const std::string& Content, Color TextColor, Color Color);
            PU_SMART_CTOR(Button)
            ~Button();

            s32 GetX();
            void SetX(s32 X);
            s32 GetY();
            void SetY(s32 Y);
            s32 GetWidth();
            void SetWidth(s32 Width);
            s32 GetHeight();
            void SetHeight(s32 Height);
            std::string GetContent();
            void SetContent(const std::string& Content);
            Color GetColor();
            void SetColor(Color Color);
            void SetFontSize(s32 FontSize);
            void SetOnClick(std::function<void()> ClickCallback);
            void OnRender(render::Renderer::Ref &Drawer, s32 X, s32 Y);
            void OnInput(u64 Down, u64 Up, u64 Held, Touch Pos);
        private:
            s32 x;
            s32 y;
            s32 w;
            s32 h;
            render::NativeFont font = nullptr;
            render::NativeFont meme = nullptr;
            Color clr;
            std::string cnt;
            std::function<void()> clickcb;
            bool hover;
            s32 hoverfact;
            render::NativeTexture ntex = nullptr;
    };
}