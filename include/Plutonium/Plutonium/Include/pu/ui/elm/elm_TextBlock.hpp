
/*

    Plutonium library

    @file TextBlock.hpp
    @brief A TextBlock is a very useful Element which is used to draw text on the screen.
    @author XorTroll

    @copyright Plutonium project - an easy-to-use UI framework for Nintendo Switch homebrew

*/

#pragma once
#include <pu/ui/elm/elm_Element.hpp>

namespace pu::ui::elm
{
    class TextBlock : public Element
    {
        public:
            TextBlock(s32 X, s32 Y, const std::string& Text, s32 FontSize = 25);
            PU_SMART_CTOR(TextBlock)
            ~TextBlock();

            s32 GetX();
            void SetX(s32 X);
            s32 GetY();
            void SetY(s32 Y);
            s32 GetWidth();
            s32 GetHeight();
            s32 GetTextWidth();
            s32 GetTextHeight();
            std::string GetText();
            void SetText(const std::string& Text);
            void SetFontSize(s32 FontSize);
            Color GetColor();
            void SetColor(Color Color);
            void OnRender(render::Renderer::Ref &Drawer, s32 X, s32 Y);
            void OnInput(u64 Down, u64 Up, u64 Held, Touch Pos);
        private:
            std::string text;
            s32 x;
            s32 y;
            render::NativeFont font;
            render::NativeFont meme;
            Color clr;
            render::NativeTexture ntex = nullptr;
    };
}